
/*
 *  (C) Copyright 2007-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/utsname.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/queries.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#define DEFQUITMSG "EKG2 - It's better than sex!"
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)

#include "rivchat.h"	/* only protocol-stuff */

extern char *rivchat_cp_to_locale(char *b);	/* misc.c */
extern char *rivchat_locale_to_cp(char *b);	/* misc.c */
// extern uint32_t rivchat_fix32(uint32_t x);	/* misc.c */
#define rivchat_fix32(x) x

typedef struct {
	int fd;
	int port;
	char *nick;
	char *topic;

	uint32_t ourid;
	uint8_t seq_nr;
	uint32_t uptime;
} rivchat_private_t;

typedef struct {
	int user_locked;

	uint32_t id;		/* unikatowy numerek usera... czyt. ma byc unikatowy*/
	time_t packet_time;
	time_t ping_packet_time;
	rivchat_info_t ping_packet;

	char *ip;
	unsigned int port;
} rivchat_userlist_private_t;

/* XXX:
 *  - pozwolic podac adres do nasluchu i broadcast-do-wysylania 
 *  - uzywac protocol_message_emit() zeby moc logowac... 
 *  - powiadomic usera, ze po /connect moze sie nic ciekawego nie wydarzyc. 
 *  - rivchat_fix32() 
 *  - sprawdzic czy nie zostaly gdzies memleaki. */

static int rivchat_theme_init();

PLUGIN_DEFINE(rivchat, PLUGIN_PROTOCOL, rivchat_theme_init);

static int rivchat_send_packet(session_t *s, uint32_t type, userlist_t *user, const char *buf, size_t buflen);
static int rivchat_send_packet_string(session_t *s, uint32_t type, userlist_t *user, const char *str);

#define rivchat_userlist_priv_get(u) ((rivchat_userlist_private_t *) userlist_private_get(&rivchat_plugin, u))

static QUERY(rivchat_validate_uid) {
	char *uid = *(va_arg(ap, char **));
	int *valid = va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "rivchat:", 8) && uid[8]) {
		(*valid)++;
		return -1;
	}
	return 0;
}

static QUERY(rivchat_session_init) {
	char *session = *(va_arg(ap, char**));

	session_t *s = session_find(session);
	rivchat_private_t *j;

	if (!s || s->priv || s->plugin != &rivchat_plugin)
		return 1;

	j = xmalloc(sizeof(rivchat_private_t));
	j->fd = -1;

	s->priv = j;
	return 0;
}

static QUERY(rivchat_session_deinit) {
	char *session = *(va_arg(ap, char**));

	session_t *s = session_find(session);
	rivchat_private_t *j;

	if (!s || !(j = s->priv) || s->plugin != &rivchat_plugin)
		return 1;

	s->priv = NULL;
	xfree(j->nick);
	xfree(j->topic);
	xfree(j);

	return 0;
}

static QUERY(rivchat_print_version) {
	print("generic", "ekg2 plugin for RivChat protocol http://rivchat.prv.pl/");
	return 0;
}

static QUERY(rivchat_userlist_info_handle) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);
	rivchat_userlist_private_t *user;

	if (!u || !(user = u->priv))
		return 1;

	if (valid_plugin_uid(&rivchat_plugin, u->uid) != 1) 
		return 1;

	printq("rivchat_info_ip", user->ip, itoa(user->port));

	if (user->ping_packet_time) {
		rivchat_info_t *ping = &(user->ping_packet);
		char ver[8];
		char *user, *host;
		char *prog, *os;

		if (ping->filetransfer)
			printq("rivchat_info_have_dcc", itoa(ping->filetransfer));

		if (ping->master)
			printq("rivchat_info_master", itoa(ping->master));

		printq("rivchat_info_words", itoa(ping->slowa));

		printq("rivchat_info_connected", itoa(ping->online * 10));

	/* user, host */
		user = rivchat_cp_to_locale(xstrndup(ping->user, sizeof(ping->user)));
		host = rivchat_cp_to_locale(xstrndup(ping->host, sizeof(ping->host)));
		printq("rivchat_info_username", user, host);
		xfree(user); xfree(host);

	/* prog, os, version */
		prog = rivchat_cp_to_locale(xstrndup(ping->prog, sizeof(ping->prog)));
		os = rivchat_cp_to_locale(xstrndup(ping->os, sizeof(ping->os)));
		sprintf(ver, "%u.%u", ping->version[0], ping->version[1]);
		printq("rivchat_info_version", prog, ver, os);
		xfree(prog); xfree(os);
	}

	return 0;
}

static QUERY(rivchat_userlist_priv_handler) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int function	= *va_arg(ap, int *);
	rivchat_userlist_private_t *p;

	if (!u || (valid_plugin_uid(&rivchat_plugin, u->uid) != 1))
		return 1;

	if (!(p = u->priv)) {
		if (function == EKG_USERLIST_PRIVHANDLER_FREE)
			return -1;

		p = xmalloc(sizeof(rivchat_userlist_private_t));
		u->priv = p;
	}
		
	switch (function) {
		case EKG_USERLIST_PRIVHANDLER_FREE:
			xfree(p->ip);
			xfree(u->priv);
			u->priv = NULL;
			break;

		case EKG_USERLIST_PRIVHANDLER_GET:
			*va_arg(ap, void **) = p;
			break;

		case EKG_USERLIST_PRIVHANDLER_GETVAR_BYNAME:
		{
			const char *name	= *va_arg(ap, const char **);
			const char **r		= va_arg(ap, const char **);

			if (!xstrcmp(name, "ip"))
				*r = p->ip;
			else if (!xstrcmp(name, "port"))
				*r = itoa(p->port);
			else
				return 2;
			break;
		}

		default:
			return 2;
	}

	return -1;
}

static QUERY(rivchat_topic_header) {
	char **top   = va_arg(ap, char **);
	char **setby = va_arg(ap, char **);
	char **modes = va_arg(ap, char **);

	session_t *sess = window_current->session;
	char *targ = window_current->target;

	if (targ && sess && sess->plugin == &rivchat_plugin && sess->connected && sess->priv)
	{
		rivchat_private_t *j = sess->priv;

		*top = xstrdup(j->topic);

		*setby = *modes = NULL;
		return 5;
	}
	return -3;
}

