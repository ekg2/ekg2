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
int gg_config_split_messages;

static int gg_private_init(session_t *s) {
	gg_private_t *g;

	if (!s)
		return -1;

	if (xstrncasecmp(session_uid_get(s), "gg:", 3))
		return -1;

	g = xmalloc(sizeof(gg_private_t));

	userlist_free(s);
	userlist_read(s);
	session_private_set(s, g);

	return 0;
}

static int gg_private_destroy(session_t *s) {
	gg_private_t *g;
	list_t l;

	if (!s)
		return -1;

	if (xstrncasecmp(session_uid_get(s), "gg:", 3))
		return -1;

	if (!(g = session_private_get(s)))
		return -1;

	if (g->sess)
		gg_free_session(g->sess);

	for (l = g->searches; l; l = l->next)
		gg_pubdir50_free((gg_pubdir50_t) l->data);

	xfree(g);

	session_private_set(s, NULL);

	return 0;
}

static QUERY(gg_userlist_info_handle) {
	userlist_t *u	= *va_arg(ap, userlist_t **);
	int quiet	= *va_arg(ap, int *);
	if (!u)
		return 0;

	if (valid_plugin_uid(&gg_plugin, u->uid) != 1) 
		return 0;

	if (u->port == 2)
		wcs_printq("user_info_not_in_contacts");
	if (u->port == 1)
		wcs_printq("user_info_firewalled");
	if ((u->protocol & GG_HAS_AUDIO_MASK))
		wcs_printq("user_info_voip");

	if ((u->protocol & 0x00ffffff)) {
		int v = u->protocol & 0x00ffffff;
		const CHAR_T *ver = NULL;

		if (v < 0x0b)
			ver = TEXT("<= 4.0.x");
		if (v >= 0x0f && v <= 0x10)
			ver = TEXT("4.5.x");
		if (v == 0x11)
			ver = TEXT("4.6.x");
		if (v >= 0x14 && v <= 0x15)
			ver = TEXT("4.8.x");
		if (v >= 0x16 && v <= 0x17)
			ver = TEXT("4.9.x");
		if (v >= 0x18 && v <= 0x1b)
			ver = TEXT("5.0.x");
		if (v >= 0x1c && v <= 0x1e)
			ver = TEXT("5.7");
		if (v == 0x20)
			ver = TEXT("6.0 (build >= 129)");
		if (v == 0x21)
			ver = TEXT("6.0 (build >= 133)");
		if (v == 0x22)
			ver = TEXT("6.0 (build >= 140)");
		if (v == 0x24)
			ver = TEXT("6.1 (build >= 155)");
		if (v == 0x25)
			ver = TEXT("7.0 (build >= 1)");
		if (v == 0x26)
			ver = TEXT("7.0 (build >= 20)");
		if (v == 0x27)
			ver = TEXT("7.0 (build >= 22)");
		if (ver)
			wcs_printq("user_info_version", ver);

		else {
			CHAR_T *tmp = wcsprintf(TEXT("nieznana (%#.2x)"), v);
			wcs_printq("user_info_version", tmp);
			xfree(tmp);
		}
	}
	return 0;
}

static QUERY(gg_session_handle) {
	char **uid = va_arg(ap, char**);
	session_t *s = session_find(*uid);

	if (!s)
		return 0;

	if (data)
		gg_private_init(s);
	else
		gg_private_destroy(s);

	return 0;
}

