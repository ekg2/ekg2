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

#include "jabber.h"

static COMMAND(jabber_command_dcc) {
	jabber_private_t *j = session_private_get(session);

	if (!xstrncasecmp(params[0], "se", 2)) { /* send */
		struct stat st;
		userlist_t *u;
		dcc_t *d;
		int fd;

		if (!params[1] || !params[2]) {
			wcs_printq("not_enough_params", name);
			return -1;
		}

		if (!(u = userlist_find(session, get_uid(session, params[1])))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if (!session_connected_get(session)) {
			wcs_printq("not_connected");
			return -1;
		}

		if (!xstrcmp(u->status, EKG_STATUS_NA)) {
			printq("dcc_user_not_avail", format_user(session, u->uid));
			return -1;
		}

		if (!stat(params[2], &st) && S_ISDIR(st.st_mode)) {
			printq("dcc_open_error", params[2], strerror(EISDIR));
			return -1;
		}
		
		if ((fd = open(params[2], O_RDONLY)) == -1) {
			printq("dcc_open_error", params[2], strerror(errno));
			return -1;
		}

		close(fd);

		{
			char *filename;
			char *touid = saprintf("%s/%s", u->uid, u->resource);
			jabber_dcc_t *p;
			d = dcc_add(touid, DCC_SEND, NULL);
			dcc_filename_set(d, params[2]);
			dcc_size_set(d, st.st_size);

			p = xmalloc(sizeof(jabber_dcc_t));
			p->session = session;
			p->req = saprintf("offer%d", dcc_id_get(d));
			p->sid = xstrdup(itoa(j->id++));
			dcc_private_set(d, p);

			filename = jabber_escape(params[2]); /* mo¿e obetniemy path? */

			watch_write(j->send_watch, "<iq type=\"set\" id=\"%s\" to=\"%s\">"
					"<si xmlns=\"http://jabber.org/protocol/si\" id=\"%s\" profile=\"http://jabber.org/protocol/si/profile/file-transfer\">"
					"<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" size=\"%d\" name=\"%s\">"
					"<range/></file>"
					"<feature xmlns=\"http://jabber.org/protocol/feature-neg\"><x xmlns=\"jabber:x:data\" type=\"form\">"
					"<field type=\"list-single\" var=\"stream-method\">"
					"<option><value>http://jabber.org/protocol/bytestreams</value></option>"
/*					"<option><value>http://jabber.org/protocol/ibb</value></option>" */
					"</field></x></feature></si></iq>", p->req, d->uid+4, p->sid, st.st_size, filename);
			xfree(filename);
			xfree(touid);
		}
		return 0;
	}
	if (!xstrncasecmp(params[0], "g", 1) || !xstrncasecmp(params[0], "re", 2)) { /* get, resume */
		dcc_t *d = NULL;
		list_t l;
		
		for (l = dccs; l; l = l->next) {
			dcc_t *D = l->data;
			userlist_t *u;

			if (!dcc_filename_get(D) || dcc_type_get(D) != DCC_GET)
				continue;
			
			if (!params[1]) {
				if (dcc_active_get(D))
					continue;
				d = D;
				break;
			}

			if (params[1][0] == '#' && xstrlen(params[1]) > 1 && atoi(params[1] + 1) == dcc_id_get(D)) {
				d = D;
				break;
			}

			if ((u = userlist_find(session, dcc_uid_get(D)))) {
				if (!xstrcasecmp(params[1], u->uid) || (u->nickname && !xstrcasecmp(params[1], u->nickname))) {
					d = D;
					break;
				}
			}
		}

		if (!d || !d->priv) {
			printq("dcc_not_found", (params[1]) ? params[1] : "");
			return -1;
		}
		if (dcc_active_get(d)) {
			printq("dcc_receiving_already", dcc_filename_get(d), format_user(session, dcc_uid_get(d)));
			return -1;
		}

		{
			jabber_dcc_t *p = d->priv;
			session_t *s = p->session;
			jabber_private_t *j = jabber_private(s);

			watch_write(j->send_watch, "<iq type=\"result\" to=\"%s\" id=\"%s\">"
					"<si xmlns=\"http://jabber.org/protocol/si\"><feature xmlns=\"http://jabber.org/protocol/feature-neg\">"
					"<x xmlns=\"jabber:x:data\" type=\"submit\">"
					"<field var=\"stream-method\"><value>http://jabber.org/protocol/bytestreams</value></field>"
					"</x></feature></si></iq>", d->uid+4, p->req);
		}
		/* TODO */
		return -1;
#if 0
		printq("dcc_get_getting", format_user(session, dcc_uid_get(d)), dcc_filename_get(d));
		dcc_active_set(d, 1);
		
		return 0;
#endif
	}
	if (!xstrncasecmp(params[0], "vo", 2)) { /* voice */
		return -1;
	}
	return cmd_dcc(name, params, session, target, quiet);
}

static void jabber_command_connect_child(
	const char *server, 
#ifdef NO_POSIX_SYSTEM
	HANDLE fd	/* fd[1] */
#else
	int fd		/* fd[1]*/
#endif
	) {
	struct in_addr a;

	if ((a.s_addr = inet_addr(server)) == INADDR_NONE) {
		struct hostent *he = gethostbyname(server);

		if (!he)
			a.s_addr = INADDR_NONE;
		else
			memcpy(&a, he->h_addr, sizeof(a));
	}
#ifdef NO_POSIX_SYSTEM
	DWORD written = 0;
	WriteFile(fd, &a, sizeof(a), &written, NULL);
#else
	write(fd, &a, sizeof(a));
#endif
	sleep(1);
}

#ifdef NO_POSIX_SYSTEM
struct win32_temp { char server[100]; HANDLE fd; HANDLE fd2; };

static int jabber_command_connect_child_win32(void *data) {
	struct win32_temp *helper = data;
	
	CloseHandle(helper->fd2);
	jabber_command_connect_child(helper->server, helper->fd);
	xfree(helper);
	return 0;
}

#endif

static COMMAND(jabber_command_connect)
{
	const char *server, *realserver = session_get(session, "server"); 
	int res, fd[2];
	jabber_private_t *j = session_private_get(session);
	
	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (!session_get(session, "__new_acount") && !(session_get(session, "password"))) {
		printq("no_config");
		return -1;
	}

	debug("session->uid = %s\n", session->uid);
		/* XXX, nie wymagac od usera podania calego uida w postaci: tlen:ktostam@tlen.pl tylko samo tlen:ktostam? */
	if (!(server = xstrchr(session->uid, '@'))) {
		printq("wrong_id", session->uid);
		return -1;
	}
	xfree(j->server);
	j->server	= xstrdup(++server);

	if (j->istlen) server = TLEN_HUB;
	if (!realserver) realserver = server;

	debug("[jabber] resolving %s\n", realserver);

	if (pipe(fd) == -1) {
		printq("generic_error", strerror(errno));
		return -1;
	}

	debug("[jabber] resolver pipes = { %d, %d }\n", fd[0], fd[1]);
#ifdef NO_POSIX_SYSTEM
	struct win32_temp *helper = xmalloc(sizeof(struct win32_temp));

	xstrncpy((char *) &helper->server, realserver, sizeof(helper->server));
	DuplicateHandle(GetCurrentProcess(), (HANDLE) fd[1], GetCurrentProcess(), &(helper->fd), DUPLICATE_SAME_ACCESS, TRUE, DUPLICATE_SAME_ACCESS);
	DuplicateHandle(GetCurrentProcess(), (HANDLE) fd[0], GetCurrentProcess(), &(helper->fd2), DUPLICATE_SAME_ACCESS, TRUE, DUPLICATE_SAME_ACCESS);

	if ((res = win32_fork(&jabber_command_connect_child_win32, helper)) == -1)
#else
	if ((res = fork()) == -1) 
#endif
	{
		printq("generic_error", strerror(errno));
		close(fd[0]);
		close(fd[1]);
		return -1;
	}

	if (res) {
#ifdef NO_POSIX_SYSTEM
		CloseHandle((HANDLE) fd[1]);
#else
		close(fd[1]);
#endif
		/* XXX dodaæ dzieciaka do przegl±dania */
		watch_add(&jabber_plugin, fd[0], WATCH_READ, jabber_handle_resolver, session);
	} else {
#ifndef NO_POSIX_SYSTEM
		close(fd[0]);
		jabber_command_connect_child(realserver, fd[1]);
		exit(0);
#endif
	}

	j->connecting = 1;

	printq("connecting", session_name(session));

	if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);
	return 0;
}

