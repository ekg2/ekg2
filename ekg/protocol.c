/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
 *		  2004 Adam Mikuta <adammikuta@poczta.onet.pl>
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
#include "win32.h"

#include <stdio.h>
#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <string.h>

#include "debug.h"
#include "dynstuff.h"
#include "xmalloc.h"

#include "commands.h"
#include "emoticons.h"
#include "objects.h"
#include "userlist.h"
#include "windows.h"

#include "log.h"
#include "msgqueue.h"
#include "protocol.h"
#include "stuff.h"
#include "themes.h"

static int auto_find_limit = 100; /* counter of persons who we were looking for when autofind */
list_t dccs = NULL;

static QUERY(protocol_disconnected);
static QUERY(protocol_connected);
static QUERY(protocol_message_ack);
static QUERY(protocol_status);
static QUERY(protocol_message);
static QUERY(protocol_xstate);

/*
 * protocol_init()
 *
 * rejestruje wszystkie zapytania zwi±zane z protoko³em, które bêd±
 * wysy³ane przez pluginy.
 */
void protocol_init()
{
	query_connect(NULL, ("protocol-status"), protocol_status, NULL);
	query_connect(NULL, ("protocol-message"), protocol_message, NULL);
	query_connect(NULL, ("protocol-message-ack"), protocol_message_ack, NULL);
	query_connect(NULL, ("protocol-xstate"), protocol_xstate, NULL);

	query_connect(NULL, ("protocol-connected"), protocol_connected, NULL);
	query_connect(NULL, ("protocol-disconnected"), protocol_disconnected, NULL);
}


/*
 * protocol_reconnect_handler()
 *
 * obs³uga timera reconnectu.
 */
static TIMER(protocol_reconnect_handler) {	/* temporary */
	char *session = (char*) data;
	session_t *s;

	if (type == 1) {
		xfree(session);
		return 0;
	}

	s = session_find(session);

        if (!s || session_connected_get(s) == 1)
                return -1;

	debug("reconnecting session %s\n", session);

	command_exec(NULL, s, ("/connect"), 0);
	return -1;
}

/*
 * protocol_disconnect()
 *
 * obs³uga zerwanego po³±czenia.
 */
static QUERY(protocol_disconnected)
{
	char *session	= *(va_arg(ap, char **));
	char *reason	= *(va_arg(ap, char **));
	int type	= *(va_arg(ap, int*));

	session_t *s	= session_find(session);

	userlist_clear_status(s, NULL);

	switch (type) {
		case EKG_DISCONNECT_NETWORK:
		case EKG_DISCONNECT_FAILURE:
		{
			int tmp;

			if (type == EKG_DISCONNECT_NETWORK)
				print("conn_broken", session_name(s));
			else
				print("conn_failed", reason, session_name(s));

			if (s && (tmp = session_int_get(s, "auto_reconnect")) && tmp != -1)
				timer_add(plugin_find_uid(s->uid), "reconnect", tmp, 0, protocol_reconnect_handler, xstrdup(session));

			break;
		}

		case EKG_DISCONNECT_USER:
			if (reason)
				print("disconnected_descr", reason, session_name(s));
			else
				print("disconnected", session_name(s));
			break;

		case EKG_DISCONNECT_FORCED:
			print("conn_disconnected", session_name(s));
			break;
			
		case EKG_DISCONNECT_STOPPED:
			print("conn_stopped", session_name(s));
			break;

		default:
			print("generic_error", "protocol_disconnect internal error, report to authors");
			break;
	}

	return 0;
}

/*
 * protocol_connect()
 *
 * obs³uga udanego po³±czenia.
 */
static QUERY(protocol_connected)
{
	char **session = va_arg(ap, char**);
	session_t *s = session_find(*session);
	const char *descr = session_descr_get(s);
	
        ekg_update_status(s);

	if (descr)
		print("connected_descr", descr, session_name(s));
	else
		wcs_print("connected", session_name(s));

	if (!msg_queue_flush(*session))
		wcs_print("queue_flush", session_name(s));

	return 0;
}

/*
 * protocol_status()
 *
 * obs³uga zapytania "protocol-status" wysy³anego przez pluginy protoko³ów.
 */
