/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
 *		  2004 Adam Mikuta <adammikuta@poczta.onet.pl>
 *		  2005 Leszek Krupi�ski <leafnode@wafel.com>
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

#include <stdio.h>
#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <string.h>

#include "emoticons.h"
#include "objects.h"

static int auto_find_limit = 100; /* counter of persons who we were looking for when autofind */
dcc_t *dccs = NULL;

static QUERY(protocol_disconnected);
static QUERY(protocol_connected);
static QUERY(protocol_message_ack);
static QUERY(protocol_status);
static QUERY(protocol_message);
static QUERY(protocol_xstate);
static QUERY(protocol_userlist_changed);

/**
 * protocol_init()
 *
 * Init communication between core and PROTOCOL plugins<br>
 * <br>
 * Here, we register <b>main</b> <i>communication channels</i> like:<br>
 *	- status changes:			<i>PROTOCOL_STATUS</i><br>
 *	- message I/O:				<i>PROTOCOL_MESSAGE</i><br>
 *	- acknowledge of messages:		<i>PROTOCOL_MESSAGE_ACK</i><br>
 *	- misc user events like typing notifies:<i>PROTOCOL_XSTATE</i><br>
 *	- session connection/disconnection:	<i>PROTOCOL_CONNECTED</i> and <i>PROTOCOL_DISCONNECTED</i>
 *	- roster changes:			<i>USERLIST_ADDED</i> and <i>USERLIST_REMOVED</i> and <i>USERLIST_RENAMED</i>
 *
 * @sa query_connect()	- Function to add listener on specified events.
 * @sa query_emit()	- Function to emit specified events.
 */

void protocol_init() {
	query_connect(NULL, "protocol-status", protocol_status, NULL);
	query_connect(NULL, "protocol-message", protocol_message, NULL);
	query_connect(NULL, "protocol-message-ack", protocol_message_ack, NULL);
	query_connect(NULL, "protocol-xstate", protocol_xstate, NULL);

	query_connect(NULL, "protocol-connected", protocol_connected, NULL);
	query_connect(NULL, "protocol-disconnected", protocol_disconnected, NULL);

	query_connect(NULL, "userlist-added",		protocol_userlist_changed, NULL);
	query_connect(NULL, "userlist-removed",	protocol_userlist_changed, NULL);
	query_connect(NULL, "userlist-renamed",	protocol_userlist_changed, NULL);
}

/*
 * XXX,
 *	This code is from ncurses ncurses_userlist_changed()
 *
 *	It's now proper ekg2-way (and also for gtk/readline)
 *
 *	However in USERLIST_* event we don't pass session...
 *	Yep, it's buggy.
 */

static QUERY(protocol_userlist_changed) {
	char **p1 = va_arg(ap, char**);
	char **p2 = va_arg(ap, char**);
	
	window_t *w;

	for (w = windows; w; w = w->next) {
		if (!w->target || xstrcasecmp(w->target, *p1))
			continue;

		xfree(w->target);
		w->target = xstrdup(*p2);

		query_emit(NULL, "ui-window-target-changed", &w);
	}

	return 0;
}


/**
 * protocol_reconnect_handler()
 *
 * Handler of reconnect timer created by protocol_disconnected()<br>
 *
 * @param type -	0 - If timer should do his job<br>
 *			1 - If timer'll be destroy, and handler should free his data
 * @param s - session to reconnect
 *
 * @return -1 [TEMPORARY TIMER]
 */

static TIMER_SESSION(protocol_reconnect_handler) {
	if (type == 1)
		return 0;

	if (!s || s->connected)
		return -1;

	debug("protocol_reconnect_handler() reconnecting session %s\n", s->uid);

	command_exec(NULL, s, ("/connect"), 0);
	return -1;
}