static QUERY(gg_user_offline_handle) {
	userlist_t *u	= *(va_arg(ap, userlist_t **));
	session_t *s	= *(va_arg(ap, session_t **));
	gg_private_t *g = session_private_get(s);
	int uin;

	if (!session_check(s, 1, "gg")) return 1;
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
	gg_private_t *g = session_private_get(s);
	int quiet = (int) data;
	int uin;

	if (!session_check(s, 1, "gg")) return 1;
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
	struct tm *t;
	time_t n;
	int mqc, now_days;
	char *tmp, *priv, *r1, *r2, buf[100];
	gg_private_t *g;

	if (!s) {
		debug("Function gg_status_show_handle() called with NULL data\n");
		return -1;
	}
	if (!(g = session_private_get(s)))
		return -1;

	if (config_profile)
		print("show_status_profile", config_profile);

	if ((u = userlist_find(s, s->uid)) && u->nickname)
		print("show_status_uid_nick", s->uid, u->nickname);
	else
		print("show_status_uid", s->uid);

	n = time(NULL);
	t = localtime(&n);
	now_days = t->tm_yday;

	t = localtime(&s->last_conn);
	strftime(buf, sizeof(buf), format_find((t->tm_yday == now_days) ? "show_status_last_conn_event_today" : "show_status_last_conn_event"), t);

	if (!g->sess || g->sess->state != GG_STATE_CONNECTED) {
		char *tmp = format_string(format_find("show_status_notavail"), "");
		print("show_status_status_simple", tmp);
		xfree(tmp);

		if (s->last_conn)
			print("show_status_disconnected_since", buf);
		if ((mqc = msg_queue_count()))
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
	print("show_status_connected_since", buf);

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

/* 
 * trzeba dodaæ numer u¿ytkownika do listy osób, o których
 * zmianach statusów chcemy byæ informowani 
 */
static QUERY(gg_add_notify_handle) {
	char **session_uid = va_arg(ap, char**);
	char **uid = va_arg(ap, char**);
	session_t *s = session_find(*session_uid);
	gg_private_t *g;

	if (!s) {
		debug("Function gg_add_notify_handle() called with NULL data\n");
		return 1;
	}

	if (!session_check(s, 0, "gg"))		return 1;	/* not gg session. */
	if (!(g = session_private_get(s)))
		return 1;

	gg_add_notify_ex(g->sess, str_to_uin(xstrchr(*uid, ':') + 1), gg_userlist_type(userlist_find(s, s->uid))); 
	return 0;
}

/*
 * trzeba usun±æ numer u¿ytkownika z listy osób, o których
 * zmianach statusów chcemy byæ informowani
 */
static QUERY(gg_remove_notify_handle) {
	char **session_uid = va_arg(ap, char**);
	session_t *s = session_find(*session_uid);
	char *uid = *(va_arg(ap, char**));
	gg_private_t *g;
	char *tmp;

	if (!s) {
		debug("Function gg_remove_notify_handle() called with NULL data\n");
		return 1;
	}
	if (!session_check(s, 1, "gg"))		return 1;
	if (!(g = session_private_get(s)))	return 1;
	if (!(tmp = xstrchr(uid, ':')))		return 1;

	gg_remove_notify(g->sess, str_to_uin(tmp+1));
	return 0;
}

/*
 * gg_print_version()
 *
 * wy¶wietla wersjê pluginu i biblioteki.
 */
static QUERY(gg_print_version) {
	char **tmp1 = array_make(GG_DEFAULT_CLIENT_VERSION, ", ", 0, 1, 0);
	char *tmp2 = array_join(tmp1, ".");
	CHAR_T *tmp3 = wcsprintf(TEXT("libgadu %s (headers %s), protocol %s (0x%.2x)"), gg_libgadu_version(), GG_LIBGADU_VERSION, tmp2, GG_DEFAULT_PROTOCOL_VERSION);

	wcs_print("generic", tmp3);

	xfree(tmp3);
	xfree(tmp2);
	array_free(tmp1);

	return 0;
}

/*
 * gg_validate_uid()
 *
 * sprawdza, czy dany uid jest poprawny i czy plugin do obs³uguje.
 */
static QUERY(gg_validate_uid) {
	char *uid	= *(va_arg(ap, char **));
	int *valid	= va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "gg:", 3) && xstrlen(uid)>3) {
		/* sprawdzmy, czy w uidzie wystepuja tylko cyferki... */
		uid+=3;
		for (; *uid; uid++)
			if (!isdigit(*uid))
				return 0;

		(*valid)++;
		return -1;
	}
	return 0;
}

/*
 * gg_ping_timer_handler()
 *
 * pinguje serwer co jaki¶ czas, je¶li jest nadal po³±czony.
 */
static TIMER(gg_ping_timer_handler) {
	session_t *s = session_find((char*) data);
	gg_private_t *g;

	if (type == 1) {
		xfree(data);
		return 0;
	}
	if (!s || !session_connected_get(s)) {
		return -1;
	}

	if ((g = session_private_get(s))) {
		gg_ping(g->sess);
	}
	return 0;
}

/*
 * gg_session_handler_success()
 *
 * obs³uga udanego po³±czenia z serwerem.
 */