static QUERY(protocol_status)
{
	char **__session	= va_arg(ap, char**), *session = *__session;
	char **__uid		= va_arg(ap, char**), *uid = *__uid;
	char *status		= *(va_arg(ap, char**));
	char **__descr		= va_arg(ap, char**), *descr = *__descr;
	char *host		= *(va_arg(ap, char**));
	int port		= *(va_arg(ap, int*));
	time_t when		= *(va_arg(ap, time_t*));
	ekg_resource_t *r	= NULL;
	userlist_t *u;
	session_t *s;

	char *st;			/* status	u->status || r->status */
	char *de;			/* descr 	u->descr  || r->descr  */

	int ignore_level;
        int ignore_status, ignore_status_descr, ignore_events, ignore_notify;

	if (!(s = session_find(session)))
		return 0;

	/* we are checking who user we know */
	if (!(u = userlist_find(s, uid))) {
		if (config_auto_user_add) u = userlist_add(s, uid, uid);
		if (!u) {
			if (config_display_unknown) {
				const char *format = ekg_status_label(status, descr, "status_");
				print_window(uid, s, 0, format, format_user(s, uid), NULL, session_name(s), descr);
			}
			return 0;
		}
	}
	ignore_level = ignored_check(s, uid);

	ignore_status = ignore_level & IGNORE_STATUS;
	ignore_status_descr = ignore_level & IGNORE_STATUS_DESCR;
	ignore_events = ignore_level & IGNORE_EVENTS;
	ignore_notify = ignore_level & IGNORE_NOTIFY;

	/* znajdz resource... */
	if (u->resources) {
		char *res = xstrchr(uid, '/');	/* resource ? */
		if (res) r = userlist_resource_find(u, res+1); 
	}

	/* status && descr ? */
	if (r) {		/* resource status && descr */
		st = r->status;
		de = r->descr;
	} else {		/* global status && descr */
		st = u->status;
		de = u->descr;
	}

	/* zapisz adres IP i port */
	u->ip = (host) ? inet_addr(host) : 0;
	u->port = port;

	if (host)
		u->last_ip = inet_addr(host);
	if (port)
		u->last_port = port;

	/* je¶li te same stany...  i te same opisy (lub brak opisu), ignoruj */
	if (!xstrcasecmp(status, st) && !xstrcmp(descr, de)) 
		return 0;

	/* je¶li kto¶ nam znika, zapamiêtajmy kiedy go widziano */
	if (!u->resources && xstrcasecmp(u->status, EKG_STATUS_NA) && !xstrcasecmp(status, EKG_STATUS_NA))
		u->last_seen = when ? when : time(NULL);

	/* XXX dodaæ events_delay */
	
	/* je¶li dostêpny lub zajêty, dopisz to taba. je¶li niedostêpny, usuñ */
	if (!xstrcasecmp(status, EKG_STATUS_AVAIL) && config_completion_notify && u->nickname)
		tabnick_add(u->nickname);
	if (!xstrcasecmp(status, EKG_STATUS_AWAY) && (config_completion_notify & 4) && u->nickname)
		tabnick_add(u->nickname);
	if (!xstrcasecmp(status, EKG_STATUS_NA) && (config_completion_notify & 2) && u->nickname)
		tabnick_remove(u->nickname);


	/* je¶li ma³o wa¿na zmiana stanu... */
	if ((session_int_get(s, "display_notify") == 2 || (session_int_get(s, "display_notify") == -1 && config_display_notify == 2)) && xstrcasecmp(st, EKG_STATUS_NA)) {
		/* je¶li na zajêty, ignorujemy */
		if (!xstrcasecmp(st, EKG_STATUS_AWAY))
			goto notify_plugins;

		/* je¶li na dostêpny, ignorujemy */
		if (!xstrcasecmp(st, EKG_STATUS_AVAIL))
			goto notify_plugins;
	}

	/* ignorowanie statusu - nie wy¶wietlamy, ale pluginy niech robi± co chc± */
        if (ignore_status)
		goto notify_plugins;

	/* nie zmieni³ siê status, zmieni³ siê opis */
	if (ignore_status_descr && !xstrcmp(status, st) && xstrcmp(descr, de))
		goto notify_plugins;

	/* daj znaæ d¼wiêkiem... */
	if (config_beep && config_beep_notify)
		query_emit(NULL, ("ui-beep"));

	/* ...i muzyczk± */
	if (config_sound_notify_file)
		play_sound(config_sound_notify_file);

        /* wy¶wietlaæ na ekranie? */
        if (!session_int_get(s, "display_notify")) 
                goto notify_plugins;

	if (!config_display_notify && session_int_get(s, "display_notify") == -1)
		goto notify_plugins;

	/* poka¿ */
	if (u->nickname) {
		const char *format = ekg_status_label(status, ignore_status_descr ? NULL : descr, "status_");
		print_window(u->nickname, s, 0, format, format_user(s, uid), (u->first_name) ? u->first_name : u->nickname, session_name(s), descr);
	}

notify_plugins:
	if (xstrcasecmp(st, EKG_STATUS_NA)) {
	        xfree(u->last_status);
	        u->last_status = xstrdup(st);
	        xfree(u->last_descr);
	        u->last_descr = xstrdup(de);
	}

	if (!xstrcasecmp(st, EKG_STATUS_NA) && xstrcasecmp(status, EKG_STATUS_NA) && !ignore_events)
		query_emit(NULL, ("event_online"), __session, __uid);

	if (!ignore_status) {
		if (r) {
			xfree(r->status);
			r->status = xstrdup(status);
		}

		if (u->resources) { 		/* get higest prio status */
			xfree(u->status);
			u->status = xstrdup( ((ekg_resource_t *) (u->resources->data))->status);
		} else {
			xfree(u->status);
			u->status = xstrdup(status);
		}
	}

	if (xstrcasecmp(de, descr) && !ignore_events)
		query_emit(NULL, ("event_descr"), __session, __uid, __descr);

	if (!ignore_status && !ignore_status_descr) {
		if (r) {
			xfree(r->descr);
			r->descr = xstrdup(descr);
		}

		if (u->resources) { 	/* get highest prio descr */
			xfree(u->descr);
			u->descr = xstrdup( ((ekg_resource_t *) (u->resources->data))->descr);
		} else {
			xfree(u->descr);
			u->descr = xstrdup(descr);
		}

		if (!u->resource || u->resources->data == r) 
			u->status_time = when ? when : time(NULL);
	}
	
	query_emit(NULL, ("userlist-changed"), __session, __uid);

	if (!xstrcasecmp(status, EKG_STATUS_AVAIL) && !ignore_events)
		query_emit(NULL, ("event_avail"), __session, __uid);
	if (!xstrcasecmp(status, EKG_STATUS_AWAY) && !ignore_events)
                query_emit(NULL, ("event_away"), __session, __uid);
        if (!xstrcasecmp(status, EKG_STATUS_NA) && !ignore_events)
                query_emit(NULL, ("event_na"), __session, __uid);

	return 0;
}

