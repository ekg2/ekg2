/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		       Tomasz Torcz <zdzichu@irc.pl>	
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
#include <sys/ioctl.h>
#include <sys/utsname.h> /* dla jabber:iq:version */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>

#ifdef HAVE_EXPAT_H
#  include <expat.h>
#endif

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/log.h>

#include "jabber.h"

COMMAND(jabber_command_connect)
{
	char *password = (char *) session_get(session, "password");
	const char *server, *realserver = session_get(session, "server"); 
	int res, fd[2], ret = 0;
	jabber_private_t *j = session_private_get(session);
	
	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		ret = -1;
		goto end;
	}

	if (j->connecting) {
		printq("during_connect", session_name(session));
		ret = -1;
		goto end;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		ret = -1;
		goto end;
	}

	if (!password) {
		printq("no_config");
		ret = -1;
		goto end;
	}

	debug("session->uid = %s\n", session->uid);
	
	if (!(server = xstrchr(session->uid, '@'))) {
		printq("wrong_id", session->uid);
		ret = -1;
		goto end;
	}

	xfree(j->server);
	j->server = xstrdup(++server) ;

	debug("[jabber] resolving %s\n", (realserver ? realserver : server));

	if (pipe(fd) == -1) {
		printq("generic_error", strerror(errno));
		ret = -1;
		goto end;
	}

	debug("[jabber] resolver pipes = { %d, %d }\n", fd[0], fd[1]);

	if ((res = fork()) == -1) {
		printq("generic_error", strerror(errno));
		ret = -1;
		goto end;
	}

	if (!res) {
		struct in_addr a;

		if ((a.s_addr = inet_addr(server)) == INADDR_NONE) {
			struct hostent *he = gethostbyname(realserver ? realserver : server);

			if (!he)
				a.s_addr = INADDR_NONE;
			else
				memcpy(&a, he->h_addr, sizeof(a));
		}

		write(fd[1], &a, sizeof(a));

		sleep(1);

		exit(0);
	} else {
                jabber_handler_data_t *jdta = xmalloc(sizeof(jabber_handler_data_t));

		close(fd[1]);
		
		jdta->session = session;
		jdta->roster_retrieved = 0;
		/* XXX dodaæ dzieciaka do przegl±dania */

		watch_add(&jabber_plugin, fd[0], WATCH_READ, 0, jabber_handle_resolver, jdta);
	}

	j->connecting = 1;

	printq("connecting", session_name(session));

	if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);
end:
	return ret;
}

