/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                2003 Adam Czerwiski <acze@acze.net>
 * 		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 * 		  2006 Adam Mikuta <adamm@ekg2.org>
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

#ifdef HAVE_LIBGIF
# define GIF_OCR
#endif
#ifdef HAVE_LIBUNGIF
# define GIF_OCR
#endif

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

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
#include <setjmp.h>

#ifdef HAVE_JPEGLIB_H
#  include <jpeglib.h>
#endif
#ifdef HAVE_GIF_LIB_H
#  include <fcntl.h>    /* open() */
#  include <gif_lib.h>
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
#include "images.h"
#include "misc.h"
#include "pubdir.h"
#include "pubdir50.h"
#include "token.h"

COMMAND(gg_command_connect)
{
	PARUNI
	gg_private_t *g = session_private_get(session);
	uin_t uin = (session) ? atoi(session->uid + 3) : 0;
	int ret = 0;
	
	if (!xwcscasecmp(name, TEXT("disconnect")) || (!xwcscasecmp(name, TEXT("reconnect")))) {

	        /* if ,,reconnect'' timer exists we should stop doing */
	        if (timer_remove(&gg_plugin, "reconnect") == 0) {
			printq("auto_reconnect_removed", session_name(session));
	                return 0;
		}

		if (!g->sess) {
			printq("not_connected", session_name(session));
		} else {
			char *__session = xstrdup(session->uid);
			CHAR_T *__reason = xwcsdup(params[0]);
			int __type = EKG_DISCONNECT_USER;

			if (__reason) {
                		CHAR_T *tmp = xwcsdup(__reason);
				char *sreason = wcs_to_normal(tmp);
				unsigned char *tmp_ = NULL;		/* znaczki w cp1250 */

	                        if (!xwcscmp(tmp, TEXT("-"))) {
        	                        xfree(tmp);
                	                tmp = NULL;
                        	} else 
	                		tmp_ = gg_locale_to_cp(tmp);

          			if (config_keep_reason)
					session_descr_set(session, sreason);
				
				gg_change_status_descr(g->sess, GG_STATUS_NOT_AVAIL_DESCR, tmp_);
                		xfree(tmp);
				free_utf(sreason);
				free_utf(tmp_);
        		} else
  			        gg_change_status(g->sess, GG_STATUS_NOT_AVAIL);

			watch_remove(&gg_plugin, g->sess->fd, g->sess->check);
			
			gg_logoff(g->sess);
			gg_free_session(g->sess);
			g->sess = NULL;
			session_connected_set(session, 0);

			{
				CHAR_T *session = normal_to_wcs(__session);
				query_emit(NULL, "wcs_protocol-disconnected", &session, &__reason, &__type, NULL);
				free_utf(session);
			}

			xfree(__session);
			xfree(__reason);
		}
	}

	if (!xwcscasecmp(name, TEXT("connect")) || !xwcscasecmp(name, TEXT("reconnect"))) {
		struct gg_login_params p;
		const char *tmp, *local_ip = session_get(session, "local_ip");
		int tmpi;
                int _status = gg_text_to_status(session_status_get(session), session_descr_get(session));
                const char *realserver = session_get(session, "server");
                int port = session_int_get(session, "port");
		char *password = (char *) session_get(session, "password");

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
			wcs_printq("no_config");
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
		p.image_size = gg_config_image_size;

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

		xfree(gg_proxy_host);
		xfree(gg_proxy_username);
		xfree(gg_proxy_password);

		gg_proxy_host = NULL;
		gg_proxy_username = NULL;
		gg_proxy_password = NULL;
		gg_proxy_port = 0;
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
	PARASC
	gg_private_t *g = session_private_get(session);
	CHAR_T *descr;
	char *cpdescr, *f = NULL, *fd = NULL, *df = NULL, *params0 = xstrdup(params[0]);
	const char *status;
	int timeout = session_int_get(session, "scroll_long_desc");
	int autoscroll = 0;
	int _status;

	if (xstrlen(params0))
		session->scroll_pos = 0;

	if (!xwcscasecmp(name, TEXT("_autoscroll"))) {
		autoscroll = 1;
		status = session_status_get(session);
		if (!xstrcasecmp(status, EKG_STATUS_AWAY) ||
						!xstrcasecmp(status, EKG_STATUS_AUTOAWAY)) {
				fd = "away_descr";
		} else if (!xstrcasecmp(status, EKG_STATUS_AVAIL)) {
				fd = "back_descr";
		} else if (!xstrcasecmp(status, EKG_STATUS_INVISIBLE)) {
				fd = "invisible_descr";
		}
		xfree(params0);
		params0 = xstrdup(session_descr_get(session));
		session->scroll_last = time(NULL);

		if (!xstrlen(params0)) {
			xfree(params0);
			return -1;
		}

		/* debug("%s [%s] %d\n", session_name(session), fd, session->scroll_pos); */
		if (xstrlen(params0) < GG_STATUS_DESCR_MAXSIZE) {
				xfree(params0);
				return -1;
		}
	} else if (!xwcscasecmp(name, TEXT("away"))) {
		session_status_set(session, EKG_STATUS_AWAY);
		df = "away"; f = "away"; fd = "away_descr";
		session_unidle(session);
	} else if (!xwcscasecmp(name, TEXT("_autoaway"))) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		df = "away"; f = "auto_away"; fd = "auto_away_descr";
	} else if (!xwcscasecmp(name, TEXT("back"))) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = "back"; f = "back"; fd = "back_descr";
		session_unidle(session);
	} else if (!xwcscasecmp(name, TEXT("_autoback"))) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = "back"; f = "auto_back"; fd = "auto_back_descr";
		session_unidle(session);
	} else if (!xwcscasecmp(name, TEXT("invisible"))) {
		session_status_set(session, EKG_STATUS_INVISIBLE);
		df = "quit"; f = "invisible"; fd = "invisible_descr";
		session_unidle(session);
	} else {
		xfree(params0);
		return -1;
	}

	if (params0) {
		if (xstrlen(params0) > GG_STATUS_DESCR_MAXSIZE && config_reason_limit) {
			if (!timeout) {
				char *descr_poss = xstrndup(params0, GG_STATUS_DESCR_MAXSIZE);
				char *descr_not_poss = xstrdup(params0 + GG_STATUS_DESCR_MAXSIZE);

				printq("descr_too_long", itoa(xstrlen(params0) - GG_STATUS_DESCR_MAXSIZE), descr_poss, descr_not_poss);
				session->scroll_op = 0;

				xfree(descr_poss);
				xfree(descr_not_poss);

				xfree(params0);
				return -1;
			}
		}

		session_descr_set(session, (!xstrcmp(params0, "-")) ? NULL : params0);
	} else {
		char *tmp;

		if ((tmp = ekg_draw_descr(df))) {
			session_status_set(session, tmp);
			xfree(tmp);
		}

		if (!config_keep_reason) {
			session_descr_set(session, NULL);
		}

	}

	reason_changed = 1;
	if (!session_descr_get(session))
			autoscroll = timeout = 0;

	if (autoscroll || (timeout && xstrlen(params0) > GG_STATUS_DESCR_MAXSIZE)) {
		const char *mode = session_get(session, "scroll_mode");
		char *desk;

		timeout = autoscroll;
		autoscroll = session -> scroll_pos;
		desk = xstrndup(session_descr_get(session) + autoscroll,
							GG_STATUS_DESCR_MAXSIZE-1);
		/* this is made especially to make other people happy ;)
		 * and make it easy to ignore states with '>' at beginning
		 */
		if (autoscroll)
				descr = wcsprintf(TEXT("<%s"), desk);
		else 
				descr = wcsprintf(TEXT("%s>"), desk);
		xfree(desk);

		if (!xstrcmp(mode, "bounce")) {
			if (!session->scroll_op) {
				session->scroll_pos++;
			} else {
				session->scroll_pos--;
			}
			/* I've changed xor to simple setting to 0 and 1 because
			 * it was possible to screw things up by playing with
			 * scroll_mode session variable
			 */
			if (session->scroll_pos <= 0)
					session->scroll_op=0;
			else if (session->scroll_pos >=
							xstrlen(session_descr_get(session)) - GG_STATUS_DESCR_MAXSIZE+1)
					session->scroll_op=1;
		} else if (!xstrcmp(mode, "simple")) {
			session->scroll_pos++;
			if (session->scroll_pos >
							xstrlen(session_descr_get(session)) - GG_STATUS_DESCR_MAXSIZE+1)
				session->scroll_pos=0;
		}
		/* I wanted to add one more 'constant' to the left [or right]
		 * but I'd have to change some things, and I'm soooo lazy
		 */

		autoscroll = timeout;
	} else {
#if USE_UNICODE
		descr = normal_to_wcs(session_descr_get(session));
#else
		descr = xstrdup(session_descr_get(session));
#endif
	}
	debug(CHARF " - %s\n", name, descr);

	status = session_status_get(session);

	if (!autoscroll) {
		if (descr)
			printq(fd, descr, "", session_name(session));
		else
			printq(f, session_name(session));
	}

	if (!g->sess || !session_connected_get(session)) {
		xfree(descr);
		xfree(params0);
		return 0;
	}

	ekg_update_status(session);

	cpdescr = gg_locale_to_cp(descr);
	_status = GG_S(gg_text_to_status(status, cpdescr)); /* descr can be NULL it doesn't matter... */

	if (session_int_get(session, "private"))
                _status |= GG_STATUS_FRIENDS_MASK;

	if (descr)	gg_change_status_descr(g->sess, _status, cpdescr);
	else		gg_change_status(g->sess, _status);

	xfree(params0);
	xfree(cpdescr);
