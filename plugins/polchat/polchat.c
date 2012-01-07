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

#include "ekg2.h"

#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <sys/stat.h>
#include <netdb.h>

#include <sys/time.h>

#ifdef __sun
#include <sys/filio.h>
#endif

#include <string.h>
#include <errno.h>

#include <ekg/net.h>

#define DEFPARTMSG "EKG2 bejbi! http://ekg2.org/"
#define DEFQUITMSG "EKG2 - It's better than sex!"

#define SGPARTMSG(x) session_get(x, "PART_MSG")
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")

#define PARTMSG(x,r) (r?r: SGPARTMSG(x)?SGPARTMSG(x):DEFPARTMSG)
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)

#define DEFPART

#define polchat_uid(target) protocol_uid("polchat", target)

typedef struct {
	GCancellable *connect_cancellable;
	GDataOutputStream *out_stream;
	GString *recvbuf;

	char *nick;
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

static int polchat_sendpkt(session_t *s, short headercode, ...)  {
	va_list ap;

	polchat_private_t *j;

	char **arr = NULL;
	char *tmp;
	int size;
	int i;
	GString *buf;

/* XXX, headercode brzydko */

	if (!s || !(j = s->priv)) {
		debug_error("polchat_sendpkt() Invalid params\n");
		return -1;
	}

	size = 8;	/* size [4 bytes] + headers [4 bytes] */

	if (headercode)
		size += (2 * 1);

	va_start(ap, headercode);
	while ((tmp = va_arg(ap, char *))) {
		char *r = ekg_locale_to_utf8_dup(tmp);

		array_add(&arr, r);
		size += strlen(r) + 3;
	}
	va_end(ap);

	buf = g_string_new(NULL);
	g_string_append_len(buf, dword_str(size), 4);
	g_string_append_len(buf, word_str(headercode ? 1 : 0), 2);	/* headerlen / 256 + headerlen % 256 */
	g_string_append_len(buf, word_str(g_strv_length(arr)), 2);

/* headers */
	if (headercode)
		g_string_append_len(buf, word_str(headercode), 2);

	if (arr) {
		for (i = 0; arr[i]; i++) {
			size_t len = xstrlen(arr[i]);
			g_string_append_len(buf, word_str(len), 2);	/* LEN */
			g_string_append_len(buf, arr[i], len);		/* str */
			g_string_append_c(buf, '\0');			/* NUL */
		}
		g_strfreev(arr);
	}

	ekg_connection_write_buf(j->out_stream, buf->str, buf->len);
	g_string_free(buf, TRUE);
	return 0;
}

static int polchat_sendmsg(session_t *s, const char *message, ...) {
	va_list ap;
	char *msg;

	int res;

	va_start(ap, message);
	msg = vsaprintf(message, ap);
	va_end(ap);

	res = polchat_sendpkt(s, 0x019a, msg, NULL);

	xfree(msg);
	return res;
}

static int polchat_send_target_msg(session_t *s, const char *target, const char *message, ...) {
	va_list ap;
	char *msg;

	int res;

	va_start(ap, message);
	msg = vsaprintf(message, ap);
	va_end(ap);

	res = polchat_sendpkt(s, 0x019a, msg, target, NULL);

	xfree(msg);
	return res;
}

static QUERY(polchat_validate_uid) {
	char	*uid	= *(va_arg(ap, char **));
	int	*valid	= va_arg(ap, int *);

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

	xfree(j->nick);
	xfree(j);

	return 0;
}

static void polchat_handle_disconnect(session_t *s, const char *reason, int type) {
	polchat_private_t *j;

	if (!s || !(j = s->priv))
		return;

	if (!s->connected && !s->connecting)
		return;

	userlist_free(s);

	protocol_disconnected_emit(s, reason, type);

}

#include "polchat_handlers.inc"
/* extern void polchat_processpkt(session_t *s, unsigned short nheaders, unsigned short nstrings, unsigned char *data, size_t len); */

static void polchat_handle_stream(GDataInputStream *input, gpointer data) {
	session_t *s = data;
	polchat_private_t *j = NULL;
	gsize count;

	if (!s || !(j = s->priv)) {
		debug_error("polchat_handle_stream() s: 0x%x j: 0x%x\n", s, j);
		return;
	}

	count = g_buffered_input_stream_get_available(G_BUFFERED_INPUT_STREAM(input));

	debug_function("polchat_handle_stream() read %d bytes\n", count);

	if (count>0) {
		unsigned char *buffer;
		char *tmp = g_malloc (count);
		gssize res = g_input_stream_read(G_INPUT_STREAM(input), tmp, count, NULL, NULL);

		g_string_append_len(j->recvbuf, tmp, res);
		g_free(tmp);

		buffer = (unsigned char *) j->recvbuf->str;

		while (j->recvbuf->len >= 8) {
			unsigned int rlen = (buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3]);

			debug("polchat_handle_stream() rlen: %u buflen: %d\n", rlen, j->recvbuf->len);

			if (rlen < 8) {	/* bad packet */
				debug_error("polchat_handle_stream() RECV BAD PACKET rlen < 8\n");
				return;
			}

			if (rlen > 1024 * 1024) {
				debug_error("polchat_handle_stream() RECV BAD PACKET rlen > 1MiB\n");
				return;
			}

			if (j->recvbuf->len >= rlen) {
				short headerlen	= buffer[4] << 8 | buffer[5];
				short nstrings	= buffer[6] << 8 | buffer[7];

				if (!headerlen && !nstrings) {
					debug_error("polchat_handle_stream() <blink> CONNECTION LOST :-( </blink>");
					return;
				}

				polchat_processpkt(s, headerlen, nstrings, &buffer[8], rlen-8);

				g_string_erase(j->recvbuf, 0, rlen);

			} else {
				debug_warn("polchat_handle_stream() NEED MORE DATA\n");
				return;
			}
		}
		return;
	}

	debug_error("polchat_handle_stream() Connection closed/ error XXX\n");
	return;
}