/**
 * protocol_disconnected()
 *
 * Handler for <i>PROTOCOL_DISCONNECTED</i><br>
 * When session notify core about disconnection we do here:<br>
 *	- clear <b>all</b> user status, presence, resources details. @sa userlist_clear_status()<br>
 *	- update s->last_conn state, and set s->connected to 0<br>
 *	- check if disconnect @a type was either <i>EKG_DISCONNECT_NETWORK:</i> or <i>EKG_DISCONNECT_FAILURE:</i> and
 *		if yes, create reconnect timer (if user set auto_reconnect variable)<br>
 *	- display notify through UI-plugin
 *
 * @note About different types [@a type] of disconnections:<br>
 *		- <i>EKG_DISCONNECT_USER</i>	- when user do /disconnect [<b>with reason</b>, in @a reason we should have param of /disconnect command][<b>without reconnection</b>]<br>
 *		- <i>EKG_DISCONNECT_NETWORK</i>	- when smth is wrong with network... (read: when recv() fail, or send() or SSL wrappers for rcving/sending data fail with -1 and with bad errno)
 *						[<b>with reason</b> describiny why we fail (strerror() is good here)][<b>with reconnection</b>]<br>
 *		- <i>EKG_DISCONNECT_FORCED</i>	- when server force us to disconnection. [<b>without reason</b>][<b>without reconnection</b>]<br>
 *		- <i>EKG_DISCONNECT_FAILURE</i> - when we fail to connect to server (read: when we fail connect session, after /connect) 
 *						[<b>with reason</b> describiny why we fail (strerror() is good here)][<b>with reconnection</b>]<br>
 *		- <i>EKG_DISCONNECT_STOPPED</i> - when user do /disconnect during connection [<b>without reason</b>] [<b>without reconnection</b>]<br>
 *
 * @param ap 1st param: <i>(char *) </i><b>session</b> - session uid which goes disconnect
 * @param ap 2nd param: <i>(char *) </i><b>reason</b>  - reason why session goes disconnect.. It's reason specifed by user if EKG_DISCONNECT_USER, else 
 *								string with error description like from: strerror().. [if EKG_DISCONNECT_FAILURE]
 * @param ap 3rd param: <i>(int) </i><b>type</b> - type of disconnection one of: 
 *					[EKG_DISCONNECT_USER, EKG_DISCONNECT_NETWORK, EKG_DISCONNECT_FORCED, EKG_DISCONNECT_FAILURE, EKG_DISCONNECT_STOPPED]
 *
 * @param data NULL
 *
 * @return 0
 *
 */