static void rivchat_print_payload(unsigned char *payload, size_t len) {
	#define MAX_BYTES_PER_LINE 16
        int offset = 0;

	while (len) {
		int display_len;
		int i;

		if (len > MAX_BYTES_PER_LINE)
			display_len = MAX_BYTES_PER_LINE;
		else	display_len = len;
	
	/* offset */
        	debug_iorecv("\t0x%.4x  ", offset);
	/* hexdump */
		for(i = 0; i < MAX_BYTES_PER_LINE; i++) {
			if (i < display_len)
				debug_iorecv("%.2x ", payload[i]);
			else	debug_iorecv("   ");
		}
	/* seperate */
		debug_iorecv("   ");

	/* asciidump if printable, else '.' */
		for(i = 0; i < display_len; i++)
			debug_iorecv("%c", isprint(payload[i]) ? payload[i] : '.');
		debug_iorecv("\n");

		payload	+= display_len;
		offset	+= display_len;
		len 	-= display_len;
	}
}

static char *rivchat_packet_name(int type) {
	switch (type) {
		case RC_MESSAGE:	return "msg";
		case RC_INIT:		return "init";
		case RC_NICKCHANGE:	return "newnick";
		case RC_QUIT:		return "quit";
		case RC_ME:		return "me";
		//  RC_PING, RC_NICKPROTEST
		case RC_TOPIC:		return "topic";
		case RC_NEWTOPIC:	return "newtopic";
		case RC_AWAY:		return "away";
		case RC_REAWAY:		return "reaway";
		case RC_KICK:		return "kick";
		case RC_POP:		return "pop";
		case RC_REPOP:		return "repop";
		case RC_KICKED:		return "kicked";
		case RC_IGNORE:		return "ignore";
		case RC_NOIGNORE:	return "noignore";
		// RC_REPOPIGNORED
		case RC_ECHOMSG:	return "echomsg";
		case RC_PINGAWAY:	return "pingaway";
		// RC_FILEPROPOSE, RC_FILEREQUEST, RC_FILECANCEL, RC_FILECANCEL2
		/* XXX */
	}
	return NULL;
}

static char *rivchat_make_window(unsigned int port) {
	static char buf[50];

	sprintf(buf, "rivchat:%u", port);
	return buf;
}

static char *rivchat_make_formatname(int type, int is_our, int is_priv) {
	static char buf[36];	// zwiekszac.

	const char *typename; 
	const char *outname;
	const char *outpriv;

	if (!(typename = rivchat_packet_name(type)))
		return NULL;

	outname = (is_our)  ? "send" : "recv";
	outpriv = (is_priv) ? "priv" : "ch";

	sprintf(buf, "rivchat_%s_%s_%s", typename, outname, outpriv);
	if (format_exists(buf))
		return buf;

	sprintf(buf, "rivchat_%s_%s", typename, outname);
	if (format_exists(buf))
		return buf;

	sprintf(buf, "rivchat_%s", typename);
	if (format_exists(buf))
		return buf;

	return NULL;
}

static userlist_t *rivchat_find_user(session_t *s, const char *target) {
	rivchat_private_t *j = s->priv;

	if (!xstrcmp(target, rivchat_make_window(j->port)))
		return NULL;	/* main channel */

	return userlist_find(s, target); 
}

static void rivchat_dcc_close(struct dcc_s *dcc) {
	/* XXX, DCC */
	if (dcc->type == DCC_GET) {
		if (dcc->active) {
			/* XXX */

		} else {
			/* XXX, FILECANCEL, FILECANCEL2 */
			rivchat_send_packet_string(dcc->session, RC_FILECANCEL2, rivchat_find_user(dcc->session, dcc->uid), dcc->filename);
		}

	} else {


	}
}

static void memncpy(char *dest, const char *src, size_t len) {
	size_t srclen = (xstrlen(src)+1);

/* XXX, maybe: memset(dest, 0, len) */
	if (!src)
		return;

	if (len < srclen)
		debug_error("rivchat, memncpy() truncation of data!!!\n");

	if (len > srclen)
		len = srclen;

	memcpy(dest, src, len);
}

static char *rivchat_generate_data(session_t *s) {
/* XXX, this struct in j->info_hdr? */
/* XXX, if nothing changed, do nothing.... */
	static rivchat_info_t hdr;
	rivchat_private_t *j = s->priv;
	const char *os;
	const char *prog;

	memncpy(hdr.host, session_get(s, "hostname"), sizeof(hdr.host));
	memncpy(hdr.user, session_get(s, "username"), sizeof(hdr.user));

/* VERSION_SYS */
	if ((os = session_get(s, "VERSION_SYS"))) {
		memncpy(hdr.os, os, sizeof(hdr.os));
	} else {
		struct utsname un;

		if (uname(&un) != -1) {
			memncpy(hdr.os, un.sysname, sizeof(hdr.os));
			/* XXX: un.release, un.machine */
		} else {
			memncpy(hdr.os, "unknown OS", sizeof(hdr.os));
		}
	}

/* VERSION_NAME, VERSION_NO, default: ekg2-rivchat 0.1 */
	if ((prog = session_get(s, "VERSION_NAME"))) {
		memncpy(hdr.prog, prog, sizeof(hdr.prog));
	} else {
		memncpy(hdr.prog, "ekg2-rivchat", sizeof(hdr.prog));
	}

	/* XXX, VERSION-NO */

	hdr.version[0]	= 0;
	hdr.version[1]	= 1;

	hdr.away = !(s->status == EKG_STATUS_AVAIL);
	hdr.master = 0;
	hdr.slowa = -1;			/* ha, it's 0xFFFFFFFF 4 294 967 295 words! */
	hdr.kod = RC_ENCRYPTED;
	hdr.plec = 0;
	hdr.online = rivchat_fix32(j->uptime);
	hdr.filetransfer = RC_FILETRANSFER;
	hdr.pisze = 0;

	return (char *) &hdr;
}