static COMMAND(jabber_command_disconnect)
{
	jabber_private_t *j = session_private_get(session);
	char *descr = NULL;
	int fd = j->fd;

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
	if (xstrcmp(name, ("reconnect"))) {
		if (params[0])
			descr = xstrdup(params[0]);
		else
			descr = ekg_draw_descr("quit");
	} else
		descr = xstrdup(session_descr_get(session));

/* w libtlenie jest <show>unavailable</show> + eskejpiete tlen_encode() */

	if (descr) {
		char *tmp = jabber_escape(descr);
		watch_write(j->send_watch, "<presence type=\"unavailable\"><status>%s</status></presence>", tmp ? tmp : "");
		xfree(tmp);
	} else
		watch_write(j->send_watch, "<presence type=\"unavailable\"/>");

	if (!j->istlen) watch_write(j->send_watch, "</stream:stream>");
	else		watch_write(j->send_watch, "</s>");

	if (j->connecting) 
		j->connecting = 0;

	userlist_free(session);
	if (j->connecting)
		jabber_handle_disconnect(session, descr, EKG_DISCONNECT_STOPPED);
	else
		jabber_handle_disconnect(session, descr, EKG_DISCONNECT_USER);
	watch_remove(&jabber_plugin, fd, WATCH_READ);
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

/* CHECK: po co my wlasciwcie robimy to get_uid ? */
/* a) jak target jest '$' to zwraca aktualne okienko... 	(niepotrzebne raczej, CHECK)
   b) szuka targeta na userliscie 				(w sumie to trzeba jeszcze)
   c) robi to co my nizej tylko dla kazdego plugina 		(niepotrzebne)
 */
#if 1
	if (!(uid = get_uid(s, target))) 
		uid = target;
#endif
/* CHECK: i think we can omit it */
	if (((!istlen && xstrncasecmp(uid, "jid:", 4)) || (istlen && xstrncasecmp(uid, "tlen:", 5)))) {
		wcs_printq("invalid_session");
		return NULL;
	}
	return uid;
}

static COMMAND(jabber_command_msg)
{
	jabber_private_t *j = session_private_get(session);
	int chat = !xstrcasecmp(name, ("chat"));
	int subjectlen = xstrlen(config_subject_prefix);
	char *msg;
	char *subject = NULL;
	const char *uid;
	int payload = 4 + j->istlen;

	newconference_t *c;
	int ismuc = 0;

	if (!xstrcmp(target, "*")) {
		if (msg_all(session, name, params[1]) == -1)
			printq("list_empty");
		return 0;
	}
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	/* czy wiadomo¶æ ma mieæ temat? */
	if (!j->istlen && config_subject_prefix && !xstrncmp(params[1], config_subject_prefix, subjectlen)) {
		char *subtmp = xstrdup((params[1]+subjectlen)); /* obcinamy prefix tematu */
		char *tmp;

		/* je¶li ma wiêcej linijek, zostawiamu tylko pierwsz± */
		if ((tmp = xstrchr(subtmp, 10)))
			*tmp = 0;

		subject = jabber_escape(subtmp);
		/* body of wiadomo¶æ to wszystko po koñcu pierwszej linijki */
		msg = (tmp) ? jabber_escape(tmp+1) : NULL;
		xfree(subtmp);
	} else 
		msg = tlenjabber_escape(params[1]); /* bez tematu */
	if ((c = newconference_find(session, target))) 
		ismuc = 1;

	if (j->send_watch) j->send_watch->transfer_limit = -1;

	if (ismuc)
		watch_write(j->send_watch, "<message type=\"groupchat\" to=\"%s\" id=\"%d\">", uid+payload, time(NULL));
	else
		watch_write(j->send_watch, "<message %sto=\"%s\" id=\"%d\">", 
			chat ? "type=\"chat\" " : "",
/*				j->istlen ? "type=\"normal\" " : "",  */
			uid+payload, time(NULL));

	if (subject) {
		watch_write(j->send_watch, "<subject>%s</subject>", subject); 
		xfree(subject); 
	}
	if (msg) {
		watch_write(j->send_watch, "<body>%s</body>", msg);
        	if (config_last & 4) 
        		last_add(1, uid, time(NULL), 0, msg);
		xfree(msg);
	}
	if (!j->istlen) 
		watch_write(j->send_watch, "<x xmlns=\"jabber:x:event\">%s%s<displayed/><composing/></x>", 
			( config_display_ack == 1 || config_display_ack == 2 ? "<delivered/>" : ""),
			( config_display_ack == 1 || config_display_ack == 3 ? "<offline/>"   : "") );
	else ;

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
		int secure	= 0;

		rcpts[0] 	= xstrdup(uid);
		rcpts[1] 	= NULL;

		if (ismuc)
			class |= EKG_NO_THEMEBIT;
		
		query_emit(NULL, ("protocol-message"), &me, &me, &rcpts, &msg, &format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(msg);
		xfree(me);
		array_free(rcpts);
	}

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
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xstrcmp(name, ("back"))) {
		format = "back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xstrcmp(name, ("_autoaway"))) {
		format = "auto_away";
		session_status_set(session, EKG_STATUS_AUTOAWAY);
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
	        session_status_set(session, EKG_STATUS_FREE_FOR_CHAT);
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

	username = xstrdup(session->uid + 4);
	*(xstrchr(username, '@')) = 0;

//	username = xstrndup(session->uid + 4, xstrchr(session->uid+4, '@') - session->uid+4);

	passwd = jabber_escape(params[0]);
	watch_write(j->send_watch, 
		"<iq type=\"set\" to=\"%s\" id=\"passwd%d\"><query xmlns=\"jabber:iq:register\"><username>%s</username><password>%s</password></query></iq>",
		j->server, j->id++, username, passwd);
	
	session_set(session, "__new_password", params[0]);

	xfree(username);
	xfree(passwd);

	return 0;
}

static COMMAND(jabber_command_auth) 
{
	jabber_private_t *j = session_private_get(session);
	session_t *s = session;
	const char *action;
	const char *uid;
	int payload;


	if (!(uid = jid_target2uid(session, params[1], quiet)))
		return -1;
	/* user jest OK, wiêc lepiej mieæ go pod rêk± */
	tabnick_add(uid);

	payload = 4 + j->istlen;

	if (match_arg(params[0], 'r', ("request"), 2)) {
		action = "subscribe";
		printq("jabber_auth_request", uid+payload, session_name(s));
	} else if (match_arg(params[0], 'a', ("accept"), 2)) {
		action = "subscribed";
		printq("jabber_auth_accept", uid+payload, session_name(s));
	} else if (match_arg(params[0], 'c', ("cancel"), 2)) {
		action = "unsubscribe";
		printq("jabber_auth_unsubscribed", uid+payload, session_name(s));
	} else if (match_arg(params[0], 'd', ("deny"), 2)) {
		action = "unsubscribed";

		if (userlist_find(session, uid))  // mamy w rosterze
			printq("jabber_auth_cancel", uid+payload, session_name(s));
		else // nie mamy w rosterze
			printq("jabber_auth_denied", uid+payload, session_name(s));
	
	} else if (match_arg(params[0], 'p', ("probe"), 2)) {
	/* ha! undocumented :-); bo 
	   [Used on server only. Client authors need not worry about this.] */
		action = "probe";
		printq("jabber_auth_probe", uid+payload, session_name(s));
	} else {
		wcs_printq("invalid_params", name);
		return -1;
	}

	watch_write(j->send_watch, "<presence to=\"%s\" type=\"%s\" id=\"roster\"/>", uid+payload, action);
	return 0;
}

