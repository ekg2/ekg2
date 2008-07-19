/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *		  2004 - 2006 Adam Mikuta <adamm@ekg2.org>
 *		  2005 Leszek Krupiñski <leafnode@wafel.com>
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

#include "ekg2-config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>

#include <libgadu.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/msgqueue.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include <ekg/queries.h>

#include "dcc.h"
#include "gg.h"
#include "misc.h"
#include "pubdir.h"
#include "images.h"
#include "pubdir50.h"

static int gg_theme_init();
static void gg_session_handler_msg(session_t *s, struct gg_event *e);

PLUGIN_DEFINE(gg, PLUGIN_PROTOCOL, gg_theme_init);

list_t gg_currently_checked = NULL;
char *last_tokenid;
int gg_config_display_token;
int gg_config_skip_default_format;
int gg_config_split_messages;

/**
 * gg_session_init()
 *
 * Handler for: <i>SESSION_ADDED</i><br>
 * Init private session struct gg_private_t if @a session is gg one.<br>
 * Read saved userlist by userlist_read()
 *
 * @param ap 1st param: <i>(char *) </i><b>session</b> - uid of session
 * @param data NULL
 *
 * @return	0 if @a session is gg one, and we init memory<br>
 * 		1 if we don't found such session, or it wasn't gg session <b>[most probable]</b>, or we already init memory.
 */

static QUERY(gg_session_init) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

	gg_private_t *g;

	if (!s || s->priv || s->plugin != &gg_plugin)
		return 1;

	g = xmalloc(sizeof(gg_private_t));

	userlist_read(s);

	s->priv = g;

	return 0;
}

/**
 * gg_session_deinit()
 *
 * Handler for: <i>SESSION_REMOVED</i><br>
 * Free memory allocated by gg_private_t if @a session is gg one.
 *
 * @param ap 1st param: <i>(char *) </i><b>session</b> - uid of session
 * @param data NULL
 *
 * @todo Check if we really free all memory allocated by session.
 *
 * @return 	0 if @a session is gg one, and memory allocated where xfree()'d.<br>
 * 		1 if not such session, or it wasn't gg session <b>[most probable]</b>, or we already free memory.
 */

static QUERY(gg_session_deinit) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

	gg_private_t *g;
	list_t l;

	if (!s || !(g = s->priv) || s->plugin != &gg_plugin)
		return 1;

	if (g->sess)
		gg_free_session(g->sess);

	for (l = g->searches; l; l = l->next)
		gg_pubdir50_free((gg_pubdir50_t) l->data);

	list_destroy(g->searches, 0);

	xfree(g);

	s->priv = NULL;

	return 0;
}

/**
 * gg_userlist_info_handle()
 *
 * Handler for: <i>USERLIST_INFO</i><br>
 * (Emited by: <i>/list</i> command, when we want know more about given user)<br>
 * printq() all gg-protocol-only-data like: possible client version [read: which version of protocol he use], if he has voip, etc..
 *
 * @param ap 1st param: <i>(userlist_t *) </i><b>u</b>	- item.
 * @param ap 2nd param: <i>(int) </i><b>quiet</b>	- If quiet for printq()
 * @param data NULL
 *
 * @return 	1 - If no @a u passed, or it's invalid for gg plugin<br>
 * 		else printq() info and return 0
 */

static QUERY(gg_userlist_info_handle) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);
	gg_userlist_private_t *up;

	if (!u || valid_plugin_uid(&gg_plugin, u->uid) != 1 || !(up = gg_userlist_priv_get(u)))
		return 1;

	if (up->first_name && xstrcmp(up->first_name, "") && up->last_name && up->last_name && xstrcmp(up->last_name, ""))
		printq("gg_user_info_name", up->first_name, up->last_name);
	if (up->first_name && xstrcmp(up->first_name, "") && (!up->last_name || !xstrcmp(up->last_name, "")))
		printq("gg_user_info_name", up->first_name, "");
	if ((!up->first_name || !xstrcmp(up->first_name, "")) && up->last_name && xstrcmp(up->last_name, ""))
		printq("gg_user_info_name", up->last_name, "");

	if (up->mobile && xstrcmp(up->mobile, ""))
		printq("gg_user_info_mobile", up->mobile);

	if (up->ip) {
		char *ip_str = saprintf("%s:%s", inet_ntoa(*((struct in_addr*) &up->ip)), itoa(up->port));
		printq("gg_user_info_ip", ip_str);
		xfree(ip_str);
	} else if (up->last_ip) {
		char *last_ip_str = saprintf("%s:%s", inet_ntoa(*((struct in_addr*) &up->last_ip)), itoa(up->last_port));
		printq("gg_user_info_last_ip", last_ip_str);
		xfree(last_ip_str);
	}

	if (up->port == 2)
		printq("gg_user_info_not_in_contacts");
	if (up->port == 1)
		printq("gg_user_info_firewalled");
	if ((up->protocol & GG_HAS_AUDIO_MASK))
		printq("gg_user_info_voip");

	if ((up->protocol & 0x00ffffff)) {
		int v = up->protocol & 0x00ffffff;
		const char *ver = NULL;

		if (v < 0x0b)
			ver = ("<= 4.0.x");
		if (v >= 0x0f && v <= 0x10)
			ver = ("4.5.x");
		if (v == 0x11)
			ver = ("4.6.x");
		if (v >= 0x14 && v <= 0x15)
			ver = ("4.8.x");
		if (v >= 0x16 && v <= 0x17)
			ver = ("4.9.x");
		if (v >= 0x18 && v <= 0x1b)
			ver = ("5.0.x");
		if (v >= 0x1c && v <= 0x1e)
			ver = ("5.7");
		if (v == 0x20)
			ver = ("6.0 (build >= 129)");
		if (v == 0x21)
			ver = ("6.0 (build >= 133)");
		if (v == 0x22)
			ver = ("6.0 (build >= 140)");
		if (v == 0x24)
			ver = ("6.1 (build >= 155) || 7.6 (build >= 1359)");
		if (v == 0x25)
			ver = ("7.0 (build >= 1)");
		if (v == 0x26)
			ver = ("7.0 (build >= 20)");
		if (v == 0x27)
			ver = ("7.0 (build >= 22)");
		if (v == 0x28)
			ver = ("7.5.0 (build >= 2201)");
		if (v == 0x29)
			ver = ("7.6 (build >= 1688)");
		if (v == 0x2a)
			ver = ("7.7 (build >= 3315)");
		if (v == 0x2d)
			ver = ("8.0 (build >= 4881)");

		if (ver) {
			printq("gg_user_info_version", ver);
		} else {
			char *tmp = saprintf(("unknown (%#.2x)"), v);
			printq("gg_user_info_version", tmp);
			xfree(tmp);
		}
	}
	return 0;
}

static QUERY(gg_user_offline_handle) {
	userlist_t *u	= *(va_arg(ap, userlist_t **));
	session_t *s	= *(va_arg(ap, session_t **));
	gg_private_t *g;
	int uin;

	if (!s || !(g = s->priv) || s->plugin != &gg_plugin)
		return 1;
	uin = atoi(u->uid + 3);

	gg_remove_notify_ex(g->sess, uin, gg_userlist_type(u));
	ekg_group_add(u, "__offline");
	print("modify_offline", format_user(s, u->uid));
	gg_add_notify_ex(g->sess, uin, gg_userlist_type(u));

	return 0;
}

