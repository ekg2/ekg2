/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "emoticons.h"
#include "events.h"
#include "log.h"
#include "msgqueue.h"
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "voice.h"
#include "xmalloc.h"

static int hide_notavail = 0;	/* czy ma ukrywaæ niedostêpnych -- tylko zaraz po po³±czeniu */

static int dcc_limit_time = 0;	/* czas pierwszego liczonego po³±czenia */
static int dcc_limit_count = 0;	/* ilo¶æ po³±czeñ od ostatniego razu */


/*
 * print_message()
 *
 * funkcja ³adnie formatuje tre¶æ wiadomo¶ci, zawija linijki, wy¶wietla
 * kolorowe ramki i takie tam.
 *
 *  - e - zdarzenie wiadomo¶ci
 *  - u - wpis u¿ytkownika w userli¶cie
 *  - chat - rodzaj wiadomo¶ci (0 - msg, 1 - chat, 2 - sysmsg)
 *  - secure - czy wiadomo¶æ jest bezpieczna
 *
 * nie zwraca niczego. efekt widaæ na ekranie.
 */
void print_message(struct gg_event *e, struct userlist *u, int chat, int secure)
{
	int width, next_width, i, j, mem_width = 0;
	time_t tt, t = e->event.msg.time;
	int separate = (e->event.msg.sender != config_uin || chat == 3);
	int timestamp_type = 0;
	char *mesg, *buf, *line, *next, *format = NULL, *format_first = "";
	char *next_format = NULL, *head = NULL, *foot = NULL;
	char *timestamp = NULL, *save, *secure_label = NULL;
	char *line_width = NULL, timestr[100], *target, *cname;
	char *formatmap = NULL;
	struct tm *tm;
	int now_days;
	struct conference *c = NULL;

	/* tworzymy mapê formatowanego tekstu. dla ka¿dego znaku wiadomo¶ci
	 * zapisujemy jeden znak koloru z docs/themes.txt lub \0 je¶li nie
	 * trzeba niczego zmieniaæ. */

	if (e->event.msg.formats && e->event.msg.formats_length) {
		unsigned char *p = e->event.msg.formats;
		char last_attr = 0, *attrmap;

		if (config_display_color_map && strlen(config_display_color_map) == 8)
			attrmap = config_display_color_map;
		else
			attrmap = "nTgGbBrR";

		formatmap = xcalloc(1, strlen(e->event.msg.message));
		
		for (i = 0; i < e->event.msg.formats_length; ) {
			int pos = p[i] + p[i + 1] * 256;

			if ((p[i + 2] & GG_FONT_COLOR)) {
				formatmap[pos] = color_map(p[i + 3], p[i + 4], p[i + 5]);				
				if (formatmap[pos] == 'k')
					formatmap[pos] = 'n';
			}

			if ((p[i + 2] & 7) || !p[i + 2] || !(p[i + 2] && GG_FONT_COLOR) || ((p[i + 2] & GG_FONT_COLOR) && !p[i + 3] && !p[i + 4] && !p[i + 5]))
				formatmap[pos] = attrmap[p[i + 2] & 7];

			i += (p[i + 2] & GG_FONT_COLOR) ? 6 : 3;
		}

		/* teraz powtarzamy formaty tam, gdzie jest przej¶cie do
		 * nowej linii i odstêpy. dziêki temu oszczêdzamy sobie
		 * mieszania ni¿ej w kodzie. */

		for (i = 0; i < strlen(e->event.msg.message); i++) {
			if (formatmap[i])
				last_attr = formatmap[i];

			if (i > 0 && strchr(" \n", e->event.msg.message[i - 1]))
				formatmap[i] = last_attr;
		}
	}

	if (secure)
		secure_label = format_string(format_find("secure"));
	
	if (e->event.msg.recipients) {
		c = conference_find_by_uins(e->event.msg.sender, 
			e->event.msg.recipients, e->event.msg.recipients_count, 0);

		if (!c) {
			string_t tmp = string_init(NULL);
			int first = 0, i;

			for (i = 0; i < e->event.msg.recipients_count; i++) {
				if (first++) 
					string_append_c(tmp, ',');

			        string_append(tmp, itoa(e->event.msg.recipients[i]));
			}

			string_append_c(tmp, ' ');
			string_append(tmp, itoa(e->event.msg.sender));

			c = conference_create(tmp->str);

			string_free(tmp, 1);
		}
		
		if (c)
			target = xstrdup(c->name);
		else
			target = xstrdup((chat == 2) ? "__status" : ((u && u->display) ? u->display : itoa(e->event.msg.sender)));
	} else
	        target = xstrdup((chat == 2) ? "__status" : ((u && u->display) ? u->display : itoa(e->event.msg.sender)));

	cname = (c ? c->name : "");

	tt = time(NULL);
	tm = localtime(&tt);
	now_days = tm->tm_yday;

	tm = localtime(&e->event.msg.time);

	if (t - config_time_deviation <= tt && tt <= t + config_time_deviation)
		timestamp_type = 2;
	else if (now_days == tm->tm_yday)
		timestamp_type = 1;
	
	switch (chat) {
		case 0:
			format = "message_line";
			format_first = (c) ? "message_conference_line_first" : "message_line_first";
			line_width = "message_line_width";
			head = (c) ? "message_conference_header" : "message_header";
			foot = "message_footer";

			if (timestamp_type == 1)
				timestamp = "message_timestamp_today";
			else if (timestamp_type == 2)
				timestamp = "message_timestamp_now";
			else
				timestamp = "message_timestamp";
			
			break;
			
		case 1:
			format = "chat_line"; 
			format_first = (c) ? "chat_conference_line_first" : "chat_line_first";
			line_width = "chat_line_width";
			head = (c) ? "chat_conference_header" : "chat_header";
			foot = "chat_footer";
			
			if (timestamp_type == 1)
				timestamp = "chat_timestamp_today";
			else if (timestamp_type == 2)
				timestamp = "chat_timestamp_now";
			else
				timestamp = "chat_timestamp";

			break;
			
		case 2:
			format = "sysmsg_line"; 
			line_width = "sysmsg_line_width";
			head = "sysmsg_header";
			foot = "sysmsg_footer";
			break;
			
		case 3:
		case 4:
			format = "sent_line"; 
			format_first = (c) ? "sent_conference_line_first" : "sent_line_first";
			line_width = "sent_line_width";
			head = (c) ? "sent_conference_header" : "sent_header";
			foot = "sent_footer";
			
			if (timestamp_type == 1)
				timestamp = "sent_timestamp_today";
			else if (timestamp_type == 2)
				timestamp = "sent_timestamp_now";
			else
				timestamp = "sent_timestamp";

			break;
	}	

	/* je¿eli chcemy, dodajemy do bufora ,,last'' wiadomo¶æ... */
	if (config_last & 3 && (chat >= 0 && chat <= 2))
	       last_add(0, e->event.msg.sender, tt, e->event.msg.time, e->event.msg.message);
	
	strftime(timestr, sizeof(timestr), format_find(timestamp), tm);

	if (!(width = atoi(format_find(line_width))))
		width = ui_screen_width - 2;

	if (width < 0) {
		width = ui_screen_width + width;

		if (config_timestamp)
			width -= strlen(config_timestamp) - 6;
	}

	next_width = width;
	
	if (!strcmp(format_find(format_first), "")) {
		print_window(target, separate, head, format_user(e->event.msg.sender), timestr, cname, (secure) ? secure_label : "");
		next_format = format;
		mem_width = width + 1;
	} else {
		char *tmp, *p;

		next_format = format;
		format = format_first;

		/* zmniejsz d³ugo¶æ pierwszej linii o d³ugo¶æ prefiksu z rozmówc±, timestampem itd. */
		tmp = format_string(format_find(format), "", format_user(e->event.msg.sender), timestr, cname);
		mem_width = width + strlen(tmp);
		for (p = tmp; *p && *p != '\n'; p++) {
			if (*p == 27) {
				/* pomiñ kolorki */
				while (*p && *p != 'm')
					p++;
			} else
				width--;
		}
		
		xfree(tmp);

		tmp = format_string(format_find(next_format), "", "", "", "");
		next_width -= strlen(tmp);
		xfree(tmp);
	}

	buf = xmalloc(mem_width);
	mesg = save = xstrdup(e->event.msg.message);

	for (i = 0; i < strlen(mesg); i++)	/* XXX ³adniejsze taby */
		if (mesg[i] == '\t')
			mesg[i] = ' ';
	
	while ((line = gg_get_line(&mesg))) {
		const char *last_format_ansi = "";
		int buf_offset;

		for (; strlen(line); line = next) {
			char *emotted = NULL, *formatted;

			if (strlen(line) <= width) {
				strcpy(buf, line);
				next = line + strlen(line);
			} else {
				int len = width;
				
				for (j = width; j; j--)
					if (line[j] == ' ') {
						len = j;
						break;
					}

				strncpy(buf, line, len);
				buf[len] = 0;
				next = line + len;

				while (*next == ' ')
					next++;
			}
			
			buf_offset = (int) (line - save);

			if (formatmap) {
				string_t s = string_init("");
				int i;

				string_append(s, last_format_ansi);

				for (i = 0; i < strlen(buf); i++) {
					if (formatmap[buf_offset + i]) {
						last_format_ansi = format_ansi(formatmap[buf_offset + i]);
						string_append(s, last_format_ansi);
					}

					string_append_c(s, buf[i]);
				}
				formatted = string_free(s, 0);
			} else
				formatted = xstrdup(buf);

			if (config_emoticons)
				emotted = emoticon_expand(formatted);

			print_window(target, separate, format, (emotted) ? emotted : formatted , format_user(e->event.msg.sender), timestr, cname);

			width = next_width;
			format = next_format;

			xfree(emotted);
			xfree(formatted);
		}
	}
	
	xfree(buf);
	xfree(save);
	xfree(secure_label);
	xfree(formatmap);

	if (!strcmp(format_find(format_first), ""))
		print_window(target, separate, foot);

	xfree(target);
}