/*
 * message_print()
 *
 * wy¶wietla wiadomo¶æ w odpowiednim oknie i w odpowiedniej postaci.
 *
 * zwraca target
 */
char *message_print(const char *session, const char *sender, const char **rcpts, const char *__text, const uint32_t *format, time_t sent, int class, const char *seq, int dobeep, int secure)
{
	char *class_str, timestamp[100], *t = NULL, *text = xstrdup(__text), *emotted = NULL;
	const char *target = sender, *user;
        time_t now;
	session_t *s = session_find(session);
        struct conference *c = NULL;
	int empty_theme = 0;

	if (class & EKG_NO_THEMEBIT) {
		empty_theme = 1;
		class &= ~EKG_NO_THEMEBIT;
	}

	switch (class) {
		case EKG_MSGCLASS_SENT:
			class_str = "sent";
			target = (rcpts) ? rcpts[0] : NULL;
			break;
		case EKG_MSGCLASS_CHAT:
			class_str = "chat";
			break;
		case EKG_MSGCLASS_SYSTEM:
			class_str = "system";
			target = "__status";
			break;
		case EKG_MSGCLASS_SENT_CHAT:
			class_str = "sent";
                        target = (rcpts) ? rcpts[0] : NULL;
        	        break;
		default:
			class_str = "message";
	}

	/* dodajemy kolorki do tekstu */
	if (format) {
		string_t s = string_init("");
		const char *attrmap;
		int i;

		if (config_display_color_map && xstrlen(config_display_color_map) == 8)
			attrmap = config_display_color_map;
		else
			attrmap = "nTgGbBrR";

		for (i = 0; text[i]; i++) {
			if (i == 0 || format[i] != format[i - 1]) {
				char attr = 'n';

				if ((format[i] & EKG_FORMAT_COLOR)) {
					attr = color_map(format[i] & 0x0000ff, (format[i] & 0x0000ff00) >> 8, (format[i] & 0x00ff0000) >> 16);
					if (attr == 'k')
						attr = 'n';
				}

				if ((format[i] & 0xfe000000)) {
					uint32_t f = (format[i] & 0xff000000);

					if ((format[i] & EKG_FORMAT_COLOR))
						attr = toupper(attr);
					else
						attr = attrmap[(f >> 25) & 7];
				}

				string_append_c(s, '%');
				string_append_c(s, attr);
			}

			if (text[i] == '%')
				string_append_c(s, '%');
			
			string_append_c(s, text[i]);
		}

		xfree(text);
		text = format_string(s->str);
		string_free(s, 1);
	}

	/* wyznaczamy odstêp czasu miêdzy wys³aniem wiadomo¶ci, a chwil±
	 * obecn±, ¿eby wybraæ odpowiedni format timestampu. */
	{
		char tmp[100], *timestamp_type;
	        struct tm *tm_now, *tm_msg;

		now = time(NULL);
		tm_now = localtime(&now);
		tm_msg = localtime(&sent);

		if (sent - config_time_deviation <= now && now <= sent + config_time_deviation)
			timestamp_type = "timestamp_now";
		else if (tm_now->tm_yday == tm_msg->tm_yday)
			timestamp_type = "timestamp_today";
		else	timestamp_type = "timestamp";

		snprintf(tmp, sizeof(tmp), "%s_%s", class_str, timestamp_type);
		if (!strftime(timestamp, sizeof(timestamp), format_find(tmp), tm_msg)
				&& xstrlen(format_find(tmp))>0)
			xstrcpy(timestamp, "TOOLONG");
	}

	/* if there is a lot of recipients, conference should be made */
	{
		int recipients_count = array_count((char **) rcpts);

		if (xstrcmp(class_str, "sent") && recipients_count > 0) {
			c = conference_find_by_uids(s, sender, rcpts, recipients_count, 0);

	                if (!c) {
	                        string_t tmp = string_init(NULL);
	                        int first = 0, i;
	
	                        for (i = 0; i < recipients_count; i++) {
	                                if (first++)
	                                        string_append_c(tmp, ',');
	
	                                string_append(tmp, rcpts[i]);
	                        }
	
	                        string_append_c(tmp, ' ');
	                        string_append(tmp, sender);
	
	                        c = conference_create(s, tmp->str);
	
	                        string_free(tmp, 1);
	                }

	                if (c) {
	                        target = c->name;
				class_str = "conference";
			}
		}
	}

	/* daj znaæ d¼wiêkiem i muzyczk± */
	if (class == EKG_MSGCLASS_CHAT) {

		if (config_beep && config_beep_chat && dobeep)
			query_emit(NULL, ("ui-beep"));
	
		if (config_sound_chat_file && dobeep)
			play_sound(config_sound_chat_file);

	} else if (class == EKG_MSGCLASS_MESSAGE) {

		if (config_beep && config_beep_msg && dobeep)
			query_emit(NULL, ("ui-beep"));
		if (config_sound_msg_file && dobeep)
			play_sound(config_sound_chat_file);

	} else if (class == EKG_MSGCLASS_SYSTEM && config_sound_sysmsg_file)
			play_sound(config_sound_sysmsg_file);
	
        if (config_last & 3 && xstrcasecmp(class_str, "sent")) 
	        last_add(0, sender, now, sent, text);
	
	user = xstrcasecmp(class_str, "sent") ? format_user(s, sender) : session_format_n(sender);

	if (config_emoticons && text)
		emotted = emoticon_expand(text);

	if (empty_theme)
		class_str = "empty";

	print_window(target, s, (class == EKG_MSGCLASS_CHAT || class == EKG_MSGCLASS_SENT_CHAT), class_str, user, timestamp, (emotted) ? emotted : text, (!xstrcasecmp(class_str, "sent")) ? session_alias_uid(s) : get_nickname(s, sender), (!xstrcasecmp(class_str, "sent")) ? s->uid : get_uid(s, sender), (secure) ? format_string(format_find("secure")) : "");

	xfree(text);
	xfree(t);
	xfree(emotted);
	return xstrdup(target);
}