static QUERY(gg_user_online_handle) {
	userlist_t *u	= *(va_arg(ap, userlist_t **));
	session_t *s	= *(va_arg(ap, session_t **));
	gg_private_t *g;
	int quiet = (data == NULL);
	int uin;

	if (!s || !(g = s->priv) || s->plugin != &gg_plugin)
		return 1;
	uin = atoi(u->uid + 3);

	gg_remove_notify_ex(g->sess, uin, gg_userlist_type(u));
	ekg_group_remove(u, "__offline");
	printq("modify_online", format_user(s, u->uid));
	gg_add_notify_ex(g->sess, uin, gg_userlist_type(u));

	return 0;
}

static QUERY(gg_status_show_handle) {
	char **uid = va_arg(ap, char**);
	session_t *s = session_find(*uid);
	userlist_t *u;
	struct in_addr i;
	int mqc;
	char *tmp, *priv, *r1, *r2;
	gg_private_t *g;

	if (!s) {
		debug("Function gg_status_show_handle() called with NULL data\n");
		return -1;
	}
	if (!(g = session_private_get(s)))
		return -1;

	if ((u = userlist_find(s, s->uid)) && u->nickname)
		print("show_status_uid_nick", s->uid, u->nickname);
	else
		print("show_status_uid", s->uid);

	if (!g->sess || g->sess->state != GG_STATE_CONNECTED) {
		char *tmp = format_string(format_find("show_status_notavail"), "");
		print("show_status_status_simple", tmp);
		xfree(tmp);

		if ((mqc = msg_queue_count_session(s->uid)))
			print("show_status_msg_queue", itoa(mqc));
		return 0;
	}

	if (GG_S_F(g->sess->status))
		priv = format_string(format_find("show_status_private_on"));
	else
		priv = format_string(format_find("show_status_private_off")); 

	r1 = xstrmid(s->descr, 0, GG_STATUS_DESCR_MAXSIZE);
	r2 = xstrmid(s->descr, GG_STATUS_DESCR_MAXSIZE, -1);

	tmp = format_string(format_find(ekg_status_label(s->status, s->descr, "show_status_")), r1, r2);

	xfree(r1);
	xfree(r2);

	i.s_addr = g->sess->server_addr;

	print("show_status_status", tmp, priv);
#ifdef __GG_LIBGADU_HAVE_OPENSSL
	if (g->sess->ssl)
		print("show_status_server_tls", inet_ntoa(i), itoa(g->sess->port));
	else
#endif
		print("show_status_server", inet_ntoa(i), itoa(g->sess->port));

	xfree(tmp);
	xfree(priv);

	return 0;
}

/*
 * str_to_uin()
 *
 * funkcja, która zajmuje siê zamian± stringa na
 * liczbê i sprawdzeniem, czy to prawid³owy uin.
 *
 * zwraca uin lub 0 w przypadku b³êdu.
 */
/* XXX, pozamieniac atoi() na str_to_uin() */
uin_t str_to_uin(const char *text) {
	char *tmp;
	long num;

	if (!text)
		return 0;

	errno = 0;
	num = strtol(text, &tmp, 0);

	if (*text == '\0' || *tmp != '\0')
		return 0;

	if ((errno == ERANGE || (num == LONG_MAX || num == LONG_MIN)) || num > UINT_MAX || num < 0)
		return 0;

	return (uin_t) num;
}

/**
 * gg_add_notify_handle()
 *
 * Handler for: <i>ADD_NOTIFY</i><br>
 * (Emited by: <i>/add</i> command, when you successfully add smb to the userlist)<br>
 * Notify gg server about it.
 *
 * @todo	We ignore gg_add_notify_ex() result
 *
 * @param ap 1st param: <i>(char *) </i><b>session_uid</b> 	- session uid
 * @param ap 2nd param: <i>(char *) </i><b>uid</b>		- user uid
 * @param data NULL
 *
 * @return 	1 - If smth is wrong, @a session_uid or @a uid isn't valid gg number, or session is without private struct.<br>
 * 		else 0
 *
 */

static QUERY(gg_add_notify_handle) {
	char *session_uid 	= *(va_arg(ap, char **));
	char *uid		= *(va_arg(ap, char **));

	session_t *s = session_find(session_uid);
	gg_private_t *g;

/* Check session */
	if (!s) {
		debug("Function gg_add_notify_handle() called with NULL data\n");
		return 1;
	}

	if (!(g = s->priv) || s->plugin != &gg_plugin)
		return 1;

/* Check uid */
	if (valid_plugin_uid(&gg_plugin, uid) != 1)
		return 1;

	gg_add_notify_ex(g->sess, str_to_uin(uid+3), gg_userlist_type(userlist_find(s, s->uid))); 
	return 0;
}

/**
 * gg_remove_notify_handle()
 *
 * Handler for: <i>REMOVE_NOTIFY</i><br>
 * (Emited by: <i>/del</i> command, when we sucessfully remove smb from userlist.<br>
 * Notify gg server about it.
 *
 * @todo	We ignore gg_remove_notify() result
 *
 * @param ap 1st param: <i>(char *) </i><b>session_uid</b>	- session uid
 * @param ap 2nd param: <i>(char *) </i><b>uid</b>		- user uid
 * @param data NULL
 *
 * @return	1 - If smth is wrong, @a session_uid or @a uid isn't valid gg number, or session is without private struct.<br>
 * 		else 0
 */

static QUERY(gg_remove_notify_handle) {
	char *session_uid 	= *(va_arg(ap, char **));
	char *uid 		= *(va_arg(ap, char **));

	session_t *s = session_find(session_uid);
	gg_private_t *g;

/* Check session */
	if (!s) {
		debug("Function gg_remove_notify_handle() called with NULL data\n");
		return 1;
	}

	if (!(g = s->priv) || s->plugin != &gg_plugin)
		return 1;

/* Check uid */
	if (valid_plugin_uid(&gg_plugin, uid) != 1)
		return 1;

	gg_remove_notify(g->sess, str_to_uin(uid+3));
	return 0;
}

/**
 * gg_print_version()
 *
 * Handler for: <i>PLUGIN_PRINT_VERSION</i>
 * print info about libgadu version.
 *
 * @return 0
 */

static QUERY(gg_print_version) {
	char protov[3];
	char clientv[sizeof(GG_DEFAULT_CLIENT_VERSION)];

	{		/* that IMO would be lighter than array_make+array_join */
		char *p, *q;

		for (p = GG_DEFAULT_CLIENT_VERSION, q = clientv; *p; p++) {
			if (*p == ',')
				*(q++) = '.';
			else if (*p != ' ')
				*(q++) = *p;
		}
		*q = '\0';
	}

	snprintf(protov, 3, "%.2x", GG_DEFAULT_PROTOCOL_VERSION);
	print("gg_version", gg_libgadu_version(), GG_LIBGADU_VERSION, clientv, protov);

	return 0;
}

/**
 * gg_validate_uid()
 *
 * handler for <i>PROTOCOL_VALIDATE_UID</i><br>
 * checks, if @a uid is <i>proper for gg plugin</i>.
 *
 * @note <i>Proper for gg plugin</i> means if @a uid starts with "gg:" and uid len > 3
 * @todo Blah, irc does xstrncasecmp() here it's only xstrncmp() let's decide... GG: and gg: is proper, or only gg:
 * @todo Maybe let's check if after gg: we have max 32b number.. because libgadu and gg protocol only support 32bit uids... ;)
 *
 * @param ap 1st param: <i>(char *) </i><b>uid</b>  - of user/session/command/whatever
 * @param ap 2nd param: <i>(int) </i><b>valid</b> - place to put 1 if uid is valid for gg plugin.
 * @param data NULL
 *
 * @return 	-1 if it's valid uid for gg plugin<br>
 * 		 0 if not
 */