COMMAND(jabber_command_disconnect)
{
	jabber_private_t *j = session_private_get(session);
	char *descr = NULL;


	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	/* jesli istnieje timer reconnecta, to znaczy, ze przerywamy laczenie */
	if (timer_remove(&jabber_plugin, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!j->connecting && !session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	/* je¶li jest /reconnect, nie mieszamy z opisami */
	if (xstrcmp(name, "reconnect")) {
		if (params[0])
			descr = xstrdup(params[0]);
		else
			descr = ekg_draw_descr("quit");
	} else
		descr = xstrdup(session_descr_get(session));

	if (descr) {
		char *tmp = jabber_escape(descr);
		jabber_write(j, "<presence type=\"unavailable\"><status>%s</status></presence>", tmp);
		xfree(tmp);
	} else
		jabber_write(j, "<presence type=\"unavailable\"/>");

	xfree(descr);
		
	jabber_write(j, "</stream:stream>");

	if (j->connecting) 
		j->connecting = 0;

	{
		char *__session = xstrdup(session->uid);
		char *__reason = params[0] ? xstrdup(params[0]) : NULL;
                int __type = EKG_DISCONNECT_USER;

		query_emit(NULL, "protocol-disconnected", &__session, &__reason, &__type, NULL);

                xfree(__reason);
                xfree(__session);
	}

	userlist_free(session);

	/* wywo³a jabber_handle_disconnect() */
	watch_remove(&jabber_plugin, j->fd, WATCH_READ);

	return 0;
}

COMMAND(jabber_command_reconnect)
{
	jabber_private_t *j = session_private_get(session);

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}
	
	if (j->connecting || session_connected_get(session)) {
		jabber_command_disconnect(name, params, session, target, quiet);
	}

	return jabber_command_connect(name, params, session, target, quiet);
}

COMMAND(jabber_command_msg)
{
	jabber_private_t *j = session_private_get(session);
	int chat = (strcasecmp(name, "msg"));
	char *msg;
	char *subject = NULL;
	char *subtmp;
	const char *uid, *seq = NULL;
	int secure = 0;

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

        if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		return -1;
	}
	
	if (!xstrcmp(params[0], "*")) {
		if (msg_all(session, name, params[1]) == -1)
			printq("list_empty");
		return 0;
	}
	
	if (!(uid = get_uid(session, params[0]))) {
		uid = params[0];

		if (xstrchr(uid, '@') && xstrchr(uid, '@') < xstrchr(uid, '.')) {
			printq("user_not_found", params[0]);
			return -1;
		}
	} else {
		if (xstrncasecmp(uid, "jid:", 4)) {
			printq("invalid_session");
			return -1;
		}

		uid += 4;
	}
	
	/* czy wiadomo¶æ ma mieæ temat? */
	if (config_subject_prefix && !xstrncmp(params[1], config_subject_prefix, xstrlen(config_subject_prefix))) {
		/* obcinamy prefix tematu */
		subtmp = xstrdup((params[1]+xstrlen(config_subject_prefix)));

		/* je¶li ma wiêcej linijek, zostawiamu tylko pierwsz± */
		if (xstrchr(subtmp, 10)) *(xstrchr(subtmp, 10)) = 0;

		subject = jabber_escape(subtmp);
		/* body of wiadomo¶æ to wszystko po koñcu pierwszej linijki */
		msg = jabber_escape(xstrchr(params[1], 10)); 
		xfree(subtmp);
	} else 
		msg = jabber_escape(params[1]); /* bez tematu */

	jabber_write(j, "<message %sto=\"%s\" id=\"%d\">", (!xstrcasecmp(name, "chat")) ? "type=\"chat\" " : "", uid, time(NULL));

	if (subject) jabber_write(j, "<subject>%s</subject>", subject);

	if (msg) jabber_write(j, "<body>%s</body>", msg);

	jabber_write(j, "<x xmlns=\"jabber:x:event\">%s%s<displayed/><composing/></x>", 
		( config_display_ack == 1 || config_display_ack == 2 ? "<delivered/>" : ""),
		( config_display_ack == 1 || config_display_ack == 3 ? "<offline/>"   : "") );
	jabber_write(j, "</message>");

        if (config_last & 4) 
        	last_add(1, get_uid(session, params[0]), time(NULL), 0, msg);

	xfree(msg);
	xfree(subject);

	if (!quiet) {
		char **rcpts = xmalloc(sizeof(char *) * 2);
		const int class = (chat) ? EKG_MSGCLASS_SENT_CHAT : EKG_MSGCLASS_SENT;
		const int ekgbeep = EKG_TRY_BEEP;
		char *me = xstrdup(session_uid_get(session));
		const time_t sent = time(NULL);
		char *format = NULL;

		rcpts[0] = saprintf("jid:%s", uid);
		rcpts[1] = NULL;
		
		query_emit(NULL, "protocol-message", &me, &me, &rcpts, &params[1], &format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(me);
		xfree(rcpts[0]);
		xfree(rcpts);
	}

	session_unidle(session);

	return 0;
}

COMMAND(jabber_command_inline_msg)
{
	const char *p[2] = { target, params[0] };
	
	if (p[1])
		return jabber_command_msg("chat", p, session, target, quiet);
	else
		return 0;
}

COMMAND(jabber_command_xml)
{
	jabber_private_t *j = session_private_get(session);

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	jabber_write(j, "%s", params[0]);

	return 0;
}

COMMAND(jabber_command_away)
{
	const char *descr, *format;
	
	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (params[0]) {
		session_descr_set(session, (!xstrcmp(params[0], "-")) ? NULL : params[0]);
		reason_changed = 1;
	}

	if (!xstrcmp(name, "_autoback")) {
		format = "auto_back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
		goto change;
	}

	if (!xstrcmp(name, "back")) {
		format = "back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
		goto change;
	}

	if (!xstrcmp(name, "_autoaway")) {
		format = "auto_away";
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		goto change;
	}

	if (!xstrcmp(name, "away")) {
		format = "away"; 
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
		goto change;
	}

	if (!xstrcmp(name, "dnd")) {
		format = "dnd";
		session_status_set(session, EKG_STATUS_DND);
		session_unidle(session);
		goto change;
	}
        
	if (!xstrcmp(name, "ffc")) {
	        format = "chat";
	        session_status_set(session, EKG_STATUS_FREE_FOR_CHAT);
                session_unidle(session);
                goto change;
        }
	
	if (!xstrcmp(name, "xa")) {
		format = "xa";
		session_status_set(session, EKG_STATUS_XA);
		session_unidle(session);
		goto change;
	}

	if (!xstrcmp(name, "invisible")) {
		format = "invisible";
		session_status_set(session, EKG_STATUS_INVISIBLE);
		session_unidle(session);
		goto change;
	}

	return -1;

change:
	if (!params[0]) {
                char *tmp;

                if ((tmp = ekg_draw_descr(format))) {
                        session_status_set(session, tmp);
                        xfree(tmp);
                }

                if (!config_keep_reason) {
                        session_descr_set(session, NULL);
                }
	}

	descr = (char *) session_descr_get(session);

	ekg_update_status(session);
	
	if (descr) {
		char *f = saprintf("%s_descr", format);
		printq(f, descr, session_name(session));
		xfree(f);
	} else
		printq(format, session_name(session));

	jabber_write_status(session);
	
	return 0;
}

COMMAND(jabber_command_passwd)
{
	jabber_private_t *j = session_private_get(session);
	char *username, *passwd;

        if (!session_check(session, 1, "jid")) {
                printq("invalid_session");
                return -1;
        }

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

        if (!params[0]) {
                printq("not_enough_params", name);
                return -1;
        }

	username = xstrdup(session->uid + 4);
	*(xstrchr(username, '@')) = 0;

	passwd = jabber_escape(params[0]);
	jabber_write(j, "<iq type=\"set\" to=\"%s\" id=\"passwd%d\"><query xmlns=\"jabber:iq:register\"><username>%s</username><password>%s</password></query></iq>", j->server, j->id++, username, passwd);
	xfree(passwd);
	
	session_set(session, "__new_password", params[0]);

	return 0;
}

COMMAND(jabber_command_auth) 
{
	jabber_private_t *j = session_private_get(session);
	session_t *s = session;
	const char *action;
	char *uid;

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
        }

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!(uid = get_uid(session, params[1]))) {
		uid = (char *) params[1];

		if (!(xstrchr(uid,'@') && xstrchr(uid, '@') < xstrchr(uid, '.'))) {
			printq("user_not_found", params[1]);
			return -1;
		}
	} else {
		if (xstrncasecmp(uid, "jid:", 4)) {
			printq("invalid_session");
			return -1;
		}

		/* user jest OK, wiêc lepiej mieæ go pod rêk± */
		tabnick_add(uid);

		uid += 4;
	};

	if (params[0] && match_arg(params[0], 'r', "request", 2)) {
		action = "subscribe";
		printq("jabber_auth_request", uid, session_name(s));
		goto success;
	}

	if (params[0] && match_arg(params[0], 'a', "accept", 2)) {
		action = "subscribed";
		printq("jabber_auth_accept", uid, session_name(s));
		goto success;
	}

	if (params[0] && match_arg(params[0], 'c', "cancel", 2)) {
		action = "unsubscribe";
		printq("jabber_auth_unsubscribed", uid, session_name(s));
		goto success;
	}

	if (params[0] && match_arg(params[0], 'd', "deny", 2)) {
		char *tmp;
		action = "unsubscribed";

		tmp = saprintf("jid:%s", uid);
		if (userlist_find(session, tmp))  // mamy w rosterze
			printq("jabber_auth_cancel", uid, session_name(s));
		else // nie mamy w rosterze
			printq("jabber_auth_denied", uid, session_name(s));
		xfree(tmp);
	
		goto success;
	};

	/* ha! undocumented :-); bo 
	   [Used on server only. Client authors need not worry about this.] */
	if (params[0] && match_arg(params[0], 'p', "probe", 2)) {
		action = "probe";
		printq("jabber_auth_probe", uid, session_name(s));
		goto success;
	};

	goto  fail;