/*
 * NOTE: previous version of ekg2-rivchat uses another approach:
 * 	- we send text
 *	- we display it [no matter what happened with it after sendto()]
 *
 * now:
 * 	- we send text
 * 	- if we recv it, we check fromid of packet, if it's ok. than ok :) if not, than sorry...
 *
 * It's like orginal rivchat client do.
 */

static int rivchat_send_packet(session_t *s, uint32_t type, userlist_t *user, const char *buf, size_t buflen) {
	rivchat_private_t *j;
	rivchat_userlist_private_t *p = NULL;

	struct sockaddr_in sin;
	rivchat_header_t hdr;
	int errno2;
	int len;

	if (!s || !(j = s->priv)) {
		errno = EFAULT;
		return -1;
	}

	if (user && (!(p = rivchat_userlist_priv_get(user)))) {
		errno = ENOENT;
		return -1;
	}

	if (buflen > RC_DATASIZE) {
		debug_error("rivchat_send_packet() truncation of data!!!\n");
		buflen = RC_DATASIZE;
	}

	memset(&hdr, 0, sizeof(hdr));
	memcpy(hdr.header, rivchat_magic, sizeof(rivchat_magic));
	hdr.size = rivchat_fix32(RC_SIZE);

	hdr.fromid = rivchat_fix32(j->ourid);
	hdr.toid = rivchat_fix32((user == NULL) ? RC_BROADCAST : p->id);
	hdr.type = rivchat_fix32(type);

	memncpy(hdr.nick, j->nick, sizeof(hdr.nick));

	if (buf && buflen)
		memcpy(hdr.data, buf, buflen);

	/* RGB colors */
	hdr.colors[0] = 0;
	hdr.colors[1] = 0;
	hdr.colors[2] = 0xFF;

	hdr.seq = j->seq_nr++;		/* XXX */
	hdr.encrypted = (user) ? RC_ENCRYPTED : 0;
#if 0
	uint8_t gender;				/* 1 - man, 2 - woman */
	uint8_t bold;				/* ? */
#endif
        sin.sin_family = AF_INET;
        sin.sin_port = htons(j->port);
	sin.sin_addr.s_addr = INADDR_BROADCAST;	/* XXX */
	sin.sin_addr.s_addr = inet_addr((user == NULL) ? "10.1.0.255" : (p->ip));

	len = sendto(j->fd, &hdr, RC_SIZE, 0, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
	errno2 = errno;

	debug("sendto(%d, %d, %x) == %d\n", j->fd, type, user, len);

	errno = errno2;
	return len;
}

static int rivchat_send_packet_string(session_t *s, uint32_t type, userlist_t *user, const char *str) {
	int ret;
	char *recodedstring = rivchat_locale_to_cp(xstrdup(str));

	ret = rivchat_send_packet(s, type, user, recodedstring, xstrlen(recodedstring));

	xfree(recodedstring);
	return ret;
}

static void rivchat_parse_packet(session_t *s, rivchat_header_t *_hdr, const char *ip) {
	rivchat_private_t *j = s->priv;
	rivchat_userlist_private_t *p = NULL;

/* XXX, protect from spoofing, i've got some ideas... */
	int is_our = (rivchat_fix32(_hdr->fromid) == j->ourid);
	int is_priv = (rivchat_fix32(_hdr->toid) != RC_BROADCAST);
	int type = rivchat_fix32(_hdr->type);

	int display_activity = EKG_WINACT_NONE;
	char *display_data = NULL;
	userlist_t *u;
	char *nick, *uid;
	int userlist_changed = 0;

	if (is_priv && rivchat_fix32(_hdr->toid) != j->ourid) {
		/* leave-them-alone */
		return;
	}

	nick = rivchat_cp_to_locale(xstrndup(_hdr->nick, sizeof(_hdr->nick)));
	uid = saprintf("rivchat:%s", nick);

	u = userlist_find(s, nick);

	if (!u && type != RC_QUIT) {	/* stworzmy */
		u = userlist_add(s, uid, nick);
		userlist_changed = 1;
	}

	if (u && !(p = rivchat_userlist_priv_get(u))) {
		/* XXX, smth bad happened */
	}

	if (p && p->user_locked) {
		int zle = 0;

		if (p->id != rivchat_fix32(_hdr->fromid)) {
			debug_error("[RIVCHAT, BAKCYL... FROMID (%.8x) != ID (%.8x) ...\n", p->id, rivchat_fix32(_hdr->fromid));
			zle++;
		}

		if (xstrcmp(p->ip, ip)) {
			debug_error("[RIVCHAT, INNY BAKCYL... UIP (%s) != %s\n", p->ip, ip);
			zle++;
		}
		/* spoof, protect */
		if (zle) {
			/* do something... */
		}
	}

	if (p && !p->user_locked) {
		p->user_locked = 1;
		xfree(p->ip); p->ip = xstrdup(ip);
		p->port = j->port;
		p->id = rivchat_fix32(_hdr->fromid);
	}

	if (u->status == EKG_STATUS_NA) {
		u->status = EKG_STATUS_AVAIL;
		userlist_changed = 1;
	}

	if (p) {
		switch (type) {
			case RC_PING:
			case RC_INIT:
				memcpy(&p->ping_packet, (rivchat_info_t *) _hdr->data, sizeof(p->ping_packet));
				p->ping_packet_time = time(NULL);
				break;

		}

		p->packet_time = time(NULL);
	}

	/* XXX, decrypt message if needed */
	switch (type) {
		case RC_MESSAGE:	// XXX, ladniej.
		{
			int to_us;
			
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			to_us = !!xstrstr(display_data, j->nick);
			display_activity = (is_priv || to_us) ? EKG_WINACT_IMPORTANT : EKG_WINACT_MSG;
			break;
		}

		case RC_INIT:
		{
			rivchat_info_t *hdr2 = (rivchat_info_t *) _hdr->data;

			char *user = rivchat_cp_to_locale(xstrndup(hdr2->user, sizeof(hdr2->user)));
			char *host = rivchat_cp_to_locale(xstrndup(hdr2->host, sizeof(hdr2->host)));

			if (is_our) {	/* we join? */
				window_t *w = window_new(rivchat_make_window(j->port), s, 0);

				window_switch(w->id);
			} else {
				/* XXX, instead of NULL pass user? */
				rivchat_send_packet(s, RC_PING, NULL, rivchat_generate_data(s), RC_INFOSIZE);	/* dajmy znac o sobie */
				if (j->topic)
					rivchat_send_packet_string(s, RC_TOPIC, NULL, j->topic);	/* oryginalny klient, jakos inaczej to robi.. nvm */
			}

			display_data = saprintf("%s!%s", user, host);
			display_activity = EKG_WINACT_JUNK;

			xfree(user); xfree(host);
			break;
		}

		/* XXX, don-t display (?) */
		case RC_PINGAWAY:
		{
			/* if user is already in away state, do nothing... else do type = RC_AWAY */
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));
			if (u->status == EKG_STATUS_AWAY && !xstrcmp(u->descr, display_data)) {
				xfree(display_data);
				display_data = NULL;
				break;	
			}
			type = RC_AWAY;
			/* no-break */
		}

		case RC_AWAY:
		{
			display_activity = EKG_WINACT_JUNK;
			if (!display_data)
				display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			xfree(u->descr); u->descr = xstrdup(display_data);
			u->status = EKG_STATUS_AWAY;
			userlist_changed = 1;
			break;
		}

		case RC_REAWAY:
			display_activity = EKG_WINACT_JUNK;
			display_data = NULL;

			xfree(u->descr); u->descr = NULL;
			u->status = EKG_STATUS_AVAIL;
			userlist_changed = 1;
			break;

		case RC_QUIT:
		{
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));
			if (!xstrlen(display_data)) {
				xfree(display_data);
				display_data = xstrdup("no reason");
			}
			display_activity = EKG_WINACT_JUNK;

			userlist_remove(s, u); u = NULL;
			userlist_changed = 1;
			break;
		}

		case RC_TOPIC:
		case RC_NEWTOPIC: 
		{
			display_activity = EKG_WINACT_MSG;
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			if (type == RC_NEWTOPIC) {
				xfree(j->topic);
				j->topic = saprintf("%s (%s)", display_data, nick);
			} else {
				if (j->topic && xstrcmp(j->topic, display_data)) {	/* old-new topic? */
					/* XXX, change type to NEWTOPIC, when somebody mess with topics? */
					xfree(j->topic);
					j->topic = NULL;
				}
				
				if (!j->topic)
					j->topic = xstrdup(display_data);
			}
			break;
		}

		case RC_POP:	/* XXX, ladniej */
		{
			char *pop_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			print_window(rivchat_make_window(j->port), s, is_priv ? EKG_WINACT_IMPORTANT : EKG_WINACT_JUNK, 1, 
					is_priv ? "rivchat_pop_recv" : "rivchat_pop_broadcast",
					s->uid, nick, pop_data, ip);

			xfree(pop_data);
			break;
		}

		case RC_NICKCHANGE:
		case RC_IGNORE:
		case RC_NOIGNORE:
		case RC_ME:
		{
			display_activity = EKG_WINACT_MSG;
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));
			break;
		}

		/* dont-display */
		case RC_PING:
		{
			rivchat_info_t *hdr2 = (rivchat_info_t *) _hdr->data;

			if ((u->status == EKG_STATUS_AWAY && !hdr2->away) || (u->status == EKG_STATUS_AVAIL && hdr2->away)) {
				u->status = hdr2->away ? EKG_STATUS_AWAY : EKG_STATUS_AVAIL;
				/* type = hdr2->away ? RC_AWAY : RC_REAWAY; */	/* oldcode */
				/* display_activity = .... */
			}

			/* XXX
			 * 	rivchat dziala tak:
			 * 	 - piszemy do kogos na privie (nie jest rozsylane RC_PING.. wiec nie dowiemy sie ze do nas pisze
			 * 	 - pisze na kanale, jest rozsylany RC_PING.. ale my mamy otwarte z nim okienko wiec wyswietla sie jakby pisal do nas.
			 *
			 * 	XXX, naprawic
			 */

			if ((u->typing && !hdr2->pisze) || (!u->typing && hdr2->pisze)) {
				if (hdr2->pisze)
					protocol_xstate_emit(s, uid, EKG_XSTATE_TYPING, 0);
				else
					protocol_xstate_emit(s, uid, 0, EKG_XSTATE_TYPING);
			}

			break;
		}

		case RC_FILEPROPOSE:
		{
			char *filename;
			dcc_t *d;
			uint64_t size;

			if (!is_priv)
				debug_error("rivchat+RC_FILEPROPOSE, is_priv unset?\n");
			if (is_our) {
				debug_error("rivchat+RC_FILEPROPOSE, is_our set?\n");
				break;
			}

			filename = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));
	/* XXX, na pewno 64bity? */
			size =
		/*
				(_hdr->reserved[4] << 56) |
				(_hdr->reserved[3] << 48) |
				(_hdr->reserved[2] << 40) |
				(_hdr->reserved[1] << 32) |
		 */
				(_hdr->reserved[0] << 24) |
				(_hdr->bold	   << 16) |
				(_hdr->encrypted   <<  8) |
				(_hdr->gender	   <<  0);


			d = dcc_add(s, uid, DCC_GET, NULL);
			dcc_filename_set(d, filename);		/* XXX, sanityzuj */
			dcc_size_set(d, size);
			dcc_close_handler_set(d, rivchat_dcc_close);

			print("dcc_get_offer", format_user(s, uid), filename, itoa(size), itoa(d->id));

			xfree(filename);
			break;
		}