static QUERY(gg_validate_uid) {
	char *uid	= *(va_arg(ap, char **));
	int *valid	= va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "gg:", 3) && uid[3]) {
		uid+=3;
		/* now let's check if after gg: we have only digits */
		for (; *uid; uid++)
			if (!isdigit(*uid))
				return 0;

		(*valid)++;
		return -1;
	}
	return 0;
}

/**
 * gg_protocols()
 *
 * handler for <i>GET_PLUGIN_PROTOCOLS</i><br>
 * It just add "gg:" to @a arr
 *
 * @note I know it's nowhere used. It'll be used by d-bus plugin.
 *
 * @param ap 1st param: <i>(char **) </i><b>arr</b> - array with available protocols
 * @param data NULL
 *
 * @return 0
 */

static QUERY(gg_protocols) {
	char ***arr	= va_arg(ap, char ***);

	array_add(arr, "gg:");
	return 0;
}

static QUERY(gg_userlist_priv_handler) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int function	= *va_arg(ap, int *);

	if (!u || ((valid_plugin_uid(&gg_plugin, u->uid) != 1)
			&& !(function == EKG_USERLIST_PRIVHANDLER_READING && atoi(u->uid))))
		return 1;

	{
		gg_userlist_private_t *p = u->priv;

		if (!u->priv) {
			if (function == EKG_USERLIST_PRIVHANDLER_FREE)
				return 0;

			p = xmalloc(sizeof(gg_userlist_private_t));
			u->priv = p;
		}
		
		switch (function) {
			case EKG_USERLIST_PRIVHANDLER_FREE:
				xfree(p->first_name);
				xfree(p->last_name);
				xfree(p->mobile);
				xfree(u->priv);
				u->priv = NULL;
				break;
			case EKG_USERLIST_PRIVHANDLER_GET:
				*va_arg(ap, void **) = p;
				break;
			case EKG_USERLIST_PRIVHANDLER_READING:
			{
				char **entry	= *va_arg(ap, char ***);

				if (atoi(u->uid)) {	/* backwards compatibility / userlist -g hack for GG */
					char *tmp = u->uid;
					u->uid = saprintf("gg:%s", tmp);
					xfree(tmp);
				} 

				p->first_name 	= entry[0];	entry[0] = NULL;
				p->last_name	= entry[1];	entry[1] = NULL;
				p->mobile	= entry[4];	entry[4] = NULL;
				break;
			}
			case EKG_USERLIST_PRIVHANDLER_WRITING:
			{
				char **entry	= *va_arg(ap, char ***);

				if (p->first_name) {
					xfree(entry[0]);
					entry[0] = xstrdup(p->first_name);
				}
				if (p->last_name) {
					xfree(entry[1]);
					entry[1] = xstrdup(p->last_name);
				}
				if (p->mobile) {
					xfree(entry[4]);
					entry[4] = xstrdup(p->mobile);
				}
				break;
			}
			case EKG_USERLIST_PRIVHANDLER_GETVAR_BYNAME:
			{
				const char *name	= *va_arg(ap, const char **);
				const char **r		= *va_arg(ap, const char ***);

				if (!xstrcmp(name, "mobile"))
					*r = p->mobile;
				else if (!xstrcmp(name, "ip"))
					*r = inet_ntoa(*((struct in_addr*) &p->ip));
				else if (!xstrcmp(name, "port"))
					*r = itoa(p->port);
				break;
			}
			case EKG_USERLIST_PRIVHANDLER_GETVAR_IPPORT:
			{
				const char **ip		= *va_arg(ap, const char ***);
				const char **port	= *va_arg(ap, const char ***);

				*ip	= inet_ntoa(*((struct in_addr*) &p->ip));
				*port	= itoa(p->port);
				break;
			}
			case EKG_USERLIST_PRIVHANDLER_SETVAR_BYNAME:
			{
				const char *name	= *va_arg(ap, const char **);
				const char *val		= *va_arg(ap, const char **);

				if (!xstrcmp(name, "first_name")) {
					xfree(p->first_name);
					p->first_name = xstrdup(val);
				} else if (!xstrcmp(name, "last_name")) {
					xfree(p->last_name);
					p->last_name = xstrdup(val);
				} else if (!xstrcmp(name, "mobile")) {
					xfree(p->mobile);
					p->mobile = xstrdup(val);
				}
				break;
			}
		}
	}
	return 1;
}

/*
 * gg_ping_timer_handler()
 *
 * pinguje serwer co jaki¶ czas, je¶li jest nadal po³±czony.
 */
static TIMER_SESSION(gg_ping_timer_handler) {
	gg_private_t *g;

	if (type == 1)
		return 0;

	if (!s || !session_connected_get(s)) {
		return -1;
	}

	if ((g = session_private_get(s))) {
		gg_ping(g->sess);
	}
	return 0;
}

/* 
 * gg_inv_check_handler()
 *
 * checks if user marked as invisible, is still connected
 */

static TIMER(gg_inv_check_handler)
{
	const gg_currently_checked_t *c = (gg_currently_checked_t *) data;
	userlist_t *u;
	
	if (type == 1) {
		xfree(c->uid);
		xfree(data);
		return -1;
	}
	
	if ((u = userlist_find(c->session, c->uid)) && (u->status == EKG_STATUS_INVISIBLE)) {
		command_exec_format(c->uid, c->session, 1, ("/gg:check_conn"));
	}
	
	return -1;
}

/*
 * gg_session_handler_success()
 *
 * obs³uga udanego po³±czenia z serwerem.
 */
static void gg_session_handler_success(session_t *s) {
	gg_private_t *g = s->priv;

	int status;
	int _status;
	char *descr;
	char *cpdescr; 

	if (!g || !g->sess) {
		debug("[gg] gg_session_handler_success() called with null gg_private_t\n");
		return;
	}

	protocol_connected_emit(s);

	gg_userlist_send(g->sess, s->userlist);

	/* zapiszmy adres serwera */
	if (session_int_get(s, "connection_save") == 1) {
		struct in_addr addr;		

		addr.s_addr = g->sess->server_addr;
		session_set(s, "server", inet_ntoa(addr));
		session_int_set(s, "port", g->sess->port);
	}
	/* pamiêtajmy, ¿eby pingowaæ */
	if (timer_find_session(s, "ping") == NULL)
		timer_add_session(s, "ping", 180, 1, gg_ping_timer_handler);

	descr = xstrdup(session_descr_get(s));
	status = session_status_get(s);

	cpdescr = gg_locale_to_cp(descr);

	/* ustawiamy swój status */
	_status = GG_S(gg_text_to_status(status, s->descr ? cpdescr : NULL));
	if (session_int_get(s, "private")) 
		_status |= GG_STATUS_FRIENDS_MASK;

	if (s->descr) {
		gg_change_status_descr(g->sess, _status, cpdescr);
	} else {
		gg_change_status(g->sess, _status);
	}
	xfree(cpdescr);	
}

/*
 * gg_session_handler_failure()
 *
 * obs³uga nieudanego po³±czenia.
 */