static void gg_session_handler_success(session_t *s) {
	gg_private_t *g = session_private_get(s);
	const char *status;
	char *__session;
	char buf[100];
	int _status;
	CHAR_T *descr;
	char *cpdescr; 

	if (!g || !g->sess) {
		debug("[gg] gg_session_handler_success() called with null gg_private_t\n");
		return;
	}

	session_connected_set(s, 1);
	session_unidle(s);

	__session = xstrdup(session_uid_get(s));
	query_emit(NULL, TEXT("protocol-connected"), &__session);
	xfree(__session);

	gg_userlist_send(g->sess, s->userlist);

	s->last_conn = time(NULL);

	/* zapiszmy adres serwera */
	if (session_int_get(s, "connection_save") == 1) {
		struct in_addr addr;		

		addr.s_addr = g->sess->server_addr;
		session_set(s, "server", inet_ntoa(addr));
		session_int_set(s, "port", g->sess->port);
	}
	/* XXX, check if that timer already exists !!! */
	/* pamiêtajmy, ¿eby pingowaæ */
	snprintf(buf, sizeof(buf), "ping-%s", s->uid + 3);
	timer_add(&gg_plugin, buf, 180, 1, gg_ping_timer_handler, xstrdup(s->uid));
#if USE_UNICODE
	descr = normal_to_wcs(session_descr_get(s));
#else
	descr = xstrdup(session_descr_get(s));
#endif
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
#if USE_UNICODE
	xfree(descr);
#endif
}

/*
 * gg_session_handler_failure()
 *
 * obs³uga nieudanego po³±czenia.
 */
static void gg_session_handler_failure(session_t *s, struct gg_event *e) {
	const char *reason;
	gg_private_t *g = session_private_get(s);

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
	}


	gg_free_session(g->sess);
	g->sess = NULL;

	{
		char *__session = xstrdup(session_uid_get(s));
		char *__reason = xstrdup(format_find(reason));
		int __type = EKG_DISCONNECT_FAILURE;

		query_emit(NULL, TEXT("protocol-disconnected"), &__session, &__reason, &__type, NULL);

		xfree(__reason);
		xfree(__session);
	}
}

/*
 * gg_session_handler_disconnect()
 *
 * obs³uga roz³±czenia z powodu pod³±czenia drugiej sesji.
 */
static void gg_session_handler_disconnect(session_t *s) {
	gg_private_t *g = session_private_get(s);
	char *__session	= xstrdup(session_uid_get(s));
	char *__reason	= NULL;
	int __type	= EKG_DISCONNECT_FORCED;

	session_connected_set(s, 0);

	query_emit(NULL, TEXT("protocol-disconnected"), &__session, &__reason, &__type, NULL);

	xfree(__session);
	xfree(__reason);

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
	char *__session	= xstrdup(session_uid_get(s));
	CHAR_T *__uid	= wcsprintf(TEXT("gg:%d"), uin);
	CHAR_T *__status= xwcsdup(gg_status_to_text(status));
	char *__descr	= xstrdup(descr);
	char *__host	= (ip) ? xstrdup(inet_ntoa(*((struct in_addr*)(&ip)))) : NULL;
	time_t when	= time(NULL);
	int __port	= port, i, j, dlen, state = 0, m = 0;
	userlist_t *u;
	CHAR_T *sdescr;

	sdescr = gg_cp_to_locale(__descr);

	if ((u = userlist_find(s, wcs_to_normal(__uid)))) /* UUU */
		u->protocol = protocol;

	for (i = 0; i < xwcslen(sdescr); i++)
		if (sdescr[i] == 10 || sdescr[i] == 13)
			m++;
	dlen = i;
	/* if it is not set it'll be -1 so, everythings ok */
	if ( (i = session_int_get(s, "concat_multiline_status")) && m > i)
	{
		for (m = i = j = 0; i < dlen; i++) {
			if (sdescr[i] != 10 && sdescr[i] != 13) {
				sdescr[j++] = sdescr[i];
				state = 0;
			} else {
				if (!state && sdescr[i] == 10)
					sdescr[j++] = ' ';
				else
					m++;
				if (sdescr[i] == 10)
					state++;
			}
		}
		sdescr[j] = '\0';
		if (m > 3) {
			memmove (__descr+4, __descr, j + 1);
			/* multiline tag */
			sdescr[0] = '['; sdescr[1] = 'm'; sdescr[2] = ']'; sdescr[3] = ' ';
		}

	}
	{
		CHAR_T *session	= normal_to_wcs(__session);
		CHAR_T *host	= normal_to_wcs(__host);

		query_emit(NULL, TEXT("wcs_protocol-status"), &session, &__uid, &__status, &sdescr, &host, &__port, &when, NULL);

		free_utf(session);
		free_utf(host);
	}
	free_utf(sdescr);

	xfree(__host);
	xfree(__descr);
	xfree(__status);
	xfree(__uid);
	xfree(__session);
}

/*
 * gg_session_handler_msg()
 *
 * obs³uga przychodz±cych wiadomo¶ci.
 */