static QUERY(protocol_disconnected) {
	char *session	= *(va_arg(ap, char **));
	char *reason	= *(va_arg(ap, char **));
	int type	= *(va_arg(ap, int*));

	session_t *s	= session_find(session);

	userlist_clear_status(s, NULL);

	if (s) {
		if (s->connected) {
			int one = 1;

			s->last_conn = time(NULL);
			s->connected = 0;
			query_emit(NULL, "session-event", &s, &one);	/* notify UI */
		} else
			s->connecting = 0;
		command_exec(NULL, s, "/session --unlock", 1);
	}

	switch (type) {
		case EKG_DISCONNECT_NETWORK:
		case EKG_DISCONNECT_FAILURE:
		{
			int tmp;

			if (type == EKG_DISCONNECT_NETWORK)
				print("conn_broken", session_name(s), reason);
			else
				print("conn_failed", reason, session_name(s));

			if (s && (tmp = session_int_get(s, "auto_reconnect")) && tmp != -1 && timer_find_session(s, "reconnect") == NULL)
				timer_add_session(s, "reconnect", tmp, 0, protocol_reconnect_handler);

			break;
		}

		case EKG_DISCONNECT_USER:
			if (reason)
				print("disconnected_descr", reason, session_name(s));
			else
				print("disconnected", session_name(s));
			/* We don't use session_unidle(), because:
			 * 1) we don't want to print 'Auto back: ...' - that'd be stupid
			 * 2) we don't want to risk if some _autoback could make magic at this state of connection */
			if (s && s->autoaway)
				session_status_set(s, EKG_STATUS_AUTOBACK);
			if (s)
				s->activity = 0; /* mark we'd like PROTOCOL_CONNECTED to set activity */
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

int protocol_disconnected_emit(const session_t *s, const char *reason, int type) {
	char *session   = xstrdup(s->uid);
	char *reason_ro = xstrdup(reason);
	int result      = query_emit(NULL, "protocol-disconnected", &session, &reason_ro, &type);

	xfree(session);
	xfree(reason_ro);
	return result;
}

/**
 * protocol_connected()
 *
 * Handler for <i>PROTOCOL_CONNECTED</i><br>
 * When session notify core about connection we do here:<br>
 *	- If we have ourselves on the userlist. It update status and description<br>
 *	- Display notify through UI-plugin<br>
 *	- If we have messages in session queue, than send it and display info.<br>
 *	- Update last_conn state and set connected state to 1.<br>
 *	- Remove "reconnect" timer.
 *
 * @param ap 1st param: <i>(char *) </i><b>session</b> - session uid which goes connected.
 * @param data NULL
 *
 * @return 0
 */

static QUERY(protocol_connected) {
	char *session = *(va_arg(ap, char**));

	session_t *s = session_find(session);
	const char *descr = session_descr_get(s);
	
	ekg_update_status(s);

	if (descr)
		print("connected_descr", descr, session_name(s));
	else
		print("connected", session_name(s));

	if (s) {
		int two = 2;

		s->last_conn = time(NULL);
		if (!s->activity) /* someone asks us to set activity */
			s->activity  = s->last_conn;
		s->connecting = 0;
		s->connected = 1;
		timer_remove_session(s, "reconnect");

		query_emit(NULL, "session-event", &s, &two);	/* Notify UI */
	}

	if (!msg_queue_flush(session))
		print("queue_flush", session_name(s));

	return 0;
}

int protocol_connected_emit(const session_t *s) {
	char *session = xstrdup(s->uid);
	int result    = query_emit(NULL, "protocol-connected", &session);

	xfree(session);
	return result;
}

/*
 * protocol_status()
 *
 * obs�uga zapytania "protocol-status" wysy�anego przez pluginy protoko��w.
 */
static QUERY(protocol_status)
{
	char **__session	= va_arg(ap, char**), *session = *__session;
	char **__uid		= va_arg(ap, char**), *uid = *__uid;
	int status		= *(va_arg(ap, int*));
	char **__descr		= va_arg(ap, char**), *descr = *__descr;
	time_t when		= *(va_arg(ap, time_t*));
	ekg_resource_t *r	= NULL;
	userlist_t *u;
	session_t *s;

	int st;				/* status	u->status || r->status */
	char *de;			/* descr	u->descr  || r->descr  */

	int ignore_level;
	int ignore_status, ignore_status_descr, ignore_events, ignore_notify;
	int sess_notify;

	if (!(s = session_find(session)))
		return 0;
	
	sess_notify = session_int_get(s, "display_notify");
	/* we are checking who user we know */
	if (!(u = userlist_find(s, uid))) {
		if (config_auto_user_add && xstrncmp(uid, session, xstrlen(session)) ) {
			char *tmp = xstrdup(uid);
			char *p = xstrchr(tmp, '/');
			if (p) *p = 0;

			u = userlist_add(s, tmp, tmp);

			xfree(tmp);
		}
		if (!u) {
			if ((sess_notify == -1 ? config_display_notify : sess_notify) & 4) {
				const char *format = ekg_status_label(status, descr, "status_");
				print_info(uid, s, format, format_user(s, uid), NULL, session_name(s), descr);
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

	/* je�li te same stany...  i te same opisy (lub brak opisu), ignoruj */
	if ((status == st) && !xstrcmp(descr, de)) 
		return 0;

	/* je�li kto� nam znika, zapami�tajmy kiedy go widziano */
	if (!u->resources && !EKG_STATUS_IS_NA(u->status) && EKG_STATUS_IS_NA(status))
		u->last_seen = when ? when : time(NULL);

	/* XXX doda� events_delay */
	
	/* je�li dost�pny lub zaj�ty, dopisz to taba. je�li niedost�pny, usu� */
	if (EKG_STATUS_IS_AVAIL(status) && config_completion_notify && u->nickname)
		tabnick_add(u->nickname);
	if (!EKG_STATUS_IS_AWAY(status) && (config_completion_notify & 4) && u->nickname)
		tabnick_add(u->nickname);
	if (EKG_STATUS_IS_NA(status) && (config_completion_notify & 2) && u->nickname)
		tabnick_remove(u->nickname);


	/* je�li ma�o wa�na zmiana stanu...
	 * XXX someone can tell me what this should do, 'cos I can't understand the way it's written? */
	
	/* z dokumentacji, co powinno robic:
	 *	warto�� 2 wy�wietla tylko zmiany z niedost�pnego na dost�pny i na odwr�t. 
	 */

	if (((sess_notify == -1 ? config_display_notify : sess_notify) & 2)
			&& !(EKG_STATUS_IS_NA(st) ^ EKG_STATUS_IS_NA(status)))
		goto notify_plugins;

	/* ignorowanie statusu - nie wy�wietlamy, ale pluginy niech robi� co chc� */
	if (ignore_status || ignore_notify)
		goto notify_plugins;

	/* nie zmieni� si� status, zmieni� si� opis */
	if (ignore_status_descr && (status == st) && xstrcmp(descr, de))
		goto notify_plugins;

	/* daj zna� d�wi�kiem... */
	if (config_beep && config_beep_notify)
		query_emit(NULL, "ui-beep");

	/* ...i muzyczk� */
	if (config_sound_notify_file)
		play_sound(config_sound_notify_file);

	/* wy�wietla� na ekranie? */
	if (!((sess_notify == -1 ? config_display_notify : sess_notify) & 3))
		goto notify_plugins;

	/* poka� */
	if (u->nickname) {
		const char *format = ekg_status_label(status, ignore_status_descr ? NULL : descr, "status_");
		print_info(u->nickname, s, format, format_user(s, uid), get_user_name(u), session_name(s), descr);
	}

notify_plugins:
	if (!EKG_STATUS_IS_NA(st)) {
		u->last_status = st;
		xfree(u->last_descr);
		u->last_descr = xstrdup(de);
		if (EKG_STATUS_IS_NA(status) && !ignore_events)
			query_emit(NULL, "event-offline", __session, __uid);
	} else if (!EKG_STATUS_IS_NA(status) && !ignore_events)
		query_emit(NULL, "event-online", __session, __uid);

	if (!ignore_status) {
		if (r) {
			r->status = status;
		}

		if (u->resources) {		/* get higest prio status */
			u->status = u->resources->status;
		} else {
			u->status = status;
		}
	}

	if (xstrcasecmp(de, descr) && !ignore_events)
		query_emit(NULL, "event-descr", __session, __uid, __descr);

	if (!ignore_status && !ignore_status_descr) {
		if (r) {
			xfree(r->descr);
			r->descr = xstrdup(descr);
		}

		xfree(u->descr);
		u->descr = xstrdup(u->resources ? u->resources->descr : descr);	/* get highest prio descr */

		de = xstrdup(u->descr);
		if (de) {
			char *p;
			while ((p=xstrstr(de, "\r\n"))) memmove(p, p+1, xstrlen(p));	/* dos2unix */
			while ((p=xstrchr(de,'\n'))) *p = ' ';				/* all in 1 line */
		}
		xfree(u->descr1line);
		u->descr1line = de;

		if (!u->resources || u->resources == r) 
			u->status_time = when ? when : time(NULL);
	}
	
	query_emit(NULL, "userlist-changed", __session, __uid);

	/* Currently it behaves like event means grouped statuses,
	 * i.e. EVENT_AVAIL is for avail&ffc
	 *	EVENT_AWAY for away&xa&dnd
	 *	... */
	if (!ignore_events) {
		if (EKG_STATUS_IS_AVAIL(status))
			query_emit(NULL, "event-avail", __session, __uid);
		else if (EKG_STATUS_IS_AWAY(status))
			query_emit(NULL, "event-away", __session, __uid);
		else if (EKG_STATUS_IS_NA(status))
			query_emit(NULL, "event-na", __session, __uid);
	}

	return 0;
}

int protocol_status_emit(const session_t *s, const char *uid, int status, char *descr, time_t when) {
	char *session  = xstrdup(s->uid);
	char *uid_ro   = xstrdup(uid);
	char *descr_ro = xstrdup(descr);
	int result     = query_emit(NULL, "protocol-status", &session, &uid_ro, &status, &descr_ro, &when);

	xfree(session);
	xfree(uid_ro);
	xfree(descr_ro);
	return result;
}

/*
 * message_print()
 *
 * wy�wietla wiadomo�� w odpowiednim oknie i w odpowiedniej postaci.
 *
 * zwraca target
 */
char *message_print(const char *session, const char *sender, const char **rcpts, const char *__text, const guint32 *format, time_t sent, int mclass, const char *seq, int dobeep, int secure)
{
	char *class_str, timestamp[100], *text = xstrdup(__text);
	char *securestr = NULL;
	const char *target = sender, *user;
	time_t now;
	session_t *s = session_find(session);
	struct conference *c = NULL;
	int empty_theme = 0, is_me = 0, to_me = 1, activity = 0, separate = 0;

	if (mclass & EKG_MSGCLASS_NOT2US) {
		to_me = 0;
		mclass &= ~EKG_MSGCLASS_NOT2US;
	}

	if (mclass & EKG_NO_THEMEBIT) {
		empty_theme = 1;
		mclass &= ~EKG_NO_THEMEBIT;
	}

	switch (mclass) {
		case EKG_MSGCLASS_SENT:
		case EKG_MSGCLASS_SENT_CHAT:
			if (/*config_display_me && */ !xstrncmp(text, "/me ", 4)) {
				class_str = "sent_me";
				is_me = 1;
			} else
				class_str = "sent";
			target = (rcpts) ? rcpts[0] : NULL;
			break;
		case EKG_MSGCLASS_CHAT:
			if (/*config_display_me && */ !xstrncmp(text, "/me ", 4)) {
				class_str = "chat_me";
				is_me = 1;
			} else
				class_str = "chat";
			break;
		case EKG_MSGCLASS_SYSTEM:
			class_str = "system";
			target = "__status";
			break;
		case EKG_MSGCLASS_LOG:
			class_str = "log";
			break;
		case EKG_MSGCLASS_SENT_LOG:
			class_str = "sent_log";
			target = (rcpts) ? rcpts[0] : NULL;
			break;
		default:
			if (mclass != EKG_MSGCLASS_MESSAGE)
				debug("[message_print] got unexpected mclass = %d\n", mclass);
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

			guint32 f = format[i];

			if (i == 0 || f != format[i - 1]) {
				char attr = 'n';

				if ((f & EKG_FORMAT_COLOR)) {
					attr = color_map(f & EKG_FORMAT_B_MASK, (f & EKG_FORMAT_G_MASK) >> 8, (f & EKG_FORMAT_B_MASK) >> 16);
					if (attr == 'k')
						attr = 'n';
				}

				if ((f & ~(EKG_FORMAT_COLOR | EKG_FORMAT_RGB_MASK))) {

					if ((f & EKG_FORMAT_COLOR))
						attr = toupper(attr);
					else
						attr = attrmap[(f >> 25) & 7];
				}

				if (attr=='N')
					attr='T';

				string_append_c(s, '%');
				string_append_c(s, attr);
			}

			if ((text[i] == '/') && (text[i+1] == '|'))	/* /| set margin */
				if ((i == 0) || (text[i-1] != '/'))
					string_append_c(s, '/');

			if (text[i] == '%')
				string_append_c(s, '%');
			
			string_append_c(s, text[i]);
		}

		xfree(text);
		text = format_string(s->str);
		string_free(s, 1);
	}

	/* wyznaczamy odst�p czasu mi�dzy wys�aniem wiadomo�ci, a chwil�
	 * obecn�, �eby wybra� odpowiedni format timestampu. */
	{
		char tmp[100], *timestamp_type;
		struct tm *tm_msg;
		int tm_now_day;			/* it's localtime(&now)->tm_yday */

		now = time(NULL);

		tm_now_day = localtime(&now)->tm_yday;
		tm_msg = localtime(&sent);

		if (sent - config_time_deviation <= now && now <= sent + config_time_deviation)
			timestamp_type = "timestamp_now";
		else if (tm_now_day == tm_msg->tm_yday)
			timestamp_type = "timestamp_today";
		else	timestamp_type = "timestamp";

		snprintf(tmp, sizeof(tmp), "%s_%s", class_str, timestamp_type);
		if (!strftime(timestamp, sizeof(timestamp), format_find(tmp), tm_msg)
				&& xstrlen(format_find(tmp))>0)
			xstrcpy(timestamp, "TOOLONG");
	}

	/* if there is a lot of recipients, conference should be made */
	{
		int recipients_count = g_strv_length((char **) rcpts);

		if ((mclass < EKG_MSGCLASS_SENT) && recipients_count > 0) {
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
			} else if (c->ignore) {
				xfree(text);
				return NULL;
			}

			if (c) {
				target = c->name;
				class_str = "conference";
			}
		}
	}

		/* XXX: I personally think this should be moved outta here
		 * I don't think that beeping should be considered as 'printing' */
	/* daj zna� d�wi�kiem i muzyczk� */
	if (mclass == EKG_MSGCLASS_CHAT) {

		if (config_beep && config_beep_chat && dobeep)
			query_emit(NULL, "ui-beep");
	
		if (config_sound_chat_file && dobeep)
			play_sound(config_sound_chat_file);

	} else if (mclass == EKG_MSGCLASS_MESSAGE) {

		if (config_beep && config_beep_msg && dobeep)
			query_emit(NULL, "ui-beep");
		if (config_sound_msg_file && dobeep)
			play_sound(config_sound_msg_file);

	} else if (mclass == EKG_MSGCLASS_SYSTEM && config_sound_sysmsg_file)
			play_sound(config_sound_sysmsg_file);
	
	if (config_last & 3 && (mclass < EKG_MSGCLASS_SENT))
		last_add(0, sender, now, sent, text);
	
	user = (mclass < EKG_MSGCLASS_SENT) ? format_user(s, sender) : session_format_n(sender);

	if (config_emoticons && text) {
		char *tmp = emoticon_expand(text);
		xfree(text);
		text = tmp;
	}

	if (empty_theme)
		class_str = "empty";

	if (secure) 
		securestr = format_string(format_find("secure"));

	if (mclass == EKG_MSGCLASS_LOG || mclass == EKG_MSGCLASS_SENT_LOG)
		separate = 1;

	if ( (mclass == EKG_MSGCLASS_CHAT || mclass == EKG_MSGCLASS_SENT_CHAT) || 
		  (!(config_make_window & 4) && (mclass == EKG_MSGCLASS_MESSAGE || mclass == EKG_MSGCLASS_SENT)) ) {
		activity = to_me ? EKG_WINACT_IMPORTANT : EKG_WINACT_MSG;
		separate = 1;
	}

	print_window(target, s, activity, separate, class_str, user, timestamp,
		(is_me ? text+4 : text),
					/* XXX, get_uid() get_nickname() */
		(mclass >= EKG_MSGCLASS_SENT ?
			(is_me && config_nickname ? config_nickname : session_alias_uid(s))
			: get_nickname(s, sender)),
		(mclass >= EKG_MSGCLASS_SENT ? s->uid : get_uid(s, sender)),
		(secure ? securestr : ""));

	xfree(text);
	xfree(securestr);
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
	char **ptext	= (va_arg(ap, char**));
	guint32 *format= *(va_arg(ap, guint32**));
	time_t sent	= *(va_arg(ap, time_t*));
	int mclass	= *(va_arg(ap, int*));
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
	if (config_display_blinking && userlist && (mclass < EKG_MSGCLASS_SENT) && (!rcpts || !rcpts[0])) {
		int oldstate = userlist->blink;

		if (config_make_window && xstrcmp(get_uid(session_class, window_current->target), get_uid(session_class, uid))) 
			userlist->blink = 1;
		else if (!config_make_window) {
			window_t *w;

			/*
			 * now we are checking if there is some window with query for this
			 * user 
			 */
			w = window_find_s(session_class, uid);

			if (w ? (window_current->id != w->id) : (window_current->id != 1))
				userlist->blink = 1;
		}

		if (oldstate != userlist->blink)
			query_emit(NULL, "userlist-changed", &session, &uid);
	}
	
	if (mclass & EKG_NO_THEMEBIT) {
		mclass &= ~EKG_NO_THEMEBIT;
		empty_theme = 1;
	}
	our_msg = (mclass >= EKG_MSGCLASS_SENT);

	/* there is no need to decode our messages */
	if (!our_msg && !empty_theme) {	/* empty_theme + decrpyt? i don't think so... */
		char *___session = xstrdup(session);
		char *___sender = xstrdup(uid);
		char *___message = xstrdup(*ptext);
		int ___decrypted = 0;

		query_emit(NULL, "message-decrypt", &___session, &___sender, &___message, &___decrypted, NULL);

		if (!___message) {
			/* Message has been cancelled (crypto plugin's internal comm) */
			xfree(___session);
			xfree(___sender);
			return 0;
		}

		if (___decrypted) {
			xfree(*ptext);
			*ptext = ___message;
			___message = NULL;
			secure = 1;
		}

		xfree(___session);
		xfree(___sender);
		xfree(___message);
	}

	if (our_msg)	query_emit(NULL, "protocol-message-sent", &session, &(rcpts[0]), ptext);
	else		query_emit(NULL, "protocol-message-received", &session, &uid, &rcpts, ptext, &format, &sent, &mclass, &seq, &secure);

	query_emit(NULL, "protocol-message-post", &session, &uid, &rcpts, ptext, &format, &sent, &mclass, &seq, &secure);

	/* show it ! */
	if (!(our_msg && !config_display_sent)) {
		if (empty_theme)
			mclass |= EKG_NO_THEMEBIT;
		if (!(target = message_print(session, uid, (const char**) rcpts, *ptext, format, sent, mclass, seq, dobeep, secure)))
			return -1;
	}

	/* je�eli nie mamy podanego uid'u w li�cie kontakt�w to trzeba go dopisa� do listy dope�nianych */
	if (!userlist && !our_msg)	/* don't add us to tabnick */ 
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

			list_add(&autofinds, xstrdup(uid));

			command_exec_format(target, session_class, 0, ("/find %s"), uid);
		}
	}

	xfree(target);
	return 0;
}

int protocol_message_emit(const session_t *s, const char *uid, char **rcpts, const char *text, const guint32 *format, time_t sent, int mclass, const char *seq, int dobeep, int secure) {
	char *session = xstrdup(s->uid);
	char *uid_ro  = xstrdup(uid);
	char *text_ro = xstrdup(text);
	char *seq_ro  = xstrdup(seq);
	/* XXX, rcpts_ro, format_ro */
	int result    = query_emit(NULL, "protocol-message", &session, &uid_ro, &rcpts, &text_ro, &format, &sent, &mclass, &seq_ro, &dobeep, &secure);

	xfree(session);
	xfree(uid_ro);
	xfree(text_ro);
	xfree(seq_ro);
	return result;
}

/**
 * protocol_message_ack()
 *
 * Handler for <i>PROTOCOL_MESSAGE_ACK</i>
 * When session notifies core about receiving acknowledgement for our message, we:<br>
 *	- Remove message with given sequence id (@a seq) from msgqueue @sa msg_queue_remove_seq()<br>
 *	- If corresponding @a config_display_ack variable bit is set, then display notification through UI-plugin
 *
 * @note	About different types of confirmations (@a __status):<br>
 *			- <i>EKG_ACK_DELIVERED</i>	- when message was successfully delivered to user<br>
 *			- <i>EKG_ACK_QUEUED</i>		- when user is somewhat unavailable and server confirmed to accept the message for later delivery<br>
 *			- <i>EKG_ACK_DROPPED</i>	- when user or server rejected to deliver our message (forbidden content?) and it was dropped; further retries will probably fail, if second side doesn't perform some kind of action (e.g. add us to roster in GG)<br>
 *			- <i>EKG_ACK_TEMPFAIL</i>	- when server failed temporarily to deliver our message, but encourages us to try again later (e.g. message queue full)<br>
 *			- <i>EKG_ACK_UNKNOWN</i>	- when it's not clear what happened with our message<br>
 *
 * @todo	Should we remove msg from msgqueue only when sequenceid and session and rcpt matches?
 *		I think it's buggy cause user at jabber can send us acknowledge of message
 *		which we never send, but if seq match with other message, which wasn't send (because session was for example disconnected)
 *		we remove that messageid, and than we'll never send it, and we'll never know that we don't send it.
 *
 * @param ap 1st param: <i>(char *) </i><b>session</b>	- session which send this notify
 * @param ap 2nd param: <i>(char *) </i><b>rcpt</b>	- user uid who confirm receiving messages
 * @param ap 3rd param: <i>(char *) </i><b>seq</b>	- sequence id of message
 * @param ap 4th param: <i>int </i><b>__status</b> - type of confirmation; one of: [EKG_ACK_DELIVERED, EKG_ACK_QUEUED, EKG_ACK_DROPPED, EKG_ACK_TEMPFAIL, EKG_ACK_UNKNOWN]
 *
 * @param data NULL
 *
 * @return 0
 */

static QUERY(protocol_message_ack) {
	const char *ackformats[] = {"ack_delivered", "ack_queued", "ack_filtered", "ack_tempfail", "ack_unknown"};

	char *session		= *(va_arg(ap, char **));
	char *rcpt		= *(va_arg(ap, char **));
	char *seq		= *(va_arg(ap, char **));
	int __status		= *(va_arg(ap, int *));

	session_t *s	= session_find(session);
	userlist_t *u	= userlist_find(s, rcpt);
	const char *target = (u && u->nickname) ? u->nickname : rcpt;

	msg_queue_remove_seq(seq);
	
	if ((__status >= 0) && (__status < EKG_ACK_MAX) && (config_display_ack & (1 << __status)))
		print_info(target, s, ackformats[__status], format_user(s, rcpt));

	return 0;
}

int protocol_message_ack_emit(const session_t *s, const char *rcpt, const char *seq, int status) {
	char *session = xstrdup(s->uid);
	char *rcpt_ro = xstrdup(rcpt);
	char *seq_ro  = xstrdup(seq);
	int result    = query_emit(NULL, "protocol-message-ack", &session, &rcpt_ro, &seq_ro, &status);

	xfree(session);
	xfree(rcpt_ro);
	xfree(seq_ro);
	return result;
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
			w->in_typing	= 0;
		else if (state & EKG_XSTATE_TYPING)
			w->in_typing	= 1;
		else
			goto xs_userlist;

		query_emit(NULL, "ui-window-act-changed", &w);		/* XXX, UI_WINDOW_TYPING_CHANGED? :> */
	}

xs_userlist:
	if ((u = userlist_find(s, uid)) || (config_auto_user_add && (u = userlist_add(s, uid, uid)))) {
		if (offstate & EKG_XSTATE_TYPING)
			u->typing	= 0;
		else if (state & EKG_XSTATE_TYPING)
			u->typing	= 1;
		else
			return 0;

		query_emit(NULL, "userlist-changed", __session, __uid);
	}

	return 0;
}