static COMMAND(jabber_command_modify)
/* XXX REWRITE IT */
{
	jabber_private_t *j = session_private_get(session);
	const char *uid = NULL;
	char *nickname = NULL;
	int ret = 0;
	userlist_t *u;
	list_t m;
	
	int addcomm = !xstrcasecmp(name, ("add"));

	if (!(u = userlist_find(session, target))) {
		if (!addcomm) {
			printq("user_not_found", target);
			return -1;
		} else {
			/* khm ? a nie powinnismy userlist_add() ? */
			u = xmalloc(sizeof(userlist_t));
		}
	} else if (addcomm) {
		printq("user_exists_other", params[0], format_user(session, u->uid), session_name(session));
		return -1;
	}
	if (params[1]) {
		char **argv = array_make(params[1], " \t", 0, 1, 1);
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
								printq("group_member_not_yet", format_user(session, u->uid), tmp[x] + 1);
							}
							break;
						case '+':
							off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

							if (!ekg_group_member(u, tmp[x] + 1 + off)) {
								ekg_group_add(u, tmp[x] + 1 + off);
							} else {
								printq("group_member_already", format_user(session, u->uid), tmp[x] + 1);
							}
							break;
						default:
							off = (tmp[x][0] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

							if (!ekg_group_member(u, tmp[x] + off)) {
								ekg_group_add(u, tmp[x] + off);
							} else {
								printq("group_member_already", format_user(session, u->uid), tmp[x]);
							}
					}

				array_free(tmp);
				continue;
			}

			if (match_arg(argv[i], 'n', ("nickname"), 2) && argv[i + 1]) {
				xfree(nickname);
				nickname = jabber_escape(argv[++i]);
				continue;
			}
		/* emulate gg:modify behavior */
			if (!addcomm && match_arg(argv[i], 'o', ("online"), 2)) {	/* only jabber:iq:privacy */
				command_exec_format(target, session, 0, ("/jid:privacy --set %s +pin"), u->uid);
				continue;
			}
			
			if (!addcomm && match_arg(argv[i], 'O', ("offline"), 2)) {	/* only jabber:iq:privacy */
				command_exec_format(target, session, 0, ("/jid:privacy --set %s -pin"), u->uid);
				continue;
			}
		}
		array_free(argv);
	} 
	
	if (addcomm) {
		if (!nickname && params[1])
			nickname = jabber_escape(params[1]);
	} 
/* TODO: co robimy z nickname jesli jest jid:modify ? Pobieramy z userlisty jaka mamy akutualnie nazwe ? */
#if 0
	else if (!nickname && target) /* jesli jest modify i hmm. nie mamy nickname czyli params[1] to zamieniamy na target ? czyli zamieniamy na to samo co mielismy ? */
			nickname = jabber_escape(target);
#endif

	if (!(uid = jid_target2uid(session, target, quiet))) 
		return -1;

	if (j->send_watch) j->send_watch->transfer_limit = -1;	/* let's send this in one/two packets not in 7 or more. */

	watch_write(j->send_watch, "<iq type=\"set\"><query xmlns=\"jabber:iq:roster\">");

	/* nickname always should be set */
	if (nickname)	watch_write(j->send_watch, "<item jid=\"%s\" name=\"%s\"%s>", uid+4, nickname, (u->groups ? "" : "/"));
	else		watch_write(j->send_watch, "<item jid=\"%s\"%s>", uid+4, (u->groups ? "" : "/"));

	for (m = u->groups; m ; m = m->next) {
		struct ekg_group *g = m->data;
		char *gname = jabber_escape(g->name);

		watch_write(j->send_watch, "<group>%s</group>", gname);
		xfree(gname);
	}

	if (u->groups)
		watch_write(j->send_watch, "</item>");

	watch_write(j->send_watch, "</query></iq>");
	JABBER_COMMIT_DATA(j->send_watch); 

	xfree(nickname);
	
	if (addcomm) {
		xfree(u);
		return command_exec_format(target, session, 0, ("/auth --request %s"), uid);
	}
	
	return ret;
}

static COMMAND(jabber_command_del)
{
	const char *uid;
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	{
		jabber_private_t *j = session_private_get(session);
		char *xuid = jabber_escape(uid+4);
		watch_write(j->send_watch, "<iq type=\"set\" id=\"roster\"><query xmlns=\"jabber:iq:roster\">");
		watch_write(j->send_watch, "<item jid=\"%s\" subscription=\"remove\"/></query></iq>", xuid);
		xfree(xuid);
	}
	print("user_deleted", target, session_name(session));
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
	const char *query_res, *uid;
        userlist_t *ut;
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;

	if (!(ut = userlist_find(session, uid))) {
		print("user_not_found", session_name(session));
		return -1;
	}
	if (xstrcasecmp(ut->status, EKG_STATUS_NA) == 0) {
		print("jabber_status_notavail", session_name(session), ut->uid);
		return -1;
	}

	if (!(query_res = ut->resource)) {
		print("jabber_unknown_resource", session_name(session), target);
		return -1;
	}
	{
		jabber_private_t *j = session_private_get(session);
		char *xuid = jabber_escape(uid+4);
		char *xquery_res = jabber_escape(query_res);
       		watch_write(j->send_watch, "<iq id='%d' to='%s/%s' type='get'><query xmlns='jabber:iq:version'/></iq>", \
			     j->id++, xuid, xquery_res);
		xfree(xuid);
		xfree(xquery_res);
	}
	return 0;
}

static COMMAND(jabber_command_userinfo)
{
	const char *uid;

	/* jabber id: [user@]host[/resource] */
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	{ 
		jabber_private_t *j = session_private_get(session);
		char *xuid = jabber_escape(uid+4);
       		watch_write(j->send_watch, "<iq id='%d' to='%s' type='get'><vCard xmlns='vcard-temp'/></iq>", \
			     j->id++, xuid);
		xfree(xuid);
	}
	return 0;
}

static COMMAND(jabber_command_change)
{
#define pub_sz 6
#define strfix(s) (s ? s : "")
	jabber_private_t *j = session_private_get(session);
	char *pub[pub_sz] = { NULL, NULL, NULL, NULL, NULL, NULL };
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
		}

	}
	for (i=0; i<pub_sz; i++) 
		pub[i] = jabber_escape(pub[i]);
	watch_write(j->send_watch, "<iq type=\"set\"><vCard xmlns='vcard-temp'>"
			"<FN>%s</FN>" "<NICKNAME>%s</NICKNAME>"
			"<ADR><LOCALITY>%s</LOCALITY><COUNTRY>%s</COUNTRY></ADR>"
			"<BDAY>%s</BDAY><DESC>%s</DESC></vCard></iq>\n", 
			strfix(pub[0]), strfix(pub[1]), strfix(pub[2]), strfix(pub[5]), strfix(pub[3]), strfix(pub[4]));

	for (i=0; i<pub_sz; i++) 
		xfree(pub[i]);
	return 0;
}