static void gg_session_handler_failure(session_t *s, struct gg_event *e) {
	gg_private_t *g = s->priv;

	const char *reason;

	switch (e->event.failure) {
		case GG_FAILURE_CONNECTING:	reason = "conn_failed_connecting";	break;
		case GG_FAILURE_INVALID:	reason = "conn_failed_invalid";		break;
		case GG_FAILURE_READING:	reason = "conn_failed_disconnected";	break;
		case GG_FAILURE_WRITING:	reason = "conn_failed_disconnected";	break;
		case GG_FAILURE_PASSWORD:	reason = "conn_failed_password";	break;
		case GG_FAILURE_404:		reason = "conn_failed_404";		break;
#ifdef __GG_LIBGADU_HAVE_OPENSSL
		case GG_FAILURE_TLS:		reason = "conn_failed_tls";		break;
#endif
		default:			reason = "conn_failed_unknown";		break;
	}

	if (session_int_get(s, "connection_save") == 1) {
		session_set(s, "server", NULL);
		session_int_set(s, "port", GG_DEFAULT_PORT);
	} else
	{		/* If we have some servers in 'server' variable and we're unable to connect to the first one,
			 * then we should move it to the end and set the second one as default,
			 * maybe that's kinda dirty way, but IMO most flexible [mg] */
		const char *oldserver = session_get(s, "server");
		const char *comma;

		if ((comma = xstrchr(oldserver, ','))) {
			char *newserver = xmalloc(xstrlen(oldserver)+1);

			xstrcpy(newserver, comma+1);
			xstrcat(newserver, ",");
			xstrncat(newserver, oldserver, comma-oldserver);

			session_set(s, "server", newserver);
			
			xfree(newserver);
		}
	}

	gg_free_session(g->sess);
	g->sess = NULL;

	protocol_disconnected_emit(s, format_find(reason), EKG_DISCONNECT_FAILURE);
}

/*
 * gg_session_handler_disconnect()
 *
 * obs³uga roz³±czenia z powodu pod³±czenia drugiej sesji.
 */
static void gg_session_handler_disconnect(session_t *s) {
	gg_private_t *g = s->priv;

	protocol_disconnected_emit(s, NULL, EKG_DISCONNECT_FORCED);

	gg_logoff(g->sess);		/* zamknie gniazdo */
	gg_free_session(g->sess);
	g->sess = NULL;
}

/*
 * gg_session_handler_status()
 *
 * obs³uga zmiany stanu przez u¿ytkownika.
 */
static void gg_session_handler_status(session_t *s, uin_t uin, int status, const char *descr, uint32_t ip, uint16_t port, int protocol) {
	char *__uid	= saprintf(("gg:%d"), uin);
	char *__descr	= gg_cp_to_locale(xstrdup(descr));
	int i, j, dlen, state = 0, m = 0;

	{
		userlist_t *u;
		gg_userlist_private_t *up;

		if ((u = userlist_find(s, __uid)) && (up = gg_userlist_priv_get(u))) {
			up->protocol = protocol;
			/* zapisz adres IP i port */
			up->ip = ip;
			up->port = port;

			up->last_ip = ip;
			up->last_port = port;
		}
	}

	for (i = 0; i < xstrlen(__descr); i++)
		if (__descr[i] == 10 || __descr[i] == 13)
			m++;
	dlen = i;
	/* if it is not set it'll be -1 so, everythings ok */
	if ( (i = session_int_get(s, "concat_multiline_status")) && m > i) {
		for (m = i = j = 0; i < dlen; i++) {
			if (__descr[i] != 10 && __descr[i] != 13) {
				__descr[j++] = __descr[i];
				state = 0;
			} else {
				if (!state && __descr[i] == 10)
					__descr[j++] = ' ';
				else
					m++;
				if (__descr[i] == 10)
					state++;
			}
		}
		__descr[j] = '\0';
		if (m > 3) {
			memmove (__descr+4, __descr, j + 1);
			/* multiline tag */
			__descr[0] = '['; __descr[1] = 'm'; __descr[2] = ']'; __descr[3] = ' ';
		}

	}

	protocol_status_emit(s, __uid, gg_status_to_text(status), __descr, time(NULL));

	xfree(__descr);
	xfree(__uid);
}

/*
 * gg_session_handler_msg()
 *
 * obs³uga przychodz±cych wiadomo¶ci.
 */
static void gg_session_handler_msg(session_t *s, struct gg_event *e) {
	gg_private_t *g = s->priv;

	char *__sender, **__rcpts = NULL;
	char *__text;
	uint32_t *__format = NULL;
	int image = 0, check_inv = 0;
	int i;

	if (e->event.msg.msgclass & GG_CLASS_CTCP) {
		struct gg_dcc *d;
		char *__host = NULL;
		char uid[16];
		int __port = -1, __valid = 1;
		userlist_t *u;
		gg_userlist_private_t *up;
		watch_t *w;

		if (!gg_config_dcc) return;

		snprintf(uid, sizeof(uid), "gg:%d", e->event.msg.sender);

		if (!(u = userlist_find(s, uid)) || !(up = gg_userlist_priv_get(u)))
			return;

		query_emit(NULL, ("protocol-dcc-validate"), &__host, &__port, &__valid, NULL);
/*		xfree(__host); */

		if (!__valid) {
			print_status("dcc_attack", format_user(s, uid));
			command_exec_format(NULL, s, 0, ("/ignore %s"), uid);
			return;
		}

		if (!(d = gg_dcc_get_file(up->ip, up->port, atoi(session_uid_get(s) + 3), e->event.msg.sender))) {
			print_status("dcc_error", strerror(errno));
			return;
		}

		w = watch_add(&gg_plugin, d->fd, d->check, gg_dcc_handler, d);
		watch_timeout_set(w, d->timeout);
		return;
	}

	for (i = 0; i < e->event.msg.recipients_count; i++)
		array_add(&__rcpts, saprintf("gg:%d", e->event.msg.recipients[i]));

	__text = gg_cp_to_locale(xstrdup((const char *) e->event.msg.message));

	if (e->event.msg.formats && e->event.msg.formats_length) {
		unsigned char *p = e->event.msg.formats;
		int i, len = xstrlen(__text), ii, skip = gg_config_skip_default_format;
		static char win_gg_default_format[6] = { 0x00, 0x00, 0x08, 0x00, 0x00, 0x00 };

		gg_debug(GG_DEBUG_DUMP, "// formats:");
		for (ii = 0; ii < e->event.msg.formats_length; ii++) {
			skip &= (p[ii] == win_gg_default_format[ii]);
			gg_debug(GG_DEBUG_DUMP, " %.2x", (unsigned char) p[ii]);
		}
		if (skip)
			gg_debug(GG_DEBUG_DUMP, " <- skipping");
		else
			__format = xcalloc(len, sizeof(uint32_t));
		gg_debug(GG_DEBUG_DUMP, "\n");

/* XXX, check it. especially this 'pos' */
		if (!skip) for (i = 0; i < e->event.msg.formats_length; ) {
			int j, pos = p[i] + p[i + 1] * 256;
			uint32_t val = 0;

			if ((p[i + 2] & GG_FONT_IMAGE))	{
				struct gg_msg_richtext_image *img = (void *) &p[i+3];
					
				/* XXX, needed? */
				if (i+3 + sizeof(struct gg_msg_richtext_image) > e->event.msg.formats_length) {
					debug_error("gg_session_handler_msg() wtf?\n");
					break;
				}

				debug_function("gg_session_handler_msg() inline image: sender=%d, size=%d, crc32=0x%.8x\n", 
					e->event.msg.sender, 
					img->size, 
					img->crc32);

				image=1;

				if (img->crc32 == GG_CRC32_INVISIBLE)
					check_inv = 1;

				if (gg_config_get_images)
					gg_image_request(g->sess, e->event.msg.sender, img->size, img->crc32);

				i+=sizeof(struct gg_msg_richtext_image);

			} else {
				if ((p[i + 2] & GG_FONT_BOLD))
					val |= EKG_FORMAT_BOLD;

				if ((p[i + 2] & GG_FONT_ITALIC))
					val |= EKG_FORMAT_ITALIC;

				if ((p[i + 2] & GG_FONT_UNDERLINE))
					val |= EKG_FORMAT_UNDERLINE;

				if ((p[i + 2] & GG_FONT_COLOR)) {
					val |= EKG_FORMAT_COLOR | p[i + 3] | (p[i + 4] << 8) | (p[i + 5] << 16);
					i += 3;
				}
			}

			i += 3;

			//if (val!=0) // only image format
			for (j = pos; j < len; j++)
				__format[j] = val;
		}
	}
	__sender = saprintf("gg:%d", e->event.msg.sender);
	
	if (image && check_inv) {
		print("gg_we_are_being_checked", session_name(s), format_user(s, __sender));
	} else {
		int __class	= e->event.msg.sender ? EKG_MSGCLASS_CHAT : EKG_MSGCLASS_SYSTEM;

/*		if (!check_inv || xstrcmp(__text, ""))
			printq("generic", "image in message.\n"); - or something
 */
		protocol_message_emit(s, __sender, __rcpts, __text, __format, e->event.msg.time, __class, NULL, EKG_TRY_BEEP, 0);
	}
	
	xfree(__text);
	xfree(__sender);
	xfree(__format);
	array_free(__rcpts);
}

