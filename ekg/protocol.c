/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include "config.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

#include "commands.h"
#include "log.h"
#include "msgqueue.h"
#include "objects.h"
#include "protocol.h"
#include "stuff.h"
#include "userlist.h"
#include "themes.h"
#include "xmalloc.h"

/*
 * protocol_init()
 *
 * rejestruje wszystkie zapytania zwi±zane z protoko³em, które bêd±
 * wysy³ane przez pluginy.
 */
void protocol_init()
{
	query_connect(NULL, "protocol-status", protocol_status, NULL);
	query_connect(NULL, "protocol-message", protocol_message, NULL);
	query_connect(NULL, "protocol-message-ack", protocol_message_ack, NULL);

	query_connect(NULL, "protocol-connected", protocol_connected, NULL);
	query_connect(NULL, "protocol-disconnected", protocol_disconnected, NULL);
}

/*
 * protocol_reconnect_handler()
 *
 * obs³uga timera reconnectu.
 */
static void protocol_reconnect_handler(int type, void *data)
{
	char *session = (char*) data;

	if (type == 1) {
		xfree(session);
		return;
	}

	debug("reconnecting session %s\n", session);

	command_exec(NULL, session_find(session), "/connect", 0);
}

/*
 * protocol_disconnect()
 *
 * obs³uga zerwanego po³±czenia.
 */
int protocol_disconnected(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**), *session = *__session;
	char **__reason = va_arg(ap, char**), *reason = *__reason;
	int *__type = va_arg(ap, int*), type = *__type;

	switch (type) {
		case EKG_DISCONNECT_NETWORK:
		case EKG_DISCONNECT_FAILURE:
		{
			int tmp;

			if ((tmp = session_int_get_n(session, "auto_reconnect")))
				timer_add(NULL, "reconnect", tmp, 0, protocol_reconnect_handler, xstrdup(session));

			if (type == EKG_DISCONNECT_NETWORK)
				print("conn_broken");
			else
				print("conn_failed", reason);
			
			break;
		}

		case EKG_DISCONNECT_USER:
			if (reason)
				print("disconnected_descr", reason);
			else
				print("disconnected");

			break;

		case EKG_DISCONNECT_FORCED:
			print("conn_disconnected");

			break;
	}

	return 0;
}

/*
 * protocol_connect()
 *
 * obs³uga udanego po³±czenia.
 */
int protocol_connected(void *data, va_list ap)
{
	char **session = va_arg(ap, char**);
	const char *descr = session_descr_get_n(*session);

	if (descr)
		print("connected_descr", descr);
	else
		print("connected");

	return 0;
}

/*
 * protocol_status()
 *
 * obs³uga zapytania "protocol-status" wysy³anego przez pluginy protoko³ów.
 */
int protocol_status(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**), *session = *__session;
	char **__uid = va_arg(ap, char**), *uid = *__uid;
	char **__status = va_arg(ap, char**), *status = *__status;
	char **__descr = va_arg(ap, char**), *descr = *__descr;
	char **__host = va_arg(ap, char**), *host = *__host;
	int *__port = va_arg(ap, int*), port = *__port;
	userlist_t *u;

	/* ignorujemy nieznanych nam osobników */
	if (!(u = userlist_find(session, uid)))
		return 0;

	/* zapisz adres IP i port */
	u->ip = (host) ? inet_addr(host) : 0;
	u->port = port;

	/* je¶li te same stany... */
	if (!strcasecmp(status, u->status)) {
		/* ...i nie ma opisu, ignoruj */
		if (!descr && !u->descr)
			return 0;
	
		/* ...i te same opisy, ignoruj */
		if (descr && u->descr && !strcmp(descr, u->descr))
			return 0;
	}

	/* je¶li kto¶ nam znika, zapamiêtajmy kiedy go widziano */
	if (!strcasecmp(status, EKG_STATUS_NA))
		u->last_seen = time(NULL);

	/* XXX dodaæ sprawdzanie ignorowanych, events_delay */
	
	/* zaloguj */
	if (config_log_status)
		put_log(uid, "status,%s,%s,%s:%d,%s,%s%s%s\n", uid, (u->nickname) ? u->nickname : "", inet_ntoa(*((struct in_addr*) &u->ip)), u->port, log_timestamp(time(NULL)), u->status, (u->descr) ? "," : "", (u->descr) ? u->descr : "");
	
	/* je¶li dostêpny lub zajêty, dopisz to taba. je¶li niedostêpny, usuñ */
	if (!strcasecmp(status, EKG_STATUS_AVAIL) && config_completion_notify && u->nickname)
		tabnick_add(u->nickname);
	if (!strcasecmp(status, EKG_STATUS_AWAY) && (config_completion_notify & 4) && u->nickname)
		tabnick_add(u->nickname);
	if (!strcasecmp(status, EKG_STATUS_NA) && (config_completion_notify & 2) && u->nickname)
		tabnick_remove(u->nickname);

	/* je¶li ma³o wa¿na zmiana stanu... */
	if (config_display_notify == 2 && strcasecmp(u->status, EKG_STATUS_NA)) {
		/* je¶li na zajêty, ignorujemy */
		if (!strcasecmp(status, EKG_STATUS_AWAY))
			goto notify_plugins;

		/* je¶li na dostêpny, ignorujemy */
		if (!strcasecmp(status, EKG_STATUS_AVAIL))
			goto notify_plugins;
	}

	/* daj znaæ d¼wiêkiem... */
	if (config_beep && config_beep_notify)
		query_emit(NULL, "beep");

	/* ...i muzyczk± */
	if (config_sound_notify_file)
		play_sound(config_sound_notify_file);

	/* wy¶wietlaæ na ekranie? */
	if (!config_display_notify)
		goto notify_plugins;

	/* poka¿ */
	if (u->nickname) {
		char format[100];

		snprintf(format, sizeof(format), "status_%s%s", status, (descr) ? "_descr" : "");
		print_window(u->nickname, session_find(session), 0, format, format_user(session, uid), (u->first_name) ? u->first_name : u->nickname, descr);
	}