/*
		case RC_FILEREQUEST:
		{
			if (!is_priv)
				debug_error("rivchat+RC_FILEREQUEST, is_priv unset?\n");
			if (is_our) {
				debug_error("rivchat+RC_FILEREQUEST, is_our set?\n");
				break;
			}

			debug_error("RC_FILEREQUEST\n");

			break;
		}
*/
		case RC_FILECANCEL:
		{
			debug_error("RC_FILECANCEL2\n");
			break;
		}

		case RC_FILECANCEL2:
		{
			dcc_t *d;
			char *filename;

			if (!is_priv)
				debug_error("rivchat+RC_FILECANCEL, is_priv unset?\n");
			if (is_our) {
				debug_error("rivchat+RC_FILECANCEL, is_our set?\n");
				break;
			}

			filename = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			debug("RC_FILECANCEL2\n");

			for (d = dccs; d; d = d->next) {
				if (d->session == s && d->type == DCC_SEND && !xstrcmp(d->filename, filename) && !xstrcmp(d->uid, uid)) {
					debug_error("RC_FILECANCEL2, let's try: %d\n", d->id);
					print("dcc_error_refused", format_user(d->session, d->uid));
					dcc_close(d);
					break;
				}
			}

			xfree(filename);

			break;
		}

		case RC_NICKPROTEST:
		case RC_ECHOMSG:
		case RC_REPOPIGNORED:
		case RC_REPOP:
		case RC_KICK:
		case RC_KICKED:
		default:
		{
			debug_error("rivchat_parse_packet() recv pkt->type: 0x%.4x\n", type);
			rivchat_print_payload((unsigned char *) _hdr->data, sizeof(_hdr->data));
		}
	}

	if (display_activity != EKG_WINACT_NONE) {
		char *fname = rivchat_make_formatname(type, is_our, is_priv);

		print_window(is_priv ? uid : rivchat_make_window(j->port), s, display_activity, 1, fname, s->uid, nick, display_data, ip);

		xfree(display_data);
	}

	if (userlist_changed)
		query_emit_id(NULL, USERLIST_REFRESH);

	xfree(nick);
	xfree(uid);
}