static void polchat_handle_failure(GDataInputStream *f, GError *err, gpointer data) {
	session_t *s = data;

	if (g_error_matches(err, EKG_CONNECTION_ERROR, EKG_CONNECTION_ERROR_EOF))
		polchat_handle_disconnect(s, NULL, EKG_DISCONNECT_USER);
	else
		polchat_handle_disconnect(s, err->message, EKG_DISCONNECT_NETWORK);
}

static void polchat_handle_connect(
		GSocketConnection *conn,
		GInputStream *instream,
		GOutputStream *outstream,
		gpointer data)
{
	session_t *s = data;
	polchat_private_t *j;
	const char *tmp;

	if (!s || !(j = s->priv))
		return;

	s->connecting = 2;

	debug_function("[polchat] handle_connect(%d)\n", s->connecting);

	g_string_set_size(j->recvbuf, 0);

	j->out_stream = ekg_connection_add(
			conn,
			instream,
			outstream,
			polchat_handle_stream,
			polchat_handle_failure,
			s);

	polchat_sendpkt(s, 0x0578,
		j->nick,						/* nickname */
		((tmp = session_get(s, "password")) ? tmp : ""),	/* password */
		"",							/* XXX cookie, always NUL? */
		session_get(s, "room"),					/* pokoj */
	/* XXX: */
		"http://www.polchat.pl/chat/room.phtml/?room=AmiX",	/* referer */
		"polchat.pl",						/* adres serwera */
		"nlst=1&nnum=1&jlmsg=true&ignprv=false",		/* konfiguracja */
		"ekg2-GIT-polchat",					/* klient */
		NULL);

	return;
}

static void polchat_handle_connect_failure(GError *err, gpointer data) {
	session_t *s = data;

	polchat_handle_disconnect(s, err->message, EKG_DISCONNECT_FAILURE);
}