static COMMAND(jabber_command_lastseen)
{
	const char *uid;
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
#if 0 /* ? */
	if (!userlist_find(session, uid)) {
		print("user_not_found", session_name(session));
		return -1;
	}
#endif
	{
		jabber_private_t *j = session_private_get(session);
		char *xuid = jabber_escape(uid+4);
	       	watch_write(j->send_watch, "<iq id='%d' to='%s' type='get'><query xmlns='jabber:iq:last'/></iq>", \
			     j->id++, xuid);
		xfree(xuid);
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

static COMMAND(jabber_command_search) {
	jabber_private_t *j = session_private_get(session);
		/* XXX implementation bug ? should server be last variable? */
	const char *server = params[0] ? params[0] : jabber_default_search_server ? jabber_default_search_server : j->server; /* jakis server obsluguje jabber:iq:search ? :) */ 
	char **splitted		= NULL;

	if (array_count((char **) params) > 1 && !(splitted = jabber_params_split(params[1], 0))) {
		printq("invalid_params", name);
		return -1;
	}

	watch_write(j->send_watch, 
		"<iq type=\"%s\" to=\"%s\" id=\"search%d\"><query xmlns=\"jabber:iq:search\">", params[1] ? "set" : "get", server, j->id++);

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
	array_free (splitted);

	return -1;
}

static COMMAND(tlen_command_pubdir) {
	int issearch = !xstrcmp(name, ("search"));

	if (!issearch && !xstrcmp(params[0], "show")) 
		return command_exec(target, session, ("/jid:register tuba"), quiet);
	if (params[0]) {
		int res;
		char **splitted;
		string_t str;
		int i;

		if (!(splitted = jabber_params_split(params[0], 1))) {
			printq("invalid_params", name);
			return -1;
		}
			
			/* execute jabber_command_search.. | _register */
				/* params[0] == "tuba" */
				/* params[1] == parsed params[0] */
		if (issearch)	str = string_init(("/jid:search tuba "));
		else		str = string_init(("/jid:register tuba "));

		for (i=0; (splitted[i] && splitted[i+1]); i+=2) {
			char *valname = 0;
			if (!xstrcmp(splitted[i], "first") || !xstrcmp(splitted[i], "last") || !xstrcmp(splitted[i], "nick") || !xstrcmp(splitted[i], "email")) 
				valname = splitted[i];

			/* translated varsname from libtlen (http://libtlen.sourceforge.net/) */
			else if (!xstrcmp(splitted[i], "id")) 			valname = "i";
			else if (!xstrcmp(splitted[i], "city")) 		valname = "c";
			else if (!xstrcmp(splitted[i], "school")) 		valname = "e";
			else if (!xstrcmp(splitted[i], "gender")) 		valname = "s";
			else if (!xstrcmp(splitted[i], "job"))			valname = "j";
			else if (!xstrcmp(splitted[i], "look-for"))		valname = "r";
			else if (!xstrcmp(splitted[i], "voice"))		valname = "g";
			else if (!xstrcmp(splitted[i], "plans"))		valname = "p";
			else if (issearch && !xstrcmp(splitted[i], "status")) 	valname = "m";
			else if (issearch && !xstrcmp(splitted[i], "age_min")) 	valname = "d";
			else if (issearch && !xstrcmp(splitted[i], "age_max"))	valname = "u";
			else if (!issearch && !xstrcmp(splitted[i], "visible")) valname = "v";
			else if (!issearch && !xstrcmp(splitted[i], "birthyear")) valname = "b";

			if (valname) {
				string_append(str, ("--"));
				string_append(str, valname);
				string_append_c(str, ' ');
				string_append(str, splitted[i+1]);
			} else debug("option --%s not supported in /tlen:%s! skipping.\n", splitted[i], name);
		}
		array_free(splitted);

		res = command_exec(target, session, str->str, quiet);
		string_free(str, 1);

		return res;
	}

	printq("jabber_form_title", session_name(session), "tuba", issearch ? "Szukanie w katalogu tlena" : "Rejestracja w katalogu tlena");
	printq("jabber_form_command", session_name(session), "", issearch ? "tlen:search" : "tlen:change", "");
#define show_field(x) printq("jabber_form_item", session_name(session), "tuba", x, x, "", "", " ")
	show_field("first");
	show_field("last");
	show_field("nick");
	show_field("email");
	show_field("id");
	show_field("city");
	show_field("school");
	show_field("gender");
	if (issearch) {
		show_field("status");
		show_field("age_min");
		show_field("age_max");
	} else {
		show_field("visible");
		show_field("birthyear");
	}
	show_field("job");
	show_field("look-for");
	show_field("voice");
	show_field("plans");
#undef show_field
	printq("jabber_form_end", session_name(session), "", issearch ? "tlen:search" : "tlen:change");
	return 0;
}


static COMMAND(jabber_command_register)
{
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *passwd = session_get(session, "password");
	char **splitted	= NULL;

	if (!session_connected_get(session) && (!passwd || !xstrcmp(passwd, "foo"))) {
		session_set(session, "__new_acount", "1");
		if (params[0]) session_set(session, "password", params[0]);
		jabber_command_connect(("connect"), NULL, session, target, quiet);
		return 0;
	} else if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (!j->send_watch) return -1;
	j->send_watch->transfer_limit = -1;

	if (array_count((char **) params) > 1 && !(splitted = jabber_params_split(params[1], 0))) {
		printq("invalid_params", name);
		return -1;
	}
	watch_write(j->send_watch, "<iq type=\"%s\" to=\"%s\" id=\"transpreg%d\"><query xmlns=\"jabber:iq:register\">", params[1] ? "set" : "get", server, j->id++);
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

static COMMAND(jabber_command_vacation) { /* JEP-0109: Vacation Messages (DEFERRED) */
	jabber_private_t *j = session_private_get(session);
	char *message = jabber_escape(params[0]);
/* XXX, wysylac id: vacation%d... porobic potwierdzenia ustawiania/ usuwania. oraz jesli nie ma statusu to wyswylic jakies 'no vacation status'... */

	if (!params[0]) watch_write(j->send_watch, "<iq type=\"get\" id=\"%d\"><query xmlns=\"http://jabber.org/protocol/vacation\"/></iq>", j->id++);
	else if (xstrlen(params[0]) == 1 && params[0][0] == '-') 
		watch_write(j->send_watch, "<iq type=\"set\" id=\"%d\"><query xmlns=\"http://jabber.org/protocol/vacation\"/></iq>", j->id++);
	else	watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"%d\"><query xmlns=\"http://jabber.org/protocol/vacation\">"
			"<start/><end/>" /* XXX, startdate, enddate */
			"<message>%s</message>"
			"</query></iq>", 
			j->id++, message);
	xfree(message);
	return 0;
}

static COMMAND(jabber_command_transpinfo) {
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *node   = (params[0] && params[1]) ? params[1] : NULL;

	if (node) {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"transpinfo%d\"><query xmlns=\"http://jabber.org/protocol/disco#info\" node=\"%s\"/></iq>",
			server, j->id++, node);
	} else {
		watch_write(j->send_watch, 
			"<iq type=\"get\" to=\"%s\" id=\"transpinfo%d\"><query xmlns=\"http://jabber.org/protocol/disco#info\"/></iq>", 
			server, j->id++);
	}
	return 0;

}

static COMMAND(jabber_command_transports) {
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *node   = (params[0] && params[1]) ? params[1] : NULL;
	
	if (node) {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"transplist%d\"><query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"%s\"/></iq>",
			server, j->id++, node);
	} else {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"transplist%d\"><query xmlns=\"http://jabber.org/protocol/disco#items\"/></iq>",
			server, j->id++);
	}
	return 0;
}

static COMMAND(jabber_command_stats) { /* JEP-0039: Statistics Gathering (DEFERRED) */
	jabber_private_t *j = jabber_private(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *items   = (params[0] && params[1]) ? params[1] : NULL;

	if (items) {
	/* XXX */
		char *itemp = NULL;
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"stats%d\"><query xmlns=\"http://jabber.org/protocol/stats\">%s</query></iq>",
			server, j->id++, itemp);
	} else {
		watch_write(j->send_watch,
			"<iq type=\"get\" to=\"%s\" id=\"stats%d\"><query xmlns=\"http://jabber.org/protocol/stats\"/></iq>",
			server, j->id++);
	}
	return 0;
}

int jabber_privacy_add_compare(void *data1, void *data2) {
	jabber_iq_privacy_t *a = data1;
	jabber_iq_privacy_t *b = data2;

	return (a->order - b->order);
}