static WATCHER_SESSION(rivchat_handle_stream) {
	rivchat_private_t *j;

        struct sockaddr_in oth;
        int oth_size;
	
	unsigned char buf[400];
	rivchat_header_t *hdr = (rivchat_header_t *) buf;
	int len;

	if (type) {
		/* XXX */
		return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	oth_size = sizeof(struct sockaddr_in);
	len = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) &oth, &oth_size);

	if (len < 0) {
		/* XXX */
		return -1;
	}

	if (len == RC_SIZE && !memcmp(buf, rivchat_magic, sizeof(rivchat_magic)) && rivchat_fix32(hdr->size) == RC_SIZE) {
		rivchat_parse_packet(s, hdr, inet_ntoa(oth.sin_addr));
		return 0;
	}

	debug_error("rivchat_handle_stream() len: %d Ignoring packet [Bytes: %x %x %x %x ... hdr->size: %d] \n", len, buf[0], buf[1], buf[2], buf[3], rivchat_fix32(hdr->size));
	/* XXX, dump while packet? */

	return 0;
}

static TIMER_SESSION(rivchat_pingpong) {
	rivchat_private_t *j;
	userlist_t *ul;
	time_t cur_time;
	int userlist_changed = 0;
	
	if (type)
		return 0;

	if (!s || !(j = s->priv))
		return -1;

	cur_time = time(NULL);

	j->uptime++;
	
	for (ul = s->userlist; ul;) {
		userlist_t *u = ul;
		rivchat_userlist_private_t *user = u->priv;
		/* sprawdzic wszystkich userow last_ping_time i jesli mniejszy niz (now - ping_remove) to usun usera. */

		ul = ul->next;

		if (!u) {
			debug("[RIVCHAT_PING_TIMEOUT] USER %s removed cause of non private data...\n", u->uid);
			userlist_remove(s, u);
			userlist_changed = 1;
			continue;
		}

		if ((user->ping_packet_time && (user->ping_packet_time + RC_PING_TIMEOUT < cur_time)) || 
			(user->packet_time + RC_TIMEOUT < cur_time))
		{
			print("rivchat_user_timeout", session_name(s), u->uid);

			debug("[RIVCHAT_PING_TIMEOUT] USER %s removed cause of timeout. PING: %d LAST:%d NOW: %d\n", u->uid, user->ping_packet_time, user->packet_time, cur_time);
			userlist_remove(s, u);
			userlist_changed = 1;
		}
	}

	if (userlist_changed)
		query_emit_id(NULL, USERLIST_REFRESH);

	rivchat_send_packet(s, RC_PING, NULL, rivchat_generate_data(s), RC_INFOSIZE);
	return 0;
}

static COMMAND(rivchat_command_connect) {
	rivchat_private_t *j = session->priv;
	struct sockaddr_in sin;
	int one = 1;

	const char *newnick;
	
	int port;
	int fd;
	
	port = session_int_get(session, "port");

	if (port < 0 || port > 65535) {
		/* XXX, notify? */
		port = 16127;
	}

	if (!(newnick = session_get(session, "nickname"))) {
		printq("generic_error", "gdzie lecimy ziom ?! [/session nickname]");
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		protocol_disconnected_emit(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		debug_error("rivchat, socket() failed\n");
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
		debug_error("rivchat, setsockopt(SO_REUSEADDR) failed\n");
		/* not-fatal */
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one))) {
		protocol_disconnected_emit(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		debug_error("rivchat, setsockopt(SO_BROADCAST) failed\n");
		close(fd);
		return -1;
	}

	sin.sin_port		= htons(port);
	sin.sin_family		= AF_INET;
	sin.sin_addr.s_addr	= INADDR_ANY;

	if (bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))) {
		protocol_disconnected_emit(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		debug_error("rivchat, bind() failed\n");
		close(fd);
		return -1;
	}

	debug("bind success @0.0.0.0:%d\n", port);

	/* XXX, if strlen(j->nick) > 30 notify about possible truncation? */

	xfree(j->nick);
	j->nick = xstrdup(newnick);

	j->fd = fd;
	j->port = port;
	j->seq_nr = 0;		/* XXX? */
	j->uptime = 0;		/* XXX? */
	j->ourid = rand();	/* XXX? */

	session_status_set(session, EKG_STATUS_AVAIL);
	protocol_connected_emit(session);

	watch_add_session(session, fd, WATCH_READ, rivchat_handle_stream);
	timer_add_session(session, "rc_pingpong", 10, 1, rivchat_pingpong);
	userlist_free(session);	// XXX: USERLIST_REFRESH

	rivchat_send_packet(session, RC_INIT, NULL, rivchat_generate_data(session), RC_INFOSIZE);
	return 0;
}