/*
 * protocol_message()
 */
static QUERY(protocol_message)
{
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	char **rcpts	= *(va_arg(ap, char***));
	char *text	= *(va_arg(ap, char**));
	uint32_t *format= *(va_arg(ap, uint32_t**));
	time_t sent	= *(va_arg(ap, time_t*));
	int class	= *(va_arg(ap, int*));
	char *seq	= *(va_arg(ap, char**));
	int dobeep	= *(va_arg(ap, int*));
	int secure	= *(va_arg(ap, int*));

	session_t *session_class = session_find(session);
	userlist_t *userlist = userlist_find(session_class, uid);
	char *target = NULL;
	int empty_theme = 0;
	int our_msg;

	if (ignored_check(session_class, uid) & IGNORE_MSG)
		return -1;

	/* display blinking */
	if (config_display_blinking && userlist && class != EKG_MSGCLASS_SENT && class != EKG_MSGCLASS_SENT_CHAT && (!rcpts || !rcpts[0])) {
		if (config_make_window && xstrcmp(get_uid(session_class, window_current->target), get_uid(session_class, uid))) 
			userlist->xstate |= EKG_XSTATE_BLINK;
		else if (!config_make_window) {
			window_t *w;

			/*
			 * now we are checking if there is some window with query for this
			 * user 
			 */
			w = window_find_s(session_class, uid);

			if (!w && window_current->id != 1)
				userlist->xstate |= EKG_XSTATE_BLINK; 
			if (w && window_current->id != w->id)
				userlist->xstate |= EKG_XSTATE_BLINK;
		}
	}
	
	if (class & EKG_NO_THEMEBIT) {
		class &= ~EKG_NO_THEMEBIT;
		empty_theme = 1;
	}
	our_msg = (class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT);

	/* there is no need to decode our messages */
	if (!our_msg && !empty_theme) {	/* empty_theme + decrpyt? i don't think so... */
                char *___session = xstrdup(session);
                char *___sender = xstrdup(uid);
                char *___message = xstrdup(text);
                int ___decrypted = 0;

                query_emit(NULL, ("message-decrypt"), &___session, &___sender, &___message, &___decrypted, NULL);

                if (___decrypted) {
                        text = ___message;
                        ___message = NULL;
                        secure = 1;
                }

                xfree(___session);
                xfree(___sender);
                xfree(___message);
	}

	if (our_msg)	query_emit(NULL, ("protocol-message-sent"), &session, &(rcpts[0]), &text);
	else		query_emit(NULL, ("protocol-message-received"), &session, &uid, &rcpts, &text, &format, &sent, &class, &seq, &secure);

	query_emit(NULL, ("protocol-message-post"), &session, &uid, &rcpts, &text, &format, &sent, &class, &seq, &secure);

	/* show it ! */
	if (!(our_msg && !config_display_sent)) {
		if (empty_theme)
			class |= EKG_NO_THEMEBIT;
	        target = message_print(session, uid, (const char**) rcpts, text, format, sent, class, seq, dobeep, secure);
	}

        /* je¿eli nie mamy podanego uid'u w li¶cie kontaktów to trzeba go dopisaæ do listy dope³nianych */
	if (!userlist) 
		tabnick_add(uid);

        if (!userlist && xstrcasecmp(session_class->uid, uid) && session_int_get(session_class, "auto_find") >= 1) {
                list_t l;
                int do_find = 1, i;

                for (l = autofinds, i = 0; l; l = l->next, i++) {
                        char *d = l->data;

                        if (!xstrcmp(d, uid)) {
                                do_find = 0;
                                break;
                        }
                }

                if (do_find) {
                        if (i == auto_find_limit) {
                                debug("// autofind reached %d limit, removing the oldest uin: %d\n", auto_find_limit, *((char *)autofinds->data));
                                list_remove(&autofinds, autofinds->data, 1);
                        }

                        list_add(&autofinds, (void *) uid, xstrlen(uid) + 1);

                        command_exec_format(target, session_class, 0, ("/find %s"), uid);
                }
        }

	xfree(target);
	return 0;
}