fail:
	printq("invalid_params", name);
	return -1;

success:
	jabber_write(j, "<presence to=\"%s\" type=\"%s\" id=\"roster\"/>", uid, action);
	return 0;
}

COMMAND(jabber_command_modify)
{
	jabber_private_t *j = session_private_get(session);
	char *uid = NULL, *nickname = NULL;
	char *tmp, **argv = NULL;
	int ret = 0, i;

        if (!session_check(session, 1, "jid")) {
                printq("invalid_session");
                return -1;
        }

        if (!session_connected_get(session)) {
                printq("not_connected");
                return -1;
        }

        if (!params[0]) {
                printq("not_enough_params", name);
                return -1;
        }
	
	if (params[1]) {
		argv = array_make(params[1], " \t", 0, 1, 1);

		for (i = 0; argv[i]; i++) {

			if (match_arg(argv[i], 'n', "nickname", 2) && argv[i + 1])
				nickname = jabber_escape(argv[++i]);
		}
	}
	
	if (!xstrcasecmp(name, "add")) {
	       
		uid = (char *) params[0]; 
		
		if (!nickname && params[1])
			nickname = jabber_escape(params[1]);
	}

	if (!(uid = get_uid(session, params[0]))) 
		uid = (char *) params[0]; 
	
	while (!xstrncasecmp(uid, "jid:", 4))
		uid += 4;

	/* could have 'gg:'; also should have dots in domain name */
	if (xstrchr(uid, ':') || !xstrchr(uid, '.')) { 
		printq("invalid_uid");
		return -1;
	}

	jabber_write(j, "<iq type=\"set\"><query xmlns=\"jabber:iq:roster\">");

	if (nickname)
		jabber_write(j, "<item jid=\"%s\" name=\"%s\"/>", uid, nickname);
	else
		jabber_write(j, "<item jid=\"%s\"/>", uid);

	jabber_write(j, "</query></iq>");

	xfree(nickname);
	
	if (!xstrcasecmp(name, "add")) {
		tmp = saprintf("/auth --request jid:%s", uid);
		ret = command_exec(target, session, tmp, 0);
		xfree(tmp);
	}
	
	return (ret ? ret : 0);
}