/**
 * gg_session_handler_ack()
 *
 * Support for messages acknowledgement.<br>
 * Handler for libgadu: <i>GG_EVENT_ACK</i> events
 */

static void gg_session_handler_ack(session_t *s, struct gg_event *e) {
	char *__session = xstrdup(s->uid);
	char *__rcpt	= saprintf("gg:%d", e->event.ack.recipient);
	char *__seq	= xstrdup(itoa(e->event.ack.seq));
	int __status;

/* ifndef + defines for old libgadu */
#ifndef GG_ACK_BLOCKED
#define GG_ACK_BLOCKED 0x0001
#endif

#ifndef GG_ACK_MBOXFULL
#define GG_ACK_MBOXFULL 0x0004
#endif

	switch (e->event.ack.status) {
		case GG_ACK_DELIVERED:		/* from libgadu.h 1.1 (15-Oct-01) */
			__status = EKG_ACK_DELIVERED;
			break;
		case GG_ACK_QUEUED:		/* from libgadu.h 1.1 (15-Oct-01) */
			__status = EKG_ACK_QUEUED;
			break;
		case GG_ACK_NOT_DELIVERED:	/* from libgadu.h 1.50 (21-Dec-01) */
			__status = EKG_ACK_DROPPED;
			break;
		case GG_ACK_BLOCKED:		/* from libgadu.h 1.175 (21-Dec-04) */
			__status = EKG_ACK_DROPPED;
			break;
		case GG_ACK_MBOXFULL:		/* from libgadu.h 1.175 (21-Dec-04) */
			__status = EKG_ACK_TEMPFAIL;
			break;
		default:			/* unknown neither for ekg2 nor libgadu */
			debug_error("gg_session_handler_ack() unknown message ack status. consider upgrade [0x%x]\n", e->event.ack.status);
			__status = EKG_ACK_UNKNOWN;
			break;
	}
	query_emit_id(NULL, PROTOCOL_MESSAGE_ACK, &__session, &__rcpt, &__seq, &__status);

	xfree(__seq);
	xfree(__rcpt);
	xfree(__session);
}

/*
 * gg_session_handler_image()
 *
 * support image request or reply
 * now it is used only by /check_inv 
 * we don't use support images
 */
static void gg_session_handler_image(session_t *s, struct gg_event *e) {
	gg_private_t *g = s->priv;

	switch (e->type) {
		case GG_EVENT_IMAGE_REQUEST:
			{
				list_t l;

				debug("GG_EVENT_IMAGE_REQUEST (crc32 - %d)\n", e->event.image_request.crc32);

				if (e->event.image_request.crc32 == GG_CRC32_INVISIBLE) {
					char *tmp = saprintf("gg:%d", e->event.image_request.sender);
					list_t l;

					for (l = gg_currently_checked; l; ) {
						gg_currently_checked_t *c = l->data;

						l = l->next;

						if (c->session == s) {
							userlist_t *u = userlist_find(s, tmp);
							if (u) {
								const int interval = session_int_get(s, "invisible_check_interval");
								gg_currently_checked_t *c_timer;
								
								if (interval > 0) {
									c_timer = xmalloc(sizeof(gg_currently_checked_t));
									c_timer->uid = xstrdup(tmp);
									c_timer->session = s;
									timer_add(&gg_plugin, NULL, interval, 0, gg_inv_check_handler, c_timer);
								}
								if (u->status == EKG_STATUS_NA)
									protocol_status_emit(s, tmp, EKG_STATUS_INVISIBLE, u->descr, time(NULL));
							} else
								print("gg_user_is_connected", session_name(s), format_user(s, tmp));
							xfree(c->uid);
							list_remove(&gg_currently_checked, c, 1);
							break;
						}

					}

					xfree(tmp);
					break;
				}

				for (l = images; l;) {
					image_t *i = l->data;

					l = l->next;
					if (e->event.image_request.crc32 == i->crc32 && 
							e->event.image_request.size == i->size) {
						gg_image_reply(g->sess, e->event.image_request.sender, i->filename, i->data, i->size);
						image_remove_queue(i);
						break;
					}
				}
				break;
			}
		case GG_EVENT_IMAGE_REPLY:
			if (e->event.image_reply.image) {
				const char *image_basedir;
				char *image_file;
				FILE *fp;
				int i;

		/* 0th, get basedir */
				image_basedir = gg_config_images_dir ? 
					gg_config_images_dir : 			/* dir specified by config */
					prepare_pathf("images");		/* (ekg_config)/images */

		/* 1st, create directories.. */
				if (mkdir_recursive(image_basedir, 1)) {
					print("gg_image_cant_open_file", image_basedir, strerror(errno));
					return;
				}

/* XXX, recode from cp1250 to locales [e->event.image_reply.filename] */
/* XXX, sanity path */
				image_file = saprintf("%s/gg_%d_%.4x_%s", 
					image_basedir, 
					e->event.image_reply.sender, 
					e->event.image_reply.crc32, 
					e->event.image_reply.filename);

				debug("image from %d called %s\n", e->event.image_reply.sender, image_file);

				if (!(fp = fopen(image_file, "w"))) {
					print("gg_image_cant_open_file", image_file, strerror(errno));
					xfree(image_file);
					return;
				}

				for (i = 0; i<e->event.image_reply.size; i++) {
					fputc(e->event.image_reply.image[i],fp);
				}
				fclose(fp);

				{
					char *uid = saprintf("gg:%d", e->event.image_reply.sender);
					print_info(uid, s, "gg_image_ok_get", image_file, uid, e->event.image_reply.filename);
					xfree(uid);
				}

				xfree(image_file);

				break;
			} else {
				/* XXX, display no image, from libgadu: */
					/* pusta odpowied¼ - klient po drugiej stronie nie ma ¿±danego obrazka */

			}
	}
}

/*
 * gg_session_handler_userlist()
 *
 * support for userlist's events 
 *
 */