/*
 * handle_msg()
 *
 * funkcja obs³uguje przychodz±ce wiadomo¶ci.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_msg(struct gg_event *e)
{
	struct userlist *u = userlist_find(e->event.msg.sender, NULL);
	int chat = ((e->event.msg.msgclass & 0x0f) == GG_CLASS_CHAT), secure = 0, hide = 0;

	if (!e->event.msg.message)
		return;

	if ((e->event.msg.msgclass & GG_CLASS_CTCP)) {
		list_t l;
		int dccs = 0;

		gg_debug(GG_DEBUG_MISC, "// ekg: received ctcp\n");

		for (l = watches; l; l = l->next) {
			struct gg_dcc *d = l->data;

			if (d->type == GG_SESSION_DCC)
				dccs++;
		}

		if (dccs > 50) {
			char *tmp = saprintf("/ignore %d", e->event.msg.sender);
			print_status("dcc_attack", format_user(e->event.msg.sender));
			command_exec(NULL, tmp, 0);
			xfree(tmp);

			return;
		}

		if (config_dcc && u) {
			struct gg_dcc *d;

                        if (!(d = gg_dcc_get_file(u->ip.s_addr, u->port, config_uin, e->event.msg.sender))) {
				print_status("dcc_error", strerror(errno));
				return;
			}

			list_add(&watches, d, 0);
		}

		return;
	}

#ifdef HAVE_OPENSSL
	/* XXX
	if (config_encryption) {
		char *msg = sim_message_decrypt(e->event.msg.message, e->event.msg.sender);

		if (msg) {
			strcpy(e->event.msg.message, msg);
			xfree(msg);
			secure = 1;
		} else
			gg_debug(GG_DEBUG_MISC, "// ekg: simlite decryption failed: %s\n", sim_strerror(sim_errno));
	}
	*/