/*
 * protocol_message_ack()
 */
static QUERY(protocol_message_ack)
{
	char *session		= *(va_arg(ap, char **));
	char *rcpt		= *(va_arg(ap, char **));
	char *seq		= *(va_arg(ap, char **));
	char *__status		= *(va_arg(ap, char **));

	userlist_t *u = userlist_find(session_find(session), rcpt);
	const char *target = (u && u->nickname) ? u->nickname : rcpt;
	int display = 0;
	char format[100];

	snprintf(format, sizeof(format), "ack_%s", __status);

	msg_queue_remove_seq(seq);
	
	if (config_display_ack == 1)
		display = 1;

	if (!xstrcmp(__status, EKG_ACK_DELIVERED) && config_display_ack == 2)
		display = 1;

	if (!xstrcmp(__status, EKG_ACK_QUEUED) && config_display_ack == 3)
		display = 1;

	if (display)
		print_window(target, session_find(session), 0, format, format_user(session_find(session), rcpt));

	return 0;
}

static QUERY(protocol_xstate)
{
	/* state contains xstate bits, which should be set, offstate those, which should be cleared */
	char **__session	= va_arg(ap, char**), *session = *__session;
	char **__uid		= va_arg(ap, char**), *uid = *__uid;
	int  state		= *(va_arg(ap, int*));
	int  offstate		= *(va_arg(ap, int*));

	session_t *s;
	userlist_t *u;
	window_t *w;

	if (!(s = session_find(session)))
		return 0;
	
	if ((w = window_find_s(s, uid))) {
		if (offstate & EKG_XSTATE_TYPING)
			w->act &= ~4;
		else if (state & EKG_XSTATE_TYPING)
			w->act |= 4;
		query_emit(NULL, "ui-window-act-changed");
	}

	if ((u = userlist_find(s, uid)) || (config_auto_user_add && (u = userlist_add(s, uid, uid)))) {
		if (offstate & EKG_XSTATE_TYPING)
			u->xstate &= ~EKG_XSTATE_TYPING;
		else if (state & EKG_XSTATE_TYPING)
			u->xstate |= EKG_XSTATE_TYPING;
		query_emit(NULL, "userlist-changed", __session, __uid);
	}

	return 0;
}