static COMMAND(jabber_command_privacy) {	/* jabber:iq:privacy in ekg2 (RFC 3921) by my watch */
/* NEED SOME more IDEAs... */
	jabber_private_t *j = jabber_private(session);
	int needsync = 0;

	if (!params[0] || !xstrcmp(params[0], "--lists")) {	
		/* Usage:	--lists			-- request for lists */

		watch_write(j->send_watch, "<iq type=\"get\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"/></iq>", j->id++);
		return 0;
	}

	if (!xstrcmp(params[0], "--session") || !xstrcmp(params[0], "--default")) {	
		/* Usage:	--session <list>	-- set session's list */
		/* Usage:	--default <list>	-- set default list */

		char *val = jabber_escape(params[1]);
		int unset = !xstrcmp(params[1], "-");
		char *tag = !xstrcmp(params[0], "--session") ? "active" : "default";
		if (!val) {
			/* XXX, display current default/session list? */
			printq("invalid_params", name);
			return -1;
		}
		if (unset)	watch_write(j->send_watch, "<iq type=\"set\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"><%s/></query></iq>", j->id++, tag);
		else		watch_write(j->send_watch, "<iq type=\"set\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"><%s name=\"%s\"/></query></iq>", j->id++, tag, val);
		xfree(val);
		return 0;
	}

	if (!xstrcmp(params[0], "--get")) {		/* display list */
		/* Usage:	--get [list] 		-- display list */

		char *val = jabber_escape(params[1] ? params[1] : session_get(session, "privacy_list"));

		watch_write(j->send_watch, "<iq type=\"get\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"><list name=\"%s\"/></query></iq>", j->id++, val ? val : "ekg2");
		xfree(val);
		return 0;
	}

	if (!xstrcmp(params[0], "--modify")) {		/* modify list's entry */
		printq("generic_error", "not impl, sorry");
		return -1;
		needsync = 1;
	}

	if (!xstrcmp(params[0], "--delete")) {		/* delete list's entry */
		printq("generic_error", "not impl, sorry");
		return -1;
		needsync = 1;
	}

	if (!xstrcmp(params[0], "--set")) {
		/* Usage: 	--set [list] jid:/@grupa/typ
		 *    --order xyz	: only with new lists... if you want to modify, please use --modify 
		 *    -*  		: set order to 1, enable blist[PRIVACY_LIST_ALL] 
		 *    +* 		: set order to 0, enable alist[PRIVACY_LIST_ALL]
		 *    -* +pin +pout +msg: set order to 1, enable alist[PRIVACY_LIST_PRESENCE_IN, PRIVACY_LIST_PRESENCE_OUT, PRIVACY_LIST_MESSAGE] && blist[PRIVACY_LIST_ALL] 
		 *    -pout -pin	: (order doesn't matter) enable blist[PRIVACY_LIST_PRESENCE_IN, PRIVACY_LIST_PRESENCE_OUT]
		 */

		const char *type;		/* <item type */
		const char *value;		/* <item value */

		if (!params[1]) {
			wcs_printq("invalid_params", name);
			return -1;
		}

		if (!xstrncmp(params[1], "jid:", 4))	{ type = "jid";		value = params[1]+4; }
		else if (params[1][0] == '@')		{ type = "group";	value = params[1]+1; }
		else if (!xstrcmp(params[1], "none") || !xstrcmp(params[1], "both") || !xstrcmp(params[1], "from") || !xstrcmp(params[1], "to"))
							{ type = "subscription"; value = params[1]; }
		else {
			wcs_printq("invalid_params", name);
			return -1;
		}

		if (session_int_get(session, "auto_privacylist_sync") == 0) {
			wcs_printq("generic_error", "If you really want to use jabber:iq:privacy list, you need to set session variable auto_privacylist_sync to 1 and reconnect.");
			return -1;
		}
#if 0
		/* order swaper */
		if (allowlist && denylist && (denylist->items & PRIVACY_LIST_ALL) && allowlist->order > denylist->order) {
			/* swap order if -* is passed (firstlist should be allowlist) */
			int tmp		= denylist->order;
			denylist->order		= allowlist->order;
			allowlist->order	= tmp;
		}
#endif
		if (params[2]) { /* parsing params[2] */
			char **p = array_make(params[2], " ", 0, 1, 0);
			jabber_iq_privacy_t *allowlist= NULL;
			jabber_iq_privacy_t *denylist = NULL;
			unsigned int order = 0;
			int opass = 0;
			int i;
			list_t l;

			for (l = j->privacy; l; l = l->next) {
				jabber_iq_privacy_t *p = l->data;

				if (!xstrcmp(p->value, value) && !xstrcmp(p->type, type)) {
					if (p->allow)	allowlist	= p;
					else		denylist	= p;
				}
			}

			for (i=0; p[i]; i++) { 
				int flag = -1;		/* bitmask PRIVACY_LIST_MESSAGE...PRIVACY_LIST_ALL */
				char *cur = p[i];	/* current */
				jabber_iq_privacy_t *lista = NULL;
				jabber_iq_privacy_t *lista2= NULL;

				if (!xstrcmp(p[i], "--order") && p[i + 1]) {
					order = atoi(p[++i]);
					opass = 1;
					continue;
				}

				if (cur[0] == '-')	{
					if (!denylist) denylist = xmalloc(sizeof(jabber_iq_privacy_t));
					lista = denylist; cur++; 
					lista2= allowlist;
				}
				else if (cur[0] == '+')	{
					if (!allowlist) allowlist = xmalloc(sizeof(jabber_iq_privacy_t));
					lista = allowlist; cur++; 
					lista2 = denylist;
				}
				
				if 	(!xstrcmp(cur, "iq"))	flag = PRIVACY_LIST_IQ;
				else if (!xstrcmp(cur, "msg")) 	flag = PRIVACY_LIST_MESSAGE;
				else if (!xstrcmp(cur, "pin"))	flag = PRIVACY_LIST_PRESENCE_IN;
  				else if (!xstrcmp(cur, "pout"))	flag = PRIVACY_LIST_PRESENCE_OUT;
  				else if (!xstrcmp(cur, "*"))	flag = PRIVACY_LIST_ALL;
  
 				if (flag == -1 || !lista) {
 					debug("[JABBER, PRIVACY] INVALID PARAM @ p[%d] = %s... [%d, 0x%x] \n", i, cur, flag, lista);
  					wcs_printq("invalid_params", name);
 					if (allowlist && !allowlist->value)	xfree(allowlist);
 					if (denylist && !denylist->value)	xfree(denylist);
  					array_free(p);
  					return -1;
  				}
  
 				lista->items |= flag;
  
				if (flag != PRIVACY_LIST_ALL && lista2 && (lista2->items & flag))	lista2->items		&= ~flag;	/* uncheck it on 2nd list */
  			}
  			array_free(p);
  
				/* jesli nie podano order, to bierzemy ostatnia wartosc */
 			if (!opass && !order && ( (denylist && !denylist->value) || (allowlist && !allowlist->value) )) for (l = j->privacy; l; l = l->next) {
 				jabber_iq_privacy_t *p = l->data;
  
 				if (!l->next) order = p->order+1;
 			}
 				/* podano order, musimy przesunac wszystkie elementy ktora maja ten order o 1 lub 2 (jesli jest tez denylist) w przod) */
 			else if (opass && order) for (l = j->privacy; l; l = l->next) {
 				jabber_iq_privacy_t *p = l->data;
 				int nextorder = (allowlist && denylist && !allowlist->value && !denylist->value);
 
 				if (p == allowlist || p == denylist) continue;
 
 				if (p->order == order || p->order == order+nextorder) {
 					jabber_iq_privacy_t *m = l->data;
 					list_t j = l;
 
 					if (p->order == order+nextorder) nextorder--;
 
 					do {
 						m = j->data;
 						m->order += (1 + nextorder);
 						j = j->next;
 					} while (j && ((jabber_iq_privacy_t *) j->data)->order <= m->order);
 					break;
 				}
 			}
				/* jesli jest -* to wtedy allowlista powinna byc przed denylista */
			if (denylist && !denylist->value && denylist->items & PRIVACY_LIST_ALL && allowlist && !allowlist->value)	denylist->order = 1;
				/* w przeciwnym wypadku najpierw deynlista */
			else if (denylist && allowlist && !denylist->value && !allowlist->value)					allowlist->order = 1;
 
 			if (allowlist && !allowlist->value) {
 				allowlist->value = xstrdup(value);
 				allowlist->type	 = xstrdup(type);
 				allowlist->allow = 1;
 				allowlist->order += order;
 				list_add_sorted(&j->privacy, allowlist, 0, jabber_privacy_add_compare);
 			} 
  
 			if (denylist && !denylist->value) {
 				denylist->value = xstrdup(value);
 				denylist->type	= xstrdup(type);
 /*				denylist->allow = 0; */
 				denylist->order += order;
 				list_add_sorted(&j->privacy, denylist, 0, jabber_privacy_add_compare);
 			} 
 		}
 		needsync = 1;
 	}
	if (!xstrcmp(params[0], "--unset")) {
		/* usage: 	--unset [lista] Unset / remove list */

		char *val = jabber_escape(!needsync && params[1] ? params[1] : session_get(session, "privacy_list"));
		watch_write(j->send_watch, "<iq type=\"set\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"><list name=\"%s\"/></query></iq>", j->id++, val ? val : "ekg2");
		xfree(val);
		return 0;
	}
 
 	if ((needsync || !xstrcmp(params[0], "--sync")) && j->privacy) {
 		/* Usage:	--sync			-- sync default list [internal use] */

 		static unsigned int last_order;
 		char *val = jabber_escape(!needsync && params[1] ? params[1] : session_get(session, "privacy_list"));
 		list_t l;
 
 		last_order = 0;

		if (j->send_watch)	j->send_watch->transfer_limit = -1;
 
 		watch_write(j->send_watch, "<iq type=\"set\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"><list name=\"%s\">", j->id++, val ? val : "ekg2");
 
 		for (l = j->privacy; l; l = l->next) {
 			jabber_iq_privacy_t *p = l->data;
 			char *eval;
 
 			if (!p->items) continue;			/* XXX, remove? */
 			eval = jabber_escape(p->value);
 		/* XXX XXX */
 			if (last_order == p->order) p->order++;
 			last_order = p->order;
 
 			watch_write(j->send_watch, "<item type=\"%s\" value=\"%s\" action=\"%s\" order=\"%d\">", 
 				p->type,												/* type (jid/group/subscription)*/
 				eval, 													/* value */
 				p->allow ? "allow" : "deny",										/* action */
 				p->order);												/* order */
 
 			if (p->items & PRIVACY_LIST_MESSAGE)		watch_write(j->send_watch, "<message/>");
 			if (p->items & PRIVACY_LIST_IQ)			watch_write(j->send_watch, "<iq/>");
 			if (p->items & PRIVACY_LIST_PRESENCE_IN)	watch_write(j->send_watch, "<presence-in/>");
 			if (p->items & PRIVACY_LIST_PRESENCE_OUT)	watch_write(j->send_watch, "<presence-out/>");
 
 			watch_write(j->send_watch, "</item>");
  		}
  		watch_write(j->send_watch, "</list></query></iq>");
 		xfree(val);

		JABBER_COMMIT_DATA(j->send_watch);

 		return 0;
 	}

	if (params[0] && params[0][0] != '-') /* jesli nie opcja, to pewnie jest to lista, wyswietlamy liste */
		return command_exec_format(target, session, 0, "/jid:privacy --get %s", params[0]);

 	wcs_print("invalid_params", name);
 	return 1;
}
 
#if 0
static const char *jabber_ignore_format(int level) {
 	static char buf[100];
 
 	buf[0] = 0;
 
 	if (level == PRIVACY_LIST_ALL) return "*";
 	
 	if (level & PRIVACY_LIST_MESSAGE)	xstrcat(buf, "msg,");
 	if (level & PRIVACY_LIST_IQ)		xstrcat(buf, "iq,");
 	if (level & PRIVACY_LIST_PRESENCE_IN)	xstrcat(buf, "notify,");
 	if (level & PRIVACY_LIST_PRESENCE_OUT)	xstrcat(buf, "notifyout,");
 
 	if (!buf[0]) return buf;
 
 	buf[xstrlen(buf)-1] = 0;
 
 	return buf;
}

static COMMAND(jabber_command_ignore) {	/* emulates ekg2's /ignore with jabber_command_privacy() */
 	jabber_private_t *j = jabber_private(session);
 	const char *uid;
 
 	if (*name == 'i' || *name == 'I') {
 		int flags, modified = 0;
 
 		if (!params[0]) {
 			list_t l;
 			int i = 0;
 
 			for (l = session->userlist; l; l = l->next) {
 				userlist_t *u = l->data;
 				int level;
 
 				if (!(level = ignored_check(session, u->uid))) continue;
 				printq("ignored_list", format_user(session, u->uid), ignore_format(level));
 				i++;
 			}
 			for (l = j->privacy; l; l = l->next) {
 				jabber_iq_privacy_t *p = l->data;
 
 				if (!p->items) continue;
 				if (!xstrcmp(p->type, "uid")) printq("ignored_list", format_user(session, p->value), jabber_ignore_format(p->items));
 				i++;
 			}
 
 			if (!i)
 				wcs_printq("ignored_list_empty");
 
 			return 0;
 		}
 		if (params[0][0] == '#') {
 			return command_exec_format(NULL, NULL, quiet, ("/conference --ignore %s"), params[0]);
 		}
	/* XXX, use privacy list here */
		if ((flags = ignored_check(session, get_uid(session, params[0]))))
			modified = 1;

		if (params[1]) {
			int __flags = ignore_flags(params[1]);

			if (!__flags) {
				wcs_printq("invalid_params", name);
				return -1;
			}

			flags |= __flags;
		} else
			flags = IGNORE_ALL;

		if (!(uid = get_uid(session, params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}

		if (modified)
			ignored_remove(session, uid);

		if (!ignored_add(session, uid, flags)) {
			if (modified)
				printq("ignored_modified", format_user(session, uid));
			else
				printq("ignored_added", format_user(session, uid));
			config_changed = 1;
		}
	/* XXX, end */
  	} else {
 		int unignore_all = ((params[0] && !xstrcmp(params[0], "*")) ? 1 : 0);
 		int retcode = 0;
 
 		if (!params[0]) {
 			wcs_printq("not_enough_params", name);
  			return -1;
  		}
 		if (params[0][0] == '#') {
 			return command_exec_format(NULL, NULL, quiet, ("/conference --unignore %s"), params[0]);
 		}
 
 		if (unignore_all) {
 			list_t l;
			int x = 0;
/* 			int x = command_exec_format(NULL, session, 1, ("/jid:privacy --unset")) == 0 ? 1 : 0; */	/* bad idea? */	/* --delete * -*  ??!! XXX */ 
 
 			for (l = session->userlist; l; ) {
 				userlist_t *u = l->data;
 				l = l->next;
 
 				if (!ignored_remove(session, u->uid)) 	x = 1;
 			}
  
 			if (x) {
 				wcs_printq("ignored_deleted_all");
 				config_changed = 1;
 			} else {
 				wcs_printq("ignored_list_empty");
				return -1;
 			}
 			
 			return 0;
 		} else if (!(uid = get_uid(session, params[0]))) {
 			printq("user_not_found", params[0]);
 			return -1;
  		}
 		retcode = command_exec_format(NULL, session, 1, ("/jid:privacy --delete %s"), params[0]) == 0 ? 1 : 0;
 		retcode |= !ignored_remove(session, uid);
  
 		if (retcode) 	{ printq("ignored_deleted", format_user(session, params[0]));	config_changed = 1;	}
 		else		{ printq("error_not_ignored", format_user(session, params[0]));	return -1; 		}
  	}
  	return 0;
}
#endif

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
	char *username = (params[1]) ? xstrdup(params[1]) : (tmp = xstrchr(session->uid, '@')) ? xstrndup(session->uid+4, tmp-session->uid-4) : NULL;
	char *password = (params[1] && params[2]) ? saprintf("<password>%s</password>", params[2]) : NULL;

	if (!username) { /* rather impossible */
		wcs_printq("invalid_params", name);
		return -1;
	}

	if (!xstrncmp(target, "jid:", 4)) target += 4; /* remove jid: */

	watch_write(j->send_watch, "<presence to='%s/%s'><x xmlns='http://jabber.org/protocol/muc#user'>%s</x></presence>", 
			target, username, password ? password : "");
	{
		char *uid = saprintf("jid:%s", target);
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
		printq("generic_error", "/jid:part only valid in MUC");
		return -1;
	}

	status = params[1] ? saprintf(" <status>%s</status> ", params[1]) : NULL;

	watch_write(j->send_watch, "<presence to=\"%s/%s\" type=\"unavailable\">%s</presence>", c->name+4, c->private, status ? status : "");

	xfree(status);
	newconference_destroy(c, 1 /* XXX, dorobic zmienna */);
	return 0;
}

static COMMAND(jabber_muc_command_admin) {
	jabber_private_t *j = session_private_get(session);
	newconference_t *c;

	if (!(c = newconference_find(session, target))) {
		printq("generic_error", "/jid:admin only valid in MUC");
		return -1;
	}

	if (!params[1]) {
		watch_write(j->send_watch,
			"<iq id=\"mucadmin%d\" to=\"%s\" type=\"get\">"
			"<query xmlns=\"http://jabber.org/protocol/muc#owner\"/>"
			"</iq>", j->id++, c->name+4);
	} else {
		char **splitted = NULL;
		int i;
		int isinstant = !xstrcmp(params[1], "--instant");

		if (isinstant) {
			watch_write(j->send_watch,
				"<iq type=\"set\" to=\"%s\" id=\"mucadmin%d\">"
				"<query xmlns=\"http://jabber.org/protocol/muc#owner\">"
				"<x xmlns=\"jabber:x:data\" type=\"submit\"/>"
				"</query></iq>", c->name+4, j->id++);
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
				,c->name+4, j->id++);

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
		printq("generic_error", "/jid:ban && /jin:kick && /jid:unban only valid in MUC");
		return -1;
	}
/* XXX, make check if command = "kick" than check if user is on the muc channel... cause we can make /unban */

	if (!params[1]) {
		watch_write(j->send_watch, 
			"<iq id=\"%d\" to=\"%s\" type=\"get\">"
			"<query xmlns=\"http://jabber.org/protocol/muc#admin\"><item affiliation=\"outcast\"/></query>"
			"</iq>", j->id++, c->name+4);
	} else {
		char *reason	= jabber_escape(params[2]);
		const char *jid	= params[1];

		if (!xstrncmp(jid, "jid:", 4)) jid += 4;

		watch_write(j->send_watch,
			"<iq id=\"%d\" to=\"%s\" type=\"set\">"
			"<query xmlns=\"http://jabber.org/protocol/muc#admin\"><item affiliation=\"%s\" jid=\"%s\"><reason>%s</reason></item></query>"
			"</iq>", j->id++, c->name+4, 
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
		printq("generic_error", "/jid:topic only valid in MUC");
		return -1;
	}
	
	if (!params[1]) {
		/* XXX, display current topic */

	} else {
		char *subject = jabber_escape(params[1]);
		watch_write(j->send_watch, "<message to=\"%s\" type=\"groupchat\"><subject>%s</subject></message>", c->name+4, subject);
	} 

	return 0;
}

static COMMAND(jabber_command_control) {
	/* w params[0] full uid albo samo resource....						*/
	/* w params[1] polecenie... jesli nie ma to chcemy dostac liste dostepnych polecen 	*/
	/* w params[2] polecenia do sparsowania 						*/

	jabber_private_t *j = session_private_get(session);
	char *resource, *uid;
	char *tmp;
	char *nodename = NULL;

/* if !params[0] display list of all our resources? */

	if ((tmp = xstrchr(params[0], '/'))) {
		uid		= xstrndup(params[0], tmp-params[0]);
		resource	= xstrdup(tmp+1);
	} else {
		uid		= xstrdup(session->uid+4);
		resource	= xstrdup(params[0]);
	}
	debug("jabber_command_control() uid: %s res: %s\n", uid, resource);
	
	if (params[1]) {
		/* short param to long nodename :) */
		if (!xstrcmp(params[1], "set-status"))		nodename = saprintf("http://jabber.org/protocol/rc#set-status");
		else if (!xstrcmp(params[1], "forward")) 	nodename = saprintf("http://jabber.org/protocol/rc#forward");
		else if (!xstrcmp(params[1], "set-options"))	nodename = saprintf("http://jabber.org/protocol/rc#set-options");
		else if (!xstrcmp(params[1], "ekg-set-all-options"))	nodename = saprintf("http://ekg2.org/jabber/rc#ekg-set-all-options");
		else if (!xstrcmp(params[1], "ekg-command-execute"))	nodename = saprintf("http://ekg2.org/jabber/rc#ekg-command-execute");
		else if (!xstrcmp(params[1], "ekg-manage-plugins"))	nodename = saprintf("http://ekg2.org/jabber/rc#ekg-manage-plugins");
		else if (!xstrcmp(params[1], "ekg-manage-sessions"))	nodename = saprintf("http://ekg2.org/jabber/rc#ekg-manage-sesions");
	}
	switch (array_count((char **) params)) {
		case 1:
			watch_write(j->send_watch, 
					"<iq type=\"get\" to=\"%s/%s\" id=\"control%d\">"
					"<query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"http://jabber.org/protocol/commands\"/></iq>",
					uid, resource, j->id++);
			/* wrapper to jid:transports ? */
			/*		return command_format_exec(target, session, quiet, "/jid:transports %s http://jabber.org/protocol/commands"); */
			break;
		case 2:
			/* .... */
			watch_write(j->send_watch,
					"<iq type=\"set\" to=\"%s/%s\" id=\"control%d\">"
					"<command xmlns=\"http://jabber.org/protocol/commands\" node=\"%s\"/></iq>",
					uid, resource, j->id++, nodename ? nodename : params[1]);
			break;
		default: {
				 char **splitted;
				 char *fulluid = saprintf("%s/%s", uid, resource); 
				 char *FORM_TYPE = xstrdup(nodename ? nodename : params[1]), *tmp;
				 int i;

				 if ((tmp = xstrchr(FORM_TYPE, '#'))) *tmp = '\0';

				 if (!(splitted = jabber_params_split(params[2], 0))) {
					 printq("invalid_params", name);
					 goto cleanup;
					 return -1;
				 }
				 printq("jabber_remotecontrols_executing", session_name(session), fulluid, nodename ? nodename : params[1], params[2]) ;

				 watch_write(j->send_watch, 
						 "<iq type=\"set\" to=\"%s\" id=\"control%d\">"
						 "<command xmlns=\"http://jabber.org/protocol/commands\" node=\"%s\">"
						 "<x xmlns=\"jabber:x:data\" type=\"submit\">"
						 "<field var=\"FORM_TYPE\" type=\"hidden\"><value>%s</value></field>",
						 fulluid, j->id++, nodename ? nodename : params[1], FORM_TYPE);

				 for (i=0; (splitted[i] && splitted[i+1]); i+=2) {
					 char *varname = jabber_escape(splitted[i]);
					 char *varval = jabber_escape(splitted[i+1]); /* ? */

					 watch_write(j->send_watch, "<field var=\"%s\"><value>%s</value></field>", varname, varval);

					 xfree(varname); xfree(varval);
				 }
				 watch_write(j->send_watch, "</x></command></iq>");
				 array_free(splitted);

cleanup:
				 xfree(fulluid);
				 xfree(FORM_TYPE);
		}
	}

	xfree(nodename);

	xfree(uid);
	xfree(resource);
	return 0;
}

static COMMAND(jabber_command_private) {
	jabber_private_t *j = jabber_private(session);
	char *namespace; 	/* <nazwa> */

	int config = 0;			/* 1 if name == jid:config */
	int bookmark = 0;		/* 1 if name == jid:bookmark */

	if (!xstrcmp(name, ("config")))	config = 1;
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
				wcs_printq("invalid_params", name);
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
				wcs_printq("invalid_params", name);
				return -1;
			}
		}
	}

	if (match_arg(params[0], 'g', ("get"), 2) || match_arg(params[0], 'd', ("display"), 2)) {	/* get/display */
		watch_write(j->send_watch,
			"<iq type=\"get\" id=\"%s%d\">"
			"<query xmlns=\"jabber:iq:private\">"
			"<%s/>"
			"</query></iq>", 
			(match_arg(params[0], 'g', ("get"), 2) && (config || bookmark) ) ? "config" : "private", 
			j->id++, namespace);
		return 0;
	}

	if (match_arg(params[0], 'p', ("put"), 2)) {							/* put */
		list_t l;

		if (j->send_watch) j->send_watch->transfer_limit = -1;
		watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"private%d\">"
			"<query xmlns=\"jabber:iq:private\">"
			"<%s>", j->id++, namespace);

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
				list_t n;
				watch_write(j->send_watch, "<session xmlns=\"ekg2:session\" uid=\"%s\" password=\"%s\">", s->uid, s->password);

				for (n = session->params; n; n = n->next) {
					session_param_t *v = n->data;
					if (v->value)	watch_write(j->send_watch, "<%s>%s</%s>", v->key, v->value, v->key);
					else		watch_write(j->send_watch, "<%s/>", v->key);
				}
				watch_write(j->send_watch, "</session>");
			}
		} else if (bookmark) {	/* synchronize with j->bookmarks using JEP-0048 */
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
		} else {
			if (params[0] && params[1] && params[2]) /* XXX check */
				watch_write(j->send_watch, "%s", params[2]);
		}
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
		if (bookmark) jabber_bookmarks_free(j);			/* let's destroy previously saved bookmarks */
		watch_write(j->send_watch,
			"<iq type=\"set\" id=\"private%d\">"
			"<query xmlns=\"jabber:iq:private\">"
			"<%s/></query></iq>", j->id++, namespace);
		return 0;
	}

	wcs_printq("invalid_params", name);
	return -1;
}