#endif

	cp_to_iso(e->event.msg.message);
	
	if (e->event.msg.recipients_count) {
		struct conference *c = conference_find_by_uins(
			e->event.msg.sender, e->event.msg.recipients,
			e->event.msg.recipients_count, 0);

		if (c && c->ignore)
			return;
	}

	if (ignored_check(e->event.msg.sender) & IGNORE_MSG) {
		if (config_log_ignored)
			put_log(e->event.msg.sender, "%sign,%ld,%s,%s,%s,%s\n", (chat) ? "chatrecv" : "msgrecv", e->event.msg.sender, ((u && u->display) ? u->display : ""), log_timestamp(time(NULL)), log_timestamp(e->event.msg.time), e->event.msg.message);

		return;
	}

#ifdef HAVE_OPENSSL
	/* XXX
	if (config_encryption && !strncmp(e->event.msg.message, "-----BEGIN RSA PUBLIC KEY-----", 20)) {
		char *name;
		const char *target = ((u && u->display) ? u->display : itoa(e->event.msg.sender));
		FILE *f;

		print_window(target, 0, "key_public_received", format_user(e->event.msg.sender));	

		if (mkdir(prepare_path("keys", 1), 0700) && errno != EEXIST) {
			print_window(target, 0, "key_public_write_failed", strerror(errno));
			return;
		}

		name = saprintf("%s/%d.pem", prepare_path("keys", 0), e->event.msg.sender);

		if (!(f = fopen(name, "w"))) {
			print_window(target, 0, "key_public_write_failed", strerror(errno));
			xfree(name);
			return;
		}
		
		fprintf(f, "%s", e->event.msg.message);
		fclose(f);
		xfree(name);

		return;
	}
	*/
