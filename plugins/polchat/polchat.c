/*
 *  (C) Copyright 2006	Jakub 'ABUKAJ' Kowalski
 *  (C) Copyright 2006, 2008 Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <sys/stat.h>
#define __USE_POSIX
#include <netdb.h>

#include <sys/time.h>

#ifdef __sun
#include <sys/filio.h>
#endif

#include <string.h>
#include <errno.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/userlist.h>

#include <ekg/queries.h>

#define DEFQUITMSG "EKG2 - It's better than sex!"
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)

typedef struct {
	int fd;
	int connecting;

	char *nick;
	char *room;
	char *newroom;

	string_t recvbuf;
} polchat_private_t;

static int polchat_theme_init();

PLUGIN_DEFINE(polchat, PLUGIN_PROTOCOL, polchat_theme_init);

#define POLCHAT_DEFAULT_HOST "s1.polchat.pl"
#define POLCHAT_DEFAULT_PORT "14003"

/* HELPERS */
static inline char *dword_str(int dword) {	/* 4 bajty BE */
	static unsigned char buf[4];
	buf[0] = (dword & 0xff000000) >> 24;
	buf[1] = (dword & 0x00ff0000) >> 16;
	buf[2] = (dword & 0x0000ff00) >> 8;
	buf[3] = (dword & 0x000000ff);

	return (char *) buf;
}

static inline char *word_str(short word) {	/* 2 bajty BE */
	static unsigned char buf[2];
	buf[0] = (word & 0xff00) >> 8;
	buf[1] = (word & 0x00ff);

	return (char *) buf;
}

/* w data rozmiar danych do wyslania */

static WATCHER_LINE(polchat_handle_write) {
	static time_t t = 0;	/* last time_t execute of this function */

	size_t fulllen = (size_t) data;
	int len;

	if (type) 
		return 0;

	if (t == time(NULL)) 
		return 0;	/* flood-protection [XXX] */

	len = write(fd, watch, fulllen);

	if (len == fulllen) {	/* we sent all data, ok.. */
		watch_t *next_watch = NULL;
		list_t l;

		/* turn on next watch */

		for (l = watches; l; l = l->next) {	/* watche sa od najnowszego po najstarszy.. dlatego musimy znalezc ostatni... */
			watch_t *w = l->data;

			if (w && w->fd == fd && w->type == WATCH_NONE) 
				next_watch = w;
		}

		if (next_watch) 
			next_watch->type = WATCH_WRITE;	/* turn on watch */
		t = time(NULL);
		errno = 0;

	} else if (len > 0) {
		list_t l;

		for (l = watches; l; l = l->next) {
			watch_t *w = l->data;

			if (w && w->fd == fd && w->type == WATCH_WRITE_LINE && w->data == data) { /* this watch */
				w->data = (void *) fulllen - len;
				break;
			}
		}
	}

	return (fulllen == len) ? -1 : len;
}

static watch_t *polchat_sendpkt(session_t *s, short headercode, ...)  {
	va_list ap;

	watch_t *w;
	polchat_private_t *j;
	int fd;

	char **arr = NULL;
	char *tmp;
	int size;
	int i;
/* XXX, headercode brzydko */

	if (!s || !(j = s->priv)) {
		debug_error("polchat_sendpkt() Invalid params\n");
		return NULL;
	}

	fd = j->fd;

	if (watch_find(&polchat_plugin, fd, WATCH_WRITE_LINE)) {
		w = watch_add_line(&polchat_plugin, fd, WATCH_WRITE_LINE, polchat_handle_write, NULL);
		w->type = WATCH_NONE;

	} else 
		w = watch_add_line(&polchat_plugin, fd, WATCH_WRITE_LINE, polchat_handle_write, NULL);

	size = 8;	/* size [4 bytes] + headers [4 bytes] */
	
	if (headercode) 
		size += (2 * 1);

	va_start(ap, headercode);
	while ((tmp = va_arg(ap, char *))) {
		char *r;

	/* XXX, use cache */
		if ((r = ekg_convert_string(tmp, NULL, "UTF-8"))) {
			array_add(&arr, r);
			size += strlen(r) + 3;
		} else {
			array_add(&arr, xstrdup(tmp));
			size += strlen(tmp) + 3;
		}
	}
	va_end(ap);

	string_append_raw(w->buf, dword_str(size), 4);
	string_append_raw(w->buf, word_str(headercode ? 1 : 0), 2);	/* headerlen / 256 + headerlen % 256 */
	string_append_raw(w->buf, word_str(array_count(arr)), 2);

/* headers */
	if (headercode)
		string_append_raw(w->buf, word_str(headercode), 2);

	if (arr) {
		for (i = 0; arr[i]; i++) {
			size_t len = xstrlen(arr[i]);
			string_append_raw(w->buf, word_str(len), 2);	/* LEN */
			string_append_n(w->buf, arr[i], len);		/* str */
			string_append_c(w->buf, '\0');			/* NUL */
		}
		array_free(arr);
	}
	
	w->data = (void *) w->buf->len;
	return w;
}