int protocol_xstate_emit(const session_t *s, const char *uid, int state, int offstate) {
	char *session = xstrdup(s->uid);
	char *uid_ro  = xstrdup(uid);
	int result    = query_emit(NULL, "protocol-xstate", &session, &uid_ro, &state, &offstate);

	xfree(session);
	xfree(uid_ro);
	return result;
}

/*
 * protocol_uid()
 *
 * return saprintf("%s:%s", proto, target);
 */
char *protocol_uid(const char *proto, const char *target) {
/* XXX, simplify some code inside plugins?
 *
 *	if (!xstrncmp(target, proto, xstrlen(proto)))
 *		return xstrdup(target);
 */
	return saprintf("%s:%s", proto, target);
}

static LIST_FREE_ITEM(dcc_free_item, dcc_t *) {
	if (data->close_handler)
		data->close_handler(data);
	xfree(data->uid); xfree(data->filename);
}

DYNSTUFF_LIST_DECLARE(dccs, dcc_t, dcc_free_item, 
	static __DYNSTUFF_LIST_ADD,			/* dccs_add() */
	static __DYNSTUFF_LIST_REMOVE_SAFE,		/* dccs_remove() */
	__DYNSTUFF_NODESTROY)				/* XXX dccs_destroy() XXX, we don't care? */

/**
 * dcc_add()
 *
 */

dcc_t *dcc_add(session_t *session, const char *uid, dcc_type_t type, void *priv) {
	dcc_t *d;
	int id = 1, id_ok;

	do {
		id_ok = 1;

		for (d = dccs; d; d = d->next) {
			if (d->id == id) {
				id++;
				id_ok = 0;
				break;
			}
		}
		if (id < 1) {		/* protect from posibble deadlock */
			debug_error("dcc_add() too many dcc's id < 1...\n");
			return NULL;
		}
	} while (!id_ok);

	d = xmalloc(sizeof(dcc_t));
	d->session = session;
	d->uid = xstrdup(uid);
	d->type = type;
	d->priv = priv;
	d->started = time(NULL);
	d->id = id;

	dccs_add(d);

	return d;
}

int dcc_close(dcc_t *d)
{
	if (!d)
		return -1;

	dccs_remove(d);
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
 * vim: noet
 */