static void gg_session_handler_msg(session_t *s, struct gg_event *e) {
	char *__sender, **__rcpts = NULL;
	char *__text;
	CHAR_T *ltext;
	uint32_t *__format = NULL;
	int image = 0, check_inv = 0;
	int i;

	gg_private_t *g = session_private_get(s);

	if (gg_config_dcc && (e->event.msg.msgclass & GG_CLASS_CTCP)) {
		struct gg_dcc *d;
		char *__host = NULL;
		char uid[16];
		int __port = -1, __valid = 1;
		userlist_t *u;
		watch_t *w;

		snprintf(uid, sizeof(uid), "gg:%d", e->event.msg.sender);

		if (!(u = userlist_find(s, uid)))
			return;

		query_emit(NULL, TEXT("protocol-dcc-validate"), &__host, &__port, &__valid, NULL);
/*		xfree(__host); */

		if (!__valid) {
			print_status("dcc_attack", format_user(s, uid));
			command_exec_format(NULL, s, 0, TEXT("/ignore %s"), uid);
			return;
		}

		if (!(d = gg_dcc_get_file(u->ip, u->port, atoi(session_uid_get(s) + 3), e->event.msg.sender))) {
			print_status("dcc_error", strerror(errno));
			return;
		}

		w = watch_add(&gg_plugin, d->fd, d->check, gg_dcc_handler, d);
		watch_timeout_set(w, d->timeout);

		return;
	}

	if (e->event.msg.msgclass & GG_CLASS_CTCP)
		return;

	for (i = 0; i < e->event.msg.recipients_count; i++)
		array_add(&__rcpts, saprintf("gg:%d", e->event.msg.recipients[i]));

	__text = xstrdup(e->event.msg.message);
	ltext = gg_cp_to_locale(__text);

	if (e->event.msg.formats && e->event.msg.formats_length) {
		unsigned char *p = e->event.msg.formats;
		int i, len = xstrlen(__text), ii;

		__format = xcalloc(len, sizeof(uint32_t));

		gg_debug(GG_DEBUG_DUMP, "// formats:");
		for (ii = 0; ii < e->event.msg.formats_length; ii++)
			gg_debug(GG_DEBUG_DUMP, " %.2x", (unsigned char) p[ii]);
		gg_debug(GG_DEBUG_DUMP, "\n");

		for (i = 0; i < e->event.msg.formats_length; ) {
			int j, pos = p[i] + p[i + 1] * 256;
			uint32_t val = 0;

			if ((p[i + 2] & GG_FONT_IMAGE))	{
				image=1;

				if (((struct gg_msg_richtext_image*)&p[i+3])->crc32 == GG_CRC32_INVISIBLE)
					check_inv = 1;

				if (gg_config_get_images){
					gg_image_request(g->sess, e->event.msg.sender, ((struct gg_msg_richtext_image*)&p[i+3])->size, ((struct gg_msg_richtext_image*)&p[i+3])->crc32);
				}
				i+=10;

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
		char *__session = xstrdup(session_uid_get(s));
		char *__seq	= NULL;
		time_t __sent	= e->event.msg.time;
		int __class	= e->event.msg.sender ? EKG_MSGCLASS_CHAT : EKG_MSGCLASS_SYSTEM;
		int ekgbeep	= EKG_TRY_BEEP;
		int secure	= 0;

/*		if (!check_inv || xstrcmp(__text, ""))
			printq("generic", "image in message.\n"); - or something
 */
		{
			char *text = wcs_to_normal(ltext);
			query_emit(NULL, TEXT("protocol-message"), &__session, &__sender, &__rcpts, &text, &__format, &__sent, &__class, &__seq, &ekgbeep, &secure);
			free_utf(text);
		}
		xfree(__session);

/*		xfree(__seq); */
	}
	
	xfree(__text);
	xfree(__sender);
	xfree(__format);
	array_free(__rcpts);
	free_utf(ltext);
}

/*
 * gg_session_handler_ack()
 *
 * obs³uga potwierdzeñ wiadomo¶ci.
 */
static void gg_session_handler_ack(session_t *s, struct gg_event *e) {
	char *__session = xstrdup(session_uid_get(s));
	CHAR_T *__rcpt	= wcsprintf(TEXT("gg:%d"), e->event.ack.recipient);
	CHAR_T *__seq	= xwcsdup(wcs_itoa(e->event.ack.seq));
	CHAR_T *__status;

	switch (e->event.ack.status) {
		case GG_ACK_DELIVERED:
			__status = xwcsdup(EKG_ACK_DELIVERED);
			break;
		case GG_ACK_QUEUED:
			__status = xwcsdup(EKG_ACK_QUEUED);
			break;
		case GG_ACK_NOT_DELIVERED:
			__status = xwcsdup(EKG_ACK_DROPPED);
			break;
		default:
			debug("[gg] unknown message ack status. consider upgrade\n");
			__status = xwcsdup(EKG_ACK_UNKNOWN);
			break;
	}
	{
		CHAR_T *session = normal_to_wcs(__session);
		query_emit(NULL, TEXT("protocol-message-ack"), &session, &__rcpt, &__seq, &__status, NULL);
		free_utf(session);
	}

	xfree(__status);
	xfree(__seq);
	xfree(__rcpt);
	xfree(__session);
}

/*
 * image_open_file()
 *
 * create and open file 
 * for image
 */

static FILE* image_open_file(const char *path) {
	struct stat statbuf;
	char *dir, *slash;
	int slash_pos = 0;

        debug("[gg] opening image file\n");

	while (1) {
		if (!(slash = xstrchr(path + slash_pos, '/'))) {
			// nie ma juz slashy - zostala tylko nazwa pliku
			break;		// konczymy petle
		};

		slash_pos = slash - path + 1;
		dir = xstrndup(path, slash_pos);

		if (stat(dir, &statbuf) != 0 && mkdir(dir, 0700) == -1) {
			CHAR_T *bo = wcsprintf(TEXT("nie mozna %s bo %s"), dir, strerror(errno));
			wcs_print("generic",bo); // XXX usun±æ !! 
			xfree(bo);
			xfree(dir);
			return NULL;
		}
		xfree(dir);
	} // while mkdir..

	return fopen(path, "w");
};

/*
 * gg_session_handler_image()
 *
 * support image request or reply
 * now it is used only by /check_inv 
 * we don't use support images
 */
static void gg_session_handler_image(session_t *s, struct gg_event *e) {
	gg_private_t *g = session_private_get(s);

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

						if (!session_compare(c->session, s) && !xstrcmp(c->uid, tmp)) {
							print("gg_user_is_connected", session_name(s), format_user(s, tmp));
							list_remove(&gg_currently_checked, c, 1);
							break;
						}

					}

					xfree(tmp);
					break;
				}

				for (l = images; l; l = l->next) {
					image_t *i = l->data;

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
			{
				char *image_file = NULL;
				FILE *fp;

				// TODO: file name format should be defined by user
				image_file = saprintf("%s/%s_%s_%s", gg_config_images_dir, itoa(e->event.image_reply.sender), itoa(e->event.image_reply.crc32), e->event.image_reply.filename);
				debug("image from %d called %s\n", e->event.image_reply.sender, image_file);

				if ((fp = image_open_file(image_file)) == NULL) {
					print("gg_image_cant_open_file", image_file);
				} else {
					int i;

					for (i = 0; i<e->event.image_reply.size; i++) {
						fputc(e->event.image_reply.image[i],fp);
					}
					fclose(fp);
					print("gg_image_ok_get", image_file);
				}
				xfree(image_file);
			}
		default:
			debug("// gg_session_handler_image() - This function is not supported yet\n");
			break;
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
			wcs_print("userlist_get_ok");

			if (e->event.userlist.reply) {
				CHAR_T *reply;
				list_t l;
				gg_private_t *g = session_private_get(s);

				/* remove all contacts from notification list on server */
				for (l = s->userlist; l; l = l->next) {
					userlist_t *u = l->data;
					char *parsed;

					if (!u || !(parsed = xstrchr(u->uid, ':')))
						continue;

					gg_remove_notify_ex(g->sess, str_to_uin(parsed + 1), gg_userlist_type(u));
				}
				reply = gg_cp_to_locale(e->event.userlist.reply);
				userlist_set(s, reply);
				gg_userlist_send(g->sess, s->userlist);

				config_changed = 1;
			}
			break;
		case GG_USERLIST_PUT_REPLY:
			switch (gg_userlist_put_config) {
				case 0:	wcs_print("userlist_put_ok");		break;
				case 1:	wcs_print("userlist_config_put_ok");	break;
				case 2:	wcs_print("userlist_clear_ok");		break;
				case 3:	wcs_print("userlist_config_clear_ok");	break;
			}
			break;
	}
}