static watch_t *polchat_sendmsg(session_t *s, const char *message, ...) {
	va_list ap;
	char *msg;

	watch_t *res;
	
	va_start(ap, message);
	msg = vsaprintf(message, ap);
	va_end(ap);

	res = polchat_sendpkt(s, 0x019a, msg, NULL);
	
	xfree(msg);
	return res;
}

static QUERY(polchat_validate_uid) {
	char	*uid 	= *(va_arg(ap, char **));
	int	*valid 	= va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "polchat:", 8) && uid[8]) {
		(*valid)++;
		return -1;
	}

	return 0;
}

static QUERY(polchat_print_version) {
	 print("generic", 
	 	"polchat plugin, proto code based on AmiX v0.2 (http://213.199.197.135/~kowalskijan/amix/) (c ABUKAJ) "
		"and on http://eter.sytes.net/polchatproto/ v0.3");
	 return 0;
} 

static QUERY(polchat_session_init) {
	char		*session = *(va_arg(ap, char**));
	session_t	*s = session_find(session);
	polchat_private_t *j;

	if (!s || s->priv || s->plugin != &polchat_plugin)
		return 1;

	j = xmalloc(sizeof(polchat_private_t));
	j->fd = -1;
	j->recvbuf = string_init(NULL);

	s->priv = j;
	
	return 0;
}

static QUERY(polchat_session_deinit) {
	char		*session = *(va_arg(ap, char**));
	session_t	*s = session_find(session);
	polchat_private_t *j;

	if (!s || !(j = s->priv) || s->plugin != &polchat_plugin)
		return 1;

	s->priv = NULL;

	string_free(j->recvbuf, 1);

	xfree(j->newroom);
	xfree(j->room);
	xfree(j->nick);
	xfree(j);

	return 0;
}

static void polchat_handle_disconnect(session_t *s, const char *reason, int type) {
	polchat_private_t *j;

	if (!s || !(j = s->priv))
		return;

	if (!s->connected && !j->connecting)
		return;
	
	j->connecting = 0;
	userlist_free(s);
	{
		char *__session = xstrdup(session_uid_get(s));
		char *__reason = xstrdup(reason);

		query_emit_id(NULL, PROTOCOL_DISCONNECTED, &__session, &__reason, &type);

		xfree(__session);
		xfree(__reason);
	}

	if (j->fd != -1) {
		list_t l;

		for (l = watches; l; l = l->next) {
			watch_t *w = l->data;

			if (!w || w->fd != j->fd) continue;

			if (1 /* || w->type == WATCH_NONE || w->type == WATCH_WRITE_LINE */)
				watch_free(w);
		}

		close(j->fd);
		j->fd = -1;
	}
}

#include "polchat_handlers.c"
/* extern void polchat_processpkt(session_t *s, unsigned short nheaders, unsigned short nstrings, unsigned char *data, size_t len); */

static WATCHER_SESSION(polchat_handle_stream) {
	polchat_private_t *j; 
	char buf[1024];
	int len;

	if (type) {
		polchat_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK);
		return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	if ((len = read(fd, buf, sizeof(buf))) > 0) {
		unsigned char *buffer;
		
		debug("polchat_handle_stream() read %d bytes from fd\n", len);

		string_append_raw(j->recvbuf, buf, len);

		buffer = (unsigned char *) j->recvbuf->str;

		while (j->recvbuf->len >= 8) {
			unsigned int rlen = (buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3]);

			debug("polchat_handle_stream() rlen: %u buflen: %d\n", rlen, j->recvbuf->len);

			if (rlen < 8) {	/* bad packet */
				debug_error("polchat_handle_stream() RECV BAD PACKET rlen < 8\n");
				return -1;
			}

			if (rlen > 1024 * 1024) {
				debug_error("polchat_handle_stream() RECV BAD PACKET rlen > 1MiB\n");
				return -1;
			}

			if (j->recvbuf->len >= rlen) {
				short headerlen	= buffer[4] << 8 | buffer[5];
				short nstrings	= buffer[6] << 8 | buffer[7];

				if (!headerlen && !nstrings) {
					debug_error("polchat_handle_stream() <blink> CONNECTION LOST :-( </blink>");
					return -1;
				}

				polchat_processpkt(s, headerlen, nstrings, &buffer[8], rlen-8);

				string_remove(j->recvbuf, rlen);
			} else
				break;
		}
		return 0;
	}

	debug("polchat_handle_stream() Connection closed/ error XXX\n");
	return -1;
}

