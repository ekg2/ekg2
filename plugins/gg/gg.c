/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 * 		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include <libgadu.h>

#include <ekg/commands.h>
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
#include "pubdir50.h"

static int gg_plugin_destroy();

plugin_t gg_plugin = {
	name: "gg",
	pclass: PLUGIN_PROTOCOL,
	destroy: gg_plugin_destroy,
};

int gg_private_init(session_t *s)
{
	gg_private_t *g;
	
	if (!s)
		return -1;

	if (strncasecmp(session_uid_get(s), "gg:", 3))
		return -1;

	g = xmalloc(sizeof(gg_private_t));
	memset(g, 0, sizeof(gg_private_t));

	userlist_read(s);
	session_private_set(s, g);

	return 0;
}

int gg_private_destroy(session_t *s)
{
	gg_private_t *g;
	list_t l;
	
	if (!s)
		return -1;

	if (strncasecmp(session_uid_get(s), "gg:", 3))
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

int gg_session_handle(void *data, va_list ap)
{
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

int gg_user_offline_handle(void *data, va_list ap)
{
	userlist_t **__u = va_arg(ap, userlist_t**), *u = *__u;
	session_t **__session = va_arg(ap, session_t**), *session = *__session;
	gg_private_t *g = session_private_get(session);
	int uin = atoi(u->uid + 3);

        gg_remove_notify_ex(g->sess, uin, gg_userlist_type(u));
	ekg_group_add(u, "__offline");
        print("modify_offline", format_user(session, u->uid));
        gg_add_notify_ex(g->sess, uin, gg_userlist_type(u));

	return 0;
}

int gg_user_online_handle(void *data, va_list ap)
{
        userlist_t **__u = va_arg(ap, userlist_t**), *u = *__u;
        session_t **__session = va_arg(ap, session_t**), *session = *__session;
        gg_private_t *g = session_private_get(session);
	int quiet = (int ) data;
        int uin = atoi(u->uid + 3);

        gg_remove_notify_ex(g->sess, uin, gg_userlist_type(u));
        ekg_group_remove(u, "__offline");
        printq("modify_online", format_user(session, u->uid));
        gg_add_notify_ex(g->sess, uin, gg_userlist_type(u));

	return 0;
}

int gg_status_show_handle(void *data, va_list ap)
{
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
                char *tmp = format_string(format_find("show_status_notavail"));

                print("show_status_status", tmp, "");

                if (s->last_conn)
                        print("show_status_disconnected_since", buf);
                if ((mqc = msg_queue_count()))
                        print("show_status_msg_queue", itoa(mqc));

                print("show_status_footer");

                xfree(tmp);

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
uin_t str_to_uin(const char *text)
{
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
int gg_add_notify_handle(void *data, va_list ap)
{
	char **session_uid = va_arg(ap, char**);
	session_t *s = session_find(*session_uid);
 	char **uid = va_arg(ap, char**);
	gg_private_t *g;

	if (!s) {
		debug("Function gg_add_notify_handle() called with NULL data\n");
		return -1;
	}
        if (!(g = session_private_get(s)))
		return -1;

	gg_add_notify_ex(g->sess, str_to_uin(strchr(*uid, ':') + 1), gg_userlist_type(userlist_find(s, s->uid))); 
	return 0;
}

/*
 * trzeba usun±ææ numer u¿ytkownika z listy osób, o których
 * zmianach statusów chcemy byæ informowani
 */
int gg_remove_notify_handle(void *data, va_list ap)
{
        char **session_uid = va_arg(ap, char**);
        session_t *s = session_find(*session_uid);
        char **uid = va_arg(ap, char**);
        gg_private_t *g;

        if (!s) {
                debug("Function gg_remove_notify_handle() called with NULL data\n");
                return -1;
        }
        if (!(g = session_private_get(s)))
                return -1;

        gg_remove_notify(g->sess, str_to_uin(strchr(*uid, ':') + 1));
        return 0;
}


/*
 * gg_print_version()
 *
 * wy¶wietla wersjê pluginu i biblioteki.
 */
int gg_print_version(void *data, va_list ap)
{
	char **tmp1 = array_make(GG_DEFAULT_CLIENT_VERSION, ", ", 0, 1, 0);
	char *tmp2 = array_join(tmp1, ".");
	char *tmp3 = saprintf("libgadu %s (headers %s), protocol %s (0x%.2x)", gg_libgadu_version(), GG_LIBGADU_VERSION, tmp2, GG_DEFAULT_PROTOCOL_VERSION);

	print("generic", tmp3);

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
int gg_validate_uid(void *data, va_list ap)
{
	char **uid = va_arg(ap, char **);
	int *valid = va_arg(ap, int *);

	if (!*uid)
		return 0;

	if (!strncasecmp(*uid, "gg:", 3))
		(*valid)++;
	
	return 0;
}

/*
 * gg_ping_timer_handler()
 *
 * pinguje serwer co jaki¶ czas, je¶li jest nadal po³±czony.
 */
static void gg_ping_timer_handler(int type, void *data)
{
	session_t *s = session_find((char*) data);
	gg_private_t *g;

	if (type == 1) {
		xfree(data);
		return;
	}

	if (!s || session_connected_get(s) != 1)
		return;

	if ((g = session_private_get(s))) {
		char buf[100];

		gg_ping(g->sess);

		snprintf(buf, sizeof(buf), "ping-%s", s->uid + 3);
		timer_add(&gg_plugin, buf, 180, 0, gg_ping_timer_handler, xstrdup(s->uid));
	}
}

/*
 * gg_session_handler_success()
 *
 * obs³uga udanego po³±czenia z serwerem.
 */
static void gg_session_handler_success(session_t *s)
{
	char *__session = xstrdup(session_uid_get(s));
	gg_private_t *g = session_private_get(s);
        const char *status;
	char *descr;
	char buf[100];

	if (!g || !g->sess) {
		debug("[gg] gg_session_handler_success() called with null gg_private_t\n");
		return;
	}

	session_connected_set(s, 1);
	session_unidle(s);

	query_emit(NULL, "protocol-connected", &__session);
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

	/* pamiêtajmy, ¿eby pingowaæ */
	snprintf(buf, sizeof(buf), "ping-%s", s->uid + 3);
	timer_add(&gg_plugin, buf, 180, 0, gg_ping_timer_handler, xstrdup(s->uid));

 	descr = xstrdup(session_descr_get(s));
        status = session_status_get(s);

	gg_iso_to_cp(descr);

        /* ustawiamy swój status */
        if (s->descr)
                gg_change_status_descr(g->sess, gg_text_to_status(status, descr), descr);
        else
                gg_change_status(g->sess, gg_text_to_status(status, NULL));

}

/*
 * gg_session_handler_failure()
 *
 * obs³uga nieudanego po³±czenia.
 */
static void gg_session_handler_failure(session_t *s, struct gg_event *e)
{
	const char *reason = "conn_failed_unknown";
	gg_private_t *g = session_private_get(s);

	switch (e->event.failure) {
		case GG_FAILURE_CONNECTING:
			reason = "conn_failed_connecting";
			break;
		case GG_FAILURE_INVALID:
			reason = "conn_failed_invalid";
			break;
		case GG_FAILURE_READING:
			reason = "conn_failed_disconnected";
			break;
		case GG_FAILURE_WRITING:
			reason = "conn_failed_disconnected";
			break;
		case GG_FAILURE_PASSWORD:
			reason = "conn_failed_password";
			break;
		case GG_FAILURE_404:
			reason = "conn_failed_404";
			break;
#ifdef __GG_LIBGADU_HAVE_OPENSSL
		case GG_FAILURE_TLS:
			reason = "conn_failed_tls";
			break;
#endif
		default:
			break;
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

		query_emit(NULL, "protocol-disconnected", &__session, &__reason, &__type, NULL);

		xfree(__reason);
		xfree(__session);
	}
}

/*
 * gg_session_handler_disconnect()
 *
 * obs³uga roz³±czenia z powodu pod³±czenia drugiej sesji.
 */
static void gg_session_handler_disconnect(session_t *s)
{
	char *__session = xstrdup(session_uid_get(s));
	char *__reason = NULL;
	int __type = EKG_DISCONNECT_FORCED;
	gg_private_t *g = session_private_get(s);
	
	session_connected_set(s, 0);
		
	query_emit(NULL, "protocol-disconnected", &__session, &__reason, &__type, NULL);

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
static void gg_session_handler_status(session_t *s, uin_t uin, int status, const char *descr, uint32_t ip, uint16_t port)
{
	char *__session, *__uid, *__status, *__descr, *__host = NULL;
	int __port = 0;

	__session = xstrdup(session_uid_get(s));
	__uid = saprintf("gg:%d", uin);
	__status = xstrdup(gg_status_to_text(status));
	__descr = xstrdup(descr);
	gg_cp_to_iso(__descr);

	if (ip)
		__host = xstrdup(inet_ntoa(*((struct in_addr*)(&ip))));

	__port = port;

	query_emit(NULL, "protocol-status", &__session, &__uid, &__status, &__descr, &__host, &__port, NULL, NULL);

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
static void gg_session_handler_msg(session_t *s, struct gg_event *e)
{
	char *__session, *__sender, *__text, *__format, *__seq, **__rcpts = NULL;
	int i, __class = 0;
	time_t __sent;

	__session = xstrdup(session_uid_get(s));
	__sender = saprintf("gg:%d", e->event.msg.sender);
	__text = xstrdup(e->event.msg.message);
	gg_cp_to_iso(__text);
	__sent = e->event.msg.time;
	__seq = NULL;
	__format = NULL;
	__class = EKG_MSGCLASS_MESSAGE;

	if ((e->event.msg.msgclass & 0x0f) == GG_CLASS_CHAT || (e->event.msg.msgclass & GG_CLASS_QUEUED))
		__class = EKG_MSGCLASS_CHAT;

	/* XXX sprawdzaæ, czy dcc w³±czone */
	if ((e->event.msg.msgclass & GG_CLASS_CTCP)) {
		char *__host = NULL, uid[16];
		int __port = -1, __valid = 1;
		struct gg_dcc *d;
		userlist_t *u;
		watch_t *w;

		snprintf(uid, sizeof(uid), "gg:%d", e->event.msg.sender);

		if (!(u = userlist_find(s, uid)))
			return;

		query_emit(NULL, "protocol-dcc-validate", &__host, &__port, &__valid, NULL);
		xfree(__host);

		if (!__valid) {
			char *tmp = saprintf("/ignore %s", uid);
			print_status("dcc_attack", format_user(s, uid));
			command_exec(NULL, s, tmp, 0);
			xfree(tmp);

			return;
		}

		if (!(d = gg_dcc_get_file(u->ip, u->port, atoi(session_uid_get(s) + 3), e->event.msg.sender))) {
			print_status("dcc_error", strerror(errno));
			return;
		}

		w = watch_add(&gg_plugin, d->fd, d->check, 0, gg_dcc_handler, d);
		watch_timeout_set(w, d->timeout);

		return;
	}

	if (e->event.msg.sender == 0)
		__class = EKG_MSGCLASS_SYSTEM;

	for (i = 0; i < e->event.msg.recipients_count; i++)
		array_add(&__rcpts, saprintf("gg:%d", e->event.msg.recipients[i]));
	
	if (e->event.msg.formats && e->event.msg.formats_length) {
		unsigned char *p = e->event.msg.formats;
		int i, len = strlen(__text);
		
		__format = xmalloc(len * sizeof(uint32_t));

		for (i = 0; i < e->event.msg.formats_length; ) {
			int j, pos = p[i] + p[i + 1] * 256;
			uint32_t val = 0;
			
			if ((p[i + 3] & GG_FONT_BOLD))
				val |= EKG_FORMAT_BOLD;

			if ((p[i + 3] & GG_FONT_ITALIC))
				val |= EKG_FORMAT_ITALIC;
			
			if ((p[i + 3] & GG_FONT_UNDERLINE))
				val |= EKG_FORMAT_UNDERLINE;
			
			if ((p[i + 3] & GG_FONT_COLOR)) {
				val |= EKG_FORMAT_COLOR | p[i + 4] | (p[i + 5] << 8) | (p[i + 6] << 16);
				i += 3;
			}

			i += 3;

			for (j = pos; j < len; j++)
				__format[j] = val;
		}
	}
				
	query_emit(NULL, "protocol-message", &__session, &__sender, &__rcpts, &__text, &__format, &__sent, &__class, &__seq, NULL);

	xfree(__seq);
	xfree(__text);
	xfree(__sender);
	xfree(__session);
	xfree(__format);
}

/*
 * gg_session_handler_ack()
 *
 * obs³uga potwierdzeñ wiadomo¶ci.
 */
static void gg_session_handler_ack(session_t *s, struct gg_event *e)
{
	char *__session, *__rcpt, *__seq, *__status;

	__session = xstrdup(session_uid_get(s));
	__rcpt = saprintf("gg:%d", e->event.ack.recipient);
	__seq = strdup(itoa(e->event.ack.seq));

	switch (e->event.ack.status) {
		case GG_ACK_DELIVERED:
			__status = xstrdup(EKG_ACK_DELIVERED);
			break;
		case GG_ACK_QUEUED:
			__status = xstrdup(EKG_ACK_QUEUED);
			break;
		case GG_ACK_NOT_DELIVERED:
			__status = xstrdup(EKG_ACK_DROPPED);
			break;
		default:
			debug("[gg] unknown message ack status. consider upgrade\n");
			__status = xstrdup(EKG_ACK_UNKNOWN);
			break;
	}

	query_emit(NULL, "protocol-message-ack", &__session, &__rcpt, &__seq, &__status, NULL);
	
	xfree(__status);
	xfree(__seq);
	xfree(__rcpt);
	xfree(__session);
}

/*
 * gg_reconnect_handler()
 *
 * obs³uga powtórnego po³±czenia
 */

void gg_reconnect_handler(int type, void *data)
{
	session_t* s = session_find((char*) data);

	if (!s || session_connected_get(s) == 1)
		return;

	command_exec(NULL, s, xstrdup("/connect"), 0);
}

/*
 * gg_session_handler_userlist()
 *
 * support for userlist's events 
 *
 */
static void gg_session_handler_userlist(session_t *s, struct gg_event *e)
{
        switch (e->event.userlist.type) {
                case GG_USERLIST_GET_REPLY:
                {
	                print("userlist_get_ok");

                        if (e->event.userlist.reply) {
				list_t l;
				gg_private_t *g = session_private_get(s);

                                /* remove all contacts from notification list on server */
				for (l = s->userlist; l; l = l->next) {
                                        userlist_t *u = l->data;
                                        gg_remove_notify_ex(g->sess, str_to_uin(xstrchr(u->uid, ':') + 1), gg_userlist_type(u));
                                }

                                gg_cp_to_iso(e->event.userlist.reply);
				userlist_set(s, e->event.userlist.reply);
		                gg_userlist_send(g->sess, s->userlist);

                                config_changed = 1;
                        }

                        break;
                }

                case GG_USERLIST_PUT_REPLY:
                {
                        switch (gg_userlist_put_config) {
                                case 0:
                                        print("userlist_put_ok");
                                        break;
                                case 1:
                                        print("userlist_config_put_ok");
                                        break;
                                case 2:
                                        print("userlist_clear_ok");
                                        break;
                                case 3:
                                        print("userlist_config_clear_ok");
                                        break;
                        }
                        break;
                }
        }
}

/*
 * gg_session_handler()
 *
 * obs³uga zdarzeñ przy po³±czeniu gg.
 */
static void gg_session_handler(int type, int fd, int watch, void *data)
{
	gg_private_t *g = session_private_get((session_t*) data);
	struct gg_event *e;
	int broken = 0;

	if (type == 1) {
		/* tutaj powinni¶my usun±æ dane watcha. nie, dziêkujê. */
		return;
	}

	if (!g || !g->sess) {
		debug("[gg] gg_session_handler() called with NULL gg_session\n");
		return;
	}

	if (type == 2) {
		if (g->sess->state != GG_STATE_CONNECTING_GG) {
			session_t *s = (session_t*) data;
			print("conn_timeout", session_name(s));
			gg_free_session(g->sess);
			g->sess = NULL;
			return;
		}

		/* je¶li jest GG_STATE_CONNECTING_GG to ka¿emy stwierdziæ
		 * b³±d (EINPROGRESS) i ³±czyæ siê z kolejnym kandydatem. */
	}

	if (!(e = gg_watch_fd(g->sess))) {
		char *__session = xstrdup(session_uid_get((session_t*) data));
		char *__reason = NULL;
		int __type = EKG_DISCONNECT_NETWORK;
		int reconnect_delay;

		session_connected_set((session_t*) data, 0);
		
		query_emit(NULL, "protocol-disconnected", &__session, &__reason, &__type, NULL);
			
		xfree(__reason);
		xfree(__session);

		gg_free_session(g->sess);
		g->sess = NULL;
		
		reconnect_delay = session_int_get((session_t*) data, "auto_reconnect");
		if (reconnect_delay && reconnect_delay != -1)
			timer_add(&gg_plugin, "reconnect", reconnect_delay, 0, gg_reconnect_handler, xstrdup(((session_t*)data)->uid));

		return;
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

				gg_session_handler_status(data, n->uin, n->status, descr, n->remote_ip, n->remote_port);
			}
			
			break;
		}
			
		case GG_EVENT_STATUS:
			gg_session_handler_status(data, e->event.status.uin, e->event.status.status, e->event.status.descr, 0, 0);
			break;

#ifdef GG_STATUS60
		case GG_EVENT_STATUS60:
			gg_session_handler_status(data, e->event.status60.uin, e->event.status60.status, e->event.status60.descr, e->event.status60.remote_ip, e->event.status60.remote_port);
			break;
#endif

#ifdef GG_NOTIFY_REPLY60
		case GG_EVENT_NOTIFY60:
		{
			int i;

			for (i = 0; e->event.notify60[i].uin; i++)
				gg_session_handler_status(data, e->event.notify60[i].uin, e->event.notify60[i].status, e->event.notify60[i].descr, e->event.notify60[i].remote_ip, e->event.notify60[i].remote_port);

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

		default:
			debug("[gg] unhandled event 0x%.4x, consider upgrade\n", e->type);
	}

	if (!broken && g->sess->state != GG_STATE_IDLE && g->sess->state != GG_STATE_ERROR) {
		watch_t *w = watch_add(&gg_plugin, g->sess->fd, g->sess->check, 0, gg_session_handler, data);
		watch_timeout_set(w, g->sess->timeout);
	}

	gg_event_free(e);
}

COMMAND(gg_command_connect)
{
	gg_private_t *g = session_private_get(session);
	uin_t uin = (session) ? atoi(session->uid + 3) : 0;
	const char *password = session_get(session, "password");
	
	if (!session_check(session, 0, "gg") || !g) {
		printq("invalid_session");
		return -1;
	}

	if (!strcasecmp(name, "disconnect") || (!strcasecmp(name, "reconnect") && session_connected_get(session))) {
		if (!g->sess) {
			printq("not_connected", session_name(session));
		} else {
			char *__session = xstrdup(session->uid);
			char *__reason = xstrdup(params[0]);
			int __type = EKG_DISCONNECT_USER;

			if (__reason) {
                		char *tmp = xstrdup(__reason);

	                        if (tmp && !strcmp(tmp, "-")) {
        	                        xfree(tmp);
                	                tmp = NULL;
                        	}
				else 
	                		gg_iso_to_cp(tmp);

          			if (config_keep_reason)
					session_descr_set(session, __reason);
				
				gg_change_status_descr(g->sess, GG_STATUS_NOT_AVAIL_DESCR, tmp);
                		xfree(tmp);
        		} else
  			        gg_change_status(g->sess, GG_STATUS_NOT_AVAIL);

			watch_remove(&gg_plugin, g->sess->fd, g->sess->check);
			
			gg_logoff(g->sess);
			gg_free_session(g->sess);
			g->sess = NULL;
			session_connected_set(session, 0);

			query_emit(NULL, "protocol-disconnected", &__session, &__reason, &__type, NULL);

			xfree(__reason);
			xfree(__session);
		}
	}

	if (!strcasecmp(name, "connect") || !strcasecmp(name, "reconnect")) {
		struct gg_login_params p;
		const char *tmp, *local_ip = session_get(session, "local_ip");
		int tmpi;

		if (g->sess) {
			printq((g->sess->state == GG_STATE_CONNECTED) ? "already_connected" : "during_connect", session_name(session));
			return -1;
		}

	        if (local_ip == NULL)
			gg_local_ip = htonl(INADDR_ANY);
   	        else {
#ifdef HAVE_INET_PTON
	                int tmp = inet_pton(AF_INET, local_ip, &gg_local_ip);
	
			if (tmp == 0 || tmp == -1) {
	               		print("invalid_local_ip", session_name(session));
                        	session_set(session, "local_ip", NULL);
				config_changed = 1;
                        	gg_local_ip = htonl(INADDR_ANY);
                	}
			debug("achoj\n");
#else
                 	gg_local_ip = inet_addr(local_ip);
#endif
        	}


		if (!uin || !password) {
			printq("no_config");
			return -1;
		}

		printq("connecting", session_name(session));

		memset(&p, 0, sizeof(p));

		if (!strcmp(session_status_get(session), EKG_STATUS_NA))
			session_status_set(session, EKG_STATUS_AVAIL);

		if (gg_dcc_socket) {
			gg_dcc_ip = inet_addr("255.255.255.255");
			gg_dcc_port = gg_dcc_socket->port;
		}
			
		p.uin = uin;
		p.password = (char*) password;
		p.status = gg_text_to_status(session_status_get(session), session_descr_get(session));
		p.status_descr = (char*) session_descr_get(session);
		p.async = 1;

		if ((tmpi = session_int_get(session, "protocol")) != -1)
			p.protocol_version = tmpi;

		if ((tmpi = session_int_get(session, "last_sysmsg")) != -1)
			p.last_sysmsg = tmpi;

		const char *realserver = session_get(session, "server");
		if (realserver) {
			in_addr_t tmp_in;
			
			if ((tmp_in = inet_addr(realserver)) != INADDR_NONE)
			    p.server_addr = inet_addr(realserver);
			else {
			    print("inet_addr_failed", session_name(session));
			    return -1;
			}
		}

		int port = session_int_get(session, "port");
		if ((port < 1) || (port > 65535)) {
			print("port_number_error", session_name(session));
			return -1;
		}
		p.server_port = port;

		gg_proxy_port = 0;
		xfree(gg_proxy_host);
		gg_proxy_host = NULL;
		xfree(gg_proxy_username);
		gg_proxy_username = NULL;
		xfree(gg_proxy_password);
		gg_proxy_password = NULL;
		gg_proxy_enabled = 0;	

		if ((tmp = session_get(session, "proxy"))) {
			char **auth, **userpass = NULL, **hostport = NULL;
	
			auth = array_make(tmp, "@", 0, 0, 0);
		
			if (!auth[0] || !strcmp(auth[0], "")) {
				array_free(auth);
				goto noproxy;
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
noproxy:

		if ((tmp = session_get(session, "proxy_forwarding"))) {
			char *fwd = xstrdup(tmp), *colon = strchr(fwd, ':');

			if (!colon) {
				p.external_addr = inet_addr(fwd);
				p.external_port = 1550; /* XXX */
			} else {
				*colon = 0;
				p.external_addr = inet_addr(fwd);
				p.external_port = atoi(colon + 1);
			}

			xfree(fwd);
		}

		g->sess = gg_login(&p);

		if (!g->sess)	
			printq("conn_failed", format_find((errno == ENOMEM) ? "conn_failed_memory" : "conn_failed_connecting"), session_name(session));
		else {
			watch_t *w = watch_add(&gg_plugin, g->sess->fd, g->sess->check, 0, gg_session_handler, session);
			watch_timeout_set(w, g->sess->timeout);
		}
	}

	return 0;
}

COMMAND(gg_command_away)
{
	gg_private_t *g = session_private_get(session);
	char *descr, *f, *fd, *df;
	const char *status;

	if (!session_check(session, 1, "gg")) {
		printq("invalid_session");
		return -1;
	}

	if (!strcasecmp(name, "away")) {
		session_status_set(session, EKG_STATUS_AWAY);
		df = "away"; f = "away"; fd = "away_descr";
		session_unidle(session);
		goto change;
	}

	if (!strcasecmp(name, "_autoaway")) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		df = "away"; f = "auto_away"; fd = "auto_away_descr";
		goto change;
	}

	if (!strcasecmp(name, "back")) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = "back"; f = "back"; fd = "back_descr";
		session_unidle(session);
		goto change;
	}

	if (!strcasecmp(name, "_autoback")) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = "back"; f = "auto_back"; fd = "auto_back_descr";
		session_unidle(session);
		goto change;
	}

	if (!strcasecmp(name, "invisible")) {
		session_status_set(session, EKG_STATUS_INVISIBLE);
		df = "quit"; f = "invisible"; fd = "invisible_descr";
		session_unidle(session);
		goto change;
	}

	return -1;

change:
        reason_changed = 1;

	if (params[0]) {
		if (strlen(params[0]) > GG_STATUS_DESCR_MAXSIZE && config_reason_limit) {
			printq("descr_too_long", itoa(strlen(params[0]) - GG_STATUS_DESCR_MAXSIZE));
			return -1;
		}

		session_descr_set(session, (!strcmp(params[0], "-")) ? NULL : params[0]);
	} else {
		char *tmp;

		if ((tmp = ekg_draw_descr(df))) {
			session_descr_set(session, tmp);
			xfree(tmp);
		}
	}

	descr = xstrdup(session_descr_get(session));
	status = session_status_get(session);

	if (descr)
		printq(fd, descr, "", session_name(session));
	else
		printq(f, session_name(session));
	
	if (!g->sess || !session_connected_get(session)) {
		xfree(descr);
		return 0;
	}

	ekg_update_status(session);

	gg_iso_to_cp(descr);

	if (descr)
		gg_change_status_descr(g->sess, gg_text_to_status(status, descr), descr);
	else
		gg_change_status(g->sess, gg_text_to_status(status, NULL));

	xfree(descr);

	return 0;
}
	
COMMAND(gg_command_msg)
{
	int count, valid = 0, chat, secure = 0, formatlen = 0;
	char **nicks = NULL, *nick = NULL, **p = NULL, *add_send = NULL;
	unsigned char *msg = NULL, *raw_msg = NULL, *format = NULL;
	uint32_t *ekg_format = NULL;
	userlist_t *u;
	gg_private_t *g = session_private_get(session);

	chat = (strcasecmp(name, "msg"));

        if (!session_check(session, 1, "gg")) {
                printq("invalid_session");
                return -1;
        }
	
	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		return -1;
	}
	
	session_unidle(session);

//	if (!strcmp(params[0], "*")) {
//		msg_all_wrapper(chat, params[1], quiet);
//		return 0;
//	}

	nick = xstrdup(params[0]);

	if ((*nick == '@' || strchr(nick, ',')) && chat) {
		struct conference *c = conference_create(session, nick);
		list_t l;

		if (c) {
			xfree(nick);
			nick = xstrdup(c->name);
			
			for (l = c->recipients; l; l = l->next) 
				array_add(&nicks, xstrdup((char *) (l->data)));
			
			add_send = xstrdup(c->name);
		}
	} else if (*nick == '#') {
		struct conference *c = conference_find(nick);
		list_t l;

		if (!c) {
			printq("conferences_noexist", nick);
			xfree(nick);
			return -1;
		}

		for (l = c->recipients; l; l = l->next)
			array_add(&nicks, xstrdup((char *) (l->data)));
		
		add_send = xstrdup(c->name);
	} else {
		char **tmp = array_make(nick, ",", 0, 0, 0);
		int i;

		for (i = 0; tmp[i]; i++) {
			int count = 0;
			list_t l;

			if (tmp[i][0] != '@') {
				if (!array_contains(nicks, tmp[i], 0))
					array_add(&nicks, xstrdup(tmp[i]));
				continue;
			}

			for (l = session->userlist; l; l = l->next) {
				userlist_t *u = l->data;			
				list_t m;

				for (m = u->groups; m; m = m->next) {
					struct group *g = m->data;

					if (!strcasecmp(g->name, tmp[i] + 1)) {
						if (u->nickname && !array_contains(nicks, u->nickname, 0))
							array_add(&nicks, xstrdup(u->nickname));
						count++;
					}
				}
			}

			if (!count)
				printq("group_empty", tmp[i] + 1);
		}

		array_free(tmp);
	}

	if (!nicks) {
		xfree(nick);
		return 0;
	}

	if (strlen(params[1]) > 1989)
		printq("message_too_long");

	msg = xstrmid(params[1], 0, 1989);
	ekg_format = ekg_sent_message_format(msg);

	/* analizê tekstu zrobimy w osobnym bloku dla porz±dku */
	{
		unsigned char attr = 0, last_attr = 0;
		const unsigned char *p = msg, *end = p + strlen(p);
		int msglen = 0;
		unsigned char rgb[3], last_rgb[3];

		for (p = msg; p < end; ) {
			if (*p == 18 || *p == 3) {	/* Ctrl-R, Ctrl-C */
				p++;

				if (xisdigit(*p)) {
					int num = atoi(p);
					
					if (num < 0 || num > 15)
						num = 0;

					p++;

					if (xisdigit(*p))
						p++;

					rgb[0] = default_color_map[num].r;
					rgb[1] = default_color_map[num].g;
					rgb[2] = default_color_map[num].b;

					attr |= GG_FONT_COLOR;
				} else
					attr &= ~GG_FONT_COLOR;

				continue;
			}

			if (*p == 2) {		/* Ctrl-B */
				attr ^= GG_FONT_BOLD;
				p++;
				continue;
			}

			if (*p == 20) {		/* Ctrl-T */
				attr ^= GG_FONT_ITALIC;
				p++;
				continue;
			}

			if (*p == 31) {		/* Ctrl-_ */
				attr ^= GG_FONT_UNDERLINE;
				p++;
				continue;
			}

			if (*p < 32 && *p != 13 && *p != 10 && *p != 9) {
				p++;
				continue;
			}

			if (attr != last_attr || ((attr & GG_FONT_COLOR) && memcmp(last_rgb, rgb, sizeof(rgb)))) {
				int color = 0;

				memcpy(last_rgb, rgb, sizeof(rgb));

				if (!format) {
					format = xmalloc(3);
					format[0] = 2;
					formatlen = 3;
				}

				if ((attr & GG_FONT_COLOR))
					color = 1;

				if ((last_attr & GG_FONT_COLOR) && !(attr & GG_FONT_COLOR)) {
					color = 1;
					memset(rgb, 0, 3);
				}

				format = xrealloc(format, formatlen + ((color) ? 6 : 3));
				format[formatlen] = (msglen & 255);
				format[formatlen + 1] = ((msglen >> 8) & 255);
				format[formatlen + 2] = attr | ((color) ? GG_FONT_COLOR : 0);

				if (color) {
					memcpy(format + formatlen + 3, rgb, 3);
					formatlen += 6;
				} else
					formatlen += 3;

				last_attr = attr;
			}

			msg[msglen++] = *p;
			
			p++;
		}

		msg[msglen] = 0;

		if (format && formatlen) {
			format[1] = (formatlen - 3) & 255;
			format[2] = ((formatlen - 3) >> 8) & 255;
		}
	}

	raw_msg = xstrdup(msg);
	gg_iso_to_cp(msg);

	count = array_count(nicks);

	for (p = nicks; *p; p++) {
		const char *uid;

		if (!strcmp(*p, ""))
			continue;

		if (!(uid = get_uid(session, *p))) {
			printq("user_not_found", *p);
			continue;
		}
		
	        u = userlist_find(session, uid);

//		put_log(uin, "%s,%s,%s,%s,%s\n", ((chat) ? "chatsend" : "msgsend"), uid, ((u && u->nickname) ? u->nickname : ""), log_timestamp(time(NULL)), raw_msg);

		if (config_last & 4) 
			last_add(1, uid, time(NULL), 0, msg);

		if (!chat || count == 1) {
			unsigned char *__msg = xstrdup(msg);
			char *sid = xstrdup(uid);
			uin_t uin = atoi(uid + 3);
			const char *seq;

			secure = 0;
			
			query_emit(NULL, "message-encrypt", &sid, &__msg, &secure);

			xfree(sid);

			if (g->sess)
				seq = itoa(gg_send_message_richtext(g->sess, (chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, uin, __msg, format, formatlen));
			else
				seq = "offline";

			msg_queue_add(session_uid_get(session), params[0], params[1], seq);
			valid++;
			xfree(__msg);
		}
	}

	if (count > 1 && chat) {
		uin_t *uins = xmalloc(count * sizeof(uin_t));
		int realcount = 0;
		const char *seq;

		for (p = nicks; *p; p++) {
			const char *uid;
			
			if (!(uid = get_uid(session, *p)))
				continue;

			if (strncmp(uid, "gg:", 3))
				continue;
			
			uins[realcount++] = atoi(uid + 3);
		}

		if (g->sess) 
			seq = itoa(gg_send_message_confer_richtext(g->sess, GG_CLASS_CHAT, realcount, uins, msg, format, formatlen));
		else
			seq = "offline";

		msg_queue_add(session_uid_get(session), params[0], params[1], seq);
		valid++;

		xfree(uins);
	}

	if (!add_send)
		add_send = xstrdup(nick);

	if (valid)
		tabnick_add(add_send);

	xfree(add_send);

	if (valid && (!g->sess || g->sess->state != GG_STATE_CONNECTED))
		printq("not_connected_msg_queued", session_name(session));

	if (valid && config_display_sent && !quiet) {
		const char *rcpts[2] = { nick, NULL };
		message_print(session_uid_get(session), session_uid_get(session), rcpts, raw_msg, ekg_format, time(NULL), (chat) ? EKG_MSGCLASS_SENT_CHAT : EKG_MSGCLASS_SENT, NULL);
	}

	xfree(msg);
	xfree(raw_msg);
	xfree(format);
	xfree(nick);
	xfree(ekg_format);

	array_free(nicks);

	unidle();

	return 0;
}

COMMAND(gg_command_inline_msg)
{
	const char *p[2] = { target, params[0] };

	if(p[1]) 
		return gg_command_msg("chat", p, session, target, quiet);
	else
		return 0;
}

COMMAND(gg_command_block)
{
	const char *uid;

	if (!params[0]) {
		list_t l;
		int i = 0;

		for (l = session->userlist; l; l = l->next) {
			userlist_t *u = l->data;
				
			if (!ekg_group_member(u, "__blocked"))
				continue;

			i = 1;

			printq("blocked_list", format_user(session, u->uid));
		}

		if (!i) 
			printq("blocked_list_empty");

		return 0;
	}

	if (!(uid = get_uid(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}
		
	if (gg_blocked_add(session, uid) != -1) {
		printq("blocked_added", format_user(session, uid));
		config_changed = 1;
	} else {
		printq("blocked_exist", format_user(session, uid));
		return -1;
	}

	return 0;
}

COMMAND(gg_command_unblock)
{
	int unblock_all = (params[0] && !strcmp(params[0], "*"));
	const char *uid;

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (unblock_all) {
		list_t l;
		int x = 0;

		for (l = session->userlist; l; ) {
			userlist_t *u = l->data;
			
			l = l->next;
	
			if (gg_blocked_remove(session, u->uid) != -1)
				x = 1;
		}

		if (x) {
			printq("blocked_deleted_all");
			config_changed = 1;
		} else {
			printq("blocked_list_empty");
			return -1;
		}

		return 0;
	}

	if (!(uid = get_uid(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}
		
	if (gg_blocked_remove(session, uid) != -1) {
		printq("blocked_deleted", format_user(session, uid));
		config_changed = 1;
	} else {
		printq("error_not_blocked", format_user(session, uid));
		return -1;
	}

	return 0;
}

int gg_plugin_init()
{
	list_t l;

	plugin_register(&gg_plugin);

	query_connect(&gg_plugin, "protocol-validate-uid", gg_validate_uid, NULL);
	query_connect(&gg_plugin, "plugin-print-version", gg_print_version, NULL);
	query_connect(&gg_plugin, "session-added", gg_session_handle, (void *)1);
	query_connect(&gg_plugin, "session-removed", gg_session_handle, (void *)0);
	query_connect(&gg_plugin, "add-notify", gg_add_notify_handle, NULL);
	query_connect(&gg_plugin, "remove-notify", gg_remove_notify_handle, NULL);
	query_connect(&gg_plugin, "status-show", gg_status_show_handle, NULL);
	query_connect(&gg_plugin, "user-offline", gg_user_offline_handle, NULL);
	query_connect(&gg_plugin, "user-online", gg_user_online_handle, NULL);
        query_connect(&gg_plugin, "protocol-unignore", gg_user_online_handle, (void *)1);

#define possibilities(x) array_make(x, " ", 0, 1, 1)
#define params(x) array_make(x, " ", 0, 1, 1)

	command_add(&gg_plugin, "gg:connect", params("?"), gg_command_connect, 0, "", "³±czy siê z serwerem", "", NULL);
	command_add(&gg_plugin, "gg:disconnect", params("?"), gg_command_connect, 0, " [powód/-]", "roz³±cza siê od serwera", "", NULL);
	command_add(&gg_plugin, "gg:reconnect", params(""), gg_command_connect, 0, "", "roz³±cza i ³±czy siê ponownie", "", NULL);
	command_add(&gg_plugin, "gg:msg", params("uUC ?"), gg_command_msg, 0, 
	   " <numer/alias/@grupa> <wiadomo¶æ>", "wysy³a wiadomo¶æ", 
	   "\nMo¿na podaæ wiêksz± ilo¶æ odbiorców oddzielaj±c ich numery lub pseudonimy przecinkiem (ale bez odstêpów). Je¶li zamiast odbiorcy podany zostanie znak ,,%T*%n'', to wiadomo¶æ bêdzie wys³ana do wszystkich aktualnych rozmówców.",
	   NULL);
	command_add(&gg_plugin, "gg:chat", params("uUC ?"), gg_command_msg, 0, " <numer/alias/@grupa> <wiadomo¶æ>", "wysy³a wiadomo¶æ", "\nPolecenie jest podobne do %Tmsg%n, ale wysy³a wiadomo¶æ w ramach rozmowy, a nie jako pojedyncz±.", NULL);
	command_add(&gg_plugin, "gg:", params("?"), gg_command_inline_msg, 0, "", "", "", NULL);
	command_add(&gg_plugin, "gg:_descr", params("r"), gg_command_away, 0, " [opis/-]", "zmienia opis stanu", "", NULL);
	command_add(&gg_plugin, "gg:away", params("r"), gg_command_away, 0, " [opis/-]", "zmienia stan na zajêty", "", NULL);
	command_add(&gg_plugin, "gg:_autoaway", params("?"), gg_command_away, 0, "", "automatycznie zmienia stan na zajêty", "", NULL);
	command_add(&gg_plugin, "gg:back", params("r"), gg_command_away, 0, " [opis/-]", "zmienia stan na dostêpny", "", NULL);
	command_add(&gg_plugin, "gg:_autoback", params("?"), gg_command_away, 0, "", "automatycznie zmienia stan na dostêpny", "", NULL);
	command_add(&gg_plugin, "gg:invisible", params("r"), gg_command_away, 0, " [opis/-]", "zmienia stan na niewidoczny", "", NULL);

	command_add(&gg_plugin, "gg:block", params("uUC ?"), gg_command_block, 0, " [numer/alias]", "dodaje do listy blokowanych", "", NULL);
	command_add(&gg_plugin, "gg:unblock", params("b ?"), gg_command_unblock, 0, " <numer/alias>|*", "usuwa z listy blokowanych", "", NULL);

	command_add(&gg_plugin, "gg:remind", params("?"), gg_command_remind, 0, " [numer]", "wysy³a has³o na skrzynkê pocztow±", "", NULL);
	command_add(&gg_plugin, "gg:register", params("? ?"), gg_command_register, 0, " <email> <has³o>", "rejestruje nowe konto", "", NULL);
	command_add(&gg_plugin, "gg:unregister", params("? ?"), gg_command_unregister, 0, " <uin/alias> <has³o>", "usuwa konto z serwera", "\nPodanie numeru i has³a jest niezbêdne ze wzglêdów bezpieczeñstwa. Nikt nie chcia³by chyba usun±æ konta przypadkowo, bez ¿adnego potwierdzenia.", NULL);
	command_add(&gg_plugin, "gg:passwd", params("? ?"), gg_command_passwd, 0, " <has³o>", "zmienia has³o u¿ytkownika", "\nZmiana has³a nie wymaga ju¿ ustawienia zmiennej %Temail%n.", NULL);
	command_add(&gg_plugin, "gg:userlist", params("p ?"), gg_command_list, 0, " [opcje]", "lista kontaktów na serwerze",
	  "\n"
	  "Lista kontaktów na serwerze \"list [-p|-g|-c]\":\n"
	  "  -c, --clear  usuwa listê z serwera\n"
	  "  -g, --get    pobiera listê z serwera\n"
	  "  -p, --put    umieszcza listê na serwerze", 
	  possibilities("-c --clear -g --get -p --put") );

	command_add(&gg_plugin, "gg:find", params("puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC"), gg_command_find, 0,
	  " [numer|opcje]", "przeszukiwanie katalogu publicznego",
	  "\n"
	  "  -u, --uin <numerek>\n"
	  "  -f, --first <imiê>\n"
	  "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>\n"
	  "  -c, --city <miasto>\n"
	  "  -b, --born <min:max>    zakres roku urodzenia\n"
	  "  -a, --active            tylko dostêpni\n"
	  "  -F, --female            kobiety\n"
	  "  -M, --male              mê¿czy¼ni\n"
	  "  -s, --start <n>         wy¶wietla od n-tego numeru\n"
	  "  -A, --all               wy¶wietla wszystkich\n"
	  "  -S, --stop              zatrzymuje wszystkie poszukiwania", 
	  possibilities("-u --uin -f --first -l --last -n --nick -c --city -b --botn -a --active -F --female -M --male -s --start -A --all -S --stop") );
	
	command_add(&gg_plugin, "gg:change", params("p"), gg_command_change, 0,
	  " <opcje>", "zmienia informacje w katalogu publicznym",
	  "\n"
	  "  -f, --first <imiê>\n"
          "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>\n"
	  "  -b, --born <rok urodzenia>\n"
	  "  -c, --city <miasto>\n"
	  "  -N, --familyname <nazwisko>  nazwisko panieñskie\n"
	  "  -C, --familycity <miasto>    miasto rodzinne\n"
	  "  -F, --female                 kobieta\n"
	  "  -M, --male                   mê¿czyzna\n"
	  "\n"
	  "Je¶li który¶ z parametrów nie zostanie podany, jego warto¶æ "
	  "zostanie wyczyszczona w katalogu publicznym. Podanie parametru "
	  ",,%T-%n'' wyczy¶ci %Twszystkie%n pola.", 
	  possibilities("-f --first -l --last -n --nick -b --born -c --city -N --familyname -C --familycity -F --female -M --male") );

	command_add(&gg_plugin, "gg:dcc", params("p uU f ?"), gg_command_dcc, 0, " [opcje]", "obs³uga bezpo¶rednich po³±czeñ", "", possibilities("send rsend get resume rvoice voice close list") );

#undef possibilities
#undef params 

	plugin_var_add(&gg_plugin, "alias", VAR_STR, 0, 0);
	plugin_var_add(&gg_plugin, "auto_away", VAR_INT, "0", 0);
	plugin_var_add(&gg_plugin, "auto_back", VAR_INT, "0", 0);	
        plugin_var_add(&gg_plugin, "auto_connect", VAR_INT, "0", 0);
        plugin_var_add(&gg_plugin, "auto_find", VAR_INT, "0", 0);
        plugin_var_add(&gg_plugin, "auto_reconnect", VAR_INT, "0", 0);
        plugin_var_add(&gg_plugin, "connection_save", VAR_INT, "0", 0);
        plugin_var_add(&gg_plugin, "display_notify", VAR_INT, "0", 0);
        plugin_var_add(&gg_plugin, "local_ip", VAR_STR, 0, 0);
	plugin_var_add(&gg_plugin, "log_formats", VAR_STR, "xml,simple", 0);
        plugin_var_add(&gg_plugin, "password", VAR_STR, "foo", 1);
        plugin_var_add(&gg_plugin, "port", VAR_INT, "8074", 0);
	plugin_var_add(&gg_plugin, "server", VAR_STR, 0, 0);

	gg_debug_handler = ekg_debug_handler;
	gg_debug_level = 255;

	gg_dcc_socket_open(); /* XXX */

	for (l = sessions; l; l = l->next)
		gg_private_init((session_t*) l->data);
	
	return 0;
}

static int gg_plugin_destroy()
{
	list_t l;

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

	plugin_unregister(&gg_plugin);

	return 0;
}

/*
 * userlist_read()
 *
 * wczytuje listê kontaktów z pliku ~/.ekg/gg:NUMER-userlist w postaci eksportu
 * tekstowego listy kontaktów windzianego klienta.
 *
 * 0/-1
 */
int userlist_read(session_t *session)
{
        const char *filename;
        char *buf;
        FILE *f;
        char *tmp=saprintf("%s-userlist", session->uid);

        if (!(filename = prepare_path(tmp, 0))) {
                xfree(tmp);
                return -1;
        }       
        xfree(tmp);
        
        if (!(f = fopen(filename, "r")))
                return -1;
                        
        while ((buf = read_file(f))) {
                userlist_t u;

                memset(&u, 0, sizeof(u));
                        
                if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/')) {
                        xfree(buf);
                        continue;
                }
                
                userlist_add_entry(session,buf);
        
                xfree(buf);
        }

        fclose(f);
                
        return 0;
} 

 