static void gg_session_handler_userlist(session_t *s, struct gg_event *e) {
	switch (e->event.userlist.type) {
		case GG_USERLIST_GET_REPLY:
			print("userlist_get_ok", session_name(s));

			if (e->event.userlist.reply) {
				char *reply;
				userlist_t *ul;
				gg_private_t *g = session_private_get(s);

				/* remove all contacts from notification list on server */
				for (ul = s->userlist; ul; ul = ul->next) {
					userlist_t *u = ul;
					char *parsed;

					if (!u || !(parsed = xstrchr(u->uid, ':')))
						continue;

					gg_remove_notify_ex(g->sess, str_to_uin(parsed + 1), gg_userlist_type(u));
				}
				reply = gg_cp_to_locale(xstrdup(e->event.userlist.reply));
				gg_userlist_set(s, reply);
				xfree(reply);
				gg_userlist_send(g->sess, s->userlist);

				config_changed = 1;
			}
			session_int_set(s, "__userlist_get_config", -1);
			break;

		case GG_USERLIST_PUT_REPLY:
			switch (session_int_get(s, "__userlist_put_config")) {
				case 0:	print("userlist_put_ok", session_name(s));		break;
				case 1:	print("userlist_config_put_ok", session_name(s));	break;
				case 2:	print("userlist_clear_ok", session_name(s));		break;
				case 3:	print("userlist_config_clear_ok", session_name(s));	break;
				default:
					debug_error("gg_session_handler_userlist() occur, but __userlist_put_config: %d\n", session_int_get(s, "__userlist_put_config"));
			}
			session_int_set(s, "__userlist_put_config", -1);
			break;
	}
}

/*
 * gg_session_handler()
 *
 * obs³uga zdarzeñ przy po³±czeniu gg.
 */
WATCHER_SESSION(gg_session_handler) {		/* tymczasowe */
	gg_private_t *g;

	struct gg_event *e;
	int broken = 0;

	if (type == 1) {
		/* tutaj powinni¶my usun±æ dane watcha. nie, dziêkujê. */
		return 0;
	}

	if (!s || !(g = s->priv) || !g->sess) {
		debug_error("gg_session_handler() called with NULL gg_session\n");
		return -1;
	}

	if (type == 2) {
		if (g->sess->state != GG_STATE_CONNECTING_GG) {
			protocol_disconnected_emit(s, NULL, EKG_DISCONNECT_FAILURE);

			gg_free_session(g->sess);
			g->sess = NULL;

			return -1;
		}

		/* je¶li jest GG_STATE_CONNECTING_GG to ka¿emy stwierdziæ
		 * b³±d (EINPROGRESS) i ³±czyæ siê z kolejnym kandydatem. */
	}

	if (!(e = gg_watch_fd(g->sess))) {
		protocol_disconnected_emit(s, NULL, EKG_DISCONNECT_NETWORK);

		gg_free_session(g->sess);
		g->sess = NULL;

		return -1;
	}

	switch (e->type) {
		case GG_EVENT_NONE:
			break;

		case GG_EVENT_CONN_SUCCESS:
			gg_session_handler_success(s);
			break;

		case GG_EVENT_CONN_FAILED:
			gg_session_handler_failure(s, e);
			broken = 1;
			break;

		case GG_EVENT_DISCONNECT:
			gg_session_handler_disconnect(s);
			broken = 1;
			break;

		case GG_EVENT_NOTIFY:
		case GG_EVENT_NOTIFY_DESCR:
			{
				struct gg_notify_reply *n;

				n = (e->type == GG_EVENT_NOTIFY) ? e->event.notify : e->event.notify_descr.notify;

				for (; n->uin; n++) {
					char *descr = (e->type == GG_EVENT_NOTIFY_DESCR) ? e->event.notify_descr.descr : NULL;

					gg_session_handler_status(s, n->uin, n->status, descr, n->remote_ip, n->remote_port, n->version);
				}

				break;
			}

		case GG_EVENT_STATUS:
			gg_session_handler_status(s, e->event.status.uin, e->event.status.status, e->event.status.descr, 0, 0, 0);
			break;

#ifdef GG_STATUS60
		case GG_EVENT_STATUS60:
			gg_session_handler_status(s, e->event.status60.uin, e->event.status60.status, e->event.status60.descr, e->event.status60.remote_ip, e->event.status60.remote_port, e->event.status60.version);
			break;
#endif

#ifdef GG_NOTIFY_REPLY60
		case GG_EVENT_NOTIFY60:
			{
				int i;

				for (i = 0; e->event.notify60[i].uin; i++)
					gg_session_handler_status(s, e->event.notify60[i].uin, e->event.notify60[i].status, e->event.notify60[i].descr, e->event.notify60[i].remote_ip, e->event.notify60[i].remote_port, e->event.notify60[i].version);
				break;
			}
#endif

		case GG_EVENT_MSG:
			gg_session_handler_msg(s, e);
			break;

		case GG_EVENT_ACK:
			gg_session_handler_ack(s, e);
			break;

		case GG_EVENT_PUBDIR50_SEARCH_REPLY:
			gg_session_handler_search50(s, e);
			break;

		case GG_EVENT_PUBDIR50_WRITE:
			gg_session_handler_change50(s, e);
			break;

		case GG_EVENT_USERLIST:
			gg_session_handler_userlist(s, e);
			break;
		case GG_EVENT_IMAGE_REQUEST:
		case GG_EVENT_IMAGE_REPLY:
			gg_session_handler_image(s, e);
			break;
#ifdef HAVE_GG_DCC7
		case GG_EVENT_DCC7_NEW:
		{
			struct gg_dcc7 *dccdata = e->event.dcc7_new;
			char *uid;
			debug("GG_EVENT_DCC7_NEW\n");

			if (!gg_config_dcc) {
				gg_dcc7_reject(dccdata, GG_DCC7_REJECT_USER);
				gg_dcc7_free(dccdata);
				e->event.dcc7_new = NULL;
				break;
			}
#if 0
			if (check_dcc_limit(e) == -1)
				break;
#endif
			
			uid = saprintf("gg:%d", dccdata->peer_uin);

			switch (dccdata->dcc_type) {
				case GG_DCC7_TYPE_FILE:
				{
					struct stat st;
					char *path;
					dcc_t *d;

					d = dcc_add(s, uid, DCC_GET, dccdata);
					dcc_filename_set(d, dccdata->filename);		/* XXX< sanityzuj, cp -> iso */
					dcc_size_set(d, dccdata->size);

					print("dcc_get_offer", format_user(s, uid), d->filename, itoa(d->size), itoa(d->id));

					if (config_dcc_dir)
						path = saprintf("%s/%s", config_dcc_dir, d->filename);
					else
						path = xstrdup(d->filename);

					if (!stat(path, &st) && st.st_size < d->size)
						print("dcc_get_offer_resume", format_user(s, uid), d->filename, itoa(d->size), itoa(d->id));

					xfree(path);

					break;
				}

				case GG_DCC7_TYPE_VOICE:
				{
					dcc_t *d = dcc_add(s, uid, DCC_VOICE, dccdata);

					print("dcc_voice_offer", format_user(s, uid), itoa(d->id));
					break;
				}

				default:
					debug_error("[DCC7_NEW] unknown type %d\n", dccdata->type);
			}

			xfree(uid);

			/* XXX, add timeouter */
			/* dane watcha dostajemy w _INFO [czekamy na libgadu] */

			break;
		}

		case GG_EVENT_DCC7_REJECT:
		{
			struct gg_dcc7 *dccdata = e->event.dcc7_accept.dcc7;
			dcc_t *dcc;

			debug("GG_EVENT_DCC7_REJECT\n");

			if (!(dcc = gg_dcc_find(dccdata))) {
				debug_error("GG_EVENT_DCC7_REJECT [DCC NOT FOUND: %p]\n", dcc);
				break;
			}

			print("dcc_error_refused", format_user(dcc->session, dcc->uid));

		/* XXX, close handler powinien byc ustawiony! */
			gg_dcc7_free(dccdata);
			dcc_close(dcc);

			break;
		}

		case GG_EVENT_DCC7_ACCEPT: 
		{
			struct gg_dcc7 *dccdata = e->event.dcc7_accept.dcc7;
			dcc_t *dcc;

			debug("GG_EVENT_DCC7_ACCEPT [%p]\n", dccdata);

			timer_remove_data(&gg_plugin, dccdata);

			if (!(dcc = gg_dcc_find(dccdata))) {
				debug_error("GG_EVENT_DCC7_ACCEPT [DCC NOT FOUND: %p]\n", dcc);
				break;
			}

			watch_add(&gg_plugin, dccdata->fd, dccdata->check, gg_dcc7_handler, dccdata);
			break;
		}
#endif
		default:
			debug("[gg] unhandled event 0x%.4x, consider upgrade\n", e->type);
	}

	if (!broken && g->sess->state != GG_STATE_IDLE && g->sess->state != GG_STATE_ERROR) {
		watch_t *w;
		if ((watch == g->sess->check) && g->sess->fd == fd) { 
			if ((w = watch_find(&gg_plugin, fd, watch))) 
				watch_timeout_set(w, g->sess->timeout);
			else debug("[gg] watches managment went to hell?\n");
			gg_event_free(e);
			return 0;
		} 
		w = watch_add_session(s, g->sess->fd, g->sess->check, gg_session_handler);
		watch_timeout_set(w, g->sess->timeout);
	}
	gg_event_free(e);
	return -1;
}