#endif
	
	if (e->event.msg.sender == 0) {
		if (e->event.msg.msgclass > config_last_sysmsg) {
			if (!hide)
				print_message(e, u, 2, 0);

			if (config_beep)
				ui_beep();
		    
			play_sound(config_sound_sysmsg_file);
			config_last_sysmsg = e->event.msg.msgclass;
			config_last_sysmsg_changed = 1;
		}

		return;
	}

	if (!(ignored_check(e->event.msg.sender) & IGNORE_EVENTS))
		event_check((chat) ? EVENT_CHAT : EVENT_MSG, e->event.msg.sender, e->event.msg.message);
			
	if (u && u->display)
		add_send_nick(u->display);
	else
		add_send_nick(itoa(e->event.msg.sender));

	if (!hide) {
		print_message(e, u, chat, secure);

		if (config_beep && ((chat) ? config_beep_chat : config_beep_msg))
			ui_beep();

		play_sound((chat) ? config_sound_chat_file : config_sound_msg_file);
	}

	put_log(e->event.msg.sender, "%s,%ld,%s,%s,%s,%s\n", (chat) ? "chatrecv" : "msgrecv", e->event.msg.sender, ((u && u->display) ? u->display : ""), log_timestamp(time(NULL)), log_timestamp(e->event.msg.time), e->event.msg.message);

	if (config_sms_away && GG_S_B(config_status) && config_sms_app && config_sms_number) {
		char *foo, sender[100];

		sms_away_add(e->event.msg.sender);

		if (sms_away_check(e->event.msg.sender)) {
			if (u && u->display)
				snprintf(sender, sizeof(sender), "%s/%u", u->display, u->uin);
			else
				snprintf(sender, sizeof(sender), "%u", e->event.msg.sender);

			if (config_sms_max_length && strlen(e->event.msg.message) > config_sms_max_length)
				e->event.msg.message[config_sms_max_length] = 0;

			if (e->event.msg.recipients_count)
				foo = format_string(format_find("sms_conf"), sender, e->event.msg.message);
			else
				foo = format_string(format_find((chat) ? "sms_chat" : "sms_msg"), sender, e->event.msg.message);

			/* niech nie wysy³a smsów, je¶li brakuje formatów */
			if (strcmp(foo, ""))
				send_sms(config_sms_number, foo, 0);
	
			xfree(foo);
		}
	}

	if (e->event.msg.formats_length > 0) {
		int i;
		
		gg_debug(GG_DEBUG_MISC, "// ekg: received formatting info (len=%d):", e->event.msg.formats_length);
		for (i = 0; i < e->event.msg.formats_length; i++)
			gg_debug(GG_DEBUG_MISC, " %.2x", ((unsigned char*)e->event.msg.formats)[i]);
		gg_debug(GG_DEBUG_MISC, "\n");
	}
}