dcc_t *dcc_add(const char *uid, dcc_type_t type, void *priv)
{
	dcc_t *d = xmalloc(sizeof(dcc_t));
	int id = 1, id_ok = 0;
	list_t l;

	d->uid = xstrdup(uid);
	d->type = type;
	d->priv = priv;
	d->started = time(NULL);

	do {
		id_ok = 1;

		for (l = dccs; l; l = l->next) {
			dcc_t *D = l->data;
	
			if (D->id == id) {
				id++;
				id_ok = 0;
				break;
			}
		}
	} while (!id_ok);

	d->id = id;

	list_add(&dccs, d, 0);

	return d;
}

int dcc_close(dcc_t *d)
{
	if (!d)
		return -1;

	if (d->close_handler)
		d->close_handler(d);

	xfree(d->uid);
	xfree(d->filename);

	list_remove(&dccs, d, 1);

	return 0;
}

PROPERTY_MISC(dcc, close_handler, dcc_close_handler_t, NULL)
PROPERTY_STRING(dcc, filename)
PROPERTY_INT(dcc, offset, int)
PROPERTY_INT(dcc, size, int)
PROPERTY_STRING_GET(dcc, uid)
PROPERTY_INT_GET(dcc, id, int)
PROPERTY_PRIVATE(dcc)
PROPERTY_INT_GET(dcc, started, time_t)
PROPERTY_INT(dcc, active, int)
PROPERTY_INT(dcc, type, dcc_type_t)


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=0 noexpandtab sw=8
 */