#if USE_UNICODE
	xfree(descr);
#endif

	return 0;
}
	
COMMAND(gg_command_msg)
{
	PARUNI
	int count, valid = 0, chat, secure = 0, formatlen = 0;
	char **nicks = NULL, *nick = NULL, **p = NULL, *add_send = NULL;
#if USE_UNICODE
	CHAR_T *msg = NULL, *raw_msg = NULL;
#else
	unsigned char *msg = NULL, *raw_msg = NULL;
#endif
	unsigned char *cpmsg = NULL, *format = NULL;
	const char *seq;
	uint32_t *ekg_format = NULL;
	userlist_t *u;
	gg_private_t *g = session_private_get(session);

	chat = (xwcscasecmp(name, TEXT("msg")));

	session_unidle(session);

        if (!xwcscmp(params[0], TEXT("*"))) {
		if (msg_all(session, name, params[1]) == -1)
			wcs_printq("list_empty");
		return 0;
	}
	
	nick = xstrdup(target);

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
					struct ekg_group *g = m->data;

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

	if (gg_config_split_messages && xwcslen(params[1]) > 1989) {
		int i, len = xwcslen(params[1]);
		
		for (i = 1; i * 1989 <= len; i++) {
			CHAR_T *tmp = (i != len) ? xwcsndup(params[1] + (i - 1) * 1989, 1989) : xwcsdup(params[1] + (i - 1) * 1989);
#if USE_UNICODE
			command_exec_format(target, session, 0, TEXT("/%ls %s %ls"), name, target, tmp);
#else
			command_exec_format(target, session, 0, TEXT("/%s %s %s"), name, target, tmp);
#endif
			xfree(tmp);
		}
	
		return 0;

	} else if (xwcslen(params[1]) > 1989) {
              wcs_printq("message_too_long");
	}

	msg = xwcsmid(params[1], 0, 1989);
	ekg_format = ekg_sent_message_format(msg);

	/* analiz�tekstu zrobimy w osobnym bloku dla porzdku */
	{
		unsigned char attr = 0, last_attr = 0;
#if USE_UNICODE
		const CHAR_T *p = msg, *end = p + xwcslen(p);
#else
		const unsigned char *p = msg, *end = p + xwcslen(p);
#endif
		int msglen = 0;
		unsigned char rgb[3], last_rgb[3];

		for (p = msg; p < end; ) {
			if (*p == 18 || *p == 3) {	/* Ctrl-R, Ctrl-C */
				p++;

				if (xisdigit(*p)) {
					int num = wcs_atoi(p);
					
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

	raw_msg = xwcsdup(msg);
	cpmsg = gg_locale_to_cp(msg);

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

		if (config_last & 4) 
			last_add(1, uid, time(NULL), 0, msg);

		if (!chat || count == 1) {
			char *__msg = xstrdup(cpmsg);
			char *sid = xstrdup(session->uid);
			char *uid_tmp = xstrdup(uid);
			uin_t uin = atoi(uid + 3);

			secure = 0;
			
			query_emit(NULL, "message-encrypt", &sid, &uid_tmp, &__msg, &secure);

			xfree(sid);
			xfree(uid_tmp);

			if (g->sess)
				seq = itoa(gg_send_message_richtext(g->sess, (chat) ? GG_CLASS_CHAT : GG_CLASS_MSG, uin, __msg, format, formatlen));
			else
				seq = "offline";

			msg_queue_add(session_uid_get(session), target, params[1], seq);
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
			seq = itoa(gg_send_message_confer_richtext(g->sess, GG_CLASS_CHAT, realcount, uins, cpmsg, format, formatlen));
		else
			seq = "offline";

		msg_queue_add(session_uid_get(session), target, params[1], seq);
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
		{
			char *rmsg = wcs_to_normal(raw_msg);
			query_emit(NULL, "protocol-message", &me, &me, &rcpts, &rmsg, &ekg_format, &sent, &class, &seq, &ekgbeep, &secure);
			free_utf(rmsg);
		}

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
	PARUNI
	const CHAR_T *p[2] = { NULL, params[0] };

	if (!target || !params[0]) 
		return -1;
	return gg_command_msg(TEXT("chat"), p, session, target, quiet);
}

COMMAND(gg_command_block)
{
	PARASC
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
			wcs_printq("blocked_list_empty");

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
	PARASC
	const char *uid;

	if (!xstrcmp(params[0], "*")) {
		list_t l;
		int x = 0;

		for (l = session->userlist; l; ) {
			userlist_t *u = l->data;
			
			l = l->next;
	
			if (gg_blocked_remove(session, u->uid) != -1)
				x = 1;
		}

		if (x) {
			wcs_printq("blocked_deleted_all");
			config_changed = 1;
		} else {
			wcs_printq("blocked_list_empty");
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

#ifdef GIF_OCR

/*
 * token_gif_load()
 *
 * Reads token from a gif file. Returns -1 if error occurs (in token->data then
 * we will have error message) or 0. If token->pal_sz != 0 it means, that
 * token has some colours pallet, in which we have to check pixels (r.g.b)
 * order. Size of pallet in bites pal_sz * 3
 *
 *  - fname - name of the gif file to load 
 *  - token - pointer to structure token
 */

int token_gif_load (char *fname, struct token_t *token)
{
	char errbuf[512];
	GifFileType *file;
#ifdef TOKEN_GIF_PAL
	ColorMapObject *pal;
#endif
	int fd;
	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		snprintf (errbuf, sizeof(errbuf), "open(%s): %m", fname);
		goto err;
	}
	
	if (!(file = DGifOpenFileHandle(fd))) {
		snprintf (errbuf, sizeof(errbuf), "DGifOpenFileHandle(): %d", 
		    GifLastError());
		goto err2;
	}
	
	if (DGifSlurp(file) != GIF_OK) {
		snprintf (errbuf, sizeof(errbuf), "DGifSlurp(): %d", GifLastError());
		goto err3;
	}

	if (file->ImageCount != 1) {
		snprintf (errbuf, sizeof(errbuf), "ImageCount = %d", file->ImageCount);
		goto err3;
	}
#ifdef TOKEN_GIF_PAL
	token->pal = NULL;
	token->pal_sz = 0;
	pal = file->SavedImages[0].ImageDesc.ColorMap;
	if (!pal)
		pal = file->SColorMap;

	if (pal) {
		token->pal_sz = pal->ColorCount;
		token->pal = (unsigned char *) xmalloc(token->pal_sz * 3);
		memcpy (token->pal, pal->Colors, pal->ColorCount);
	}
#endif

	token->sx = file->SavedImages[0].ImageDesc.Width;
	token->sy = file->SavedImages[0].ImageDesc.Height;
	token->data = (unsigned char *) xmalloc(token->sx * token->sy);

	memcpy (token->data, file->SavedImages[0].RasterBits, token->sx * token->sy);
	DGifCloseFile (file);

	return 0;

err3:
	DGifCloseFile (file);
err2:
	close (fd);
err:
	token->data = (unsigned char *) xstrdup(errbuf);
	return -1;
}

/*
 * token_gif_free()
 *
 * Frees structures occupies by token (not token_t)
 * 
 *  - token - pointer to structure with data to be freed
 *
 */

void token_gif_free (struct token_t *token)
{
	if (token->data)
		xfree (token->data);

#ifdef TOKEN_GIF_PAL
	if (token->pal)
		xfree (token->pal);
#endif

	token->data = NULL;

#ifdef TOKEN_GIF_PAL
	token->pal = NULL;
#endif
}

/*
 * token_gif_get_pixel()
 *
 * Gets pixel from given position. If the position is out of range it returns
 * given coulour of background.
 *
 *  - token - pointer to structure, describing token 
 *  - x, y - pixel position
 *  - backgr_color - number of background colour
 */

char token_gif_get_pixel (struct token_t *token, size_t x, size_t y, unsigned char backgr_color)
{
	return (x < 0 || y < 0 || x >= token->sx || y >= token->sy) ? 
	    backgr_color : token->data[y * token->sx + x];
}

/*
 * token_gif_strip()
 *
 * It removes from the image everything that is not needed (lines, single
 * pixel and anyaliasing of the font).
 *
 *  - token - pointer to structure, debribing token 
 */

void token_gif_strip (struct token_t *token)
{
	unsigned char *new_data;
	size_t i;
	size_t x, y;
	unsigned char backgr_color = 0;
	size_t backgr_counts[256];

	/* Usuwamy wszystkie samotne piksele. Piksel jest uznawany za samotny 
	 * wtedy, kiedy nie ma w jego najbliszym otoczeniu, obejmujcym 8 
	 * pikseli dookola niego, przynajmniej trzech pikseli o tym samym 
	 * kolorze. To usuwa kropki i pojedyncze linie dodawane w celu 
	 * zaciemnienia obrazu tokena oraz anty-aliasing czcionek w znakach. 
	 * Otoczenie pikseli brzegowych jest uznawane za kolor ta tak, jakby 
	 * to zostao rozszerzone.
	 */

	/* Najpierw sprawdzamy kolor ta. To piksel, ktoego jest najwiecej. */

	for (i = 0; i < 256; i++)
		backgr_counts[i] = 0;

	for (i = 0; i < token->sx * token->sy; i++) {
		unsigned char pixel = token->data[i];
		if (++backgr_counts[pixel] > backgr_counts[backgr_color])
			backgr_color = pixel;
	}

	new_data = (unsigned char *) xmalloc(token->sx * token->sy);
	for (y = 0; y < token->sy; y++)
		for (x = 0; x < token->sx; x++) {
			int dx, dy;
			char new_pixel = backgr_color;

			if (token->data[y * token->sx + x] != backgr_color) {
				int num_pixels = 0;

				/* num_pixels przechowuje liczbe pikseli w otoczeniu 
				 * badanego piksela (wliczajc sam badany piksel) 
				 * o tym samym kolorze, co badany piksel.
				 */

				for (dy = -1; dy <= 1; dy++)
					for (dx = -1; dx <= 1; dx++)
						if (token_gif_get_pixel(token, x + dx, y + dy, 
						    backgr_color) == token->data[y * token->sx + x])
							num_pixels++;

				if (num_pixels >= 4)	/* 4, bo razem z badanym */
					new_pixel = token->data[y * token->sx + x];
			}

			new_data[y * token->sx + x] = new_pixel;	// ? 1 : 0;
	}

	xfree (token->data);
	token->data = new_data;
}

/*
 * token_gif_strip_txt
 *
 * It removes from given text buffer empty lines up and down. 
 * Return newly allocated buffer.
 *
 *  - buf - buffer to be stripped
 */

char *token_gif_strip_txt (char *buf)
{
	char *new_buf = NULL;
	size_t start, end, len;

	len = strlen(buf);
	for (start = 0; start < len; start++)
		if (buf[start] != 0x20 && buf[start] != '\n')
			break;

	if (!buf[start])
		return NULL;

	while (start && buf[start] != '\n')
		start--;

	if (start)
		start++;

	for (end = 0; end < len; end++)
		if (buf[len - 1 - end] != 0x20 && buf[len - 1 - end] != '\n')
			break;

	end = len - 1 - end;
	end--;

	if (end < start)
		return NULL;

	new_buf = (char *) xmalloc(end - start + 2);
	memcpy (new_buf, buf + start, end - start);
	new_buf[end - start - 1] = '\n';
	new_buf[end - start] = 0;

	return new_buf;
}

/*
 * token_gif_to_txt()
 *
 * Converts token to text. Returns text buffer with token stripped in that 
 * way to match the screen.
 *
 *  - token - pointer to token structure 
 */

char *token_gif_to_txt (struct token_t *token)
{
	char *buf, *bptr;
	size_t x, y;
#ifdef TOKEN_GIF_PAL
	size_t i;
	unsigned char min_rgb[3] = {255, 255, 255};
	unsigned char max_rgb[3] = {0, 0, 0};
	unsigned char delta_rgb[3] = {255, 255, 255};
#endif
	static const char chars[] = " !@#$&*:;-=+?";
	char mappings[256];
	int cur_char = 0;	/* Kolejny znaczek z chars[]. */

	memset (mappings, 0, sizeof(mappings));
	buf = bptr = (char *) xmalloc(token->sx * (token->sy + 1));

#ifdef TOKEN_GIF_PAL
	for (i = 0; i < token->sx * token->sy; i++) {
		unsigned char ofs = token->data[i];
		unsigned char *pent;
		size_t pent_i;

		if (ofs >= token->pal_sz)
			continue;

		pent = token->pal + ofs * 3;
		for (pent_i = 0; pent_i < 3; pent_i++) {
			if (pent[pent_i] < min_rgb[pent_i])
				min_rgb[pent_i] = pent[pent_i];

			if (pent[pent_i] > max_rgb[pent_i])
				max_rgb[pent_i] = pent[pent_i];
		}
	}

	for (i = 0; i < 3; i++)
		delta_rgb[i] = max_rgb[i] - min_rgb[i];

	for (i = 0; i < ((token->pal_sz < 256) ? token->pal_sz : 256); i++) {
		char rgb[3];
		size_t ri;

		for (ri = 0; ri < 3; ri++)
			rgb[ri] = ((int) token->pal[i * 3 + ri] - min_rgb[ri]) 
			    * 255 / delta_rgb[ri];

		intens[i] = (33 * rgb[0] + 
		    59 * rgb[1] + 
		    11 * rgb[2]) >= 50 ? 0 : 1;
	}
#endif

	for (x = 0; x < token->sx; x++) {
		for (y = 0; y < token->sy; y++) {
			unsigned char reg;

			reg = token->data[y * token->sx + (token->sx - 1 - x)];

			/* Mamy ju mapowanie dla tego koloru? */
			if (reg && !mappings[reg]) {
				mappings[reg] = ++cur_char;
				/* Podzielenie przez drugi sizeof nie jest 
				 * potrzebne, ale gdyby kto kiedy chcia 
				 * wpa�na pomys zmiany typu draw_chars, 
				 * to dla bezpiecze�twa lepiej da� */
				cur_char %= sizeof(chars) / sizeof(*chars) - 1;
			}

			*bptr++ = reg ? chars[(size_t) mappings[(size_t) reg]] : 0x20;
		}
		*bptr++ = '\n';
	}

	*bptr = 0;

	bptr = token_gif_strip_txt(buf);
	if (bptr) {
		xfree (buf);
		return bptr;
	}

	return buf;
}
#endif

#ifdef HAVE_LIBJPEG

/*
 * token_check()
 *
 * function checks if in the given place exists proposed character 
 * 
 *  - n - number from 0 to 15 (char from 0 to f)
 *  - x, y - coordinates in ocr table
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

	debug("token_check(nr=%d,x=%d,y=%d,ocr=%p,maxx=%d,maxy=%d\n", nr, x, y, ocr, maxx, maxy);

	return 1;
}

/*
 * token_ocr()
 *
 * returns text of the token
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

struct ekg_jpeg_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

void ekg_jpeg_error_exit(j_common_ptr j)
{
	struct ekg_jpeg_error_mgr *e = (struct ekg_jpeg_error_mgr *) j->err;
	/* Return control to the setjmp point */
	longjmp(e->setjmp_buffer, 1);
}
#endif

static WATCHER(gg_handle_token)
{
        struct gg_http *h = data;
	struct gg_token *t = NULL;
	char *file = NULL;

	if (!h)
		return -1;
	
       if (type == 2) {
                debug("[gg] gg_handle_token() timeout\n");
                print("register_timeout");
                goto fail;
        }

        if (type != 0)
                return 0;

	if (gg_token_watch_fd(h) || h->state == GG_STATE_ERROR) {
		print("gg_token_failed", gg_http_error_string(h->error));
		goto fail;
	}

	if (h->state != GG_STATE_DONE) {
		watch_t *w;
		if (fd == h->fd && (int) watch == h->check) return 0;	/* if this is the same watch... we leave it */

		/* otherwise we delete old one (return -1) and create new one .... 
		 * XXX, should we copy data from gg_http *h ? and free them in type == 1 ? */

		w = watch_add(&gg_plugin, h->fd, h->check, 1, gg_handle_token, h);
		watch_timeout_set(w, h->timeout);
		return -1;
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

#ifdef GIF_OCR
	if (gg_config_display_token) {
		struct token_t token;
		char *buf;
		if (token_gif_load(file, &token) == -1) {
			print("gg_token_failed_saved", token.data, file);
			xfree (token.data);
			goto fail;
		}
		token_gif_strip (&token);
		buf = token_gif_to_txt(&token);
		print("gg_token_start");
		print("gg_token_body", buf);
		print("gg_token_end");
		xfree (buf);
		token_gif_free (&token);

		goto fail;
	}
#endif

#ifdef HAVE_LIBJPEG
	if (gg_config_display_token) {
		struct jpeg_decompress_struct j;
		struct ekg_jpeg_error_mgr e;
		JSAMPROW buf[1];
		int size;
		char *token, *tmp;
		FILE *f;
		int h = 0;

		if (!(f = fopen(file, "rb"))) {
			print("gg_token_failed_saved", strerror(errno), file);
			goto fail;
		}

		j.err = jpeg_std_error(&e.pub);
		e.pub.error_exit = ekg_jpeg_error_exit;
		/* Establish the setjmp return context for ekg_jpeg_error_exit to use. */
		if (setjmp(e.setjmp_buffer)) {
			char buf[JMSG_LENGTH_MAX];
			/* If we ended up over here, then it means some call below called longjmp. */
			(e.pub.format_message)((j_common_ptr)&j, buf);
			print("gg_token_failed_saved", buf, file);
			jpeg_destroy_decompress(&j);
			fclose(f);
			goto fail;
		}
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
				print("gg_token_body", &token[i * (j.output_width + 1)]);
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
	} else
#endif	/* HAVE_LIBJPEG */
	{
		char *file2 = saprintf("%s.gif", file);

		if (rename(file, file2) == -1)
			print("gg_token", file);
		else
			print("gg_token", file2);

		xfree(file2);
	}
	/* here success... let's create some struct with token and use if they needed? XXX */

#else	/* HAVE_MKSTEMP */
	print("gg_token_unsupported");
#endif	/* HAVE_MKSTEMP */



#ifdef HAVE_MKSTEMP
	unlink(file);
fail:
	xfree(file);
#endif

	/* if we free token... we must search for it in all watches, and point data to NULL */
	/* XXX, hack... let's copy token data to all watch ? */

	list_t l;
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;
		if (w && w->data == h) {
			w->data = NULL;
			/* maybe we call remove here ? */
		}
	}

	gg_token_free(h);
	return -1;		/* watch_remove(&gg_plugin, h->fd, h->check); */
}

COMMAND(gg_command_token)
{
        struct gg_http *h;
	watch_t *w;

        if (!(h = gg_token(1))) {
                printq("gg_token_failed", strerror(errno));
                return -1;
        }

        w = watch_add(&gg_plugin, h->fd, h->check, 1, gg_handle_token, h);
        watch_timeout_set(w, h->timeout);

        return 0;
}

COMMAND(gg_command_modify)
{
	PARASC
	userlist_t *u;
	char **argv = NULL;
	int i, res = 0, modified = 0;

	if (!(u = userlist_find(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}

	if (params[1])
		argv = array_make(params[1], " \t", 0, 1, 1);

	for (i = 0; argv && argv[i]; i++) {
		
		if (nmatch_arg(argv[i], 'f', TEXT("first"), 2) && argv[i + 1]) {
			xfree(u->first_name);
			u->first_name = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (nmatch_arg(argv[i], 'l', TEXT("last"), 2) && argv[i + 1]) {
			xfree(u->last_name);
			u->last_name = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (nmatch_arg(argv[i], 'n', TEXT("nickname"), 2) && argv[i + 1]) {
			char *tmp1, *tmp2;

			if (userlist_find(session, argv[i + 1])) {
				printq("user_exists", argv[i + 1], session_name(session));
				continue;
			}

			tmp1 = xstrdup(u->nickname);
			tmp2 = xstrdup(argv[++i]);

			query_emit(NULL, "userlist-renamed", &tmp1, &tmp2);
			xfree(tmp1);
				
			xfree(u->nickname);
			u->nickname = tmp2;

			userlist_replace(session, u);
			
			modified = 1;
			continue;
		}
		
		if ((nmatch_arg(argv[i], 'p', TEXT("phone"), 2) || nmatch_arg(argv[i], 'm', TEXT("mobile"), 2)) && argv[i + 1]) {
			xfree(u->mobile);
			u->mobile = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (nmatch_arg(argv[i], 'g', TEXT("group"), 2) && argv[i + 1]) {
			char **tmp = array_make(argv[++i], ",", 0, 1, 1);
			int x, off;	/* jeli zaczyna si�od '@', pomijamy pierwszy znak */
			
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
		
		if (nmatch_arg(argv[i], 'u', TEXT("uid"), 2) && argv[i + 1]) {
			userlist_t *existing;
			char *tmp1, *tmp2;
			int q = 1;

			if (!valid_uid(argv[i + 1])) {
				wcs_printq("invalid_uid");
				array_free(argv);
				return -1;
			}

			if ((existing = userlist_find(session, argv[i + 1]))) {
				if (existing->nickname) {
					printq("user_exists_other", argv[i + 1], format_user(session, existing->uid), session_name(session));
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

			tmp1 = xstrdup(u->uid);
			tmp2 = xstrdup(argv[i + 1]);
			query_emit(NULL, "userlist-removed", &tmp1, &tmp2, &q);
			xfree(tmp1);
			xfree(tmp2);

			userlist_clear_status(session, u->uid);

			tmp1 = xstrdup(argv[++i]);
			query_emit(NULL, "userlist-added", &tmp1, &tmp1, &q);

			xfree(u->uid);
			u->uid = tmp1;

			modified = 1;
			continue;
		}

		if (nmatch_arg(argv[i], 'o', TEXT("offline"), 2)) {
			query_emit(NULL, "user-offline", &u, &session);
			modified = 2;
			continue;
		}

		if (nmatch_arg(argv[i], 'O', TEXT("online"), 2)) {
			query_emit(NULL, "user-online", &u, &session);
			modified = 2;
			continue;
		} 
		
		wcs_printq("invalid_params", name);
		array_free(argv);
		return -1;
	}

	if (xwcscasecmp(name, TEXT("add"))) {
		switch (modified) {
			case 0:
				wcs_printq("not_enough_params", name);
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

/* dj, nie rozumiem */

static TIMER(gg_checked_timer_handler)
{
        gg_currently_checked_t *c = (gg_currently_checked_t *) data;
	list_t l;

	if (type == 1) {
		xfree(data);
		return -1;
	}

	for (l = gg_currently_checked; l; l = l->next) {
		gg_currently_checked_t *c2 = l->data;

		if (!session_compare(c2->session, c->session) && !xstrcmp(c2->uid, c->uid)) {
			print("gg_user_is_not_connected", session_name(c->session), format_user(c->session, c->uid));
			return -1; 
		}
	}
	return -1; /* timer tymczasowy */
}

COMMAND(gg_command_check_conn)
{
	struct gg_msg_richtext_format_img {
		struct gg_msg_richtext rt;
		struct gg_msg_richtext_format f;
		struct gg_msg_richtext_image image;
	} msg;

	userlist_t *u;
	gg_private_t *g = session_private_get(session);
	gg_currently_checked_t c, *c_timer;
	list_t l;
 
	msg.rt.flag = 2;
	msg.rt.length = 13;
	msg.f.position = 0;
	msg.f.font = 0x80;
	msg.image.unknown1 = 0x0109;
	msg.image.size = 20;
	msg.image.crc32 = GG_CRC32_INVISIBLE;

	if (!(u = userlist_find(session, target))) {
		printq("user_not_found", target);
		return -1;
	}

        for (l = gg_currently_checked; l; l = l->next) {
                gg_currently_checked_t *c = l->data;

                if (!xstrcmp(c->uid, u->uid) && c->session == session) {
			debug("-- check_conn - we are already waiting for user to be connected\n");
                        return 0;
		}
        }

	if (gg_send_message_richtext(g->sess, GG_CLASS_MSG, atoi(u->uid + 3), "", (const char *) &msg, sizeof(msg)) == -1) {
                 debug("-- check_conn - shits happens\n");
                 return -1;
	}

        c_timer = xmalloc(sizeof(gg_currently_checked_t));
	c_timer->uid = u->uid;
	c_timer->session = session;

        c.uid = u->uid;
        c.session = session;

	list_add(&gg_currently_checked, &c, sizeof(c));

	/* if there is no reply after 15 secs user is not connected */
	timer_add(&gg_plugin, NULL, 15, 0, gg_checked_timer_handler, c_timer);

	return 0;
}

void gg_register_commands()
{
#define GG_ONLY        SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define GG_FLAGS       GG_ONLY | SESSION_MUSTBECONNECTED
#define GG_FLAGS_TARGET GG_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

#if USE_UNICODE
#define LNULL NULL
#define command_add(x, y, par, a, b, c) command_add(x, y, L##par, a, b, c)
#endif
	command_add(&gg_plugin, TEXT("gg:connect"), "r ?", gg_command_connect, 	GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:disconnect"), "r ?", gg_command_connect, 	GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:reconnect"), NULL, gg_command_connect, 	GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:msg"), "!uUC !", gg_command_msg, 		GG_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET, NULL);
	command_add(&gg_plugin, TEXT("gg:chat"), "!uUC !", gg_command_msg, 		GG_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET, NULL);
	command_add(&gg_plugin, TEXT("gg:"), "?", gg_command_inline_msg, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:_descr"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:away"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:_autoaway"), "?", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:back"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:_autoback"), "?", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:_autoscroll"), "?", gg_command_away, 	GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:check_conn"), "!uUC", gg_command_check_conn,	GG_FLAGS_TARGET, NULL);
	command_add(&gg_plugin, TEXT("gg:invisible"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, TEXT("gg:image"), "!u !f", gg_command_image, 		COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, TEXT("gg:block"), "uUC ?", gg_command_block, 0, NULL);
	command_add(&gg_plugin, TEXT("gg:unblock"), "!b ?", gg_command_unblock, 	COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, TEXT("gg:modify"), "!Uu ?", gg_command_modify, 	COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, TEXT("gg:remind"), "?", gg_command_remind, 0, NULL);
	command_add(&gg_plugin, TEXT("gg:register"), "? ? ?", gg_command_register, 0, NULL);
        command_add(&gg_plugin, TEXT("gg:token"), NULL, gg_command_token, 0, NULL);
	command_add(&gg_plugin, TEXT("gg:unregister"), "! ! !", gg_command_unregister, COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, TEXT("gg:passwd"), "! ?", gg_command_passwd, 		GG_ONLY | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, TEXT("gg:userlist"), "p ?", gg_command_list, 		GG_ONLY, "-c --clear -g --get -p --put");
	command_add(&gg_plugin, TEXT("gg:find"), "puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC", 
							gg_command_find, 	GG_ONLY, 
			"-u --uin -f --first -l --last -n --nick -c --city -b --born -a --active -F --female -M --male -s --start -A --all -S --stop");
	command_add(&gg_plugin, TEXT("gg:change"), "p", gg_command_change, 		GG_ONLY, 
			"-f --first -l --last -n --nick -b --born -c --city -N --familyname -C --familycity -F --female -M --male");
	command_add(&gg_plugin, TEXT("gg:dcc"), "p uU f ?", gg_command_dcc, 		GG_ONLY, "send rsend get resume rvoice voice close list");

}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