/**
 * gg_changed_private()
 *
 * When connected, notify gg server about our privacy policy [Do we or don't we want to notify users not in our userlist about our status/description<br>
 * Handler executed when session variable: "private" change.
 *
 * @param s	- session
 * @param var	- session variable name
 */

static void gg_changed_private(session_t *s, const char *var) {
	gg_private_t *g;
	int status;			/* status for libgadu */
	char *cpdescr;			/* description in cp1250 for libgadu */

	if (!s || !s->connected || !(g = s->priv))
		return;

	cpdescr = gg_locale_to_cp(xstrdup(s->descr));
	status	= gg_text_to_status(s->status, cpdescr);	/* XXX, check if gg_text_to_status() return smth correct */

	if (session_int_get(s, "private") > 0)
		status |= GG_STATUS_FRIENDS_MASK;

	if (cpdescr)
		gg_change_status_descr(g->sess, status, cpdescr);
	else
		gg_change_status(g->sess, status);

	xfree(cpdescr);
}

/**
 * changed_proxy()
 *
 * Handler execute when session variable: "proxy" change
 *
 * @bug BIG XXX, Mistake at art, it should use global config variable, not session ones, because it's used to inform libgadu about proxy servers.<br>
 * 	And libgadu has got this variables global, not session private. Maybe we somehow can update these variables before gg_login() by callng gg_changed_proxy() 
 * 	but now it's BAD, BAD, BAD.
 */

static void gg_changed_proxy(session_t *s, const char *var) {
	char **auth, **userpass = NULL, **hostport = NULL;
	const char *gg_config_proxy;

	gg_proxy_port = 0;
	xfree(gg_proxy_host);
	gg_proxy_host = NULL;
	xfree(gg_proxy_username);
	gg_proxy_username = NULL;
	xfree(gg_proxy_password);
	gg_proxy_password = NULL;
	gg_proxy_enabled = 0;	

	if (!(gg_config_proxy = session_get(s, var)))
		return;

	auth = array_make(gg_config_proxy, "@", 0, 0, 0);

	if (!auth[0] || !xstrcmp(auth[0], "")) {
		array_free(auth);
		return; 
	}

	gg_proxy_enabled = 1;

	if (auth[0] && auth[1]) {
		userpass = array_make(auth[0], ":", 0, 0, 0);
		hostport = array_make(auth[1], ":", 0, 0, 0);
	} else
		hostport = array_make(auth[0], ":", 0, 0, 0);

	if (userpass && userpass[0] && userpass[1]) {
		gg_proxy_username = xstrdup(userpass[0]);
		gg_proxy_password = xstrdup(userpass[1]);
	}

	gg_proxy_host = xstrdup(hostport[0]);
	gg_proxy_port = (hostport[1]) ? atoi(hostport[1]) : 8080;

	array_free(hostport);
	array_free(userpass);
	array_free(auth);
}

static int gg_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("gg_version", _("%> %TGadu-Gadu%n: libgadu %g%1%n (headers %c%2%n), protocol %g%3%n (%c0x%4%n)"), 1);
	/* /list */
	format_add("gg_user_info_name", _("%K| %nName: %T%1 %2%n\n"), 1);
	format_add("gg_user_info_mobile", _("%K| %nTelephone: %T%1%n\n"), 1);
	format_add("gg_user_info_not_in_contacts", _("%K| %nDoesn't have us in roster\n"), 1);
	format_add("gg_user_info_firewalled", _("%K| %nFirewalled/NATed\n"), 1);
	format_add("gg_user_info_ip", _("%K| %nAddress: %T%1%n\n"), 1);
	format_add("gg_user_info_last_ip", _("%K| %nLast address: %T%1%n\n"), 1);
	format_add("gg_user_info_voip", _("%K| %nVoIP-capable\n"), 1);
	format_add("gg_user_info_version", _("%K| %nVersion: %T%1%n\n"),1);
	/* token things */
	format_add("gg_token", _("%> Token was written to the file %T%1%n\n"), 1);
	format_add("gg_token_ocr", _("%> Token: %T%1%n\n"), 1);
	format_add("gg_token_body", "%1\n", 1);
	format_add("gg_token_failed", _("%! Error getting token: %1\n"), 1);
	format_add("gg_token_failed_saved", _("%! Error reading token: %1 (saved@%2)\n"), 1);
	format_add("gg_token_timeout", _("%! Token getting timeout\n"), 1);
	format_add("gg_token_unsupported", _("%! Your operating system doesn't support tokens\n"), 1);
	format_add("gg_token_missing", _("%! First get token by function %Ttoken%n\n"), 1);
	/* check_conn */
	format_add("gg_user_is_connected", _("%> (%1) User %T%2%n is connected\n"), 1);
	format_add("gg_user_is_not_connected", _("%> (%1) User %T%2%n is not connected\n"), 1);
	format_add("gg_we_are_being_checked", _("%> (%1) We are being checked by %T%2%n\n"), 1);
	/* images */
	format_add("gg_image_cant_open_file", _("%! Can't open file for image %1 (%2)\n"), 1);
	format_add("gg_image_error_send", _("%! Error sending image\n"), 1);
	format_add("gg_image_ok_send", _("%> Image sent properly\n"), 1);
	format_add("gg_image_ok_get", _("%> Image <%3> saved in %1\n"), 1);	/* %1 - path, %2 - uid, %3 - name of picture */
#endif
	return 0;
}

static QUERY(gg_setvar_default) {
	xfree(gg_config_dcc_ip);
	xfree(gg_config_images_dir);
	xfree(gg_config_dcc_limit);

	gg_config_display_token = 1;
	gg_config_get_images = 0;
	gg_config_images_dir = NULL;
	gg_config_image_size = 20;
	gg_config_dcc = 0;
	gg_config_dcc_ip = NULL;
	gg_config_dcc_limit = xstrdup("30/30");
	gg_config_dcc_port = 1550;
	return 0;
}

/**
 * libgadu_debug_handler()
 *
 * Handler for libgadu: gg_debug_handler<br>
 * It's communcation channel between libgadu debug messages, and ekg2.<br>
 * Here we translate libgadu levels to ekg2 one, and than pass it to ekg_debug_handler()
 *
 * @param level - libgadu debug level
 */

