/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                2003 Adam Czerwiski <acze@acze.net>
 * 		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
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

#ifdef HAVE_JPEGLIB_H
#  include <jpeglib.h>
#endif

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
#include "token.h"

COMMAND(gg_command_connect)
{
	gg_private_t *g = session_private_get(session);
	uin_t uin = (session) ? atoi(session->uid + 3) : 0;
	char *password = (char *) session_get(session, "password");
	int ret = 0;
	
	if (!session_check(session, 0, "gg") || !g) {
		printq("invalid_session");
		ret = -1;
		goto end;
	}

	if (!xstrcasecmp(name, "disconnect") || (!xstrcasecmp(name, "reconnect") && session_connected_get(session))) {
		if (!g->sess) {
			printq("not_connected", session_name(session));
		} else {
			char *__session = xstrdup(session->uid);
			char *__reason = xstrdup(params[0]);
			int __type = EKG_DISCONNECT_USER;

			if (__reason) {
                		char *tmp = xstrdup(__reason);

	                        if (tmp && !xstrcmp(tmp, "-")) {
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

	if (!xstrcasecmp(name, "connect") || !xstrcasecmp(name, "reconnect")) {
		struct gg_login_params p;
		const char *tmp, *local_ip = session_get(session, "local_ip");
		int tmpi;
                int _status = gg_text_to_status(session_status_get(session), session_descr_get(session));
                const char *realserver = session_get(session, "server");
                int port = session_int_get(session, "port");

		if (g->sess) {
			printq((g->sess->state == GG_STATE_CONNECTED) ? "already_connected" : "during_connect", session_name(session));
			ret = -1;
			goto end;
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
#else
                 	gg_local_ip = inet_addr(local_ip);
#endif
        	}


		if (!uin || !password) {
			printq("no_config");
			ret = -1;
			goto end;
		}

		printq("connecting", session_name(session));

		memset(&p, 0, sizeof(p));

		if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
			session_status_set(session, EKG_STATUS_AVAIL);

		
		/* dcc */
		if (gg_config_dcc) {
			gg_dcc_socket_close();
	
                        if (!gg_config_dcc_ip || !xstrcasecmp(gg_config_dcc_ip, "auto")) {
                                gg_dcc_ip = inet_addr("255.255.255.255");
                        } else {
                                if (inet_addr(gg_config_dcc_ip) != INADDR_NONE)
                                        gg_dcc_ip = inet_addr(gg_config_dcc_ip);
                                else {
                                        print("dcc_invalid_ip");
					gg_config_dcc_ip = NULL;
                                        gg_dcc_ip = 0;
                                }
                        }

			gg_dcc_port = gg_config_dcc_port;
			
			gg_dcc_socket_open(gg_config_dcc_port);
		} 

		p.uin = uin;
		p.password = (char*) password;

                _status = GG_S(_status);
                if (session_int_get(session, "private"))
                        _status |= GG_STATUS_FRIENDS_MASK;

		p.status = _status;
		p.status_descr = (char*) session_descr_get(session);
		p.async = 1;

		if ((tmpi = session_int_get(session, "protocol")) != -1)
			p.protocol_version = tmpi;

		if ((tmpi = session_int_get(session, "last_sysmsg")) != -1)
			p.last_sysmsg = tmpi;

		if (realserver) {
			in_addr_t tmp_in;
			
			if ((tmp_in = inet_addr(realserver)) != INADDR_NONE)
				p.server_addr = inet_addr(realserver);
			else {
				print("inet_addr_failed", session_name(session));
				ret = -1;
				goto end;
			}
		}

		if ((port < 1) || (port > 65535)) {
			print("port_number_error", session_name(session));
			ret = -1;
			goto end;
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
		
			if (!auth[0] || !xstrcmp(auth[0], "")) {
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
			char *fwd = xstrdup(tmp), *colon = xstrchr(fwd, ':');

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

end:
	return ret;
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

	if (!xstrcasecmp(name, "away")) {
		session_status_set(session, EKG_STATUS_AWAY);
		df = "away"; f = "away"; fd = "away_descr";
		session_unidle(session);
		goto change;
	}

	if (!xstrcasecmp(name, "_autoaway")) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		df = "away"; f = "auto_away"; fd = "auto_away_descr";
		goto change;
	}

	if (!xstrcasecmp(name, "back")) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = "back"; f = "back"; fd = "back_descr";
		session_unidle(session);
		goto change;
	}

	if (!xstrcasecmp(name, "_autoback")) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = "back"; f = "auto_back"; fd = "auto_back_descr";
		session_unidle(session);
		goto change;
	}

	if (!xstrcasecmp(name, "invisible")) {
		session_status_set(session, EKG_STATUS_INVISIBLE);
		df = "quit"; f = "invisible"; fd = "invisible_descr";
		session_unidle(session);
		goto change;
	}

	return -1;

change:
	if (params[0]) {
		if (xstrlen(params[0]) > GG_STATUS_DESCR_MAXSIZE && config_reason_limit) {
			char *descr_poss = xstrndup(params[0], GG_STATUS_DESCR_MAXSIZE);
			char *descr_not_poss = xstrdup(params[0] + GG_STATUS_DESCR_MAXSIZE);

			printq("descr_too_long", itoa(xstrlen(params[0]) - GG_STATUS_DESCR_MAXSIZE), descr_poss, descr_not_poss);

			xfree(descr_poss);
			xfree(descr_not_poss);

			return -1;
		}

		session_descr_set(session, (!xstrcmp(params[0], "-")) ? NULL : params[0]);
	} else {
		char *tmp;

		if ((tmp = ekg_draw_descr(df))) {
			session_descr_set(session, tmp);
			xfree(tmp);
		}
	}

	reason_changed = 1;

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

	if (descr) {
                int _status = gg_text_to_status(status, descr);

                _status = GG_S(_status);
		if (session_int_get(session, "private"))
	                _status |= GG_STATUS_FRIENDS_MASK;

		gg_change_status_descr(g->sess, _status, descr);
	} else {
                int _status = gg_text_to_status(status, NULL);

                _status = GG_S(_status);
		if (session_int_get(session, "private"))
	                _status |= GG_STATUS_FRIENDS_MASK;

		gg_change_status(g->sess, _status);
	}

	xfree(descr);

	return 0;
}
	
COMMAND(gg_command_msg)
{
	int count, valid = 0, chat, secure = 0, formatlen = 0;
	char **nicks = NULL, *nick = NULL, **p = NULL, *add_send = NULL;
	unsigned char *msg = NULL, *raw_msg = NULL, *format = NULL;
	const char *seq;
	uint32_t *ekg_format = NULL;
	userlist_t *u;
	gg_private_t *g = session_private_get(session);

	chat = (xstrcasecmp(name, "msg"));

        if (!session_check(session, 1, "gg")) {
                printq("invalid_session");
                return -1;
        }

	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		return -1;
	}
	
	session_unidle(session);

        if (!xstrcmp(params[0], "*")) {
		if (msg_all(session, name, params[1]) == -1)
			printq("list_empty");
		return 0;
	}
	
	nick = xstrdup(params[0]);

	if ((*nick == '@' || xstrchr(nick, ',')) && chat) {
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

					if (!xstrcasecmp(g->name, tmp[i] + 1)) {
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

	if (config_split_messages && xstrlen(params[1]) > 1989) {
		int i, len = xstrlen(params[1]);
		
		for (i = 1; i * 1989 <= len; i++) {
			char *tmp = (i != len) ? xstrndup(params[1] + (i - 1) * 1989, 1989) : xstrdup(params[1] + (i - 1) * 1989);
			char *cmd = saprintf("/%s %s %s", name, params[0], tmp);

			command_exec(target, session, cmd, 0);
			debug("cmd: %s\n", cmd);
			
			xfree(cmd);
			xfree(tmp);
		}
	
		return 0;

	} else if (xstrlen(params[1]) > 1989) {
              printq("message_too_long");
	}

	msg = xstrmid(params[1], 0, 1989);
	ekg_format = ekg_sent_message_format(msg);

	/* analizê tekstu zrobimy w osobnym bloku dla porz±dku */
	{
		unsigned char attr = 0, last_attr = 0;
		const unsigned char *p = msg, *end = p + xstrlen(p);
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

		if (!xstrcmp(*p, ""))
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

		for (p = nicks; *p; p++) {
			const char *uid;
			
			if (!(uid = get_uid(session, *p)))
				continue;

			if (xstrncmp(uid, "gg:", 3))
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

	if (valid && !quiet) {
		char **rcpts = xmalloc(sizeof(char *) * 2);
		const int class = (chat) ? EKG_MSGCLASS_SENT_CHAT : EKG_MSGCLASS_SENT;
		const int ekgbeep = EKG_TRY_BEEP;
		char *me = xstrdup(session_uid_get(session));
		const time_t sent = time(NULL);
		
		rcpts[0] = xstrdup(nick);
		rcpts[1] = NULL;
		
		query_emit(NULL, "protocol-message", &me, &me, &rcpts, &raw_msg, &ekg_format, &sent, &class, &seq, &ekgbeep, NULL);

		xfree(me);
		xfree(rcpts[0]);
		xfree(rcpts);
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
	int unblock_all = (params[0] && !xstrcmp(params[0], "*"));
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

/*
 * token_check()
 *
 * funkcja sprawdza czy w danym miejscu znajduje siê zaproponowany znaczek
 *
 *  - n - numer od 0 do 15 (znaczki od 0 do f)
 *  - x, y - wspó³rzêdne znaczka w tablicy ocr
 */
static int token_check(int nr, int x, int y, const char *ocr, int maxx, int maxy)
{
        int i;

        for (i = nr * token_char_height; i < (nr + 1) * token_char_height; i++, y++) {
                int j, xx = x;

                for (j = 0; token_id[i][j] && j + xx < maxx; j++, xx++) {
                        if (token_id[i][j] != ocr[y * (maxx + 1) + xx])
                                return 0;
                }
        }

        return 1;
}

/*
 * token_ocr()
 *
 * zwraca tre¶æ tokenu
 */
char *token_ocr(const char *ocr, int width, int height, int length)
{
        int x, y, count = 0;
        char *token;

        token = xmalloc(length + 1);
        memset(token, 0, length + 1);
        for (x = 0; x < width; x++) {
                for (y = 0; y < height - token_char_height; y++) {
                        int result = 0, token_part = 0;

                        do
                                result = token_check(token_part++, x, y, ocr, width, height);
                        while (!result && token_part < 16);

                        if (result && count < length)
                                token[count++] = token_id_char[token_part - 1];
                }
        }

        if (count == length)
                return token;

        xfree(token);

        return NULL;
}


static void gg_handle_token(int type, int fd, int watch, void *data)
{
        struct gg_http *h = data;
	struct gg_token *t = NULL;
	char *file = NULL;

       if (type == 2) {
                debug("[gg] gg_handle_token() timeout\n");
                print("register_timeout");
                goto fail;
        }

        if (type != 0)
                return;

	if (!h)
		return;
	
	if (gg_token_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("gg_token_failed", gg_http_error_string(h->error));
		goto fail;
	}

	if (h->state != GG_STATE_DONE) {
                watch_t *w = watch_add(&gg_plugin, h->fd, h->check, 0, gg_handle_token, h);
                watch_timeout_set(w, h->timeout);

		return;
	}

	if (!(t = h->data) || !h->body) {
		print("gg_token_failed", gg_http_error_string(h->error));
		goto fail;
	}

	xfree(last_tokenid);
	last_tokenid = xstrdup(t->tokenid);

#ifdef HAVE_MKSTEMP

	file = saprintf("%s/token.XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");

	if ((fd = mkstemp(file)) == -1) {
		print("gg_token_failed", strerror(errno));
		goto fail;
	}

	if ((write(fd, h->body, h->body_size) != h->body_size) || (close(fd) != 0)) {
		print("gg_token_failed", strerror(errno));
		close(fd);
		unlink(file);
		goto fail;
	}

#ifdef HAVE_LIBJPEG
	if (gg_config_display_token) {
		struct jpeg_decompress_struct j;
		struct jpeg_error_mgr e;
		JSAMPROW buf[1];
		int size;
		char *token, *tmp;
		FILE *f;
		int h = 0;

		if (!(f = fopen(file, "rb"))) {
			print("gg_token_failed", strerror(errno));
			goto fail;
		}

		j.err = jpeg_std_error(&e);
		jpeg_create_decompress(&j);
		jpeg_stdio_src(&j, f);
		jpeg_read_header(&j, TRUE);
		jpeg_start_decompress(&j);

		size = j.output_width * j.output_components;
		buf[0] = xmalloc(size);
                
                token = xmalloc((j.output_width + 1) * j.output_height);
		
		while (j.output_scanline < j.output_height) {
			int i;

			jpeg_read_scanlines(&j, buf, 1);

			for (i = 0; i < j.output_width; i++, h++)
				token[h] = (buf[0][i*3] + buf[0][i*3+1] + buf[0][i*3+2] < 384) ? '#' : '.';
			
			token[h++] = 0;
		}

		if (!(tmp = token_ocr(token, j.output_width, j.output_height, t->length))) {
			int i;

			for (i = 0; i < j.output_height; i++)
				print("gg_token_body", token[i * (j.output_width + 1)]);
		} else {
			print("gg_token_ocr", tmp);
			xfree(tmp);
		}

		xfree(token);

		jpeg_finish_decompress(&j);
		jpeg_destroy_decompress(&j);

		xfree(buf[0]);
		fclose(f);
		
		unlink(file);
	} else {
                char *file2 = saprintf("%s.jpg", file);

                if (rename(file, file2) == -1)
                        print("gg_token", file);
                else
                        print("gg_token", file2);

                xfree(file2);
	}
#else	/* HAVE_LIBJPEG */
	{
		char *file2 = saprintf("%s.jpg", file);

		if (rename(file, file2) == -1)
			print("gg_token", file);
		else
			print("gg_token", file2);

		xfree(file2);
	}
#endif	/* HAVE_LIBJPEG */

#else	/* HAVE_MKSTEMP */
	print("gg_token_unsupported");
#endif	/* HAVE_MKSTEMP */

	xfree(file);

fail:
	watch_remove(&gg_plugin, h->fd, h->check);
	gg_token_free(h);
}

COMMAND(gg_command_token)
{
        struct gg_http *h;
	watch_t *w;

        if (!(h = gg_token(1))) {
                printq("gg_token_failed", strerror(errno));
                return -1;
        }

        w = watch_add(&gg_plugin, h->fd, h->check, 0, gg_handle_token, h);
        watch_timeout_set(w, h->timeout);

        return 0;
}

COMMAND(gg_command_modify)
{
	userlist_t *u;
	char **argv = NULL;
	int i, res = 0, modified = 0;

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!(u = userlist_find(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}

	if (params[1])
		argv = array_make(params[1], " \t", 0, 1, 1);

	for (i = 0; argv && argv[i]; i++) {
		
		if (match_arg(argv[i], 'f', "first", 2) && argv[i + 1]) {
			xfree(u->first_name);
			u->first_name = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (match_arg(argv[i], 'l', "last", 2) && argv[i + 1]) {
			xfree(u->last_name);
			u->last_name = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (match_arg(argv[i], 'n', "nickname", 2) && argv[i + 1]) {
			char *tmp1, *tmp2;

			tmp1 = xstrdup(u->nickname);
			tmp2 = xstrdup(argv[++i]);
			query_emit(NULL, "userlist-renamed", &tmp1, &tmp2);
			xfree(tmp1);
				
			xfree(u->nickname);
			u->nickname = tmp2;
			
			modified = 1;
			continue;
		}
		
		if ((match_arg(argv[i], 'p', "phone", 2) || match_arg(argv[i], 'm', "mobile", 2)) && argv[i + 1]) {
			xfree(u->mobile);
			u->mobile = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (match_arg(argv[i], 'g', "group", 2) && argv[i + 1]) {
			char **tmp = array_make(argv[++i], ",", 0, 1, 1);
			int x, off;	/* je¶li zaczyna siê od '@', pomijamy pierwszy znak */
			
			for (x = 0; tmp[x]; x++)
				switch (*tmp[x]) {
					case '-':
						off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

						if (ekg_group_member(u, tmp[x] + 1 + off)) {
							ekg_group_remove(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							printq("group_member_not_yet", format_user(session, u->uid), tmp[x] + 1);
							if (!modified)
								modified = -1;
						}
						break;
					case '+':
						off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

						if (!ekg_group_member(u, tmp[x] + 1 + off)) {
							ekg_group_add(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							printq("group_member_already", format_user(session, u->uid), tmp[x] + 1);
							if (!modified)
								modified = -1;
						}
						break;
					default:
						off = (tmp[x][0] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

						if (!ekg_group_member(u, tmp[x] + off)) {
							ekg_group_add(u, tmp[x] + off);
							modified = 1;
						} else {
							printq("group_member_already", format_user(session, u->uid), tmp[x]);
							if (!modified)
								modified = -1;
						}
				}

			array_free(tmp);
			continue;
		}
		
		if (match_arg(argv[i], 'u', "uid", 2) && argv[i + 1]) {
			userlist_t *existing;
			char *tmp;

			if (!valid_uid(argv[i + 1])) {
				printq("invalid_uid");
				array_free(argv);
				return -1;
			}

			if ((existing = userlist_find(session, argv[i + 1]))) {
				if (existing->nickname) {
					printq("user_exists_other", argv[i], format_user(session, existing->uid), session_name(session));
					array_free(argv);
					return -1;
				} else {
					char *egroups = group_to_string(existing->groups, 1, 0);
					
					if (egroups) {
						char **arr = array_make(egroups, ",", 0, 0, 0);
						int i;

						for (i = 0; arr[i]; i++)
							ekg_group_add(u, arr[i]);

						array_free(arr);
					}

					userlist_remove(session, existing);
				}
			}

			tmp = xstrdup(u->uid);
			query_emit(NULL, "userlist-removed", &tmp);
			xfree(tmp);

			userlist_clear_status(session, u->uid);

			tmp = xstrdup(argv[i + 1]);
			query_emit(NULL, "userlist-added", &tmp);

			xfree(u->uid);
			u->uid = tmp;

			modified = 1;
			continue;
		}

		if (match_arg(argv[i], 'o', "offline", 2)) {
			query_emit(NULL, "user-offline", &u, &session);
			modified = 2;
			continue;
		}

		if (match_arg(argv[i], 'O', "online", 2)) {
			query_emit(NULL, "user-online", &u, &session);
			modified = 2;
			continue;
		} 
		
		printq("invalid_params", name);
		array_free(argv);
		return -1;
	}

	if (xstrcasecmp(name, "add")) {
		switch (modified) {
			case 0:
				printq("not_enough_params", name);
				res = -1;
				break;
			case 1:
				printq("modify_done", params[0]);
			case 2:
				config_changed = 1;
				break;
		}
	} else
		config_changed = 1;

	array_free(argv);

	return res;
}


void gg_register_commands()
{
	command_add(&gg_plugin, "gg:connect", "?", gg_command_connect, 0, NULL);
	command_add(&gg_plugin, "gg:disconnect", "?", gg_command_connect, 0, NULL);
	command_add(&gg_plugin, "gg:reconnect", NULL, gg_command_connect, 0, NULL);
	command_add(&gg_plugin, "gg:msg", "uUC ?", gg_command_msg, 0, NULL);
	command_add(&gg_plugin, "gg:chat", "uUC ?", gg_command_msg, 0, NULL);
	command_add(&gg_plugin, "gg:", "?", gg_command_inline_msg, 0, NULL);
	command_add(&gg_plugin, "gg:_descr", "r", gg_command_away, 0, NULL);
	command_add(&gg_plugin, "gg:away", "r", gg_command_away, 0, NULL);
	command_add(&gg_plugin, "gg:_autoaway", "?", gg_command_away, 0, NULL);
	command_add(&gg_plugin, "gg:back", "r", gg_command_away, 0, NULL);
	command_add(&gg_plugin, "gg:_autoback", "?", gg_command_away, 0, NULL);
	command_add(&gg_plugin, "gg:invisible", "r", gg_command_away, 0, NULL);
	command_add(&gg_plugin, "gg:block", "uUC ?", gg_command_block, 0, NULL);
	command_add(&gg_plugin, "gg:unblock", "b ?", gg_command_unblock, 0, NULL);
	command_add(&gg_plugin, "gg:modify", "Uu ?", gg_command_modify, 0, NULL);
	command_add(&gg_plugin, "gg:remind", "?", gg_command_remind, 0, NULL);
	command_add(&gg_plugin, "gg:register", "? ? ?", gg_command_register, 0, NULL);
        command_add(&gg_plugin, "gg:token", NULL, gg_command_token, 0, NULL);
	command_add(&gg_plugin, "gg:unregister", "? ? ?", gg_command_unregister, 0, NULL);
	command_add(&gg_plugin, "gg:passwd", "? ?", gg_command_passwd, 0, NULL);
	command_add(&gg_plugin, "gg:userlist", "p ?", gg_command_list, 0,  
	  "-c --clear -g --get -p --put");
	command_add(&gg_plugin, "gg:find", "puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC",
	 gg_command_find, 0, "-u --uin -f --first -l --last -n --nick -c --city -b --botn -a --active -F --female -M --male -s --start -A --all -S --stop");
	
	command_add(&gg_plugin, "gg:change", "p", gg_command_change, 0,
	  "-f --first -l --last -n --nick -b --born -c --city -N --familyname -C --familycity -F --female -M --male");

	command_add(&gg_plugin, "gg:dcc", "p uU f ?", gg_command_dcc, 0, 
	 "send rsend get resume rvoice voice close list");

}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