static COMMAND(rivchat_command_disconnect) {
	rivchat_private_t *j = session->priv;
	const char *reason;
	
	if (timer_remove_session(session, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	reason = params[0]?params[0]:QUITMSG(session);

	/* send quit pkt, XXX rivchat doesn't support reasons.. */
	rivchat_send_packet_string(session, RC_QUIT, NULL, reason);

	watch_remove(&rivchat_plugin, j->fd, WATCH_READ);
	close(j->fd);
	j->fd = -1;

	xfree(j->topic);
	j->topic = NULL;

	protocol_disconnected_emit(session, reason, EKG_DISCONNECT_USER);
	userlist_free(session);	// XXX: USERLIST_REFRESH
	return 0;
}

static COMMAND(rivchat_command_reconnect) {
	if (session->connected)
		rivchat_command_disconnect(name, params, session, target, quiet);

	return rivchat_command_connect(name, params, session, target, quiet);
}

static COMMAND(rivchat_command_inline_msg) {
	if (params[0])
		return rivchat_send_packet_string(session, RC_MESSAGE, rivchat_find_user(session, target), params[0]);

	return -1;
}

static COMMAND(rivchat_command_me) {
	return rivchat_send_packet_string(session, RC_ME, rivchat_find_user(session, target), params[0]);
}

static COMMAND(rivchat_command_nick) {
	rivchat_private_t *j = session->priv;

	xfree(j->nick);
	j->nick	= xstrdup(params[0]);

	return rivchat_send_packet_string(session, RC_NICKCHANGE, NULL, j->nick);
}

static COMMAND(rivchat_command_topic) {
	rivchat_private_t *j = session->priv;

	if (!params[0]) { /* display current topic */
		/* XXX, setby ? */	

		printq("rivchat_topic", rivchat_make_window(j->port), "", j->topic, "0.0.0.0");
		return 0;
	}
	return rivchat_send_packet_string(session, RC_NEWTOPIC, NULL, params[0]);
}

static COMMAND(rivchat_command_dcc) {
	/* send */
	if (params[0] && !xstrncasecmp(params[0], "se", 2)) {
		userlist_t *user;
		rivchat_userlist_private_t *up;

		struct stat st;
		int fd;

		const char *fn;
		dcc_t *d;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(fn = prepare_path_user(params[2]))) {
			printq("generic_error", "path too long"); /* XXX? */
			return -1;
		}

		if (!(user = userlist_find(session, get_uid(session, params[1])))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!session->connected) {
			printq("not_connected", session_name(session));
			return -1;
		}

		if (user->status == EKG_STATUS_NA) {
			printq("dcc_user_not_avail", format_user(session, user->uid));
			return -1;
		}

		up = rivchat_userlist_priv_get(user);

		if (!up || !up->ip || !up->ping_packet_time) {
			printq("dcc_user_aint_dcc", format_user(session, user->uid));
			return -1;
		}

		if (up->ping_packet.filetransfer != 2) {
			/* XXX */
			printq("dcc_user_aint_dcc", format_user(session, user->uid));
			debug("bad filetransfer version?\n");
			return -1;
		}

		if (!stat(fn, &st) && !S_ISREG(st.st_mode)) {
			printq("io_nonfile", params[2]);
			return -1;
		}

		if ((fd = open(fn, O_RDONLY|O_NONBLOCK)) == -1) {
			if (errno == ENXIO)
				printq("io_nonfile", params[2]);
			else
				printq("io_cantopen", params[2], strerror(errno));
			return -1;
		}

		close(fd);
	
	/* XXX, wyslac rozmiar pliku. trzeba bedzie cudowac */
		rivchat_send_packet_string(session, RC_FILEPROPOSE, user, fn);

		d = dcc_add(session, user->uid, DCC_SEND, NULL);
		dcc_filename_set(d, fn);
		dcc_close_handler_set(d, rivchat_dcc_close);
		dcc_size_set(d, st.st_size);

		return 0;
	}

	/* get */
	if (params[0] && !xstrncasecmp(params[0], "g", 1)) {
		dcc_t *d = NULL, *D;
		char *path;
		int fd;

		for (D = dccs; D; D = D->next) {
			userlist_t *u;

			if (!dcc_filename_get(D) || dcc_type_get(D) != DCC_GET)
				continue;
			
			if (!params[1]) {
				if (dcc_active_get(D))
					continue;
				d = D;
				break;
			}

			if (params[1][0] == '#' && xstrlen(params[1]) > 1 && atoi(params[1] + 1) == dcc_id_get(D)) {
				d = D;
				break;
			}

			if ((u = userlist_find(session, dcc_uid_get(D)))) {
				if (!xstrcasecmp(params[1], u->uid) || (u->nickname && !xstrcasecmp(params[1], u->nickname))) {
					d = D;
					break;
				}
			}
		}
		if (!d) {
			printq("dcc_not_found", (params[1]) ? params[1] : "");
			return -1;
		}
		if (d->active) {
			printq("dcc_receiving_already", dcc_filename_get(d), format_user(session, dcc_uid_get(d)));
			return -1;
		}
		if (xstrncmp(d->uid, "rivchat:", 8)) {
			debug_error("%s:%d /dcc command, incorrect `%s`!\n", __FILE__, __LINE__, __(d->uid));
			printq("generic_error", "Use /dcc on correct session, sorry");
			return -1;
		}
		path = NULL;
#if 0
		if (config_dcc_dir) 
		    	path = saprintf("%s/%s", config_dcc_dir, dcc_filename_get(d));
		else
		    	path = xstrdup(dcc_filename_get(d));
#endif
		fd = open(path, O_WRONLY | O_CREAT, 0600);

		if (fd == -1) {
			printq("dcc_get_cant_create", path);

			dcc_close(d);
			xfree(path);
			
			return -1;
		}
		xfree(path);

		printq("dcc_get_getting", format_user(session, dcc_uid_get(d)), dcc_filename_get(d));
		dcc_active_set(d, 1);
		/* XXX, listeing-socket, watch, etc.. */
		return 0;
	}

	return cmd_dcc(name, params, session, target, quiet);
}

typedef struct {
	char *nickname;
	int uptime;
	int words;
	char master;
} rivchat_place_t;

LIST_ADD_COMPARE(rivchat_places_sort, rivchat_place_t *) {
/* sortuj malejaco */
	if (data1->master != data2->master)	return -(data1->master - data2->master);	/* 0  compare master.		*/
	if (data1->words != data2->words)	return -(data1->words - data2->words);		/* 1st compare words.		*/
	if (data1->uptime != data2->uptime)	return -(data1->uptime - data2->uptime);	/* 2nd compare uptime.		*/
	return xstrcmp(data1->nickname, data2->nickname);					/* 3rd compare nicknamess.	*/
}

static COMMAND(rivchat_command_places) {
	list_t final = NULL;
	list_t l;
	userlist_t *ul;
	int i;

	for (ul = session->userlist; ul; ul = ul->next) {
		userlist_t *u = ul;
		rivchat_userlist_private_t *user = u->priv;
		rivchat_place_t *item;

		item = xmalloc(sizeof(rivchat_place_t));
		item->nickname = u->nickname;
		item->words  =	user ? user->ping_packet.slowa : 0;
		item->uptime =	user ? user->ping_packet.online : 0;
		item->master =	user ? user->ping_packet.master : 0;

		list_add_sorted(&final, item, rivchat_places_sort);
	}

	for (i = 1, l = final; l; l = l->next, i++) {
		rivchat_place_t *place = l->data;

		printq("rivchat_place", session->uid, place->nickname, itoa(place->words), itoa(place->uptime), place->master ? "*" : " ", itoa(i));
	}
	list_destroy(final, 1);
	return 0;
}

static void rivchat_changed_nick(session_t *s, const char *var) {
	rivchat_private_t *j;
	const char *newnick;

	if (!s || !(j = s->priv))
		return;

	if (!s->connected)
		return;

	if (!(newnick = session_get(s, "nickname")))
		return;		/* ignore */

	if (xstrcmp(newnick, j->nick)) {	/* if nick-really-changed */
		xfree(j->nick);
		j->nick = xstrdup(newnick);
		rivchat_send_packet_string(s, RC_NICKCHANGE, NULL, newnick);
	}
}

static void rivchat_notify_reconnect(session_t *s, const char *var) {
	if (s && s->connected)
		print("config_must_reconnect");
}

static void rivchat_resend_ping(session_t *s, const char *var) {
	if (s && s->connected)
		rivchat_send_packet(s, RC_PING, NULL, rivchat_generate_data(s), RC_INFOSIZE);
}

static int rivchat_theme_init() {
#ifndef NO_DEFAULT_THEME
/* format of formats ;] 
 *
 * rivchat_pkttype_type_priv 
 *      pkttype  - msg, quit, me, init, ...
 *      type     - recv, send
 *      priv     - ch, priv
 *
 * jak nie znajdzie pelnej formatki to wtedy szuka:
 *   rivchat_pkttype_type
 *
 * a potem:
 *   rivchat_pkttype
 *
 * params: 
 *	%1 - sesja %2 - nick %3 - data. %4 - ip 
 */

	/* te zrobic bardziej irssi-like */
	format_add("rivchat_msg_send_ch",	"%B<%W%2%B>%n %3", 1);
	format_add("rivchat_msg_send_priv",	"%B<%R%2%B>%n %3", 1);
	format_add("rivchat_msg_recv",		"<%2> %3", 1);
	
	/* ok */
	format_add("rivchat_init", 		"%> %C%2%n %B[%c%3@%4%B]%n has joined", 1);
	format_add("rivchat_quit", 		"%> %c%2%n %B[%c%2@%4%B]%n has quit %B[%n%3%B]", 1);

	format_add("rivchat_me",		"%W%e* %2%n %3", 1);

	format_add("rivchat_newnick_send", 	"%> You're now known as %T%3", 1);
	format_add("rivchat_newnick_recv", 	"%> %c%2%n is now known as %C%3", 1);

	format_add("rivchat_newtopic",		"%> %T%2%n changed topic to: %3", 1);
	format_add("rivchat_topic",		"%> Topic: %3", 1);

	format_add("rivchat_kicked",		"%> %c%4%n was kicked from %T%1%n by %T%2%n", 1);

	/* dziwne */
	format_add("rivchat_ignore_send",	"%) You starts ignoring %3", 1);
	format_add("rivchat_ignore_recv",	"%) %T%2%n starts ignoring %3", 1);

	format_add("rivchat_noignore_send",	"%) You stops ignoring %3", 1);
	format_add("rivchat_noignore_recv",	"%) %T%2%n stops ignoring %3", 1);

	format_add("rivchat_reaway",		"%) %T%2%n back", 1);
	format_add("rivchat_away",		"%) %T%2%n is away: %T%3", 1);

/* XXX, zrobic bardziej widoczne [ramka?, query_emit()?] */
	format_add("rivchat_pop_broadcast",	"%) %W%2%n has broadcast pop: %3", 1);
	format_add("rivchat_pop_recv",		"%) %W%2%n has sent pop: %3", 1);
	// format_add("rivchat_pop_send",	"You send pop", 1);

/*
	format_add("rivchat_ping",		_("%) ping/pong %c%2%n"), 1);
	format_add("rivchat_pingaway",		_("%) ping/pong %c%2%n"), 1);
 */


/* not-protocol-stuff */
	format_add("rivchat_info_connected",	_("%K| %nConnected for: %T%1%n seconds"), 1);
	format_add("rivchat_info_have_dcc",	_("%K| %nHas dcc support %g%1%n"), 1);
	format_add("rivchat_info_master",	_("%K| %nHe's a master! ;p"), 1);
	format_add("rivchat_info_words",	_("%K| %nWords count: %T%1"), 1);			/* %1 - wcount */
	format_add("rivchat_info_username",	_("%K| %nLogged as: %T%1@%2"), 1);			/* %1 info->user %2 info->host */
	format_add("rivchat_info_version",	_("%K| %nWorking @ %T%1%n ver %T%2%n OS: %T%3%n"), 1);	/* %1 - progname, %2 - version %3 - os */
	format_add("rivchat_info_ip",		_("%K| %nAddress: %T%1:%2"), 1);

	format_add("rivchat_user_timeout",	_("%> Utracono kontakt z uzytkownikiem %T%2"), 1);	/* %1 - sesja %2 - uid */

	/* XXX, width */
	format_add("rivchat_place",		_("%> %6 - %T%2%n %B(%gwords:%n %T%3%n %B[%n%W%5%B])"), 1);	/* %1 - sesja %2 - nick %3 - words count %4 - uptime %5 - master %6 - seq */

#endif
	return 0;
}

static plugins_params_t rivchat_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		VAR_STR, NULL, 0, NULL), 				//  0
	PLUGIN_VAR_ADD("auto_connect", 		VAR_BOOL, "0", 0, NULL),				//  1
	PLUGIN_VAR_ADD("auto_reconnect", 	VAR_INT, "0", 0, NULL),					//  2