static void libgadu_debug_handler(int level, const char *format, va_list ap) {
	int newlevel;

	if (!config_debug) return;

	switch (level) {
		/* stale z libgadu.h */
/*		case GG_DEBUG_NET: 		 1:	newlevel = 0;	break; */		/* never used ? */
		case /* GG_DEBUG_TRAFFIC */ 	 2:	newlevel = DEBUG_IO;		break;
		case /* GG_DEBUG_DUMP */	 4:	newlevel = DEBUG_IO;		break;
		case /* GG_DEBUG_FUNCTION */	 8:	newlevel = DEBUG_FUNCTION;	break;
		case /* GG_DEBUG_MISC */	16:	newlevel = DEBUG_GGMISC;	break;
		default:				newlevel = 0;			break;
	}
	ekg_debug_handler(newlevel, format, ap);
}

	/* XXX: move whole scrolling into timer? */
int gg_idle_handler(void *data) {
	struct timeval *tv = data;
	session_t *sl;

	/* sprawd¼ scroll timeouty */
	/* XXX: nie tworzyæ variabla globalnego! */
	for (sl = sessions; sl; sl = sl->next) {
		session_t *s	= sl;
		gg_private_t *g	= s->priv;
		int tmp;

		if (!s->connected || s->plugin != &gg_plugin || !g)
			continue;

		if (!(tmp = session_int_get(s, "scroll_long_desc")) || tmp == -1)
			continue;

		if (tv->tv_sec - g->scroll_last > tmp)
			command_exec(NULL, s, ("/_autoscroll"), 0);
	}

	return 0;
}

static plugins_params_t gg_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		VAR_STR, 0, 0, NULL), 
	PLUGIN_VAR_ADD("auto_away", 		VAR_INT, "600", 0, NULL),
	PLUGIN_VAR_ADD("auto_away_descr", 	VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("auto_back", 		VAR_INT, "0", 0, NULL),	
	PLUGIN_VAR_ADD("auto_connect", 		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_find", 		VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_reconnect", 	VAR_INT, "10", 0, NULL),
	PLUGIN_VAR_ADD("concat_multiline_status",VAR_INT, "3", 0, NULL),
	PLUGIN_VAR_ADD("connection_save", 	VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("display_notify", 	VAR_INT, "-1", 0, NULL),
	PLUGIN_VAR_ADD("email", 		VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("invisible_check_interval",VAR_INT, 0, 0, NULL),
	PLUGIN_VAR_ADD("local_ip", 		VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("log_formats", 		VAR_STR, "xml,simple", 0, NULL),
	PLUGIN_VAR_ADD("password", 		VAR_STR, NULL, 1, NULL),
	PLUGIN_VAR_ADD("port", 			VAR_INT, "8074", 0, NULL),
	PLUGIN_VAR_ADD("private", 		VAR_BOOL, "0", 0, gg_changed_private),
	PLUGIN_VAR_ADD("protocol", 		VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("proxy", 		VAR_STR, NULL, 0, gg_changed_proxy),
	PLUGIN_VAR_ADD("proxy_forwarding", 	VAR_STR, NULL, 0, NULL),
	PLUGIN_VAR_ADD("scroll_long_desc", 	VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("scroll_mode", 		VAR_STR, "bounce", 0, NULL),
	PLUGIN_VAR_ADD("server", 		VAR_STR, NULL, 0, NULL),

	PLUGIN_VAR_END()
};

int EXPORT gg_plugin_init(int prio) {
	va_list dummy;

	PLUGIN_CHECK_VER("gg");

	gg_plugin.params = gg_plugin_vars;

	plugin_register(&gg_plugin, prio);
	gg_setvar_default(NULL, dummy);

	query_connect_id(&gg_plugin, SET_VARS_DEFAULT, gg_setvar_default, NULL);
	query_connect_id(&gg_plugin, PROTOCOL_VALIDATE_UID, gg_validate_uid, NULL);
	query_connect_id(&gg_plugin, GET_PLUGIN_PROTOCOLS, gg_protocols, NULL);
	query_connect_id(&gg_plugin, PLUGIN_PRINT_VERSION, gg_print_version, NULL);
	query_connect_id(&gg_plugin, SESSION_ADDED, gg_session_init, NULL);
	query_connect_id(&gg_plugin, SESSION_REMOVED, gg_session_deinit, NULL);
	query_connect_id(&gg_plugin, ADD_NOTIFY, gg_add_notify_handle, NULL);
	query_connect_id(&gg_plugin, REMOVE_NOTIFY, gg_remove_notify_handle, NULL);
	query_connect_id(&gg_plugin, STATUS_SHOW, gg_status_show_handle, NULL);
	query_connect(&gg_plugin, ("user-offline"), gg_user_offline_handle, NULL);
	query_connect(&gg_plugin, ("user-online"), gg_user_online_handle, NULL);
	query_connect_id(&gg_plugin, PROTOCOL_UNIGNORE, gg_user_online_handle, (void *)1);
	query_connect_id(&gg_plugin, USERLIST_INFO, gg_userlist_info_handle, NULL);
	query_connect_id(&gg_plugin, USERLIST_PRIVHANDLE, gg_userlist_priv_handler, NULL);
	query_connect_id(&gg_plugin, CONFIG_POSTINIT, gg_convert_string_init, NULL);

	gg_register_commands();

	variable_add(&gg_plugin, ("audio"), VAR_BOOL, 1, &gg_config_audio, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, ("display_token"), VAR_BOOL, 1, &gg_config_display_token, NULL, NULL, NULL);
	variable_add(&gg_plugin, ("dcc"), VAR_BOOL, 1, &gg_config_dcc, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, ("dcc_ip"), VAR_STR, 1, &gg_config_dcc_ip, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, ("dcc_limit"), VAR_STR, 1, &gg_config_dcc_limit, NULL, NULL, NULL);
	variable_add(&gg_plugin, ("dcc_port"), VAR_INT, 1, &gg_config_dcc_port, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, ("get_images"), VAR_BOOL, 1, &gg_config_get_images, NULL, NULL, NULL);
	variable_add(&gg_plugin, ("images_dir"), VAR_STR, 1, &gg_config_images_dir, NULL, NULL, NULL);
	variable_add(&gg_plugin, ("image_size"), VAR_INT, 1, &gg_config_image_size, gg_changed_images, NULL, NULL);
	variable_add(&gg_plugin, ("skip_default_format"), VAR_BOOL, 1, &gg_config_skip_default_format, NULL, NULL, NULL);
	variable_add(&gg_plugin, ("split_messages"), VAR_BOOL, 1, &gg_config_split_messages, NULL, NULL, NULL);

	idle_add(&gg_plugin, gg_idle_handler, &ekg_tv);

	gg_debug_handler	= libgadu_debug_handler;
	gg_debug_level		= 255;

	return 0;
}

static int gg_plugin_destroy() {
	list_t l;

	list_destroy(gg_currently_checked, 1);

	for (l = gg_reminds; l; l = l->next) {
		struct gg_http *h = l->data;

		watch_remove(&gg_plugin, h->fd, h->check);
		gg_pubdir_free(h);
	}

	for (l = gg_registers; l; l = l->next) {
		struct gg_http *h = l->data;

		watch_remove(&gg_plugin, h->fd, h->check);
		gg_pubdir_free(h);
	}

	for (l = gg_unregisters; l; l = l->next) {
		struct gg_http *h = l->data;

		watch_remove(&gg_plugin, h->fd, h->check);
		gg_pubdir_free(h);
	}

	xfree(gg_register_password);
	gg_register_password = NULL;
	xfree(gg_register_email);
	gg_register_email = NULL;

	image_flush_queue();
	gg_convert_string_destroy();

	plugin_unregister(&gg_plugin);

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=0 noexpandtab sw=8
 */