/*
 * gg_session_handler()
 *
 * obs³uga zdarzeñ przy po³±czeniu gg.
 */
WATCHER(gg_session_handler)		/* tymczasowe */
{
	gg_private_t *g = session_private_get((session_t*) data);
	struct gg_event *e;
	int broken = 0;

	if (type == 1) {
		/* tutaj powinni¶my usun±æ dane watcha. nie, dziêkujê. */
		return 0;
	}

	if (!g || !g->sess) {
		debug("[gg] gg_session_handler() called with NULL gg_session\n");
		return -1;
	}

	if (type == 2) {
		if (g->sess->state != GG_STATE_CONNECTING_GG) {
			char *__session = xstrdup(session_uid_get((session_t*) data));
			char *__reason = NULL;
			int __type = EKG_DISCONNECT_FAILURE;

			query_emit(NULL, TEXT("protocol-disconnected"), &__session, &__reason, &__type, NULL);

			xfree(__reason);
			xfree(__session);
			gg_free_session(g->sess);
			g->sess = NULL;

			return -1;
		}

		/* je¶li jest GG_STATE_CONNECTING_GG to ka¿emy stwierdziæ
		 * b³±d (EINPROGRESS) i ³±czyæ siê z kolejnym kandydatem. */
	}

	if (!(e = gg_watch_fd(g->sess))) {
		char *__session = xstrdup(session_uid_get((session_t*) data));
		char *__reason = NULL;
		int __type = EKG_DISCONNECT_NETWORK;

		session_connected_set((session_t*) data, 0);

		query_emit(NULL, TEXT("protocol-disconnected"), &__session, &__reason, &__type, NULL);

		xfree(__reason);
		xfree(__session);

		gg_free_session(g->sess);
		g->sess = NULL;

		return -1;
	}

	switch (e->type) {
		case GG_EVENT_NONE:
			break;

		case GG_EVENT_CONN_SUCCESS:
			gg_session_handler_success(data);
			break;

		case GG_EVENT_CONN_FAILED:
			gg_session_handler_failure(data, e);
			broken = 1;
			break;

		case GG_EVENT_DISCONNECT:
			gg_session_handler_disconnect(data);
			broken = 1;
			break;

		case GG_EVENT_NOTIFY:
		case GG_EVENT_NOTIFY_DESCR:
			{
				struct gg_notify_reply *n;

				n = (e->type == GG_EVENT_NOTIFY) ? e->event.notify : e->event.notify_descr.notify;

				for (; n->uin; n++) {
					char *descr = (e->type == GG_EVENT_NOTIFY_DESCR) ? e->event.notify_descr.descr : NULL;

					gg_session_handler_status(data, n->uin, n->status, descr, n->remote_ip, n->remote_port, n->version);
				}

				break;
			}

		case GG_EVENT_STATUS:
			gg_session_handler_status(data, e->event.status.uin, e->event.status.status, e->event.status.descr, 0, 0, 0);
			break;

#ifdef GG_STATUS60
		case GG_EVENT_STATUS60:
			gg_session_handler_status(data, e->event.status60.uin, e->event.status60.status, e->event.status60.descr, e->event.status60.remote_ip, e->event.status60.remote_port, e->event.status60.version);
			break;
#endif

#ifdef GG_NOTIFY_REPLY60
		case GG_EVENT_NOTIFY60:
			{
				int i;

				for (i = 0; e->event.notify60[i].uin; i++)
					gg_session_handler_status(data, e->event.notify60[i].uin, e->event.notify60[i].status, e->event.notify60[i].descr, e->event.notify60[i].remote_ip, e->event.notify60[i].remote_port, e->event.notify60[i].version);
				break;
			}
#endif

		case GG_EVENT_MSG:
			gg_session_handler_msg(data, e);
			break;

		case GG_EVENT_ACK:
			gg_session_handler_ack(data, e);
			break;

		case GG_EVENT_PUBDIR50_SEARCH_REPLY:
			gg_session_handler_search50(data, e);
			break;

		case GG_EVENT_PUBDIR50_WRITE:
			gg_session_handler_change50(data, e);
			break;

		case GG_EVENT_USERLIST:
			gg_session_handler_userlist(data, e);
			break;
		case GG_EVENT_IMAGE_REQUEST:
		case GG_EVENT_IMAGE_REPLY:
			{
				gg_session_handler_image(data, e);
				break;
			}

		default:
			debug("[gg] unhandled event 0x%.4x, consider upgrade\n", e->type);
	}

	if (!broken && g->sess->state != GG_STATE_IDLE && g->sess->state != GG_STATE_ERROR) {
		watch_t *w;
		if ((watch == g->sess->check) && g->sess->fd == fd) { 
			if ((w = watch_find(&gg_plugin, fd, (int) watch))) 
				watch_timeout_set(w, g->sess->timeout);
			else debug("[gg] watches managment went to hell?\n");
			gg_event_free(e);
			return 0;
		} 
		w = watch_add(&gg_plugin, g->sess->fd, g->sess->check, gg_session_handler, data);
		watch_timeout_set(w, g->sess->timeout);
	}
	gg_event_free(e);
	return -1;
}