static COMMAND(polchat_command_connect) {
	polchat_private_t *j = session->priv;
	const char *server;
	const char *nick;
	const char *room;
	int port;
	ekg_connection_starter_t cs;
	GSocketClient *s;

	if (session->connecting) {
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


	xfree(j->nick);
	j->nick = xstrdup(nick);

	string_clear(j->recvbuf);

	session->connecting = 1;

	printq("connecting", session_name(session));

	port = session_int_get(session, "port");
	if (port < 0 || port > 65535)
		port = atoi(POLCHAT_DEFAULT_PORT);

	cs = ekg_connection_starter_new(port);
	ekg_connection_starter_set_servers(cs, server);

	s = g_socket_client_new();
	ekg_connection_starter_run(cs, s, polchat_handle_connect, polchat_handle_connect_failure, session);

	return 0;
}

static COMMAND(polchat_command_disconnect) {
	const char *reason = params[0]?params[0]:QUITMSG(session);

	if (timer_remove_session(session, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!session->connecting && !session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (reason && session_connected_get(session)) {
		polchat_sendmsg(session, "/quit %s", reason);
	}

	if (session->connecting)
		polchat_handle_disconnect(session, reason, EKG_DISCONNECT_STOPPED);
	else
		polchat_handle_disconnect(session, reason, EKG_DISCONNECT_USER);

	return 0;
}

static COMMAND(polchat_command_reconnect) {
	if (session->connecting || session_connected_get(session))
		polchat_command_disconnect(name, params, session, target, quiet);

	return polchat_command_connect(name, params, session, target, quiet);
}

static COMMAND(polchat_command_msg) {
	window_t *w;
	char *uid;

	/* NOTE: sending `/quit` msg disconnect session */	/* XXX, escape? */

	if (!xstrncmp(target, "polchat:", 8))
		target += 8;

	uid = polchat_uid(target);
	w = window_find_s(session, uid);
	xfree(uid);


	if (w && (w->userlist)) {
		polchat_send_target_msg(session, target, "%s", params[1]);
	} else {
		polchat_sendmsg(session, "/msg %s %s", target, params[1]);
	}

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
	const char *reason;
	const char *p0 = params[0];
	char *p, *t = NULL;

	debug_function("polchat_command_part(%s) reason=%s\n", target, p0);

	if (!target) {
		if (!*p0)
			return 1;
		if ((p=xstrchr(p0,' '))) {
			t = xstrndup(p0, p - p0);
			p0 = p + 1;
		} else {
			t = xstrdup(p0);
			p0 = "";
		}
		target = t;
	}

	if (!xstrncmp(target, "polchat:", 8))
		target += 8;

	reason = PARTMSG(session, p0);
	polchat_send_target_msg(session, target, "/part %s", reason);

	xfree(t);
	
	return 0;
}

static COMMAND(polchat_command_join) {

	debug_function("polchat_command_join() target=%s\n", target);

	if (!xstrncmp(target, "polchat:", 8))
		target += 8;

	polchat_sendmsg(session, "/join %s", target);

	return 0;
}

static COMMAND(polchat_command_raw) {
	if (params[0])
		polchat_sendmsg(session, "/%s %s", name, params[0]);
	else
		polchat_sendmsg(session, "/%s", name);

	return 0;
}

static COMMAND(polchat_command_target_raw) {
	if (target) {
		if (!xstrncmp(target, "polchat:", 8))
			target += 8;
		if (params[0])
			polchat_send_target_msg(session, target, "/%s %s", name, params[0]);
		else
			polchat_send_target_msg(session, target, "/%s", name);
		return 0;
	}
	return polchat_command_raw(name, params, session, target, quiet);
}

static int polchat_theme_init() {
#ifndef NO_DEFAULT_THEME
#endif
	return 0;
}

static plugins_params_t polchat_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias",			VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("auto_connect",		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("log_formats",		VAR_STR, "irssi", 0, NULL),
	PLUGIN_VAR_ADD("nickname",		VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("password",		VAR_STR, NULL, 1, NULL),
	PLUGIN_VAR_ADD("port",			VAR_INT, POLCHAT_DEFAULT_PORT, 0, NULL),
	PLUGIN_VAR_ADD("room",			VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("server",		VAR_STR, POLCHAT_DEFAULT_HOST, 0, NULL),
	PLUGIN_VAR_END()
};

EXPORT int polchat_plugin_init(int prio) {

	PLUGIN_CHECK_VER("polchat");

	polchat_plugin.params	= polchat_plugin_vars;

	plugin_register(&polchat_plugin, prio);
	ekg_recode_utf8_inc();

	query_connect(&polchat_plugin, "protocol-validate-uid", polchat_validate_uid, NULL);
	query_connect(&polchat_plugin, "session-added", polchat_session_init, NULL);
	query_connect(&polchat_plugin, "session-removed", polchat_session_deinit, NULL);
	query_connect(&polchat_plugin, "plugin-print-version", polchat_print_version, NULL);

#if 0
	query_connect(&irc_plugin, ("ui-window-kill"),	irc_window_kill, NULL);
	query_connect(&irc_plugin, ("irc-topic"),	irc_topic_header, NULL);
	query_connect(&irc_plugin, ("status-show"),	irc_status_show_handle, NULL);
#endif

#define POLCHAT_ONLY		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define POLCHAT_FLAGS		POLCHAT_ONLY | SESSION_MUSTBECONNECTED
#define POLCHAT_FLAGS_TARGET	POLCHAT_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

	command_add(&polchat_plugin, "polchat:", "?",		polchat_command_inline_msg, POLCHAT_ONLY | COMMAND_PASS_UNCHANGED, NULL);
	command_add(&polchat_plugin, "polchat:msg", "!uUw !",	polchat_command_msg,	    POLCHAT_FLAGS_TARGET, NULL);
	command_add(&polchat_plugin, "polchat:connect", NULL,	polchat_command_connect,    POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:disconnect", "r ?",polchat_command_disconnect,POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:reconnect", "r ?", polchat_command_reconnect, POLCHAT_ONLY, NULL);

	command_add(&polchat_plugin, "polchat:part", "w ?",	polchat_command_part, POLCHAT_ONLY | COMMAND_PASS_UNCHANGED, NULL);
	command_add(&polchat_plugin, "polchat:join", "!uUw",	polchat_command_join, POLCHAT_FLAGS_TARGET, NULL);

/* XXX, REQ params ? */
	command_add(&polchat_plugin, "polchat:info", "uU",	polchat_command_target_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:op", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:unop", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:halfop", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:tmphalfop", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);

	/* /guest /unguest */
	/* /buddy /unbuddy /ignore /unignore */

	command_add(&polchat_plugin, "polchat:kick", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:ban", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:banip", "?",	polchat_command_raw,	POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:unban", "uU",	polchat_command_raw,	POLCHAT_ONLY, NULL);

	return 0;
}

static int polchat_plugin_destroy() {
	plugin_unregister(&polchat_plugin);
	ekg_recode_utf8_dec();
	return 0;
}