/*
 * handle_ack()
 *
 * funkcja obs³uguje potwierdzenia wiadomo¶ci.
 *
 *  - e - opis zdarzenia.
 *
 * nie zwraca niczego.
 */
void handle_ack(struct gg_event *e)
{
	struct userlist *u = userlist_find(e->event.ack.recipient, NULL);
	int queued = (e->event.ack.status == GG_ACK_QUEUED), filtered = 0;
	const char *tmp, *target = ((u && u->display) ? u->display : itoa(e->event.ack.recipient));

	if (!e->event.ack.seq)	/* ignorujemy potwierdzenia ctcp */
		return;

	msg_queue_remove(e->event.ack.seq);

	if (!(ignored_check(e->event.ack.recipient) & IGNORE_EVENTS))
		event_check((queued) ? EVENT_QUEUED : EVENT_DELIVERED, e->event.ack.recipient, NULL);

	if (u && !queued && GG_S_NA(u->status) && !(ignored_check(u->uin) & IGNORE_STATUS)) {
		filtered = 1;
		print_window(target, 0, "ack_filtered", format_user(e->event.ack.recipient));
	}

	if (!config_display_ack)
		return;

	if (config_display_ack == 2 && queued)
		return;

	if (config_display_ack == 3 && !queued)
		return;

	if (!filtered) {
		tmp = queued ? "ack_queued" : "ack_delivered";
		print_window(target, 0, tmp, format_user(e->event.ack.recipient));
	}
}

/*
 * handle_voice()
 *
 * obs³uga danych przychodz±cych z urz±dzenia wej¶ciowego.
 *
 *  - c - struktura opisuj±ca urz±dzenie wej¶ciowe.
 *
 * brak.
 */
void handle_voice(struct gg_common *c)
{
#ifdef HAVE_VOIP
	list_t l;
	struct gg_dcc *d = NULL;
	char buf[GG_DCC_VOICE_FRAME_LENGTH_505];	/* d³u¿szy z buforów */
	int length = GG_DCC_VOICE_FRAME_LENGTH;
	
	for (l = transfers; l; l = l->next) {
		struct transfer *t = l->data;

		if (t->type == GG_SESSION_DCC_VOICE && t->dcc && (t->dcc->state == GG_STATE_READING_VOICE_HEADER || t->dcc->state == GG_STATE_READING_VOICE_SIZE || t->dcc->state == GG_STATE_READING_VOICE_DATA)) {
			d = t->dcc;
			length = (t->protocol >= 0x1b) ? GG_DCC_VOICE_FRAME_LENGTH_505 : GG_DCC_VOICE_FRAME_LENGTH;
			break;
		}
	}

	/* póki nie mamy po³±czenia, i tak czytamy z /dev/dsp */

	if (!d) {
		voice_record(buf, length, 1);	/* XXX b³êdy */
		return;
	} else {
		voice_record(buf, length, 0);	/* XXX b³êdy */
		if (config_audio_device && config_audio_device[0] != '-')
			gg_dcc_voice_send(d, buf, length);	/* XXX b³êdy */
	}
#endif /* HAVE_VOIP */
}