static void gg_changed_private(session_t *s, const char *var) {
	gg_private_t *g = (s) ? session_private_get(s) : NULL;
	const char *status = session_status_get(s);
	CHAR_T *descr;
	char *cpdescr;
	int _status;

	if (!session_connected_get(s)) {
		return;
	}

#if USE_UNICODE
	descr = normal_to_wcs(session_descr_get(s));
#else
	descr = xstrdup(session_descr_get(s));
#endif

	cpdescr = gg_locale_to_cp(descr);

	_status = GG_S(gg_text_to_status(status, s->descr ? cpdescr : NULL)); 
	if (session_int_get(s, "private"))
		_status |= GG_STATUS_FRIENDS_MASK;
	if (s->descr)
		gg_change_status_descr(g->sess, _status, cpdescr);
	else
		gg_change_status(g->sess, _status);

	xfree(cpdescr);
#if USE_UNICODE
	xfree(descr);
#endif
}

/*
 * changed_proxy()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,gg:proxy''.
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
	/* pobieranie tokenu */
	format_add("gg_token", _("%> Token was written to the file %T%1%n\n"), 1);
	format_add("gg_token_ocr", _("%> Token: %T%1%n\n"), 1);
	format_add("gg_token_body", "%1\n", 1);
	format_add("gg_token_failed", _("%! Error getting token: %1\n"), 1);
	format_add("gg_token_failed_saved", _("%! Error reading token: %1 (saved@%2)\n"), 1);
	format_add("gg_token_timeout", _("%! Token getting timeout\n"), 1);
	format_add("gg_token_unsupported", _("%! Your operating system doesn't support tokens\n"), 1);
	format_add("gg_token_missing", _("%! First get token by function %Ttoken%n\n"), 1);
	format_add("gg_user_is_connected", _("%> (%1) User %T%2%n is connected\n"), 1);
	format_add("gg_user_is_not_connected", _("%> (%1) User %T%2%n is not connected\n"), 1);
	format_add("gg_image_cant_open_file", _("%! Can't open file for image %1\n"), 1);
	format_add("gg_image_error_send", _("%! Error sending image\n"), 1);
	format_add("gg_image_ok_send", _("%> Image sent properly\n"), 1);
	format_add("gg_image_ok_get", _("%> Image saved in %1\n"), 1);
	format_add("gg_we_are_being_checked", _("%> (%1) We are being checked by %T%2%n\n"), 1);