void jabber_register_commands()
{
#define JABBER_ONLY         SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define JABBER_FLAGS        JABBER_ONLY  | SESSION_MUSTBECONNECTED
#define JABBER_FLAGS_TARGET JABBER_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET

	command_add(&jabber_plugin, ("jid:"), "?", jabber_command_inline_msg, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:_autoaway"), "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:_autoback"), "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:add"), "!U ?", jabber_command_modify, 	JABBER_FLAGS_TARGET, NULL); 
	command_add(&jabber_plugin, ("jid:auth"), "!p !uU", jabber_command_auth, 	JABBER_FLAGS | COMMAND_ENABLEREQPARAMS, 
			"-a --accept -d --deny -r --request -c --cancel");
	command_add(&jabber_plugin, ("jid:away"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:back"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:bookmark"), "!p ?", jabber_command_private, JABBER_ONLY | COMMAND_ENABLEREQPARAMS, 
			"-a --add -c --clear -d --display -m --modify -r --remove");
	command_add(&jabber_plugin, ("jid:change"), "!p ? p ? p ? p ? p ? p ?", jabber_command_change, JABBER_FLAGS | COMMAND_ENABLEREQPARAMS , 
			"-f --fullname -c --city -b --born -d --description -n --nick -C --country");
	command_add(&jabber_plugin, ("jid:chat"), "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:config"), "!p", jabber_command_private,	JABBER_ONLY | COMMAND_ENABLEREQPARAMS, 
			"-c --clear -d --display -g --get -p --put");
	command_add(&jabber_plugin, ("jid:connect"), "r ?", jabber_command_connect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:control"), "! ? ?", jabber_command_control, JABBER_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
#if 0
	command_add(&jabber_plugin, ("jid:dcc"), "p uU f ?", jabber_command_dcc,	JABBER_ONLY, 
			"send get resume voice close list");
#endif
	command_add(&jabber_plugin, ("jid:del"), "!u", jabber_command_del, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:disconnect"), "r ?", jabber_command_disconnect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:dnd"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
//	command_add(&jabber_plugin, ("jid:ignore"), "uUC I", jabber_command_ignore,	JABBER_ONLY, "status descr notify msg dcc events *");
	command_add(&jabber_plugin, ("jid:invisible"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:ffc"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:msg"), "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:modify"), "!Uu !", jabber_command_modify,JABBER_FLAGS_TARGET, 
			"-n --nickname -g --group");
	command_add(&jabber_plugin, ("jid:passwd"), "!", jabber_command_passwd, 	JABBER_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&jabber_plugin, ("jid:privacy"), "? ? ?", jabber_command_privacy,	JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, ("jid:private"), "!p ! ?", jabber_command_private,   JABBER_ONLY | COMMAND_ENABLEREQPARAMS, 
			"-c --clear -d --display -p --put");
	command_add(&jabber_plugin, ("jid:reconnect"), NULL, jabber_command_reconnect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:search"), "? ?", jabber_command_search, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, ("jid:stats"), "? ?", jabber_command_stats, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, ("jid:transpinfo"), "? ?", jabber_command_transpinfo, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, ("jid:transports"), "? ?", jabber_command_transports, JABBER_FLAGS, NULL);
//	command_add(&jabber_plugin, ("jid:unignore"), "i ?", jabber_command_ignore, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:vacation"), "?", jabber_command_vacation, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, ("jid:ver"), "!u", jabber_command_ver, 	JABBER_FLAGS_TARGET, NULL); /* ??? ?? ? ?@?!#??#!@? */
	command_add(&jabber_plugin, ("jid:userinfo"), "!u", jabber_command_userinfo, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:lastseen"), "!u", jabber_command_lastseen, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:register"), "? ?", jabber_command_register, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:xa"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:xml"), "!", jabber_command_xml, 	JABBER_ONLY | COMMAND_ENABLEREQPARAMS, NULL);
/* MUC/ old conferences XXX */
	command_add(&jabber_plugin, ("jid:admin"), "! ?", jabber_muc_command_admin, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:join"), "! ? ?", jabber_muc_command_join, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:part"), "! ?", jabber_muc_command_part, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:ban"), "! ? ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:unban"), "! ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:kick"), "! ! ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, ("jid:topic"), "! ?", jabber_muc_command_topic, JABBER_FLAGS_TARGET, NULL);


	command_add(&jabber_plugin, ("tlen:auth"), "!p !uU", 	jabber_command_auth,		JABBER_FLAGS | COMMAND_ENABLEREQPARAMS,
			"-a --accept -d --deny -r --request -c --cancel");
	command_add(&jabber_plugin, ("tlen:connect"), "r ?",	jabber_command_connect,		JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:disconnect"), "r ?",	jabber_command_disconnect,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:reconnect"), NULL,	jabber_command_reconnect,	JABBER_ONLY, NULL);

	command_add(&jabber_plugin, ("tlen:"), "?",			jabber_command_inline_msg, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:msg"), "!uU !",		jabber_command_msg, 		JABBER_FLAGS_TARGET, NULL);

	command_add(&jabber_plugin, ("tlen:change"), "?",		tlen_command_pubdir, 		JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, ("tlen:search"), "?",		tlen_command_pubdir, 		JABBER_FLAGS, NULL);

	command_add(&jabber_plugin, ("jid:_autoaway"), "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("jid:_autoback"), "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:away"), "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:back"), "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:dnd"), "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:invisible"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, ("tlen:ffc"), "r",	jabber_command_away, 	JABBER_ONLY, NULL);
};

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
