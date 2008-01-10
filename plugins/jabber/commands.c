/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		       Tomasz Torcz <zdzichu@irc.pl>	
 *		       Libtlen developers (http://libtlen.sourceforge.net/index.php?theme=teary&page=authors)
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
#include <ekg/win32.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef NO_POSIX_SYSTEM
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <sys/ioctl.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#ifndef NO_POSIX_SYSTEM
#include <netdb.h>
#endif

#ifdef HAVE_EXPAT_H /* expat is used for XEP-0071 syntax checking */
#  include <expat.h>
#endif

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/userlist.h>
#include <ekg/sessions.h>
#include <ekg/xmalloc.h>

/* ... */
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/log.h>

#include <ekg/queries.h>

#include "jabber.h"
#include "jabber_dcc.h"

extern void *jconv_out; /* for msg */

const char *jabber_prefixes[2] = { "xmpp:", "tlen:" };
int config_jabber_disable_chatstates; /* in jabber.c */

static COMMAND(jabber_command_connect)
{
	const char *realserver	= session_get(session, "server"); 
	const char *resource	= session_get(session, "resource");
	const char *server;

	jabber_private_t *j = session_private_get(session);
	
	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (!session_get(session, "__new_account") && !(session_get(session, "password"))) {
		printq("no_config");
		return -1;
	}

	if (command_exec(NULL, session, "/session --lock", 0) == -1)
		return -1;

	debug("session->uid = %s\n", session->uid);
		/* XXX, nie wymagac od usera podania calego uida w postaci: tlen:ktostam@tlen.pl tylko samo tlen:ktostam? */
	if (!(server = xstrchr(session->uid, '@'))) {
		printq("wrong_id", session->uid);
		return -1;
	}

	xfree(j->server);
	j->server	= xstrdup(++server);

	if (!realserver) {
		if (j->istlen) {
			j->istlen++;
			realserver = TLEN_HUB;
		} else
			realserver = server;
	}

	if (ekg_resolver2(&jabber_plugin, realserver, jabber_handle_resolver, session) == NULL) {
		printq("generic_error", strerror(errno));
		return -1;
	}
	/* XXX, set resolver-watch timeout? */

	if (!resource)
		resource = JABBER_DEFAULT_RESOURCE;

	xfree(j->resource);
	j->resource = xstrdup(resource);

	j->connecting = 1;

	printq("connecting", session_name(session));
	if (session_status_get(session) == EKG_STATUS_NA)
		session_status_set(session, EKG_STATUS_AVAIL);
	return 0;
}

static COMMAND(jabber_command_disconnect)
{
	jabber_private_t *j = session_private_get(session);
	char *descr = NULL;

	/* jesli istnieje timer reconnecta, to znaczy, ze przerywamy laczenie */
	if (timer_remove_session(session, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!j->connecting && !session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (session->autoaway)
		session_status_set(session, EKG_STATUS_AUTOBACK);
	/* je¶li jest /reconnect, nie mieszamy z opisami */
	if (xstrcmp(name, ("reconnect"))) {
		if (params[0]) {
			if (!xstrcmp(params[0], "-"))
				descr = NULL;
			else
				descr = xstrdup(params[0]);
		} else if (config_keep_reason && !(descr = ekg_draw_descr(EKG_STATUS_NA)))
			descr = xstrdup(session_descr_get(session));
		
		session_descr_set(session, descr);
	} else
		descr = xstrdup(session_descr_get(session));

/* w libtlenie jest <show>unavailable</show> + eskejpiete tlen_encode() */

	if (session->connected) {
#if 0
		char *lt = session_get(session, "__last_typing");

		if (lt)
			watch_write(j->send_watch, "<message type=\"chat\" to=\"%s\">"
						"<x xmlns=\"jabber:x:event\"/>"
						"<gone xmlns=\"http://jabber.org/protocol/chatstates\"/>"
						"</message>\n", lt);
#endif

		{
			char *__session = xstrdup(session_uid_get(session));

			query_emit_id(NULL, PROTOCOL_DISCONNECTING, &__session);

			xfree(__session);
		}

		if (descr) {
			char *tmp = jabber_escape(descr);
			watch_write(j->send_watch, "<presence type=\"unavailable\"><status>%s</status></presence>", tmp ? tmp : "");
			xfree(tmp);
		} else
			watch_write(j->send_watch, "<presence type=\"unavailable\"/>");
	}

	if (!j->istlen) watch_write(j->send_watch, "</stream:stream>");
	else		watch_write(j->send_watch, "</s>");

	userlist_free(session);
	if (j->connecting)
		jabber_handle_disconnect(session, descr, EKG_DISCONNECT_STOPPED);
	else
		jabber_handle_disconnect(session, descr, EKG_DISCONNECT_USER);

	xfree(descr);
	return 0;
}

static COMMAND(jabber_command_reconnect)
{
	jabber_private_t *j = session_private_get(session);

	if (j->connecting || session_connected_get(session)) {
		jabber_command_disconnect(name, params, session, target, quiet);
	}

	return jabber_command_connect(name, params, session, target, quiet);
}

static const char *jid_target2uid(session_t *s, const char *target, int quiet) {
	const char *uid;
	int istlen = jabber_private(s)->istlen;

	if (!(uid = get_uid(s, target))) 
		uid = target;

/* XXX, get_uid() checks if this plugin is ok to handle it. so we have here tlen: or xmpp: but, we must check
 * 		if this uid match istlen.. However doesn't matter which protocol is, function should work...
 */
	if (xstrncasecmp(uid, jabber_prefixes[istlen], 5)) {
		printq("invalid_session");
		return NULL;
	}
	return uid;
}

static COMMAND(jabber_command_msg)
{
	jabber_private_t *j	= session_private_get(session);
	int chat		= !xstrcmp(name, ("chat"));
	int subjectlen		= xstrlen(config_subject_prefix);
	char *msg;
	char *htmlmsg		= NULL;
	char *subject		= NULL;
	char *thread		= NULL;
	const char *uid;

	newconference_t *c;
	int ismuc		= 0;

	int secure		= 0;

	char *s;		/* used for transcoding */

	if (!xstrcmp(target, "*")) {
		if (msg_all(session, name, params[1]) == -1)
			printq("list_empty");
		return 0;
	}
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;

		/* threaded messages */
	if (!xstrcmp(name, "tmsg")) {
			/* just make it compatible with /msg */
		thread = params[1];
		params[1] = params[2];
		params[2] = thread;
		
			/* and now we can set real thread */
		thread = jabber_escape(params[2]);
	} else if (!xstrcmp(name, "msg") && (session_int_get(session, "msg_gen_thread")))
		thread = jabber_thread_gen(j, uid); /* we don't return any chars to escape */

		/* message subject, TheNewBetterWay^TM */
	if (!j->istlen && config_subject_prefix && !xstrncmp(params[1], config_subject_prefix, subjectlen)) {
		char *last = xstrchr(params[1]+subjectlen, 10);

		if (last) {
			*last	= 0;
			subject	= jabber_escape(params[1]+subjectlen);
			*last	= 10;
			msg	= last+1;
		} else {
			subject	= jabber_escape(params[1]+subjectlen);
			msg	= NULL;
		}
	} else 
		msg = params[1]; /* bez tematu */
	if ((c = newconference_find(session, target))) 
		ismuc = 1;

	if (!j->istlen) { /* Very, very simple XEP-0071 support + 'modified' jabber_encode() */
		if (!config_use_unicode) {
			s = ekg_convert_string_p(msg, jconv_out);
			if (s)
				msg = s;
		}

		if ((htmlmsg = strchr(msg, 18))) { /* ^R */
			int omitsyntaxcheck;

			*(htmlmsg++) = 0;
			if ((omitsyntaxcheck = (*htmlmsg == 18)))
				htmlmsg++;
			htmlmsg = saprintf("<html xmlns=\"http://jabber.org/protocol/xhtml-im\">"
					"<body xmlns=\"http://www.w3.org/1999/xhtml\">"
					"%s</body></html>", htmlmsg);

			if (!omitsyntaxcheck) {
				XML_Parser p = XML_ParserCreate("utf-8");
				/* expat syntax-checking needs the code to be embedded in some parent element
				 * so we create the whole block here, instead of giving %s to watch_write() */
				int r;

				if (!(r = XML_Parse(p, htmlmsg, xstrlen(htmlmsg), 1))) {
					enum XML_Error errc = XML_GetErrorCode(p);
					const char *errs;

					if (errc && (errs = XML_ErrorString(errc)))
						print_window(target, session, 0, "jabber_msg_xmlsyntaxerr", errs);
					else	print_window(target, session, 0, "jabber_msg_xmlsyntaxerr", "unknown");

					xfree(htmlmsg);
					xfree(subject);
					xfree(thread);
				}
				XML_ParserFree(p);
				if (!r)
					return -1;
			}
		}
	}

/* writing: */
	if (j->send_watch) j->send_watch->transfer_limit = -1;

	if (ismuc)
		watch_write(j->send_watch, "<message type=\"groupchat\" to=\"%s\" id=\"%d\">", uid+5, time(NULL));
	else
		watch_write(j->send_watch, "<message %sto=\"%s\" id=\"%d\">", 
			chat ? "type=\"chat\" " : "",
/*				j->istlen ? "type=\"normal\" " : "",  */
			uid+5, time(NULL));

	if (subject) {
		watch_write(j->send_watch, "<subject>%s</subject>", subject); 
		xfree(subject); 
	}
	if (thread) {
		watch_write(j->send_watch, "<thread>%s</thread>", thread);
		xfree(thread);
	}

	if (!msg) goto nomsg;

	if (session_int_get(session, "__gpg_enabled") == 1) {
		char *e_msg = xstrdup(msg);

		if ((e_msg = jabber_openpgp(session, uid, JABBER_OPENGPG_ENCRYPT, e_msg, NULL, NULL))) {
			watch_write(j->send_watch, 
					"<x xmlns=\"jabber:x:encrypted\">%s</x>"
					"<body>This message was encrypted by ekg2! (EKG2 BABY) Sorry if you cannot decode it ;)</body>", e_msg);
			secure = 1;
			xfree(e_msg);
		}
	}
	if (!secure /* || j->istlen */) {
		char *tmp = (j->istlen ? tlen_encode(msg) : xml_escape(msg));

		watch_write(j->send_watch, "<body>%s</body>", tmp);
		xfree(tmp);
	}
	if (!j->istlen && !config_use_unicode)
		xfree(s);			/* recoded string */

	if (config_last & 4) 
		last_add(1, uid, time(NULL), 0, params[1]);
nomsg:
	if (htmlmsg) {
		watch_write(j->send_watch, "%s", htmlmsg);
		xfree(htmlmsg);
	}

	if (!j->istlen) 
		watch_write(j->send_watch, "<x xmlns=\"jabber:x:event\">%s%s<displayed/><composing/></x>",
			( config_display_ack & 1 ? "<delivered/>" : ""), /* ? */
			( config_display_ack & 2 ? "<offline/>"   : ""), /* ? */
			( chat && ((config_jabber_disable_chatstates & 7) != 7) ? "<active xmlns=\"http://jabber.org/protocol/chatstates\"/>" : ""));

	watch_write(j->send_watch, "</message>");
	JABBER_COMMIT_DATA(j->send_watch);

	if (!quiet && !ismuc) { /* if (1) ? */ 
		char *me 	= xstrdup(session_uid_get(session));
		char **rcpts 	= xcalloc(2, sizeof(char *));
		char *msg	= xstrdup(params[1]);
		time_t sent 	= time(NULL);
		int class 	= (chat) ? EKG_MSGCLASS_SENT_CHAT : EKG_MSGCLASS_SENT;
		int ekgbeep 	= EKG_NO_BEEP;
		char *format 	= NULL;
		char *seq 	= NULL;

		rcpts[0] 	= xstrdup(uid);
		rcpts[1] 	= NULL;

		if (ismuc)
			class |= EKG_NO_THEMEBIT;
		
		query_emit_id(NULL, PROTOCOL_MESSAGE, &me, &me, &rcpts, &msg, &format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(msg);
		xfree(me);
		array_free(rcpts);
	}

	if (!quiet)
		session_unidle(session);

	return 0;
}

static COMMAND(jabber_command_inline_msg)
{
	const char *p[2] = { NULL, params[0] };
	if (!params[0] || !target)
		return -1;
	return jabber_command_msg(("chat"), p, session, target, quiet);
}

static COMMAND(jabber_command_xml)
{
	jabber_private_t *j = session_private_get(session);

	if (!(j->send_watch)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	watch_write(j->send_watch, "%s", params[0]);
	return 0;
}

static COMMAND(jabber_command_away)
{
	const char *descr, *format;
	
	if (params[0]) {
		session_descr_set(session, (!xstrcmp(params[0], "-")) ? NULL : params[0]);
		reason_changed = 1;
	} 
	if (!xstrcmp(name, ("_autoback"))) {
		format = "auto_back";
		session_status_set(session, EKG_STATUS_AUTOBACK);
		session_unidle(session);
	} else if (!xstrcmp(name, ("back"))) {
		format = "back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xstrcmp(name, ("_autoaway"))) {
		format = "auto_away";
		session_status_set(session, EKG_STATUS_AUTOAWAY);
	} else if (!xstrcmp(name, ("_autoxa"))) {
		format = "auto_xa";
		session_status_set(session, EKG_STATUS_AUTOXA);
	} else if (!xstrcmp(name, ("away"))) {
		format = "away"; 
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
	} else if (!xstrcmp(name, ("dnd"))) {
		format = "dnd";
		session_status_set(session, EKG_STATUS_DND);
		session_unidle(session);
	} else if (!xstrcmp(name, ("ffc"))) {
	        format = "ffc";
	        session_status_set(session, EKG_STATUS_FFC);
                session_unidle(session);
        } else if (!xstrcmp(name, ("xa"))) {
		format = "xa";
		session_status_set(session, EKG_STATUS_XA);
		session_unidle(session);
	} else if (!xstrcmp(name, ("invisible"))) {
		format = "invisible";
		session_status_set(session, EKG_STATUS_INVISIBLE);
		session_unidle(session);
	} else
		return -1;
	if (!params[0]) {
                char *tmp;

                if (!config_keep_reason) {
                        session_descr_set(session, NULL);
                } else if ((tmp = ekg_draw_descr(session_status_get(session)))) {
                        session_descr_set(session, tmp);
                        xfree(tmp);
                }
	}

	descr = (char *) session_descr_get(session);

	ekg_update_status(session);
	
	if (descr) {
		char *f = saprintf("%s_descr", format);
		printq(f, descr, "", session_name(session));
		xfree(f);
	} else
		printq(format, session_name(session));

	if (session_connected_get(session)) 
		jabber_write_status(session);
	
	return 0;
}

static COMMAND(jabber_command_passwd)
{
	jabber_private_t *j = session_private_get(session);
	char *username;
	char *passwd;

	username = xstrdup(session->uid + 5);
	*(xstrchr(username, '@')) = 0;

	if (!params[0]) {
		char *tmp = password_input();
		if (!tmp)
			return -1;
		passwd = jabber_escape(tmp);
		session_set(session, "__new_password", tmp);
		xfree(tmp);
	} else {
		passwd = jabber_escape(params[0]);
		session_set(session, "__new_password", params[0]);
	}

	watch_write(j->send_watch, 
		"<iq type=\"set\" to=\"%s\" id=\"passwd%d\"><query xmlns=\"jabber:iq:register\"><username>%s</username><password>%s</password></query></iq>",
		j->server, j->id++, username, passwd);
	

	xfree(username);
	xfree(passwd);

	return 0;
}

static COMMAND(jabber_command_auth) {
	jabber_private_t *j = session->priv;

	const char *action;
	const char *uid;

	if (params[1])
		target = params[1];
	else if (!target) {
		printq("invalid_params", name);
		return -1;
	}

	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;

	/* user jest OK, wiêc lepiej mieæ go pod rêk± */
	tabnick_add(uid);

	if (match_arg(params[0], 'r', ("request"), 2)) {
		action = "subscribe";
		printq("jabber_auth_request", uid+5, session_name(session));
	} else if (match_arg(params[0], 'a', ("accept"), 2)) {
		action = "subscribed";
		printq("jabber_auth_accept", uid+5, session_name(session));
	} else if (match_arg(params[0], 'c', ("cancel"), 2)) {
		action = "unsubscribe";
		printq("jabber_auth_unsubscribed", uid+5, session_name(session));
	} else if (match_arg(params[0], 'd', ("deny"), 2)) {
		action = "unsubscribed";

		if (userlist_find(session, uid))  // mamy w rosterze
			printq("jabber_auth_cancel", uid+5, session_name(session));
		else // nie mamy w rosterze
			printq("jabber_auth_denied", uid+5, session_name(session));
	
	} else if (match_arg(params[0], 'p', ("probe"), 2)) {	/* TLEN ? */
	/* ha! undocumented :-); bo 
	   [Used on server only. Client authors need not worry about this.] */
		action = "probe";
		printq("jabber_auth_probe", uid+5, session_name(session));
	} else {
		printq("invalid_params", name);
		return -1;
	}
	/* NOTE: libtlen send this without id */
	watch_write(j->send_watch, "<presence to=\"%s\" type=\"%s\" id=\"roster\"/>", uid+5, action);
	return 0;
}

static COMMAND(jabber_command_modify) {
	jabber_private_t *j = session->priv;

	int addcom = !xstrcmp(name, ("add"));

	const char *uid = NULL;
	char *nickname = NULL;
	list_t m;
	userlist_t *u;

		/* instead of PARAMASTARGET, 'cause that one fails with /add username in query */
	if (get_uid(session, params[0])) {
			/* XXX: create&use shift()? */
		target = params[0];
		params++;
	}
	
	u = userlist_find(session, target);

	if (u && addcom) {	/* don't allow to add user again */
		printq("user_exists_other", (params[0] ? params[0] : target), format_user(session, u->uid), session_name(session));
		return -1;
	}

	if (!u && !addcom) {
		printq("user_not_found", target);
		return -1;
	}

		/* XXX: jid_target2uid() ? */
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;

	if (!u)	u = xmalloc(sizeof(userlist_t));		/* alloc temporary memory for /xmpp:add */

	if (params[0]) {
		char **argv = array_make(params[0], " \t", 0, 1, 1);
		int i;

		for (i = 0; argv[i]; i++) {
			if (match_arg(argv[i], 'g', ("group"), 2) && argv[i + 1]) {
				char **tmp = array_make(argv[++i], ",", 0, 1, 1);
				int x, off;	/* je¶li zaczyna siê od '@', pomijamy pierwszy znak */

				for (x = 0; tmp[x]; x++)
					switch (*tmp[x]) {
						case '-':
							off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

							if (ekg_group_member(u, tmp[x] + 1 + off)) {
								ekg_group_remove(u, tmp[x] + 1 + off);
							} else {
								printq("group_member_not_yet", format_user(session, uid), tmp[x] + 1);
							}
							break;
						case '+':
							off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

							if (!ekg_group_member(u, tmp[x] + 1 + off)) {
								ekg_group_add(u, tmp[x] + 1 + off);
							} else {
								printq("group_member_already", format_user(session, uid), tmp[x] + 1);
							}
							break;
						default:
							off = (tmp[x][0] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

							if (!ekg_group_member(u, tmp[x] + off)) {
								ekg_group_add(u, tmp[x] + off);
							} else {
								printq("group_member_already", format_user(session, uid), tmp[x]);
							}
					}

				array_free(tmp);
				continue;
			}
		/* emulate gg:modify behavior */
			if (!j->istlen && match_arg(argv[i], 'o', ("online"), 2)) {	/* only jabber:iq:privacy */
				command_exec_format(target, session, 0, ("/xmpp:privacy --set %s +pin"), uid);
				continue;
			}
			
			if (!j->istlen && match_arg(argv[i], 'O', ("offline"), 2)) {	/* only jabber:iq:privacy */
				command_exec_format(target, session, 0, ("/xmpp:privacy --set %s -pin"), uid);
				continue;
			}
						/*    if this is -n smth */
						/* OR if param doesn't looks like command treat as a nickname */
			if ((match_arg(argv[i], 'n', ("nickname"), 2) && argv[i + 1] && i++) || argv[i][0] != '-') {
				if (userlist_find(session, argv[i])) {
					printq("user_exists", argv[i], session_name(session));
					continue;
				}

				xfree(nickname);
				nickname = tlenjabber_escape(argv[i]);
				continue;
			}
		}
		array_free(argv);
	}

	if (!nickname && !addcom)
		nickname = tlenjabber_escape(u->nickname);		/* use current nickname */
	
	if (j->send_watch) j->send_watch->transfer_limit = -1;	/* let's send this in one/two packets not in 7 or more. */

	watch_write(j->send_watch, "<iq type=\"set\"><query xmlns=\"jabber:iq:roster\">");

	/* nickname always should be set */
	if (nickname)	watch_write(j->send_watch, "<item jid=\"%s\" name=\"%s\"%s>", uid+5, nickname, (u->groups ? "" : "/"));
	else		watch_write(j->send_watch, "<item jid=\"%s\"%s>", uid+5, (u->groups ? "" : "/"));

	for (m = u->groups; m ; m = m->next) {
		struct ekg_group *g = m->data;
		char *gname = tlenjabber_escape(g->name);

		watch_write(j->send_watch, "<group>%s</group>", gname);
		xfree(gname);
	}

	if (u->groups)
		watch_write(j->send_watch, "</item>");

	watch_write(j->send_watch, "</query></iq>");
	JABBER_COMMIT_DATA(j->send_watch); 

	xfree(nickname);
	if (addcom) {
		xfree(u);
		return (session_int_get(session, "auto_auth") & 16 ? 0 :
			command_exec_format(target, session, quiet, ("/auth --request %s"), uid));
	}
	return 0;
}

static COMMAND(jabber_command_del) {
	jabber_private_t *j	= session->priv;
	int del_all		= !xstrcmp(params[0], "*");
	int n			= 0;
	const char *uid;
	list_t l;

		/* This is weird, I know. But it should take care of both single and multiple
		 * removal the simplest way, I think */
	for (l = session->userlist; l; l = l->next) {
		userlist_t *u = (del_all ? l->data : userlist_find_u(&l, target));

		if (u) {
			if (!(uid = u->uid) || xstrncmp(uid, jabber_prefixes[j->istlen], 5)) {
				printq("invalid_session");
				return -1;
			}

			if (!n) {
				j->send_watch->transfer_limit = -1;
				watch_write(j->send_watch, "<iq type=\"set\" id=\"roster\"><query xmlns=\"jabber:iq:roster\">");
			}
			watch_write(j->send_watch, "<item jid=\"%s\" subscription=\"remove\"/>", uid+5);

			n++;
		}

		if (!del_all)
			break;
	}

	if (n) {
		watch_write(j->send_watch, "</query></iq>");
		JABBER_COMMIT_DATA(j->send_watch);
	}

	if (del_all)
		printq(n ? "user_cleared_list" : "list_empty", session_name(session));
	else
		printq(n ? "user_deleted" : "user_not_found", target, session_name(session));
	return 0;
}

/*
 * Warning! This command is kinda special:
 * it needs destination uid, so destination must be in our userlist.
 * It won't work for unknown JIDs.
 * When implementing new command don't use jabber_command_ver as template.
 */

static COMMAND(jabber_command_ver)
{
	const char *uid;
        userlist_t *ut;
	list_t l;
	int once = 0;

	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;

	if (!(ut = userlist_find(session, uid))) {
		print("user_not_found", target);
		return -1;
	}
	if (ut->status <= EKG_STATUS_NA) {
		print("jabber_status_notavail", session_name(session), ut->uid);
		return -1;
	}

	if (!ut->resources) {
		print("jabber_unknown_resource", session_name(session), target);
		return -1;
	}

	for (l = ut->resources; l; l = l->next) {	/* send query to each resource */
		ekg_resource_t *r = l->data;

		char *to = saprintf("%s/%s", uid + 5, r->name);

		if (!jabber_iq_send(session, "versionreq_", JABBER_IQ_TYPE_GET, to, "query", "jabber:iq:version") && !once) {
			printq("generic_error", "Error while sending jabber:iq:version request, check debug window");
			once = 1;
		}
	}
	return 0;
}

static COMMAND(jabber_command_userinfo) {
	const char *uid;

	/* jabber id: [user@]host[/resource] */
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	
	if (!jabber_iq_send(session, "vcardreq_", JABBER_IQ_TYPE_GET, uid + 5, "vCard", "vcard-temp")) {
		printq("generic_error", "Error while sending vCard request, check debug window");
		return 1;
	}
	
	return 0;
}

static char *jabber_avatar_load(session_t *s, const char *path, const int quiet) {
	const char *fn = prepare_path_user(path);
	FILE *fd;
	struct stat st;
	char buf[16385]; /* XEP-0153 says we should limit avatar size to 8k,
			    but I like to be honest and give 16k */
	int len;

		/* code from dcc */
	if (!fn) {
		printq("generic_error", "path too long"); /* XXX? */
		return NULL;
	}

	if (!stat(fn, &st) && !S_ISREG(st.st_mode)) {
		printq("io_nonfile", path);
		return NULL;
	}

	if ((fd = fopen(fn, "r")) == NULL) {
		printq("io_cantopen", path, strerror(errno));
		return NULL;
	}

	if (!(len = fread(buf, 1, sizeof(buf), fd))) {
		if (ferror(fd))
			printq("io_cantread", path, strerror(errno)); /* can we use errno here? */
		else
			printq("io_emptyfile", path);
	} else if (len >= sizeof(buf))
		printq("io_toobig", path, itoa(len), sizeof(buf)-1);
	else {
		char *enc		= base64_encode(buf, len);
		char *out;
		const char *type	= "application/octet-stream";

		string_t str		= string_init(NULL);
		int enclen		= xstrlen(enc);
		char *p			= enc;

			/* those are from 'magic.mime', from 'file' utility */ 
		if (len > 4 && !xstrncmp(buf, "\x89PNG", 4))
			type = "image/png";
		else if (len > 3 && !xstrncmp(buf, "GIF", 3))
			type = "image/gif";
		else if (len > 2 && !xstrncmp(buf, "\xFF\xD8", 2))
			type = "image/jpeg";

		fclose(fd);
		session_set(s, "photo_hash", jabber_sha1_generic(buf, len));

		while (enclen > 72) {
			string_append_n(str, p, 72);
			string_append_c(str, '\n');
			
			p += 72;
			enclen -= 72;
		}
		string_append(str, p);
		xfree(enc);

		out = saprintf("<PHOTO><TYPE>%s</TYPE><BINVAL>\n%s\n</BINVAL></PHOTO>", type, str->str);
		string_free(str, 1);

		return out;
	}

	fclose(fd);
	return NULL;
}

static COMMAND(jabber_command_change)
{
#define pub_sz 6
#define strfix(s) (s ? s : "")
	jabber_private_t *j = session_private_get(session);
	char *pub[pub_sz] = { NULL, NULL, NULL, NULL, NULL, NULL };
	char *photo = NULL;
	const int hadphoto = !!session_get(session, "photo_hash");
	int i;

	for (i = 0; params[i]; i++) {
		if (match_arg(params[i], 'f', ("fullname"), 2) && params[i + 1]) {
			pub[0] = (char *) params[++i];
		} else if (match_arg(params[i], 'n', ("nickname"), 2) && params[i + 1]) {
			pub[1] = (char *) params[++i];
		} else if (match_arg(params[i], 'c', ("city"), 2) && params[i + 1]) {
			pub[2] = (char *) params[++i];
		} else if (match_arg(params[i], 'b', ("born"), 2) && params[i + 1]) {
			pub[3] = (char *) params[++i];
		} else if (match_arg(params[i], 'd', ("description"), 2) && params[i + 1]) {
			pub[4] = (char *) params[++i];
		} else if (match_arg(params[i], 'C', ("country"), 2) && params[i + 1]) {
			pub[5] = (char *) params[++i];
		} else if (match_arg(params[i], 'p', ("photo"), 2) && params[i + 1]) {
			photo = params[++i];
		}
	}
	for (i=0; i<pub_sz; i++) 
		pub[i] = jabber_escape(pub[i]);

	if (photo)
		photo = jabber_avatar_load(session, photo, quiet);
	else if (hadphoto)
		session_set(session, "photo_hash", NULL);

	watch_write(j->send_watch, "<iq type=\"set\"><vCard xmlns='vcard-temp'>"
			"<FN>%s</FN>" "<NICKNAME>%s</NICKNAME>"
			"<ADR><LOCALITY>%s</LOCALITY><COUNTRY>%s</COUNTRY></ADR>"
			"<BDAY>%s</BDAY><DESC>%s</DESC>%s</vCard></iq>\n", 
		strfix(pub[0]), strfix(pub[1]), strfix(pub[2]), strfix(pub[5]), strfix(pub[3]), strfix(pub[4]), strfix(photo));

	if (photo || hadphoto)
		jabber_write_status(session);

	xfree(photo);
	for (i=0; i<pub_sz; i++) 
		xfree(pub[i]);
	return 0;
}

static COMMAND(jabber_command_lastseen)
{
	const char *uid;

	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	
	if (!jabber_iq_send(session, "lastseenreq_", JABBER_IQ_TYPE_GET, uid + 5, "query", "jabber:iq:last")) {
		printq("generic_error", "Error while sending jabber:iq:last request, check debug window");
		return -1;
	}

	return 0;
}

static char **jabber_params_split(const char *line, int allow_empty)
{
	char **arr, **ret = NULL;
	int num = 0, i = 0, z = 0;

	if (!line)
		return NULL;

	arr = array_make(line, " ", 0, 1, 1);
	while (arr[i]) {
		ret = (char **)xrealloc (ret, (num + 2)*sizeof (char *));

		if (!z) {
			if (arr[i][0] == '-' && arr[i][1] == '-' && xstrlen(arr[i]) > 2)
				ret[num++] = xstrdup (arr[i]+2);
			else if (allow_empty) {
				ret[num++] = xstrdup("");
			} else {
				array_free (arr);
				ret[num] = NULL;
				array_free (ret);
				return NULL;
				//ret[num++] = xstrdup ("");
			}
			i++;
		} else {
			// this is the name of next param, so use "" as value and 
			// do not increment i, so we'll parse it in next loop
			if (arr[i][0] == '-' && arr[i][1] == '-' && xstrlen(arr[i]) > 2)
				ret[num++] = xstrdup("");
			else {
				ret[num++] = xstrdup(arr[i]);
				i++;
			}
		}
		z^=1;
	}
	// if the last is --param
	if (z) {
		ret = (char **)xrealloc (ret, (num + 2)*sizeof (char *));
		ret[num++] = xstrdup("");
	}
	ret [num] = NULL;

	array_free (arr);
	i = 0;
	while (ret[i]) {
		debug (" *[%d]* %s\n", i, ret[i]);
		i++;
	}
	return ret;
}

/**
 * jabber_command_search()
 *
 * @note implementation bug ? should server be last variable?
 *
 * @note any server support jabber:iq:search ?
 *
 */

static COMMAND(jabber_command_search) {
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : jabber_default_search_server ? jabber_default_search_server : j->server;	/* XXX j->server */

	char **splitted	= NULL;
	const char *id;

	if (array_count((char **) params) > 1 && !(splitted = jabber_params_split(params[1], 0))) {
		printq("invalid_params", name);
		return -1;
	}

	if (!(id = jabber_iq_reg(session, "search_", server, "query", "jabber:iq:search"))) {
		printq("generic_error", "Error in getting id for search request, check debug window");
		array_free(splitted);
		return 1;
	}

	if (j->send_watch) j->send_watch->transfer_limit = -1;

	watch_write(j->send_watch, 
		"<iq type=\"%s\" to=\"%s\" id=\"%s\"><query xmlns=\"jabber:iq:search\">", params[1] ? "set" : "get", server, id);

	if (splitted) {
		int i = 0;
		int use_x_data = 0;

		if (!xstrcmp(splitted[0], "jabber_x_data")) { 
			use_x_data = 1; i = 2; 
			watch_write(j->send_watch, "<x xmlns=\"jabber:x:data\" type=\"submit\">");
		} 

		for (; (splitted[i] && splitted[i+1]); i+=2) {
			char *value = jabber_escape(splitted[i+1]);
			if (use_x_data)
				watch_write(j->send_watch, "<field var=\"%s\"><value>%s</value></field>", splitted[i], value);
			else	watch_write(j->send_watch, "<%s>%s</%s>", splitted[i], value, splitted[i]);
			xfree(value);
		}

		if (use_x_data) watch_write(j->send_watch, "</x>");
	}
	watch_write(j->send_watch, "</query></iq>");
	array_free(splitted);

	JABBER_COMMIT_DATA(j->send_watch);

	return 0;
}

/**
 * jabber_command_private()
 *
 * @todo Read f**cking XEP-0048,--0049, and remove security hole in jabber_handle_iq()
 * 	 I suspect there should be somewhere info, about to=
 * 	 (We recv data from our JID (or requested), not server one.)
 */

static COMMAND(jabber_command_private) {
	jabber_private_t *j = jabber_private(session);
	char *namespace; 	/* <nazwa> */

	int config = 0;			/* 1 if name == jid:config */
	int bookmark = 0;		/* 1 if name == jid:bookmark */

	if (!xstrcmp(name, ("config")))		config = 1;
	if (!xstrcmp(name, ("bookmark")))	bookmark = 1;
	
	if (config)		namespace = ("ekg2 xmlns=\"ekg2:prefs\"");
	else if (bookmark)	namespace = ("storage xmlns=\"storage:bookmarks\"");
	else			namespace = (char *) params[1];

	if (bookmark) {				/* bookmark-only-commands */
		int bookmark_sync	= 0;			/* 0 - no sync; 1 - sync (item added); 2 - sync (item modified) 3 - sync (item removed)	*/
	
		if (match_arg(params[0], 'a', ("add"), 2))	bookmark_sync = 1;	/* add item */
		if (match_arg(params[0], 'm', ("modify"), 2))	bookmark_sync = 2; 	/* modify item */
		if (match_arg(params[0], 'r', ("remove"), 2))	bookmark_sync = 3; 	/* remove item */

		if (bookmark_sync) {
			const char *p[2]	= {("-p"), NULL};	/* --put */
			char **splitted		= NULL;
			
			splitted = jabber_params_split(params[1], 1);

			if (bookmark_sync && (!splitted && params[1])) {
				printq("invalid_params", name);
				return -1;
			}

			switch (bookmark_sync) {
				case (1):	{	/* add item */
						/* Usage: 
						 *	/jid:bookmark --add --url url [-- name]
						 * 	/jid:bookmark --add --conf jid [--autojoin 1] [--nick cos] [--pass cos] [-- name] 
						 */
						jabber_bookmark_t *book = NULL;

						if (!xstrcmp(splitted[0], "url")) {
							book		= xmalloc(sizeof(jabber_bookmark_t));
							book->type	= JABBER_BOOKMARK_URL;

							book->private.url = xmalloc(sizeof(jabber_bookmark_url_t));
							book->private.url->name = xstrdup(jabber_attr(splitted, ""));
							book->private.url->url	= xstrdup(splitted[1]);

						} else if (!xstrcmp(splitted[0], "conf")) {
							book		= xmalloc(sizeof(jabber_bookmark_t));
							book->type	= JABBER_BOOKMARK_CONFERENCE;

							book->private.conf = xmalloc(sizeof(jabber_bookmark_conference_t));
							book->private.conf->name = xstrdup(jabber_attr(splitted, ""));
							book->private.conf->jid	= xstrdup(splitted[1]);
							book->private.conf->nick= xstrdup(jabber_attr(splitted, "nick")); 
							book->private.conf->pass= xstrdup(jabber_attr(splitted, "pass"));

							if (jabber_attr(splitted, "autojoin") && atoi(jabber_attr(splitted, "autojoin")))	book->private.conf->autojoin = 1;
/*							else											book->private.conf->autojoin = 0; */
						} else bookmark_sync = -1;
						if (book) list_add(&(j->bookmarks), book, 0);
					}
					break;
				case (2):		/* modify item XXX */
				case (3):		/* remove item XXX */
				default:	/* error */
					bookmark_sync = -bookmark_sync;		/* make it negative -- error */
					debug("[JABBER, BOOKMARKS] switch(bookmark_sync) sync=%d ?!\n", bookmark_sync);
			}

			array_free(splitted);
			if (bookmark_sync > 0) {
				return jabber_command_private(name, (const char **) p, session, target, quiet); /* synchronize db */
			} else if (bookmark_sync < 0) {
				debug("[JABBER, BOOKMARKS] sync=%d\n", bookmark_sync);
				printq("invalid_params", name);
				return -1;
			}
		}
	}

	if (match_arg(params[0], 'g', ("get"), 2) || match_arg(params[0], 'd', ("display"), 2)) {	/* get/display */
		const char *id;

		if (!(id = jabber_iq_reg(session, 
				(match_arg(params[0], 'g', ("get"), 2) && (config || bookmark) ) ? "config_" : "private_",
				NULL, "query", "jabber:iq:private"))) 
		{
			printq("generic_error", "Error in getting id for jabber:iq:private GET/DISPLAY request, check debug window");
			return 1;
		}

		watch_write(j->send_watch, "<iq type=\"get\" id=\"%s\"><query xmlns=\"jabber:iq:private\"><%s/></query></iq>", id, namespace);
		return 0;
	}

	if (match_arg(params[0], 'p', ("put"), 2)) {							/* put */
		list_t l;
		const char *id;

		if (!(id = jabber_iq_reg(session, "private_", NULL, "query", "jabber:iq:private"))) {
			printq("generic_error", "Error in getting id for jabber:iq:private PUT request, check debug window");
			return 1;
		}

		if (j->send_watch) j->send_watch->transfer_limit = -1;

		watch_write(j->send_watch, "<iq type=\"set\" id=\"%s\"><query xmlns=\"jabber:iq:private\"><%s>", id, namespace);

/* Synchronize config (?) */
		if (config) {
			for (l = plugins; l; l = l->next) {
				plugin_t *p = l->data;
				list_t n;
				watch_write(j->send_watch, "<plugin xmlns=\"ekg2:plugin\" name=\"%s\" prio=\"%d\">", p->name, p->prio);
back:
				for (n = variables; n; n = n->next) {
					variable_t *v = n->data;
					char *vname, *tname;
					if (v->plugin != p) continue;
					tname = vname = jabber_escape(v->name);

					if (p && !xstrncmp(tname, p->name, xstrlen(p->name))) tname += xstrlen(p->name);
					if (tname[0] == ':') tname++;
				
					switch (v->type) {
						case(VAR_STR):
						case(VAR_FOREIGN):
						case(VAR_FILE):
						case(VAR_DIR):
						case(VAR_THEME):
							if (*(char **) v->ptr)	watch_write(j->send_watch, "<%s>%s</%s>", tname, *(char **) v->ptr, tname);
							else			watch_write(j->send_watch, "<%s/>", tname);
							break;
						case(VAR_INT):
						case(VAR_BOOL):
							watch_write(j->send_watch, "<%s>%d</%s>", tname, *(int *) v->ptr, tname);
							break;
						case(VAR_MAP):	/* XXX TODO */
						default:
							break;
					}
					xfree(vname);
				}
				if (p) watch_write(j->send_watch, "</plugin>");
				if (p && !l->next) { p = NULL; goto back; }
			}
			for (l = sessions; l; l = l->next) {
				session_t *s = l->data;
				plugin_t *pl = s->plugin;
				int i;

				if (!pl) {
					printq("generic_error", "Internal fatal error, plugin somewhere disappear. Report this bug");
					continue;
				}

				watch_write(j->send_watch, "<session xmlns=\"ekg2:session\" uid=\"%s\" password=\"%s\">", s->uid, s->password);

				/* XXX, escape? */
				for (i = 0; (pl->params[i].key /* && p->params[i].id != -1 */); i++) {
					if (s->values[i])	watch_write(j->send_watch, "<%s>%s</%s>", pl->params[i].key, s->values[i], pl->params[i].key);
					else			watch_write(j->send_watch, "<%s/>", pl->params[i].key);
				}

				watch_write(j->send_watch, "</session>");
			}

			goto put_finish;
		}

/* Synchronize bookmarks (?) */
		if (bookmark) {
			list_t l;
			for (l = j->bookmarks; l; l = l->next) {
				jabber_bookmark_t *book = l->data;

				switch (book->type) {
					case (JABBER_BOOKMARK_URL):
						watch_write(j->send_watch, "<url name=\"%s\" url=\"%s\"/>", book->private.url->name, book->private.url->url);
						break;
					case (JABBER_BOOKMARK_CONFERENCE):
						watch_write(j->send_watch, "<conference name=\"%s\" autojoin=\"%s\" jid=\"%s\">", book->private.conf->name, 
							book->private.conf->autojoin ? "true" : "false", book->private.conf->jid);
						if (book->private.conf->nick) watch_write(j->send_watch, "<nick>%s</nick>", book->private.conf->nick);
						if (book->private.conf->pass) watch_write(j->send_watch, "<password>%s</password>", book->private.conf->pass);
						watch_write(j->send_watch, "</conference>");
						break;
					default:
						debug("[JABBER, BOOKMARK] while syncing j->bookmarks... book->type = %d wtf?\n", book->type);
				}
			}

			goto put_finish;
		} 

/* Do what user want */
		if (params[0] && params[1] && params[2]) /* XXX check */
			watch_write(j->send_watch, "%s", params[2]);

put_finish:
		{
			char *beg = namespace;
			char *end = xstrstr(namespace, (" "));

/*			if (end) *end = '\0'; */	/* SEGV? where? why? :( */

			if (end) beg = xstrndup(namespace, end-namespace);
			else	 beg = xstrdup(namespace);

			watch_write(j->send_watch, "</%s></query></iq>", beg);

			xfree(beg);
		}
		JABBER_COMMIT_DATA(j->send_watch);
		return 0;
	}

	if (match_arg(params[0], 'c', ("clear"), 2)) {						/* clear */
		const char *id;

		if (!(id = jabber_iq_reg(session, "private_", NULL, "query", "jabber:iq:private"))) {
			printq("generic_error", "Error in getting id for jabber:iq:private CLEAR request, check debug window");
			return 1;
		}

		if (bookmark)
			jabber_bookmarks_free(j);			/* let's destroy previously saved bookmarks */

		watch_write(j->send_watch, "<iq type=\"set\" id=\"%s\"><query xmlns=\"jabber:iq:private\"><%s/></query></iq>", id, namespace);
		return 0;
	}

	printq("invalid_params", name);
	return -1;
}

static COMMAND(jabber_command_register)
{
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *passwd = session_get(session, "password");
	const int unregister = !xstrcmp(name, "unregister");
	char **splitted	= NULL;

	if (!session_connected_get(session)) {
		if ((!passwd || !*passwd || !xstrcmp(passwd, "foo"))) {
			session_set(session, "__new_account", "1");
			if (params[0]) session_set(session, "password", params[0]);
			jabber_command_connect(("connect"), NULL, session, target, quiet);
			return 0;
		}
		printq("not_connected", session_name(session));
		return -1;
	}

	if (!j->send_watch) return -1;
	j->send_watch->transfer_limit = -1;

	if (array_count((char **) params) > 1 && !(splitted = jabber_params_split(params[1], 0))) {
		printq("invalid_params", name);
		return -1;
	}
	watch_write(j->send_watch, "<iq type=\"%s\" to=\"%s\" id=\"transpreg%d\"><query xmlns=\"jabber:iq:register\">", params[1] || unregister ? "set" : "get", server, j->id++);
	if (unregister)
		watch_write(j->send_watch, "<remove/>");
	if (splitted) {
		int i = 0;
		int use_x_data = 0;

		if (!xstrcmp(splitted[0], "jabber_x_data")) { 
			use_x_data = 1; i = 2; 
			watch_write(j->send_watch, "<x xmlns=\"jabber:x:data\" type=\"submit\">");
		} 

		for (; (splitted[i] && splitted[i+1]); i+=2) {
			if (use_x_data)
				watch_write(j->send_watch, "<field var=\"%s\"><value>%s</value></field>", splitted[i], splitted[i+1]);
			else	watch_write(j->send_watch, "<%s>%s</%s>", splitted[i], splitted[i+1], splitted[i]);
		}

		if (use_x_data) watch_write(j->send_watch, "</x>");
	}
	watch_write(j->send_watch, "</query></iq>");
	array_free (splitted);

	JABBER_COMMIT_DATA(j->send_watch);
	return 0;
}

static COMMAND(jabber_command_transpinfo) {
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *node   = (params[0] && params[1]) ? params[1] : NULL;
	
	const char *id;
	
	if (!(id = jabber_iq_reg(session, "transpinfo_", server, "query", "http://jabber.org/protocol/disco#info"))) {
		printq("generic_error", "Error in getting id for transport info request, check debug window");
		return 1;
	}

	if (node) {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/disco#info\" node=\"%s\"/></iq>",
			server, id, node);
	} else {
		watch_write(j->send_watch, 
			"<iq type=\"get\" to=\"%s\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/disco#info\"/></iq>", 
			server, id);
	}
	return 0;

}

static COMMAND(jabber_command_transports) {
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *node   = (params[0] && params[1]) ? params[1] : NULL;

	const char *id;

	if (!(id = jabber_iq_reg(session, "transplist_", server, "query", "http://jabber.org/protocol/disco#items"))) {
		printq("generic_error", "Error in getting id for transport list request, check debug window");
		return 1;
	}
	
	if (node) {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"%s\"/></iq>",
			server, id, node);
	} else {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/disco#items\"/></iq>",
			server, id);
	}
	return 0;
}

static COMMAND(jabber_command_vacation) { /* JEP-0109: Vacation Messages (DEFERRED) */
	jabber_private_t *j = session_private_get(session);

	char *message;
	const char *id;

	if (!(id = jabber_iq_reg(session, "vacationreq_", NULL, "query", "http://jabber.org/protocol/vacation"))) {
		printq("generic_error", "Error in getting id for vacation request, check debug window");
		return 1;
	}

	message = jabber_escape(params[0]);

/* XXX, porobic potwierdzenia ustawiania/ usuwania. oraz jesli nie ma statusu to wyswylic jakies 'no vacation status'... */

	if (!params[0]) {
		watch_write(j->send_watch, "<iq type=\"get\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/vacation\"/></iq>", id);
	} else if (xstrlen(params[0]) == 1 && params[0][0] == '-') {
		watch_write(j->send_watch, "<iq type=\"set\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/vacation\"/></iq>", id);
	} else {
		watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"%s\"><query xmlns=\"http://jabber.org/protocol/vacation\">"
			"<start/><end/>" /* XXX, startdate, enddate */
			"<message>%s</message>"
			"</query></iq>", 
			id, message);
	}
	xfree(message);
	return 0;
}

static COMMAND(jabber_muc_command_join) {
	/* params[0] - full channel name, 
	 * params[1] - nickname || default 
	 * params[2] - password || none
	 *
	 * XXX: make (session) variable jabber:default_muc && then if exists and params[0] has not specific server than append '@' jabber:default_muc and use it.
	 * XXX: make (session) variable jabber:default_nickname.
	 * XXX: history requesting, none history requesting.. etc
	 */
	jabber_private_t *j = session_private_get(session);
	newconference_t *conf;
	char *tmp;
	char *username = (params[1]) ? xstrdup(params[1]) : (tmp = xstrchr(session->uid, '@')) ? xstrndup(session->uid+5, tmp-session->uid-5) : NULL;
	char *password = (params[1] && params[2]) ? saprintf("<password>%s</password>", params[2]) : NULL;

	if (!username) { /* rather impossible */
		printq("invalid_params", name);
		return -1;
	}

	if (!xstrncmp(target, "xmpp:", 5)) target += 5; /* remove xmpp: */

	watch_write(j->send_watch, "<presence to='%s/%s'><x xmlns='http://jabber.org/protocol/muc#user'>%s</x></presence>", 
			target, username, password ? password : "");
	{
		char *uid = saprintf("xmpp:%s", target);
		conf = newconference_create(session, uid, 1);
		conf->private = xstrdup(username);
		xfree(uid);
	}

	xfree(username);
	xfree(password);
	return 0;
}

static COMMAND(jabber_muc_command_part) {
	jabber_private_t *j = session_private_get(session);
	newconference_t *c;
	char *status;

	if (!(c = newconference_find(session, target))) {
		printq("generic_error", "/xmpp:part only valid in MUC");
		return -1;
	}

	status = (params[0] && params[1]) ? saprintf(" <status>%s</status> ", params[1]) : NULL;

	watch_write(j->send_watch, "<presence to=\"%s/%s\" type=\"unavailable\">%s</presence>", c->name+5, c->private, status ? status : "");

	xfree(status);
	newconference_destroy(c, 1 /* XXX, dorobic zmienna */);
	return 0;
}

static COMMAND(jabber_muc_command_admin) {
	jabber_private_t *j = session_private_get(session);
	newconference_t *c;

	if (!(c = newconference_find(session, target))) {
		printq("generic_error", "/xmpp:admin only valid in MUC");
		return -1;
	}

	if (!params[1]) {
		watch_write(j->send_watch,
			"<iq id=\"mucadmin%d\" to=\"%s\" type=\"get\">"
			"<query xmlns=\"http://jabber.org/protocol/muc#owner\"/>"
			"</iq>", j->id++, c->name+5);
	} else {
		char **splitted = NULL;
		int i;
		int isinstant = !xstrcmp(params[1], "--instant");

		if (isinstant) {
			watch_write(j->send_watch,
				"<iq type=\"set\" to=\"%s\" id=\"mucadmin%d\">"
				"<query xmlns=\"http://jabber.org/protocol/muc#owner\">"
				"<x xmlns=\"jabber:x:data\" type=\"submit\"/>"
				"</query></iq>", c->name+5, j->id++);
			return 0;
		}

		if (!(splitted = jabber_params_split(params[1], 0))) {
			printq("invalid_params", name);
			return -1;
		}

		if (j->send_watch) j->send_watch->transfer_limit = -1;

		watch_write(j->send_watch, 
				"<iq type=\"set\" to=\"%s\" id=\"mucadmin%d\">"
				"<query xmlns=\"http://jabber.org/protocol/muc#owner\">"
				"<x xmlns=\"jabber:x:data\" type=\"submit\">"
/*				"<field var=\"FORM_TYPE\"><value>http://jabber.org/protocol/muc#roomconfig/value></field>" */
				,c->name+5, j->id++);

		for (i=0; (splitted[i] && splitted[i+1]); i+=2) {
			char *name	= jabber_escape(splitted[i]);
			char *value	= jabber_escape(splitted[i+1]);

			watch_write(j->send_watch, "<field var=\"%s\"><value>%s</value></field>", name, value);

			xfree(value);	xfree(name);
		}
		array_free(splitted);
		watch_write(j->send_watch, "</x></query></iq>");
		JABBER_COMMIT_DATA(j->send_watch);
	}
	return 0;
}

static COMMAND(jabber_muc_command_ban) {	/* %0 [target] %1 [jid] %2 [reason] */
	jabber_private_t *j = session_private_get(session);
	newconference_t *c;
	
	if (!(c = newconference_find(session, target))) {
		printq("generic_error", "/xmpp:ban && /xmpp:kick && /xmpp:unban only valid in MUC");
		return -1;
	}
/* XXX, make check if command = "kick" than check if user is on the muc channel... cause we can make /unban */

	if (!params[1]) {
		watch_write(j->send_watch, 
			"<iq id=\"%d\" to=\"%s\" type=\"get\">"
			"<query xmlns=\"http://jabber.org/protocol/muc#admin\"><item affiliation=\"outcast\"/></query>"
			"</iq>", j->id++, c->name+5);
	} else {
		char *reason	= jabber_escape(params[2]);
		const char *jid	= params[1];

		if (!xstrncmp(jid, "xmpp:", 5)) jid += 5;

		watch_write(j->send_watch,
			"<iq id=\"%d\" to=\"%s\" type=\"set\">"
			"<query xmlns=\"http://jabber.org/protocol/muc#admin\"><item affiliation=\"%s\" jid=\"%s\"><reason>%s</reason></item></query>"
			"</iq>", j->id++, c->name+5, 
				!xstrcmp(name, "ban") ? /* ban */ "outcast" : /* unban+kick */ "none", 
			jid, reason ? reason : "");
		xfree(reason);
	}
	return 0;
}

static COMMAND(jabber_muc_command_topic) {
	jabber_private_t *j = session_private_get(session);
	newconference_t *c;
/* XXX da, /topic is possible in normal talk too... current limit only to muc. */
	if (!(c = newconference_find(session, target))) {
		printq("generic_error", "/xmpp:topic only valid in MUC");
		return -1;
	}
	
	if (!params[1]) {
		/* XXX, display current topic */

	} else {
		char *subject = jabber_escape(params[1]);
		watch_write(j->send_watch, "<message to=\"%s\" type=\"groupchat\"><subject>%s</subject></message>", c->name+5, subject);
	} 

	return 0;
}

/**
 * tlen_command_alert()
 *
 * XXX, info<br>
 * <b>ONLY TLEN PROTOCOL</b>
 *
 * @param params [0] - uid of target [target can be passed in params[0] COMMAND_PARAMASTARGET] [target is uid COMMAND_TARGET_VALID_UID]
 *
 * @return	-1 if wrong uid/session<br>
 * 		 0 on success<br>
 */

static COMMAND(tlen_command_alert) {
	jabber_private_t *j = jabber_private(session);

	if (!j->istlen) {				/* check if this is tlen session */
		printq("invalid_session");
		return -1;
	}

	if (tolower(target[0] != 't')) {		/* check if uid starts with 't' [tlen:] */
		printq("invalid_uid");
		return -1;
	}
	
	watch_write(j->send_watch, "<m to='%s' tp='a'/>", target+5);	/* sound alert */

	printq("tlen_alert_send", session_name(session), format_user(session, target));
	return 0;
}

static COMMAND(jabber_command_reply)
{
	jabber_private_t *j	= session_private_get(session);
	int subjectlen		= xstrlen(config_subject_prefix);
	int id, ret;
	char *tmp		= NULL;
	jabber_conversation_t *thr	= NULL;

	if (((params[0][0] == '#') && (id = atoi(params[0]+1)) > 0) /* #reply-id */
			|| ((id = atoi(params[0])) > 0)) { /* or without # */
		debug("We have id = %d!\n", id);
		thr = jabber_conversation_get(j, id);
	}
		/* XXX: some UID/thread/whatever match? */

	if (!thr) {
		printq("invalid_params", name);
		return -1;
	}

	debug("[jabber]_reply(), thread %d, thread-id = %s, subject = %s, uid = %s...\n", id, thr->thread, thr->subject, thr->uid);
	
		/* subject here */
	if (thr->subject && (!config_subject_prefix || xstrncmp(params[1], config_subject_prefix, subjectlen))) {
		tmp = saprintf("%s%s%s\n", config_subject_prefix,
			(xstrncmp(thr->subject, config_subject_reply_prefix, xstrlen(config_subject_reply_prefix))
			 ? config_subject_reply_prefix : ""), thr->subject);
	}

	ret = command_exec_format(target, session, 0, "/xmpp:%smsg %s %s %s%s",
		(thr->thread ? "t" : ""), thr->uid, (thr->thread ? thr->thread : ""), (tmp ? tmp : ""), params[1]);
	xfree(tmp);
	
	return ret;
}

static COMMAND(jabber_command_conversations)
{
        jabber_private_t *j	= session_private_get(session);
	int i;
	jabber_conversation_t *thr;
	
	if (!(thr = j->conversations))
		return 0;
	
	print("jabber_conversations_begin", session_name(session));
	for (i = 1; thr; i++, thr = thr->next) {
		print("jabber_conversations_item", itoa(i), get_nickname(session, thr->uid),
				(thr->subject ? thr->subject : format_find("jabber_conversations_nosubject")),
				(thr->thread ? thr->thread : format_find("jabber_conversations_nothread")));
	}
	print("jabber_conversations_end");
	
	return 0;
}

	/* like gg:find, mix of xmpp:userinfo & xmpp:search */
static COMMAND(jabber_command_find)
{
	if (get_uid(session, params[0])) {
		target = params[0];
		params++;
	}

	if (params[0] || !target) /* shifted */
		return jabber_command_search("search", params, session, NULL, quiet);
	else
		return jabber_command_userinfo("userinfo", params, session, target, quiet);
}

static COMMAND(jabber_command_userlist)
{
								/* we must use other userlist path, so that ekg2 will not overwrite it */
	const char *listfile	= (params[1] ? prepare_path_user(params[1]) : prepare_pathf("%s-userlist-backup", session_uid_get(session)));
	list_t l;
	const int replace = match_arg(params[0], 'G', "replace", 2);

	if (match_arg(params[0], 'c', "clear", 2) || replace) {	/* clear the userlist */
		const char *args[] = { "*", NULL };

			/* if using 'replace', we don't wan't any output from 'del *' */
		jabber_command_del("del", args, session, NULL, replace);
	}

	if (match_arg(params[0], 'g', "get", 2) || replace) {	/* fill userlist with data from file */
		const int istlen = jabber_private(session)->istlen;

		FILE *f = fopen(listfile, "r");
		char *line;

		if (!f) {
			printq("io_cantopen", listfile, strerror(errno));
			return -1;
		}

		while ((line = read_file(f, 0))) {
			char *uid = &line[2];
			char *nickname;

			if (xstrncmp(line, "+,", 2)) { /* XXX: '-'? */
				debug_error("jabber_command_userlist(), unknown op on '%s'\n", line);
				continue;
			}

			if ((nickname = xstrchr(uid, ','))) {
				char *p;

				*(nickname++) = '\0';
				if ((p = xstrchr(nickname, ',')))
					*p = '\0';
			}

			uid = saprintf(istlen ? "tlen:%s" : "xmpp:%s", uid);

			if (userlist_find(session, uid)) {
				if (nickname) {
					const char *args[] = { uid, "-n", nickname, NULL };

					jabber_command_modify("modify", args, session, NULL, 1);
				}
			} else {
				const char *args[] = { uid, nickname, NULL };

				jabber_command_modify("add", args, session, NULL, 1);
			}

			xfree(uid);
		}
		fclose(f);
		printq("userlist_get_ok", session_name(session));
	} else if (match_arg(params[0], 'p', "put", 2)) {	/* write userlist into file */
		FILE *f = fopen(listfile, "w");

		if (!f) {
			printq("io_cantopen", listfile, strerror(errno));
			return -1;
		}

		for (l = session->userlist; l; l = l->next) {
			userlist_t *u = l->data;

			if (u)
				fprintf(f, "+,%s,%s,\n", u->uid+5, u->nickname /*, XXX? */); /* JRU syntax */ 
		}

		fclose(f);
		printq("userlist_put_ok", session_name(session));
	}

	return 0;
}

static COMMAND(jabber_command_stanzas) {
	jabber_private_t *j	= session_private_get(session);
	list_t l;

	for (l = j->iq_stanzas; l; l = l->next) {
		jabber_stanza_t *st = l->data;

		printq("jabber_iq_stanza", session_name(session), st->type, st->xmlns, st->to, st->id);
	}
	return 0;
}

void jabber_register_commands()
{
#define JABBER_ONLY         SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define JABBER_FLAGS        JABBER_ONLY  | SESSION_MUSTBECONNECTED
		/* XXX: I changed all '* | COMMAND_ENABLEREQPARAMS' to JABBER_FLAGS_REQ,
		 * 'cause I don't see any sense in executing connection-requiring commands
		 * without SESSION_MUSTBECONNECTED */
#define JABBER_FLAGS_REQ    		JABBER_FLAGS | COMMAND_ENABLEREQPARAMS
#define JABBER_FLAGS_TARGET 		JABBER_FLAGS_REQ | COMMAND_PARAMASTARGET
#define JABBER_FLAGS_TARGET_VALID	JABBER_FLAGS_TARGET | COMMAND_TARGET_VALID_UID	/* need audit, if it can be used everywhere instead JABBER_FLAGS_TARGET */ 
	commands_lock = &commands;	/* keep it sorted or die */

	command_add(&jabber_plugin, "xmpp:", "?", jabber_command_inline_msg, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:_autoaway", "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:_autoxa", "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:_autoback", "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:_stanzas", "?", jabber_command_stanzas, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:add", "U ?", jabber_command_modify, 	JABBER_FLAGS, NULL); 
	command_add(&jabber_plugin, "xmpp:admin", "! ?", jabber_muc_command_admin, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:auth", "!p uU", jabber_command_auth, 	JABBER_FLAGS_REQ,
			"-a --accept -d --deny -r --request -c --cancel");
	command_add(&jabber_plugin, "xmpp:away", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:back", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:ban", "! ? ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:bookmark", "!p ?", jabber_command_private, JABBER_FLAGS_REQ,
			"-a --add -c --clear -d --display -m --modify -r --remove");
	command_add(&jabber_plugin, "xmpp:config", "!p", jabber_command_private,	JABBER_FLAGS_REQ,
			"-c --clear -d --display -g --get -p --put");
	command_add(&jabber_plugin, "xmpp:change", "!p ? p ? p ? p ? p ? p ?", jabber_command_change, JABBER_FLAGS_REQ, 
			"-f --fullname -c --city -b --born -d --description -n --nick -C --country");
	command_add(&jabber_plugin, "xmpp:chat", "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:connect", NULL, jabber_command_connect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:conversations", NULL, jabber_command_conversations,	JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:del", "!u", jabber_command_del, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:disconnect", "r", jabber_command_disconnect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:dnd", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:ffc", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:find", "?", jabber_command_find, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:invisible", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:join", "! ? ?", jabber_muc_command_join, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:kick", "! ! ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:lastseen", "!u", jabber_command_lastseen, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:modify", "!Uu ?", jabber_command_modify,JABBER_FLAGS_REQ, 
			"-n --nickname -g --group");
	command_add(&jabber_plugin, "xmpp:msg", "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:part", "! ?", jabber_muc_command_part, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:passwd", "?", jabber_command_passwd, 	JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:private", "!p ! ?", jabber_command_private,   JABBER_FLAGS_REQ, 
			"-c --clear -d --display -p --put");
	command_add(&jabber_plugin, "xmpp:reconnect", NULL, jabber_command_reconnect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:register", "? ?", jabber_command_register, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:reply", "! !", jabber_command_reply, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:search", "? ?", jabber_command_search, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:tmsg", "!uU ! !", jabber_command_msg, JABBER_FLAGS_TARGET, NULL); /* threaded msg */
	command_add(&jabber_plugin, "xmpp:topic", "! ?", jabber_muc_command_topic, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:transpinfo", "? ?", jabber_command_transpinfo, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:transports", "? ?", jabber_command_transports, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:unban", "! ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:unregister", "?", jabber_command_register, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:userinfo", "!u", jabber_command_userinfo, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "xmpp:userlist", "! ?", jabber_command_userlist, JABBER_FLAGS_REQ,
			"-g --get -p --put"); /* BFW: it is unlike GG, -g gets userlist from file, -p writes it into it */
	command_add(&jabber_plugin, "xmpp:vacation", "?", jabber_command_vacation, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "xmpp:ver", "!u", jabber_command_ver, 	JABBER_FLAGS_TARGET, NULL); /* ??? ?? ? ?@?!#??#!@? */
	command_add(&jabber_plugin, "xmpp:xa", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "xmpp:xml", "!", jabber_command_xml, 	JABBER_ONLY, NULL);

	commands_lock = &commands;

	command_add(&jabber_plugin, "tlen:", "?",		jabber_command_inline_msg, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:_autoaway", "r", 	jabber_command_away,		JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:_autoxa", "r", 	jabber_command_away,		JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:_autoback", "r", 	jabber_command_away,		JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:add", "U ?",	jabber_command_modify,		JABBER_FLAGS, NULL); 
	command_add(&jabber_plugin, "tlen:alert", "!u",	tlen_command_alert,		JABBER_FLAGS_TARGET_VALID, NULL);
	command_add(&jabber_plugin, "tlen:auth", "!p uU", 	jabber_command_auth,		JABBER_FLAGS_REQ,
			"-a --accept -d --deny -r --request -c --cancel");
	command_add(&jabber_plugin, "tlen:away", "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:back", "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:connect", "r ?",	jabber_command_connect,		JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:disconnect", "r ?",	jabber_command_disconnect,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:del", "!u", jabber_command_del, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "tlen:dnd", "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:ffc", "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:invisible", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:modify", "!Uu ?",	jabber_command_modify,		JABBER_FLAGS_REQ, 
			"-n --nickname -g --group");
	command_add(&jabber_plugin, "tlen:msg", "!uU !",	jabber_command_msg, 		JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "tlen:reconnect", NULL,	jabber_command_reconnect,	JABBER_ONLY, NULL);

	commands_lock = NULL;
};

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