notify_plugins:
	xfree(u->status);
	u->status = xstrdup(status);
	xfree(u->descr);
	u->descr = xstrdup(descr);
		
	query_emit(NULL, "userlist-changed", __session, __uid);

	return 0;
}

/*
 * message_print()
 *
 * wy¶wietla wiadomo¶æ w odpowiednim oknie i w odpowiedniej postaci.
 */
void message_print(const char *session, const char *sender, const char **rcpts, const char *__text, const uint32_t *format, time_t sent, int class, const char *seq)
{
	char *class_str = "message", timestamp[100], *t = NULL, *text = xstrdup(__text);
	const char *target = sender, *user;

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
	}

	/* próbujemy odszyfrowaæ wiadomo¶æ */
	{
		char *__session = xstrdup(session);
		char *__sender = xstrdup(sender);
		char *__message = xstrdup(text);
		int __decrypted = 0;

		do
			query_emit(NULL, "message-decrypt", &__session, &__sender, &__message, &__decrypted, NULL);
		while (__decrypted);

		if (__decrypted) {
			xfree(text);
			text = __message;
			__message = NULL;
		}

		xfree(__session);
		xfree(__sender);
		xfree(__message);
	}

	/* dodajemy kolorki do tekstu */
	if (format) {
		string_t s = string_init("");
		const char *attrmap;
		int i;

		if (config_display_color_map && strlen(config_display_color_map) == 8)
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
		char tmp[100], *timestamp_type = "timestamp";
		struct tm *tm_now, *tm_msg;
		time_t now;

		now = time(NULL);
		tm_now = localtime(&now);
		tm_msg = localtime(&sent);

		if (sent - config_time_deviation <= now && now <= sent + config_time_deviation)
			timestamp_type = "timestamp_now";
		else if (tm_now->tm_yday == tm_msg->tm_yday)
			timestamp_type = "timestamp_today";

		snprintf(tmp, sizeof(tmp), "%s_%s", class_str, timestamp_type);
		strftime(timestamp, sizeof(timestamp), format_find(tmp), tm_msg);
	}

	user = (class != EKG_MSGCLASS_SENT) ? format_user(session, sender) : session_format_n(sender);

	print_window(target, session_find(session), (class == EKG_MSGCLASS_CHAT), class_str, user, timestamp, text);

	xfree(t);
}

/*
 * protocol_message()
 */
int protocol_message(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**), *session = *__session;
	char **__uid = va_arg(ap, char**), *uid = *__uid;
	char ***__rcpts = va_arg(ap, char***), **rcpts = *__rcpts;
	char **__text = va_arg(ap, char**), *text = *__text;
	uint32_t **__format = va_arg(ap, uint32_t**), *format = *__format;
	time_t *__sent = va_arg(ap, time_t*), sent = *__sent;
	int *__class = va_arg(ap, int*), class = *__class;
	char **__seq = va_arg(ap, char**), *seq = *__seq;

	message_print(session, uid, (const char**) rcpts, text, format, sent, class, seq);

	return 0;
}

/*
 * protocol_message_ack()
 */
int protocol_message_ack(void *data, va_list ap)
{
	char **p_session = va_arg(ap, char**), *session = *p_session;
	char **p_rcpt = va_arg(ap, char**), *rcpt = *p_rcpt;
	char **p_seq = va_arg(ap, char**), *seq = *p_seq;
	char **p_status = va_arg(ap, char**), *status = *p_status;
	char format[100];
	userlist_t *u = userlist_find(session, rcpt);
	const char *target = (u && u->nickname) ? u->nickname : rcpt;
	int display = 0;

	snprintf(format, sizeof(format), "ack_%s", status);

	msg_queue_remove_seq(seq);
	
	if (config_display_ack == 1)
		display = 1;

	if (!strcmp(status, "delivered") && config_display_ack == 2)
		display = 1;

	if (!strcmp(status, "queued") && config_display_ack == 3)
		display = 1;

	if (display)
		print_window(target, session_find(session), 0, format, format_user(session, rcpt));

	return 0;
}

dcc_t *dcc_add(const char *uid, dcc_type_t type, void *priv)
{
	dcc_t *d = xmalloc(sizeof(dcc_t));
	int id = 1, id_ok = 0;
	list_t l;

	memset(d, 0, sizeof(dcc_t));

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

PROPERTY_MISC(dcc, close_handler, dcc_close_handler_t, NULL);
PROPERTY_STRING(dcc, filename);
PROPERTY_INT(dcc, offset, int);
PROPERTY_INT(dcc, size, int);
PROPERTY_STRING_GET(dcc, uid);
PROPERTY_INT_GET(dcc, id, int);
PROPERTY_PRIVATE(dcc);
PROPERTY_INT_GET(dcc, started, time_t);
PROPERTY_INT(dcc, active, int);
PROPERTY_INT(dcc, type, dcc_type_t);