static WATCHER_SESSION(polchat_handle_connect) {
	polchat_private_t *j;
	const char *tmp;

        int res = 0;
	socklen_t res_size = sizeof(res);

	if (type)
		return 0;

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		polchat_handle_disconnect(s, strerror(res), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	if (!s || !(j = s->priv))
		return -1;

	/* here we shouldn't have any WATCH_WRITE watch */

	j->connecting = 2;

	polchat_sendpkt(s, 0x0578, 
		j->nick,						/* nickname */
		((tmp = session_get(s, "password")) ? tmp : ""),	/* password */
		"",							/* XXX cookie, always NUL? */
		j->newroom + 8,						/* pokoj */
	/* XXX: */
		"http://www.polchat.pl/chat/room.phtml/?room=AmiX",	/* referer */
		"polchat.pl",						/* adres serwera */
		"nlst=1&nnum=1&jlmsg=true&ignprv=false",		/* konfiguracja */
		"ekg2-CVS-polchat",					/* klient */
		NULL);

	watch_add_session(s, fd, WATCH_READ, polchat_handle_stream);
	return -1;
}

static WATCHER(polchat_handle_resolver) {
	session_t *s = session_find((char *) data);
	polchat_private_t *j;

	struct sockaddr_in sin;
	struct in_addr a;

	int one = 1;
	int port;
	int res;

        if (type) {
		xfree(data);
		close(fd);
                return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	res = read(fd, &a, sizeof(a));

	if ((res != sizeof(a)) || (res && a.s_addr == INADDR_NONE /* INADDR_NONE kiedy NXDOMAIN */)) {
		if (res == -1)
			debug_error("[polchat] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug_error("[polchat] read %d bytes from resolver. not good\n", res);

		/* no point in reconnecting by polchat_handle_disconnect() */

		print("conn_failed", format_find("conn_failed_resolving"), session_name(s));
		j->connecting = 0;
		return -1;
	}

        debug_function("[polchat] resolved to %s\n", inet_ntoa(a));

	port = session_int_get(s, "port");
	if (port < 0 || port > 65535) 
		port = atoi(POLCHAT_DEFAULT_PORT);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[polchat] socket() failed: %s\n", strerror(errno));
		polchat_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_FAILURE); 
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
        sin.sin_addr.s_addr = a.s_addr;

        if (ioctl(fd, FIONBIO, &one) == -1) 
		debug_error("[polchat] ioctl() FIONBIO failed: %s\n", strerror(errno));
        if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) == -1) 
		debug_error("[polchat] setsockopt() SO_KEEPALIVE failed: %s\n", strerror(errno));

	res = connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)); 

	if (res == -1 && errno != EINPROGRESS) {
		int err = errno;

                debug_error("[polchat] connect() failed: %s (errno=%d)\n", strerror(err), err);
		polchat_handle_disconnect(s, strerror(err), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	j->fd = fd;

	watch_add_session(s, fd, WATCH_WRITE, polchat_handle_connect);

	return -1;
}

static COMMAND(polchat_command_connect) {
	polchat_private_t *j = session->priv;
	const char *server;
	const char *nick;
	const char *room;

	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (!(server = session_get(session, "server"))) {
		server = POLCHAT_DEFAULT_HOST;
		return -1;
	}

	if (!(nick = session_get(session, "nickname"))) {
		printq("generic_error", "gdzie lecimy ziom ?! [/session nickname]");
		return -1;
	}

	if (!(room = session_get(session, "room"))) {
		room = session->uid + 8;
	}

	if (!(*room)) {
		printq("generic_error", "gdzie lecimy ziom ?! [/session room]");
		return -1;
	}

	xfree(j->room);
	j->room = NULL;

	xfree(j->nick);
	j->nick = xstrdup(nick);

	xfree(j->newroom);
	j->newroom = saprintf("polchat:%s", room);

	string_clear(j->recvbuf);

	j->connecting = 1;

	if (ekg_resolver2(&polchat_plugin, server, polchat_handle_resolver, xstrdup(session->uid)) == NULL) {
		print("generic_error", strerror(errno));
		j->connecting = 0;
		return -1;
	}

	printq("connecting", session_name(session));


	return 0;
}

static COMMAND(polchat_command_disconnect) {
	polchat_private_t *j = session->priv;
	const char *reason = params[0]?params[0]:QUITMSG(session);

	if (timer_remove_session(session, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!j->connecting && !session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (reason && session_connected_get(session)) {
		polchat_sendmsg(session, "/quit %s", reason);
	}

	if (j->connecting)
		polchat_handle_disconnect(session, reason, EKG_DISCONNECT_STOPPED);
	else    
		polchat_handle_disconnect(session, reason, EKG_DISCONNECT_USER);

	return 0;
}

static COMMAND(polchat_command_reconnect) {
	polchat_private_t   *j = session->priv;

	if (j->connecting || session_connected_get(session))
		polchat_command_disconnect(name, params, session, target, quiet);

	return polchat_command_connect(name, params, session, target, quiet);
}

static COMMAND(polchat_command_msg) {
	/* w target -> target */
	/* NOTE: sending `/quit` msg disconnect session */	/* XXX, escape? */

/*	polchat_sendpkt(session, 0x019a, params[1], NULL); */
	polchat_sendmsg(session, "%s", params[1]);

	return 0;
}

static COMMAND(polchat_command_inline_msg) {
	const char	*p[2] = { NULL, params[0] };

	if (!session->connected)
		return -1;

	if (!target || !params[0])
		return -1;

	return polchat_command_msg(("msg"), p, session, target, quiet);
}

static COMMAND(polchat_command_part) {
	polchat_private_t   *j = session->priv;

	if (!j->room) {
		printq("invalid_params", name);
		return 0;
	}

	polchat_sendmsg(session, "/part");

	return 0;
}

static COMMAND(polchat_command_join) {
	polchat_private_t   *j = session->priv;

	if (j->newroom) {
		debug_error("/join but j->newroom: %s\n", j->newroom);

		printq("generic_error", "Too fast, or please look at debug.");
		return 0;
	}

	polchat_sendmsg(session, "/join %s", params[0]);

	j->newroom = saprintf("polchat:%s", params[0]);

	return 0;
}

static int polchat_theme_init() {
#ifndef NO_DEFAULT_THEME
/*
	format_add("polchat_joined",		_("%> %Y%2%n has joined %3"), 1);
	format_add("polchat_joined_you",	_("%> %RYou%n have joined %3"), 1);
 */
#endif
	return 0;
}

static plugins_params_t polchat_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		VAR_STR, NULL, 0, NULL), 
	PLUGIN_VAR_ADD("auto_connect", 		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("log_formats", 		VAR_STR, "irssi", 0, NULL),
	PLUGIN_VAR_ADD("nickname", 		VAR_STR, NULL, 0, NULL), 
	PLUGIN_VAR_ADD("password", 		VAR_STR, NULL, 1, NULL),
	PLUGIN_VAR_ADD("port", 			VAR_INT, POLCHAT_DEFAULT_PORT, 0, NULL),
	PLUGIN_VAR_ADD("room",			VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("server", 		VAR_STR, POLCHAT_DEFAULT_HOST, 0, NULL),
	PLUGIN_VAR_END()
};

EXPORT int polchat_plugin_init(int prio) {
	polchat_plugin.params = polchat_plugin_vars;

	plugin_register(&polchat_plugin, prio);

	query_connect_id(&polchat_plugin, PROTOCOL_VALIDATE_UID, polchat_validate_uid, NULL);
	query_connect_id(&polchat_plugin, SESSION_ADDED, polchat_session_init, NULL);
	query_connect_id(&polchat_plugin, SESSION_REMOVED, polchat_session_deinit, NULL);
	query_connect_id(&polchat_plugin, PLUGIN_PRINT_VERSION, polchat_print_version, NULL);

#if 0
	query_connect(&irc_plugin, ("ui-window-kill"),	irc_window_kill, NULL);
	query_connect(&irc_plugin, ("irc-topic"),	irc_topic_header, NULL);
	query_connect(&irc_plugin, ("status-show"),	irc_status_show_handle, NULL);
#endif

#define POLCHAT_ONLY 		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define POLCHAT_FLAGS 		POLCHAT_ONLY | SESSION_MUSTBECONNECTED
#define POLCHAT_FLAGS_TARGET	POLCHAT_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET
	
	command_add(&polchat_plugin, "polchat:", "?",		polchat_command_inline_msg, POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:msg", "!uUw !",	polchat_command_msg,	    POLCHAT_FLAGS_TARGET, NULL);
	command_add(&polchat_plugin, "polchat:connect", NULL,   polchat_command_connect,    POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:disconnect", "r ?",polchat_command_disconnect,POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:reconnect", "r ?", polchat_command_reconnect, POLCHAT_ONLY, NULL);

	command_add(&polchat_plugin, "polchat:part", "r",	polchat_command_part, POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:join", "!uUw",	polchat_command_join, POLCHAT_FLAGS_TARGET, NULL);

	return 0;
}

static int polchat_plugin_destroy() {
	plugin_unregister(&polchat_plugin);
	return 0;
}