#endif
	return 0;
}

static QUERY(gg_setvar_default) {
	xfree(gg_config_dcc_dir);
	xfree(gg_config_dcc_ip);
	xfree(gg_config_dcc_limit);
	xfree(gg_config_images_dir);

	gg_config_display_token = 1;
	gg_config_get_images = 0;
	gg_config_images_dir = NULL;
	gg_config_image_size = 20;
	gg_config_dcc = 0;
	gg_config_dcc_dir = NULL;
	gg_config_dcc_ip = NULL;
	gg_config_dcc_limit = xstrdup("30/30");
	gg_config_dcc_port = 1550;
	return 0;
}

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

int gg_plugin_init(int prio) {
	/* before loading plugin, do some sanity check */
#ifdef USE_UNICODE
	if (!config_use_unicode)
#else
		if (config_use_unicode)
#endif
		{	debug("plugin gg cannot be loaded because of mishmashed compilation...\n"
				"	program compilated with: --%s-unicode\n"
				"	 plugin compilated with: --%s-unicode\n",
				config_use_unicode ? "enable" : "disable",
				config_use_unicode ? "disable": "enable");
		return -1;
		}

	plugin_register(&gg_plugin, prio);
	gg_setvar_default(NULL, NULL);

	query_connect(&gg_plugin, TEXT("set-vars-default"), gg_setvar_default, NULL);
	query_connect(&gg_plugin, TEXT("protocol-validate-uid"), gg_validate_uid, NULL);
	query_connect(&gg_plugin, TEXT("plugin-print-version"), gg_print_version, NULL);
	query_connect(&gg_plugin, TEXT("session-added"), gg_session_handle, (void *)1);
	query_connect(&gg_plugin, TEXT("session-removed"), gg_session_handle, (void *)0);
	query_connect(&gg_plugin, TEXT("add-notify"), gg_add_notify_handle, NULL);
	query_connect(&gg_plugin, TEXT("remove-notify"), gg_remove_notify_handle, NULL);
	query_connect(&gg_plugin, TEXT("status-show"), gg_status_show_handle, NULL);
	query_connect(&gg_plugin, TEXT("user-offline"), gg_user_offline_handle, NULL);
	query_connect(&gg_plugin, TEXT("user-online"), gg_user_online_handle, NULL);
	query_connect(&gg_plugin, TEXT("protocol-unignore"), gg_user_online_handle, (void *)1);
	query_connect(&gg_plugin, TEXT("userlist-info"), gg_userlist_info_handle, NULL);

	gg_register_commands();

	variable_add(&gg_plugin, TEXT("audio"), VAR_BOOL, 1, &gg_config_audio, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, TEXT("display_token"), VAR_BOOL, 1, &gg_config_display_token, NULL, NULL, NULL);
	variable_add(&gg_plugin, TEXT("dcc"), VAR_BOOL, 1, &gg_config_dcc, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, TEXT("dcc_dir"), VAR_STR, 1, &gg_config_dcc_dir, NULL, NULL, NULL);
	variable_add(&gg_plugin, TEXT("dcc_ip"), VAR_STR, 1, &gg_config_dcc_ip, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, TEXT("dcc_limit"), VAR_STR, 1, &gg_config_dcc_limit, NULL, NULL, NULL);
	variable_add(&gg_plugin, TEXT("dcc_port"), VAR_INT, 1, &gg_config_dcc_port, gg_changed_dcc, NULL, NULL);
	variable_add(&gg_plugin, TEXT("get_images"), VAR_BOOL, 1, &gg_config_get_images, NULL, NULL, NULL);
	variable_add(&gg_plugin, TEXT("images_dir"), VAR_STR, 1, &gg_config_images_dir, NULL, NULL, NULL);
	variable_add(&gg_plugin, TEXT("image_size"), VAR_INT, 1, &gg_config_image_size, gg_changed_images, NULL, NULL);
	variable_add(&gg_plugin, TEXT("split_messages"), VAR_BOOL, 1, &gg_config_split_messages, NULL, NULL, NULL);

	plugin_var_add(&gg_plugin, "alias", VAR_STR, 0, 0, NULL);
	plugin_var_add(&gg_plugin, "auto_away", VAR_INT, "600", 0, NULL);
	plugin_var_add(&gg_plugin, "auto_back", VAR_INT, "0", 0, NULL);	
	plugin_var_add(&gg_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&gg_plugin, "auto_find", VAR_INT, "0", 0, NULL);
	plugin_var_add(&gg_plugin, "auto_reconnect", VAR_INT, "10", 0, NULL);
	plugin_var_add(&gg_plugin, "concat_multiline_status", VAR_INT, "3", 0, NULL);
	plugin_var_add(&gg_plugin, "connection_save", VAR_INT, "0", 0, NULL);
	plugin_var_add(&gg_plugin, "display_notify", VAR_INT, "-1", 0, NULL);
	plugin_var_add(&gg_plugin, "local_ip", VAR_STR, 0, 0, NULL);
	plugin_var_add(&gg_plugin, "log_formats", VAR_STR, "xml,simple", 0, NULL);
	plugin_var_add(&gg_plugin, "password", VAR_STR, "foo", 1, NULL);
	plugin_var_add(&gg_plugin, "port", VAR_INT, "8074", 0, NULL);
	plugin_var_add(&gg_plugin, "proxy", VAR_STR, 0, 0, gg_changed_proxy);
	plugin_var_add(&gg_plugin, "proxy_forwarding", VAR_STR, 0, 0, NULL);
	plugin_var_add(&gg_plugin, "private", VAR_BOOL, "0", 0, gg_changed_private);
	plugin_var_add(&gg_plugin, "scroll_long_desc", VAR_INT, "0", 0, NULL);
	plugin_var_add(&gg_plugin, "scroll_mode", VAR_STR, "bounce", 0, NULL);
	plugin_var_add(&gg_plugin, "server", VAR_STR, 0, 0, NULL);

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

	for (l = gg_userlists; l; l = l->next) {
		struct gg_http *h = l->data;

		watch_remove(&gg_plugin, h->fd, h->check);
		gg_pubdir_free(h);
	}

	xfree(gg_register_password);
	gg_register_password = NULL;
	xfree(gg_register_email);
	gg_register_email = NULL;

	for (l = sessions; l; l = l->next)
		gg_private_destroy((session_t*) l->data);

	image_flush_queue();

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