COMMAND(jabber_command_del)
{
	jabber_private_t *j = session_private_get(session);
	char *uid;

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!(uid = get_uid(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	} else {
		if (xstrncasecmp(uid, "jid:", 4)) {
			printq("invalid_session");
			return -1;
		}
		uid +=4;
	};

	jabber_write(j, "<iq type=\"set\" id=\"roster\"><query xmlns=\"jabber:iq:roster\">");
	jabber_write(j, "<item jid=\"%s\" subscription=\"remove\"/></query></iq>", uid);

	print("user_deleted", params[0], session_name(session));
	
	return 0;
}

COMMAND(jabber_command_ver)
{
	jabber_private_t *j = session_private_get(session);
	const char *query_uid, *query_res, *uid;
        userlist_t *ut;
        const char *resource = session_get(session, "resource");

        if (!session_check(session, 1, "jid")) {
                printq("invalid_session");
                return -1;
        }

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	query_uid = params[0];
        if (!query_uid && !(query_uid = get_uid(session, "$"))) {
                printq("not_enough_params", name);
                return -1;
        }

	if (!(uid = get_uid(session, query_uid))) {
		print("user_not_found", query_uid);
		return -1;
	}

	if (xstrncasecmp(uid, "jid:", 4) != 0) {
	  printq("invalid_session");
	  return -1;
	}

	if (!(ut = userlist_find(session, uid))) {
		print("user_not_found", session_name(session));
		return -1;
	}
	uid += 4;

	if (xstrcasecmp(ut->status, EKG_STATUS_NA) == 0) {
		print("jabber_status_notavail", session_name(session), ut->uid);
		return -1;
	}

	if (!resource)
		resource = JABBER_DEFAULT_RESOURCE;

	if (!(query_res = ut->resource)) {
		print("jabber_unknown_resource", session_name(session), query_uid);
		return -1;
	}

       	jabber_write(j, "<iq from='%s/%s' id='%d' to='%s/%s' type='get'><query xmlns='jabber:iq:version'/></iq>", \
		     jabber_escape(session->uid + 4), jabber_escape(resource), j->id++, jabber_escape(uid), jabber_escape(query_res));
	return 0;
}


void jabber_register_commands()
{
	command_add(&jabber_plugin, "jid:", "?", jabber_command_inline_msg, 0, NULL);
	command_add(&jabber_plugin, "jid:_autoaway", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:_autoback", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:add", "U ?", jabber_command_modify, 0, NULL); 
	command_add(&jabber_plugin, "jid:auth", "p uU", jabber_command_auth, 0, 
	  "-a --accept -d --deny -r --request -c --cancel");
	command_add(&jabber_plugin, "jid:away", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:back", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:chat", "uU ?", jabber_command_msg, 0, NULL);
	command_add(&jabber_plugin, "jid:connect", "?", jabber_command_connect, 0, NULL);
	command_add(&jabber_plugin, "jid:del", "u", jabber_command_del, 0, NULL);
	command_add(&jabber_plugin, "jid:disconnect", "?", jabber_command_disconnect, 0, NULL);
	command_add(&jabber_plugin, "jid:dnd", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:invisible", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:ffc", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:msg", "uU ?", jabber_command_msg, 0, NULL);
	command_add(&jabber_plugin, "jid:modify", "Uu ?", jabber_command_modify, 0, 
	  "-n --nickname");
	command_add(&jabber_plugin, "jid:passwd", "?", jabber_command_passwd, 0, NULL);
	command_add(&jabber_plugin, "jid:reconnect", NULL, jabber_command_reconnect, 0, NULL);
	command_add(&jabber_plugin, "jid:ver", "?u", jabber_command_ver, 0, NULL);
	command_add(&jabber_plugin, "jid:xa", "r", jabber_command_away, 0, NULL);
	command_add(&jabber_plugin, "jid:xml", "?", jabber_command_xml, 0, NULL);
};


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