#define RIVCHAT_VAR_HOSTNAME 3
	PLUGIN_VAR_ADD("hostname",		VAR_STR, NULL, 0, rivchat_resend_ping),			//  3
	PLUGIN_VAR_ADD("log_formats", 		VAR_STR, "irssi", 0, NULL),				//  4
#define RIVCHAT_VAR_NICKNAME 5
	PLUGIN_VAR_ADD("nickname",		VAR_STR, NULL, 0, rivchat_changed_nick),		//  5
	PLUGIN_VAR_ADD("port",			VAR_STR, "16127", 0, rivchat_notify_reconnect),		//  6
#define RIVCHAT_VAR_USERNAME 7
	PLUGIN_VAR_ADD("username",		VAR_STR, NULL, 0, rivchat_resend_ping),			//  7
	PLUGIN_VAR_ADD("VERSION_NAME", 		VAR_STR, 0, 0, rivchat_resend_ping),			//  8
	PLUGIN_VAR_ADD("VERSION_NO", 		VAR_STR, 0, 0, rivchat_resend_ping),			//  9	TODO
	PLUGIN_VAR_ADD("VERSION_SYS", 		VAR_STR, 0, 0, rivchat_resend_ping),			// 10
	PLUGIN_VAR_END()
};

EXPORT int rivchat_plugin_init(int prio) {
	static char pwd_name[100];
	static char pwd_hostname[100];

	struct passwd *pwd_entry;
	
	/* moze segvowac na niektorych architekturach */
	/* we assume you're using LE processor :> */
	/* XXX, test BE/LE or use <endian.h> */

	PLUGIN_CHECK_VER("rivchat");

/* magic stuff */
	if ((pwd_entry = getpwuid(getuid()))) {
		strlcpy(pwd_name, pwd_entry->pw_name, sizeof(pwd_name));
		/* XXX, we need to free buffer allocated by getpwuid()? */

		rivchat_plugin_vars[RIVCHAT_VAR_NICKNAME].value = pwd_name;
		rivchat_plugin_vars[RIVCHAT_VAR_USERNAME].value = pwd_name;
	}

	if (gethostname(pwd_hostname, sizeof(pwd_hostname))) {
		debug_error("[rivchat] gethostname() failed\n");
		strlcpy(pwd_hostname, "localhost", sizeof(pwd_hostname));
	}

	rivchat_plugin_vars[RIVCHAT_VAR_HOSTNAME].value = pwd_hostname;

	rivchat_plugin.params = rivchat_plugin_vars;

	plugin_register(&rivchat_plugin, prio);

	query_connect_id(&rivchat_plugin, PROTOCOL_VALIDATE_UID, rivchat_validate_uid, NULL);
	query_connect_id(&rivchat_plugin, SESSION_ADDED, rivchat_session_init, NULL);
	query_connect_id(&rivchat_plugin, SESSION_REMOVED, rivchat_session_deinit, NULL);
	query_connect_id(&rivchat_plugin, PLUGIN_PRINT_VERSION, rivchat_print_version, NULL);

	query_connect_id(&rivchat_plugin, USERLIST_INFO, rivchat_userlist_info_handle, NULL);
	query_connect_id(&rivchat_plugin, USERLIST_PRIVHANDLE, rivchat_userlist_priv_handler, NULL);

	query_connect_id(&rivchat_plugin, IRC_TOPIC, rivchat_topic_header, NULL);

#if 0
	query_connect(&irc_plugin, ("ui-window-kill"),	irc_window_kill, NULL);
	query_connect(&irc_plugin, ("status-show"),	irc_status_show_handle, NULL);
#endif

#define RIVCHAT_ONLY 		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define RIVCHAT_FLAGS		RIVCHAT_ONLY | SESSION_MUSTBECONNECTED

	command_add(&rivchat_plugin, "rivchat:", "?",		rivchat_command_inline_msg, RIVCHAT_ONLY, NULL);

	command_add(&rivchat_plugin, "rivchat:connect", NULL,   rivchat_command_connect,    RIVCHAT_ONLY, NULL);
	command_add(&rivchat_plugin, "rivchat:dcc", "p uU f ?", rivchat_command_dcc,        RIVCHAT_ONLY, "send get close list");
	command_add(&rivchat_plugin, "rivchat:disconnect", "r",	rivchat_command_disconnect, RIVCHAT_ONLY, NULL);
	command_add(&rivchat_plugin, "rivchat:me", "?",		rivchat_command_me,         RIVCHAT_FLAGS, NULL);
	command_add(&rivchat_plugin, "rivchat:nick", "!",	rivchat_command_nick,       RIVCHAT_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&rivchat_plugin, "rivchat:places", NULL,	rivchat_command_places,     RIVCHAT_FLAGS, NULL);
	command_add(&rivchat_plugin, "rivchat:topic", "?",	rivchat_command_topic,      RIVCHAT_FLAGS, NULL);
	command_add(&rivchat_plugin, "rivchat:reconnect", "r",	rivchat_command_reconnect,  RIVCHAT_ONLY, NULL);
	return 0;
}

static int rivchat_plugin_destroy() {
	plugin_unregister(&rivchat_plugin);
	return 0;
}

