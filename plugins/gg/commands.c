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

#ifdef HAVE_JPEGLIB_H
#  include <jpeglib.h>
#endif
#ifdef HAVE_GIF_LIB_H
#  include <fcntl.h>    /* open() */
#  include <gif_lib.h>
#endif

#include "dcc.h"
#include "gg.h"
#include "images.h"
#include "misc.h"
#include "pubdir.h"
#include "pubdir50.h"
#include "token.h"

static COMMAND(gg_command_connect) {
	gg_private_t *g = session_private_get(session);
	uin_t uin = (session) ? atoi(session->uid + 3) : 0;
	
	if (!xstrcmp(name, ("disconnect")) || (!xstrcmp(name, ("reconnect")))) {
	        /* if ,,reconnect'' timer exists we should stop doing */
	        if (timer_remove_session(session, "reconnect") == 0) {
			wcs_printq("auto_reconnect_removed", session_name(session));
	                return 0;
		}

		if (!g->sess) {
			wcs_printq("not_connected", session_name(session));
		} else {
			char *__session = xstrdup(session->uid);
			const char *__reason = params[0];
			char *myreason;
			unsigned char *tmp;
			int __type = EKG_DISCONNECT_USER;

			if (session->autoaway)
				session_status_set(session, EKG_STATUS_AUTOBACK);
			if (__reason) {
				if (!xstrcmp(__reason, "-")) 	myreason = NULL;
                        	else 				myreason = xstrdup(__reason);
				tmp = gg_locale_to_cp(xstrdup(myreason));
				session_descr_set(session, tmp ? myreason : NULL);
        		} else {
				myreason = xstrdup(session_descr_get(session));
				tmp = gg_locale_to_cp(xstrdup(myreason));
			}
			if (tmp)
				gg_change_status_descr(g->sess, GG_STATUS_NOT_AVAIL_DESCR, tmp);
			else
				gg_change_status(g->sess, GG_STATUS_NOT_AVAIL);
			xfree(tmp);

			watch_remove(&gg_plugin, g->sess->fd, g->sess->check);
			
			gg_logoff(g->sess);
			gg_free_session(g->sess);
			g->sess = NULL;

			query_emit_id(NULL, PROTOCOL_DISCONNECTED, &__session, &myreason, &__type, NULL);

			xfree(myreason);
			xfree(__session);
		}
	}

	if (!xstrcmp(name, ("connect")) || !xstrcmp(name, ("reconnect"))) {
		struct gg_login_params p;
		const char *tmp, *local_ip = session_get(session, "local_ip");
		int tmpi;
		int _status = gg_text_to_status(session_status_get(session), session_descr_get(session));
		const char *realserver = session_get(session, "server");
		int port = session_int_get(session, "port");
		char *password = (char *) session_get(session, "password");

		if (g->sess) {
			wcs_printq((g->sess->state == GG_STATE_CONNECTED) ? "already_connected" : "during_connect", 
					session_name(session));
			return -1;
		}
		if (command_exec(NULL, session, "/session --lock", 0) == -1)
			return -1;

		if (local_ip == NULL)
			gg_local_ip = htonl(INADDR_ANY);
		else {
#ifdef HAVE_INET_PTON
			int tmp = inet_pton(AF_INET, local_ip, &gg_local_ip);

			if (tmp == 0 || tmp == -1) {
				wcs_print("invalid_local_ip", session_name(session));
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
			return -1;
		}

		wcs_printq("connecting", session_name(session));

		memset(&p, 0, sizeof(p));

		if ((session_status_get(session) == EKG_STATUS_NA))
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
			if (gg_config_audio)
				p.has_audio = 1;

			gg_dcc_port = gg_config_dcc_port;
			
			gg_dcc_socket_open(gg_config_dcc_port);
		} 

		p.uin = uin;
		p.password = (char*) password;
		p.image_size = gg_config_image_size;
/*		p.use_sha1 = 2; */

                _status = GG_S(_status);
                if (session_int_get(session, "private"))
                        _status |= GG_STATUS_FRIENDS_MASK;

		if ((tmpi = session_int_get(session, "protocol")) != -1)
			p.protocol_version = tmpi;

		if ((tmpi = session_int_get(session, "last_sysmsg")) != -1)
			p.last_sysmsg = tmpi;

		while (realserver) {
			in_addr_t tmp_in;
			
#ifdef __GG_LIBGADU_HAVE_OPENSSL
			if (!xstrcasecmp(realserver, "tls")) {
				p.tls = 1;
				break;
			}
#endif
			if (!xstrncasecmp(realserver, "tls:", 4)) {
#ifdef __GG_LIBGADU_HAVE_OPENSSL
				p.tls = 1;
#endif
				realserver += 4;
			}

			{
				char *myserver, *comma;

				if ((comma = xstrchr(realserver, ',')))
					myserver = xstrndup(realserver, comma-realserver);
				else /* IMO duplicating the string will be more readable then using (myserver ? myserver : realserver */
					myserver = xstrdup(realserver);

				if ((tmp_in = inet_addr(myserver)) != INADDR_NONE)
					p.server_addr = inet_addr(myserver);
				else {
					wcs_print("inet_addr_failed", session_name(session));
					xfree(myserver);
					return -1;
				}
				xfree(myserver);
			}
			break;
		}

		if ((port < 1) || (port > 65535)) {
			wcs_print("port_number_error", session_name(session));
			return -1;
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
		
		/* moved this further, because of gg_locale_to_cp() allocation */
		p.status = _status;
		p.status_descr = gg_locale_to_cp(xstrdup(session_descr_get(session)));
		p.async = 1;

		g->sess = gg_login(&p);
		xfree(p.status_descr);

		if (!g->sess)
			wcs_printq("conn_failed", format_find((errno == ENOMEM) ? "conn_failed_memory" : "conn_failed_connecting"), session_name(session));
		else {
			watch_t *w = watch_add_session(session, g->sess->fd, g->sess->check, gg_session_handler);
			watch_timeout_set(w, g->sess->timeout);
		}
	}

	return 0;
}

static COMMAND(gg_command_away) {
	gg_private_t *g = session_private_get(session);
	char *descr;
	char *cpdescr, *f = NULL, *fd = NULL, *params0 = xstrdup(params[0]);
	int df = 0; /* do we really need this? */
	int status;
	int timeout = session_int_get(session, "scroll_long_desc");
	int autoscroll = 0;
	int _status;

	if (xstrlen(params0))
		session->scroll_pos = 0;

	if (!xstrcmp(name, ("_autoscroll"))) {
		autoscroll = 1;
		status = session_status_get(session);
		if ((status == EKG_STATUS_AWAY)	/*|| (status == EKG_STATUS_AUTOAWAY) */) {
				fd = "away_descr";
		} else if (status == EKG_STATUS_AVAIL) {
				fd = "back_descr";
		} else if (status == EKG_STATUS_INVISIBLE) {
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
	} else if (!xstrcmp(name, ("away"))) {
		session_status_set(session, EKG_STATUS_AWAY);
		df = EKG_STATUS_AWAY; f = "away"; fd = "away_descr";
		session_unidle(session);
	} else if (!xstrcmp(name, ("_autoaway"))) {
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		df = EKG_STATUS_AWAY; f = "auto_away"; fd = "auto_away_descr";
	} else if (!xstrcmp(name, ("back"))) {
		session_status_set(session, EKG_STATUS_AVAIL);
		df = EKG_STATUS_AVAIL; f = "back"; fd = "back_descr";
		session_unidle(session);
	} else if (!xstrcmp(name, ("_autoback"))) {
		session_status_set(session, EKG_STATUS_AUTOBACK);
		df = EKG_STATUS_AVAIL; f = "auto_back"; fd = "auto_back_descr";
		session_unidle(session);
	} else if (!xstrcmp(name, ("invisible"))) {
		session_status_set(session, EKG_STATUS_INVISIBLE);
		df = EKG_STATUS_NA; f = "invisible"; fd = "invisible_descr";
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

		if (!config_keep_reason) {
			session_descr_set(session, NULL);
		} else if ((tmp = ekg_draw_descr(df))) {
			session_descr_set(session, tmp);
			xfree(tmp);
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
				descr = saprintf(("<%s"), desk);
		else 
				descr = saprintf(("%s>"), desk);
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
		descr = xstrdup(session_descr_get(session));
	}
	debug("%s - %s\n", name, descr);

	status = session_status_get(session);

	if (!autoscroll) {
		if (descr)
			wcs_printq(fd, descr, (""), session_name(session));
		else
			wcs_printq(f, session_name(session));
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
	return 0;
}
	
static COMMAND(gg_command_msg) {
	int count, valid = 0, chat, secure = 0, formatlen = 0;
	char **nicks = NULL, *nick = NULL, **p = NULL, *add_send = NULL;
	unsigned char *msg = NULL, *raw_msg = NULL;
	unsigned char *cpmsg = NULL, *format = NULL;
	const char *seq;
	uint32_t *ekg_format = NULL;
	userlist_t *u;
	gg_private_t *g = session_private_get(session);

	chat = (xstrcmp(name, ("msg")));

	if (!quiet)
		session_unidle(session);

        if (!xstrcmp(params[0], ("*"))) {
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

	if (gg_config_split_messages && xstrlen(params[1]) > 1989) {
		int i, len = xstrlen(params[1]);
		
		for (i = 1; i * 1989 <= len; i++) {
			char *tmp = (i != len) ? xstrndup(params[1] + (i - 1) * 1989, 1989) : xstrdup(params[1] + (i - 1) * 1989);
			command_exec_format(target, session, 0, ("/%s %s %s"), name, target, tmp);
			xfree(tmp);
		}
	
		return 0;

	} else if (xstrlen(params[1]) > 1989) {
              wcs_printq("message_too_long");
	}

	msg = xstrmid(params[1], 0, 1989);
	ekg_format = ekg_sent_message_format(msg);

	/* analiz�tekstu zrobimy w osobnym bloku dla porzdku */
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

					rgb[0] = color_map_default[num].r;
					rgb[1] = color_map_default[num].g;
					rgb[2] = color_map_default[num].b;

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
			last_add(1, uid, time(NULL), 0, raw_msg);

		if (!chat || count == 1) {
			char *__msg = xstrdup(cpmsg);
			char *sid = xstrdup(session->uid);
			char *uid_tmp = xstrdup(uid);
			uin_t uin = atoi(uid + 3);

			secure = 0;
			
			query_emit_id(NULL, MESSAGE_ENCRYPT, &sid, &uid_tmp, &__msg, &secure);

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
		wcs_printq("not_connected_msg_queued", session_name(session));

	if (valid && !quiet) {
		char **rcpts = xmalloc(sizeof(char *) * 2);
		const int class = (chat) ? EKG_MSGCLASS_SENT_CHAT : EKG_MSGCLASS_SENT;
		const int ekgbeep = EKG_TRY_BEEP;
		char *me = xstrdup(session_uid_get(session));
		const time_t sent = time(NULL);
		
		rcpts[0] = xstrdup(nick);
		rcpts[1] = NULL;

		query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &me, &rcpts, &raw_msg, &ekg_format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(me);
		xfree(rcpts[0]);
		xfree(rcpts);
	}

	xfree(cpmsg);
	xfree(raw_msg);
	xfree(format);
	xfree(nick);
	xfree(ekg_format);

	array_free(nicks);

	unidle();

	return 0;
}

static COMMAND(gg_command_inline_msg) {
	const char *p[2] = { NULL, params[0] };

	if (!target || !params[0]) 
		return -1;
	return gg_command_msg(("chat"), p, session, target, quiet);
}

/**
 * gg_command_block()
 *
 * Block @a uid or printq() list of blocked uids.<br>
 * Handler for: <i>/gg:block</i> command
 *
 * @todo	Think about config_changed ... maybe let's create userlist_changed for this or smth?
 *
 * @param params [0] (<b>uid</b>) - uid to block, or NULL if you want to display list of blocked uids.
 *
 * @sa gg_blocked_add()		- for block function.
 * @sa gg_command_unblock()	- for unblock command
 *
 * @return	 0 - if @a uid == NULL, or @a uid was successfully blocked.<br>
 * 		-1 - if @a uid was neither valid gg uid, nor user nickname<br>
 * 		-2 - if @a uid is already blocked.
 */

static COMMAND(gg_command_block) {
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

	if (gg_blocked_add(session, uid) == -1) {
		printq("blocked_exist", format_user(session, uid));
		return -2;
	}

	printq("blocked_added", format_user(session, uid));
	config_changed = 1;

	return 0;
}

/**
 * gg_command_unblock()
 *
 * Unblock @a uid. Or everybody if @a uid '*'<br>
 * Handler for: <i>/gg:unlock</i> command.
 *
 * @todo	Think about config_changed ... maybe let's create userlist_changed for this or smth?
 *
 * @param params [0] (<b>uid</b>) - @a uid to unblock, or '*' to unblock everyone.
 *
 * @sa gg_blocked_remove()	- for unblock function.
 * @sa gg_command_block()	- for block command
 *
 * @return 	 0 - if somebody was unblocked.<br>
 * 		-1 - if smth went wrong.
 */

static COMMAND(gg_command_unblock) {
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

		if (!x) {
			printq("blocked_list_empty");
			return -1;
		}

		printq("blocked_deleted_all");
		config_changed = 1;
		return 0;
	}

	if (!(uid = get_uid(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}

	if (gg_blocked_remove(session, uid) == -1) {
		printq("error_not_blocked", format_user(session, uid));
		return -1;
	}
		
	printq("blocked_deleted", format_user(session, uid));
	config_changed = 1;

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

static int token_gif_load (char *fname, struct token_t *token) {
	char errbuf[512];
	GifFileType *file;
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
 * token_gif_get_pixel()
 *
 * Gets pixel from given position. If the position is out of range it returns
 * given coulour of background.
 *
 *  - token - pointer to structure, describing token 
 *  - x, y - pixel position
 *  - backgr_color - number of background colour
 */

static char token_gif_get_pixel (struct token_t *token, size_t x, size_t y, unsigned char backgr_color) {
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

static void token_gif_strip (struct token_t *token) {
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

static char *token_gif_strip_txt (char *buf) {
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

static char *token_gif_to_txt (struct token_t *token) {
	char *buf, *bptr;
	size_t x, y;
	static const char chars[] = " !@#$&*:;-=+?";
	char mappings[256];
	int cur_char = 0;	/* Kolejny znaczek z chars[]. */

	memset (mappings, 0, sizeof(mappings));
	buf = bptr = (char *) xmalloc((token->sx * (token->sy + 1))+1);

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
static char *token_ocr(const char *ocr, int width, int height, int length) {
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

static void ekg_jpeg_error_exit(j_common_ptr j)
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
		if (fd == h->fd && watch == h->check) return 0;	/* if this is the same watch... we leave it */

		/* otherwise we delete old one (return -1) and create new one .... 
		 * XXX, should we copy data from gg_http *h ? and free them in type == 1 ? */

		w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_token, h);
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

	if (query_emit(NULL, ("gg-display-token"), &file) == -1) goto fail;

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
		xfree(buf);
		xfree(token.data);
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

static COMMAND(gg_command_token) {
        struct gg_http *h;
	watch_t *w;

        if (!(h = gg_token(1))) {
                printq("gg_token_failed", strerror(errno));
                return -1;
        }

        w = watch_add(&gg_plugin, h->fd, h->check, gg_handle_token, h);
        watch_timeout_set(w, h->timeout);

        return 0;
}

static COMMAND(gg_command_modify) {
	userlist_t *u;
	gg_userlist_private_t *up;
	const char **par;
	char **argv = NULL;
	int i, res = 0, modified = 0;

	if (!xstrcmp(name, ("add"))) {
		int ret;
	/* we overlap /add command, so we need to execute it... maybe let's move it here? */
		ret = cmd_add(name, params, session, target, quiet);
		/* if adding fails, quit */
		if (ret != 0 || !params[1]) return ret;
	/* params[1] cause of: in commands.c, 
	 *	 	query_emit(NULL, ("userlist-added"), &uid, &params[1], &quiet);
	 *	and we emulate old behavior (via query handler executing command) with command handler... rewrite ? 
	 */
		par = &(params[1]);
	} else	par = params;

	if (!(u = userlist_find(session, par[0]))) {
		printq("user_not_found", par[0]);
		return -1;
	}
	up = gg_userlist_priv_get(u);

	if (par[1])
		argv = array_make(par[1], " \t", 0, 1, 1);

	for (i = 0; argv && argv[i]; i++) {
		
		if (match_arg(argv[i], 'f', ("first"), 2) && argv[i + 1]) {
			if (up) {
				xfree(up->first_name);
				up->first_name = xstrdup(argv[++i]);
				modified = 1;
			} else /* skip arg */
				i++;
			continue;
		}
		
		if (match_arg(argv[i], 'l', ("last"), 2) && argv[i + 1]) {
			if (up) {
				xfree(up->last_name);
				up->last_name = xstrdup(argv[++i]);
				modified = 1;
			} else /* skip arg */
				i++;
			continue;
		}
		
		if (match_arg(argv[i], 'n', ("nickname"), 2) && argv[i + 1]) {
			char *tmp1, *tmp2;

			if (userlist_find(session, argv[i + 1])) {
				printq("user_exists", argv[i + 1], session_name(session));
				continue;
			}

			tmp1 = xstrdup(u->nickname);
			tmp2 = xstrdup(argv[++i]);

			query_emit_id(NULL, USERLIST_RENAMED, &tmp1, &tmp2);
			xfree(tmp1);
				
			xfree(u->nickname);
			u->nickname = tmp2;

			userlist_replace(session, u);
			
			modified = 1;
			continue;
		}
		
		if ((match_arg(argv[i], 'p', ("phone"), 2) || match_arg(argv[i], 'm', ("mobile"), 2)) && argv[i + 1]) {
			if (up) {
				xfree(up->mobile);
				up->mobile = xstrdup(argv[++i]);
				modified = 1;
			} else
				i++;
			continue;
		}
		
		if (match_arg(argv[i], 'g', ("group"), 2) && argv[i + 1]) {
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
		
		if (match_arg(argv[i], 'u', ("uid"), 2) && argv[i + 1]) {
			userlist_t *existing;
			char *tmp1, *tmp2;
			int q = 1;

			if (!valid_plugin_uid(&gg_plugin, argv[i + 1]) != 1) {
				printq("invalid_uid");
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
			query_emit_id(NULL, USERLIST_REMOVED, &tmp1, &tmp2, &q);
			xfree(tmp1);
			xfree(tmp2);

			userlist_clear_status(session, u->uid);

			tmp1 = xstrdup(argv[++i]);
			query_emit_id(NULL, USERLIST_ADDED, &tmp1, &tmp1, &q);

			xfree(u->uid);
			u->uid = tmp1;

			modified = 1;
			continue;
		}

		if (match_arg(argv[i], 'o', ("offline"), 2)) {
			query_emit(NULL, ("user-offline"), &u, &session);
			modified = 2;
			continue;
		}

		if (match_arg(argv[i], 'O', ("online"), 2)) {
			query_emit(NULL, ("user-online"), &u, &session);
			modified = 2;
			continue;
		} 
		
		wcs_printq("invalid_params", name);
		array_free(argv);
		return -1;
	}

	if (xstrcmp(name, ("add"))) {
		switch (modified) {
			case 0:
				wcs_printq("not_enough_params", name);
				res = -1;
				break;
			case 1:
				printq("modify_done", par[0]);
			case 2:
				config_changed = 1;
				break;
		}
	} else
		config_changed = 1;

	array_free(argv);

	return res;
}

static TIMER(gg_checked_timer_handler)
{
	const gg_currently_checked_t *c = (gg_currently_checked_t *) data;
	list_t l;

	if (type == 1) {
		xfree(data);
		return -1;
	}

	for (l = gg_currently_checked; l; l = l->next) {
		gg_currently_checked_t *c2 = l->data;

		if (!session_compare(c2->session, c->session) && !xstrcmp(c2->uid, c->uid)) {
			userlist_t *u = userlist_find(c->session, c->uid);
			if (u) {
				if (u->status == EKG_STATUS_INVISIBLE) {
					char *session	= xstrdup(session_uid_get(c->session));
					char *uid	= xstrdup(c->uid);
					int status	= EKG_STATUS_NA;
					char *descr	= xstrdup(u->descr);
					char *host	= NULL;
					int port	= 0;
					time_t when	= time(NULL);
					
					query_emit(NULL, ("protocol-status"), &session, &uid, &status, &descr, &host, &port, &when, NULL);
					
					xfree(session);
					xfree(uid);
					xfree(descr);
				}
			} else
				print("gg_user_is_not_connected", session_name(c->session), format_user(c->session, c->uid));
			xfree(c2->uid);
			list_remove(&gg_currently_checked, c2, 1);
			return -1; 
		}
	}
	return -1; /* timer tymczasowy */
}

static COMMAND(gg_command_check_conn) {
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
	c_timer->uid = xstrdup(u->uid); /* if user gets deleted, we won't get undef value */
	c_timer->session = session;

        c.uid = c_timer->uid;
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

	command_add(&gg_plugin, ("gg:add"), "!U ? p", gg_command_modify, 	COMMAND_ENABLEREQPARAMS, "-f --find");
	command_add(&gg_plugin, ("gg:connect"), NULL, gg_command_connect, 	GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:disconnect"), "r", gg_command_connect, 	GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:reconnect"), NULL, gg_command_connect, 	GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:msg"), "!uUC !", gg_command_msg, 		GG_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET, NULL);
	command_add(&gg_plugin, ("gg:chat"), "!uUC !", gg_command_msg, 		GG_ONLY | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET, NULL);
	command_add(&gg_plugin, ("gg:"), "?", gg_command_inline_msg, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:away"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:_autoaway"), "?", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:back"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:_autoback"), "?", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:_autoscroll"), "?", gg_command_away, 	GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:check_conn"), "!uUC", gg_command_check_conn,	GG_FLAGS_TARGET, NULL);
	command_add(&gg_plugin, ("gg:invisible"), "r", gg_command_away, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:image"), "!u !f", gg_command_image, 		COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, ("gg:block"), "uUC", gg_command_block, 		GG_ONLY, NULL);
	command_add(&gg_plugin, ("gg:unblock"), "!b", gg_command_unblock, 	GG_ONLY | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, ("gg:modify"), "!Uu ?", gg_command_modify, 	COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, ("gg:remind"), "? ?", gg_command_remind, 0, NULL);
	command_add(&gg_plugin, ("gg:register"), "? ? ?", gg_command_register, 0, NULL);
	command_add(&gg_plugin, ("gg:token"), NULL, gg_command_token, 0, NULL);
	command_add(&gg_plugin, ("gg:unregister"), "! ! !", gg_command_unregister, COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, ("gg:passwd"), "! ?", gg_command_passwd, 		GG_ONLY | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&gg_plugin, ("gg:userlist"), "p ?", gg_command_list, 		GG_ONLY, "-c --clear -g --get -p --put");
	command_add(&gg_plugin, ("gg:find"), "!puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC puUC", 
							gg_command_find, 	GG_FLAGS_TARGET, 
			"-u --uin -f --first -l --last -n --nick -c --city -b --born -a --active -F --female -M --male -s --start -A --all -S --stop");
	command_add(&gg_plugin, ("gg:change"), "p", gg_command_change, 		GG_ONLY, 
			"-f --first -l --last -n --nick -b --born -c --city -N --familyname -C --familycity -F --female -M --male");
	command_add(&gg_plugin, ("gg:dcc"), "p uU f ?", gg_command_dcc, 		GG_ONLY, "send rsend get resume rvoice voice close list");

}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
