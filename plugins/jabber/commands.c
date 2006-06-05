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
#include <ekg/win32.h>

#include <sys/types.h>
#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/utsname.h> /* dla jabber:iq:version */
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "jabber.h"

COMMAND(jabber_command_dcc) {
	PARASC
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

void jabber_command_connect_child(
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

int jabber_command_connect_child_win32(void *data) {
	struct win32_temp *helper = data;
	
	CloseHandle(helper->fd2);
	jabber_command_connect_child(helper->server, helper->fd);
	xfree(helper);
	return 0;
}

#endif

COMMAND(jabber_command_connect)
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
	
	if (!(server = xstrchr(session->uid, '@'))) {
		printq("wrong_id", session->uid);
		return -1;
	}

	xfree(j->server);
	j->server	= xstrdup(++server);
	if (!realserver) realserver = server;

	debug("[jabber] resolving %s\n", server);

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
		watch_add(&jabber_plugin, fd[0], WATCH_READ, 0, jabber_handle_resolver, session);
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

COMMAND(jabber_command_disconnect)
{
	PARASC
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
	if (xwcscmp(name, TEXT("reconnect"))) {
		if (params[0])
			descr = xstrdup(params[0]);
		else
			descr = ekg_draw_descr("quit");
	} else
		descr = xstrdup(session_descr_get(session));

	if (descr) {
		CHAR_T *tmp = jabber_escape(descr);
		watch_write(j->send_watch, "<presence type=\"unavailable\"><status>" CHARF "</status></presence>", tmp ? tmp : TEXT(""));
		xfree(tmp);
	} else
		watch_write(j->send_watch, "<presence type=\"unavailable\"/>");

	watch_write(j->send_watch, "</stream:stream>");

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

COMMAND(jabber_command_reconnect)
{
	PARUNI
	jabber_private_t *j = session_private_get(session);

	if (j->connecting || session_connected_get(session)) {
		jabber_command_disconnect(name, params, session, target, quiet);
	}

	return jabber_command_connect(name, params, session, target, quiet);
}

const char *jid_target2uid(session_t *s, const char *target, int quiet) {
	const char *uid;
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
	if (xstrncasecmp(uid, "jid:", 4)) {
		printq("invalid_session");
		return NULL;
	}
	if (!xstrchr(uid, '@') || xstrchr(uid, '@') > xstrrchr(uid, '.')) {
		printq("invalid_uid", uid);
		return NULL;
	}
	return uid;
}

COMMAND(jabber_command_msg)
{
	PARUNI
	jabber_private_t *j = session_private_get(session);
	int chat = !xwcscasecmp(name, TEXT("chat"));
	int subjectlen = xstrlen(config_subject_prefix);
	CHAR_T *msg;
	char *subject = NULL;
	const char *uid;

	window_t *w;
	int ismuc = 0;

	if (!xstrcmp(target, "*")) {
		if (msg_all(session, name, params[1]) == -1)
			printq("list_empty");
		return 0;
	}
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	/* czy wiadomo¶æ ma mieæ temat? */
#ifndef USE_UNICODE
	if (config_subject_prefix && !xstrncmp(params[1], config_subject_prefix, subjectlen)) {
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
#else
#warning TOPIC NOT SUPPORTED IN UNICODE VERSION.
#endif
		msg = jabber_uescape(params[1]); /* bez tematu */
	if ((w = window_find_s(session, target)) && (w->userlist))
		ismuc = 1;

	if (ismuc)
		watch_write(j->send_watch, "<message type=\"groupchat\" to=\"%s\" id=\"%d\">", uid+4, time(NULL));
	else
		watch_write(j->send_watch, "<message %sto=\"%s\" id=\"%d\">", chat ? "type=\"chat\" " : "", uid+4, time(NULL));

	if (subject) {
		watch_write(j->send_watch, "<subject>%s</subject>", subject); 
		xfree(subject); 
	}
	if (msg) {
		watch_write(j->send_watch, "<body>" CHARF "</body>", msg);
        	if (config_last & 4) 
        		last_add(1, uid, time(NULL), 0, msg);
		xfree(msg);
	}

	watch_write(j->send_watch, "<x xmlns=\"jabber:x:event\">%s%s<displayed/><composing/></x>", 
		( config_display_ack == 1 || config_display_ack == 2 ? "<delivered/>" : ""),
		( config_display_ack == 1 || config_display_ack == 3 ? "<offline/>"   : "") );
	watch_write(j->send_watch, "</message>");

	if (!quiet && !ismuc) { /* if (1) ? */ 
		CHAR_T *me 	= xwcsdup( normal_to_wcs(  session_uid_get(session)  ));
		CHAR_T **rcpts 	= xcalloc(2, sizeof(CHAR_T *));
		CHAR_T *msg	= xwcsdup(params[1]);
		time_t sent 	= time(NULL);
		int class 	= (chat) ? EKG_MSGCLASS_SENT_CHAT : EKG_MSGCLASS_SENT;
		int ekgbeep 	= EKG_NO_BEEP;
		CHAR_T *format 	= NULL;
		CHAR_T *seq 	= NULL;
		int secure	= 0;

		rcpts[0] 	= xwcsdup(normal_to_wcs(uid));
		rcpts[1] 	= NULL;

		if (ismuc)
			class |= EKG_NO_THEMEBIT;
		
		query_emit(NULL, "wcs_protocol-message", &me, &me, &rcpts, &msg, &format, &sent, &class, &seq, &ekgbeep, &secure);

		xfree(msg);
		xfree(me);
		wcs_array_free(rcpts);
	}

	session_unidle(session);

	return 0;
}

COMMAND(jabber_command_inline_msg)
{
	PARUNI
	const CHAR_T *p[2] = { NULL, params[0] };
	if (!params[0] || !target)
		return -1;
	return jabber_command_msg(TEXT("chat"), p, session, target, quiet);
}

COMMAND(jabber_command_xml)
{
	PARUNI
	jabber_private_t *j = session_private_get(session);
	watch_write(j->send_watch, CHARF, params[0]);
	return 0;
}

COMMAND(jabber_command_away)
{
	PARASC
	const char *descr, *format;
	
	if (params[0]) {
		session_descr_set(session, (!xstrcmp(params[0], "-")) ? NULL : params[0]);
		reason_changed = 1;
	} 
	if (!xwcscmp(name, TEXT("_autoback"))) {
		format = "auto_back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xwcscmp(name, TEXT("back"))) {
		format = "back";
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
	} else if (!xwcscmp(name, TEXT("_autoaway"))) {
		format = "auto_away";
		session_status_set(session, EKG_STATUS_AUTOAWAY);
	} else if (!xwcscmp(name, TEXT("away"))) {
		format = "away"; 
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
	} else if (!xwcscmp(name, TEXT("dnd"))) {
		format = "dnd";
		session_status_set(session, EKG_STATUS_DND);
		session_unidle(session);
	} else if (!xwcscmp(name, TEXT("ffc"))) {
	        format = "ffc";
	        session_status_set(session, EKG_STATUS_FREE_FOR_CHAT);
                session_unidle(session);
        } else if (!xwcscmp(name, TEXT("xa"))) {
		format = "xa";
		session_status_set(session, EKG_STATUS_XA);
		session_unidle(session);
	} else if (!xwcscmp(name, TEXT("invisible"))) {
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

COMMAND(jabber_command_passwd)
{
	PARASC
	jabber_private_t *j = session_private_get(session);
	char *username;
	CHAR_T *passwd;

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

COMMAND(jabber_command_auth) 
{
	PARUNI
	jabber_private_t *j = session_private_get(session);
	session_t *s = session;
	const char *action;
	const char *uid;

	if (!(uid = jid_target2uid(session, params[1], quiet)))
		return -1;
	/* user jest OK, wiêc lepiej mieæ go pod rêk± */
	tabnick_add(uid);

	if (match_arg(params[0], 'r', TEXT("request"), 2)) {
		action = "subscribe";
		printq("jabber_auth_request", uid+4, session_name(s));
	} else if (match_arg(params[0], 'a', TEXT("accept"), 2)) {
		action = "subscribed";
		printq("jabber_auth_accept", uid+4, session_name(s));
	} else if (match_arg(params[0], 'c', TEXT("cancel"), 2)) {
		action = "unsubscribe";
		printq("jabber_auth_unsubscribed", uid+4, session_name(s));
	} else if (match_arg(params[0], 'd', TEXT("deny"), 2)) {
		action = "unsubscribed";

		if (userlist_find(session, uid))  // mamy w rosterze
			printq("jabber_auth_cancel", uid+4, session_name(s));
		else // nie mamy w rosterze
			printq("jabber_auth_denied", uid+4, session_name(s));
	
	} else if (match_arg(params[0], 'p', TEXT("probe"), 2)) {
	/* ha! undocumented :-); bo 
	   [Used on server only. Client authors need not worry about this.] */
		action = "probe";
		printq("jabber_auth_probe", uid+4, session_name(s));
	} else {
		wcs_printq("invalid_params", name);
		return -1;
	}

	watch_write(j->send_watch, "<presence to=\"%s\" type=\"%s\" id=\"roster\"/>", uid+4, action);
	return 0;
}

COMMAND(jabber_command_modify)
/* XXX REWRITE IT */
{
	PARASC
	jabber_private_t *j = session_private_get(session);
	const char *uid = NULL;
	CHAR_T *nickname = NULL;
	int ret = 0;
	userlist_t *u;
	list_t m;
	
	int addcomm = !xwcscasecmp(name, TEXT("add"));

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

			if (nmatch_arg(argv[i], 'g', TEXT("group"), 2) && argv[i + 1]) {
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

			if (nmatch_arg(argv[i], 'n', TEXT("nickname"), 2) && argv[i + 1]) {
				xfree(nickname);
				nickname = jabber_escape(argv[++i]);
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

	watch_write(j->send_watch, "<iq type=\"set\"><query xmlns=\"jabber:iq:roster\">");

	/* nickname always should be set */
	if (nickname)	watch_write(j->send_watch, "<item jid=\"%s\" name=\"" CHARF "\"%s>", uid+4, nickname, (u->groups ? "" : "/"));
	else		watch_write(j->send_watch, "<item jid=\"%s\"%s>", uid+4, (u->groups ? "" : "/"));

	for (m = u->groups; m ; m = m->next) {
		struct ekg_group *g = m->data;
		CHAR_T *gname = jabber_escape(g->name);

		watch_write(j->send_watch, "<group>"CHARF"</group>", gname);
		xfree(gname);
	}

	if (u->groups)
		watch_write(j->send_watch, "</item>");

	watch_write(j->send_watch, "</query></iq>");

	xfree(nickname);
	
	if (addcomm) {
		xfree(u);
		return command_exec_format(target, session, 0, TEXT("/auth --request %s"), uid);
	}
	
	return ret;
}

COMMAND(jabber_command_del)
{
	const char *uid;
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	{
		jabber_private_t *j = session_private_get(session);
		CHAR_T *xuid = jabber_escape(uid+4);
		watch_write(j->send_watch, "<iq type=\"set\" id=\"roster\"><query xmlns=\"jabber:iq:roster\">");
		watch_write(j->send_watch, "<item jid=\"" CHARF "\" subscription=\"remove\"/></query></iq>", xuid);
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

COMMAND(jabber_command_ver)
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
		CHAR_T *xuid = jabber_escape(uid+4);
		CHAR_T *xquery_res = jabber_escape(query_res);
       		watch_write(j->send_watch, "<iq id='%d' to='" CHARF"/"CHARF "' type='get'><query xmlns='jabber:iq:version'/></iq>", \
			     j->id++, xuid, xquery_res);
		xfree(xuid);
		xfree(xquery_res);
	}
	return 0;
}

COMMAND(jabber_command_userinfo)
{
	const char *uid;

	/* jabber id: [user@]host[/resource] */
	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;
	{ 
		jabber_private_t *j = session_private_get(session);
		CHAR_T *xuid = jabber_escape(uid+4);
       		watch_write(j->send_watch, "<iq id='%d' to='" CHARF "' type='get'><vCard xmlns='vcard-temp'/></iq>", \
			     j->id++, xuid);
		xfree(xuid);
	}
	return 0;
}

COMMAND(jabber_command_change)
{
	PARUNI
#define pub_sz 6
#define strfix(s) (s ? s : TEXT(""))
	jabber_private_t *j = session_private_get(session);
	CHAR_T *pub[pub_sz] = { NULL, NULL, NULL, NULL, NULL, NULL };
	int i;

	for (i = 0; params[i]; i++) {
		if (match_arg(params[i], 'f', TEXT("fullname"), 2) && params[i + 1]) {
			pub[0] = (CHAR_T *) params[++i];
		} else if (match_arg(params[i], 'n', TEXT("nickname"), 2) && params[i + 1]) {
			pub[1] = (CHAR_T *) params[++i];
		} else if (match_arg(params[i], 'c', TEXT("city"), 2) && params[i + 1]) {
			pub[2] = (CHAR_T *) params[++i];
		} else if (match_arg(params[i], 'b', TEXT("born"), 2) && params[i + 1]) {
			pub[3] = (CHAR_T *) params[++i];
		} else if (match_arg(params[i], 'd', TEXT("description"), 2) && params[i + 1]) {
			pub[4] = (CHAR_T *) params[++i];
		} else if (match_arg(params[i], 'C', TEXT("country"), 2) && params[i + 1]) {
			pub[5] = (CHAR_T *) params[++i];
		}

	}
	for (i=0; i<pub_sz; i++) 
		pub[i] = jabber_uescape(pub[i]);
	watch_write(j->send_watch, "<iq type=\"set\"><vCard xmlns='vcard-temp'>"
			"<FN>" CHARF "</FN>" "<NICKNAME>" CHARF "</NICKNAME>"
			"<ADR><LOCALITY>" CHARF "</LOCALITY><COUNTRY>" CHARF "</COUNTRY></ADR>"
			"<BDAY>" CHARF "</BDAY><DESC>" CHARF "</DESC></vCard></iq>\n", 
			strfix(pub[0]), strfix(pub[1]), strfix(pub[2]), strfix(pub[5]), strfix(pub[3]), strfix(pub[4]));

	for (i=0; i<pub_sz; i++) 
		xfree(pub[i]);
	return 0;
}

COMMAND(jabber_command_lastseen)
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
		CHAR_T *xuid = jabber_escape(uid+4);
	       	watch_write(j->send_watch, "<iq id='%d' to='" CHARF "' type='get'><query xmlns='jabber:iq:last'/></iq>", \
			     j->id++, xuid);
		xfree(xuid);
	}
	return 0;
}

char **jabber_params_split(const char *line, int allow_empty)
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

COMMAND(jabber_command_search) {
	PARASC
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server; /* jakis server obsluguje jabber:iq:search ? :) */ 
	/* XXX, made (session?) variable: jabber:default_search_server */
	char **splitted;

	if (!(splitted = jabber_params_split(params[1], 0)) && params[1]) {
		printq("invalid_params", name);
		return -1;
	}

	watch_write(j->send_watch, 
		"<iq type=\"%s\" to=\"%s\" id=\"search%d\"><query xmlns=\"jabber:iq:search\">", params[1] ? "set" : "get", server, j->id++);

	if (splitted) {
		int i;
		for (i=0; (splitted[i] && splitted[i+1]); i+=2) {
			watch_write(j->send_watch, "<%s>%s</%s>\n", splitted[i], splitted[i+1], splitted[i]);
		}
	}
	watch_write(j->send_watch, "</query></iq>");
	array_free (splitted);

	return -1;
}

COMMAND(jabber_command_register)
{
	PARASC
	jabber_private_t *j = session_private_get(session);
	const char *server = params[0] ? params[0] : j->server;
	const char *passwd = session_get(session, "password");
	char **splitted;

	if (!session_connected_get(session) && (!passwd || !xstrcmp(passwd, "foo"))) {
		session_set(session, "__new_acount", "1");
		if (params[0]) session_set(session, "password", params[0]);
		jabber_command_connect(TEXT("connect"), NULL, session, target, quiet);
		return 0;
	} else if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (!(splitted = jabber_params_split(params[1], 0)) && params[1]) {
		printq("invalid_params", name);
		return -1;
	}
	
	watch_write(j->send_watch, "<iq type=\"%s\" to=\"%s\" id=\"transpreg%d\"><query xmlns=\"jabber:iq:register\">", params[1] ? "set" : "get", server, j->id++);
	if (splitted) {
		int i;
		for (i=0; (splitted[i] && splitted[i+1]); i+=2) {
			watch_write(j->send_watch, "<%s>%s</%s>", splitted[i], splitted[i+1], splitted[i]);
		}
	}
	watch_write(j->send_watch, "</query></iq>");
	array_free (splitted);
	return 0;
}

COMMAND(jabber_command_vacation) { /* JEP-0109: Vacation Messages (DEFERRED) */
	PARASC
	jabber_private_t *j = session_private_get(session);
	CHAR_T *message = jabber_escape(params[0]);
/* XXX, wysylac id: vacation%d... porobic potwierdzenia ustawiania/ usuwania. oraz jesli nie ma statusu to wyswylic jakies 'no vacation status'... */

	if (!params[0]) watch_write(j->send_watch, "<iq type=\"get\" id=\"%d\"><query xmlns=\"http://jabber.org/protocol/vacation\"/></iq>", j->id++);
	else if (xstrlen(params[0]) == 1 && params[0][0] == '-') 
		watch_write(j->send_watch, "<iq type=\"set\" id=\"%d\"><query xmlns=\"http://jabber.org/protocol/vacation\"/></iq>", j->id++);
	else	watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"%d\"><query xmlns=\"http://jabber.org/protocol/vacation\">"
			"<start/><end/>" /* XXX, startdate, enddate */
			"<message>" CHARF "</message>"
			"</query></iq>", 
			j->id++, message);
	xfree(message);
	return 0;
}

COMMAND(jabber_command_transpinfo) {
	PARASC
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

COMMAND(jabber_command_transports) {
	PARASC
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

COMMAND(jabber_command_stats) { /* JEP-0039: Statistics Gathering (DEFERRED) */
	PARASC
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

COMMAND(jabber_command_privacy) {
	PARASC
	/* XXX, wstepna implementacja jabber:iq:privacy w/g RFC #3921 */

	enum {	/* name */			/* allow/block: */
		PRIVACY_LIST_MESSAGE = 0,	/* 	incoming messages */
		PRIVACY_LIST_IQ,		/* 	incoming iq packets */
		PRIVACY_LIST_PRESENCE_IN,	/*	incoming presence packets */
		PRIVACY_LIST_PRESENCE_OUT, 	/*	outgoint presence packets */
		PRIVACY_LIST_ALL,		/*	everythink ;) */
		PRIVACY_LIST_COUNT,		/* nothink ;) */
		/* Usage: 
		 *    -*  		 : set order to 1, enable blist[PRIVACY_LIST_ALL] 
		 *    +* 		 : set order to 0, enable alist[PRIVACY_LIST_ALL]
		 *    -* +pin +pout +msg : set order to 1, enable alist[PRIVACY_LIST_PRESENCE_IN, PRIVACY_LIST_PRESENCE_OUT, PRIVACY_LIST_MESSAGE] && blist[PRIVACY_LIST_ALL] 
		 *    -pout -pin	 : (order doesn't matter) enable blist[PRIVACY_LIST_PRESENCE_IN, PRIVACY_LIST_PRESENCE_OUT]
		 */
	};
	jabber_private_t *j = jabber_private(session);

	char req = 0;
#if 0 /* some idea, senseless ? */
	if (!xstrcmp(name, "ignore")) req = '-'
	if (!xstrcmp(name, "unignore")) req = '+'
#endif

	if (!params[0]) {
		watch_write(j->send_watch, 
			"<iq type=\"get\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\"/></iq>", j->id++);
	} else if (req || params[1]) {	/* (req || params[1]); (target -- params[0]) XXX */
		char *lname = NULL;	/* list name, if not passed. generate random one. */
		char *ename;

		const char *type;		/* <item type */
		const char *value;		/* <item value */

		int alist[PRIVACY_LIST_COUNT] = {0, 0, 0, 0, 0, };	/* allowed list */
		int blist[PRIVACY_LIST_COUNT] = {0, 0, 0, 0, 0, };	/* blocked list */
		int order = 0;		/* 0 - wpierw 'deny' 1 - wpierw 'allow' */
		int done  = 0;		/* 0x01 - done 'allow' 0x02 - done 'deny' */

		int i;
		
		if (!xstrncmp(params[0], "jid:", 4))
			{ type = "jid"; value = params[0]+4; }
		else if (!xstrcmp(params[0], "none") || !xstrcmp(params[0], "both") || !xstrcmp(params[0], "from") || !xstrcmp(params[0], "to"))
			{ type = "subscription"; value = params[0]; }
		else if (params[0][0] == '@') 
			{ type = "group"; value = params[0]+1; }
		else {
			/* XXX */
			wcs_printq("invalid_params", name);
			return -1;
		}
		
		if (params[1]) { /* parsing params[1] made in separate block */
			char **p = array_make(params[1], " ", 0, 1, 0);
			
			for (i=0; p[i]; i++) { 
				int preq = req;
				int flag = -1;		/* PRIVACY_LIST_MESSAGE...PRIVACY_LIST_ALL */
				char *cur = p[i];	/* current */

				if (cur[0] == '-')	{ preq = '-'; cur++; }
				else if (cur[0] == '+')	{ preq = '+'; cur++; }
				
				if 	(!xstrcmp(cur, "iq"))	flag = PRIVACY_LIST_IQ;
				else if (!xstrcmp(cur, "msg")) 	flag = PRIVACY_LIST_MESSAGE;
				else if (!xstrcmp(cur, "pin"))	flag = PRIVACY_LIST_PRESENCE_IN;
				else if (!xstrcmp(cur, "pout"))	flag = PRIVACY_LIST_PRESENCE_OUT;
				else if (!xstrcmp(cur, "*"))	flag = PRIVACY_LIST_ALL;

				if (flag == -1 || !preq) {
					debug("[JABBER, PRIVACY] INVALID PARAM @ p[%d] = %s... [%d, %d, %d] \n", i, cur, flag, preq, req);
					wcs_printq("invalid_params", name);
					array_free(p);
					return -1;
				}

				if (preq == '-') { blist[flag] = 1; alist[flag] = 0; }
				if (preq == '+') { alist[flag] = 1; blist[flag] = 0; }

				if (flag == PRIVACY_LIST_ALL) order = (preq == '-');
			}
			array_free(p);
		}

		lname = saprintf("__ekg2_%s__%s", type, value);	/* nicely generated name */
		ename = jabber_escape(lname);
		xfree(lname);

		watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\">"
			"<list name=\"%s\">", j->id++, ename);

		for (i=0; i < PRIVACY_LIST_COUNT; i++) {
			if ( (alist[i] && done != 0x01) || (blist[i] && done != 0x02)) {
				int *clist = (alist[i]) ? (int *) &alist : (int *) &blist;

				watch_write(j->send_watch,
					"<item type=\"%s\" value=\"%s\" action=\"%s\" order=\"%d\">%s%s%s%s</item>", type, value,

					alist[i] ? "allow" : "deny", ((alist[i] && order) || (blist[i] && !order)) ? 0 : 1, 
					clist[PRIVACY_LIST_MESSAGE]	? "<message/>"	: "",
					clist[PRIVACY_LIST_IQ]		? "<iq/>"	: "",
					clist[PRIVACY_LIST_PRESENCE_IN] ? "<presence-in/>" : "",
					clist[PRIVACY_LIST_PRESENCE_OUT]? "<presence-out/>" : "");

				if (done) break;
				if (alist[i]) done = 0x01;
				if (blist[i]) done = 0x02;
			}
		}
		watch_write(j->send_watch, "</list></query></iq>");
	} else {
		const char *type, *value;
		char *lname;
		CHAR_T *ename = NULL;

		if (!xstrncmp(params[0], "jid:", 4))
			{ type = "jid"; value = params[0]+4; }
		else if (!xstrcmp(params[0], "none") || !xstrcmp(params[0], "both") || !xstrcmp(params[0], "from") || !xstrcmp(params[0], "to"))
			{ type = "subscription"; value = params[0]; }
		else if (params[0][0] == '@') 
			{ type = "group"; value = params[0]+1; }
		else if (!xstrncmp(params[0], "__ekg2_", 7))
			{ type = NULL; value = NULL; ename = jabber_escape(params[0]); } 
		else {
			/* XXX */
			wcs_printq("invalid_params", name);
			return -1;
		}

		if (type && value) {
			lname = saprintf("__ekg2_%s__%s", type, value);	/* nicely generated name */
			ename = jabber_escape(lname);
			xfree(lname);
		}

		watch_write(j->send_watch, 
			"<iq type=\"get\" id=\"privacy%d\"><query xmlns=\"jabber:iq:privacy\">"
			"<list name=\"%s\"/>"
			"</query></iq>", j->id++, ename);
		xfree(ename);
	}
	return 0;
}

COMMAND(jabber_muc_command_join) 
{
	PARASC
	/* params[0] - full channel name, 
	 * params[1] - nickname || default 
	 * params[2] - password || none
	 *
	 * XXX: make (session) variable jabber:default_muc && then if exists and params[0] has not specific server than append '@' jabber:default_muc and use it.
	 * XXX: make (session) variable jabber:default_nickname.
	 * XXX: history requesting, none history requesting.. etc
	 */
	jabber_private_t *j = session_private_get(session);
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

	xfree(username);
	xfree(password);
	return 0;
}

COMMAND(jabber_muc_command_part) 
{
	PARASC
	jabber_private_t *j = session_private_get(session);
	window_t *w;
	char *status;

	if (!(w = window_find_s(session, target)) || !(w->userlist)) {
		printq("generic_error", "Use /jid:part only in valid MUC room/window");
		return -1;
	}

	status = params[1] ? saprintf(" <status>%s</status> ", params[1]) : NULL;

	watch_write(j->send_watch, "<presence to=\"%s/%s\" type=\"unavailable\">%s</presence>", target+4, "darkjames", status ? status : "");

	xfree(status);
	return 0;
}

COMMAND(jabber_command_private) {
	PARUNI
	jabber_private_t *j = jabber_private(session);
	CHAR_T *namespace; 	/* <nazwa> */

	int config = 0;			/* 1 if name == jid:config */
	int bookmark = 0;		/* 1 if name == jid:bookmark */

	if (!xstrcmp(name, "config"))	config = 1;
	if (!xstrcmp(name, "bookmark")) bookmark = 1;
	
	if (config)		namespace = TEXT("ekg2 xmlns=\"ekg2:prefs\"");
	else if (bookmark)	namespace = TEXT("storage xmlns=\"storage:bookmarks\"");
	else			namespace = (CHAR_T *) params[1];

	if (bookmark) {				/* bookmark-only-commands */
		int bookmark_sync	= 0;			/* 0 - no sync; 1 - sync (item added); 2 - sync (item modified) 3 - sync (item removed)	*/
	
		if (match_arg(params[0], 'a', TEXT("add"), 2))		bookmark_sync = 1;	/* add item */
		if (match_arg(params[0], 'm', TEXT("modify"), 2))	bookmark_sync = 2; 	/* modify item */
		if (match_arg(params[0], 'r', TEXT("remove"), 2))	bookmark_sync = 3; 	/* remove item */

		if (bookmark_sync) {
			const CHAR_T *p[2]	= {TEXT("-p"), NULL};	/* --put */
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
				return jabber_command_private(name, (const CHAR_T **) p, session, target, quiet); /* synchronize db */
			} else if (bookmark_sync < 0) {
				debug("[JABBER, BOOKMARKS] sync=%d\n", bookmark_sync);
				wcs_printq("invalid_params", name);
				return -1;
			}
		}
	}

	if (match_arg(params[0], 'g', TEXT("get"), 2) || match_arg(params[0], 'd', TEXT("display"), 2)) {	/* get/display */
		watch_write(j->send_watch,
			"<iq type=\"get\" id=\"%s%d\">"
			"<query xmlns=\"jabber:iq:private\">"
			"<" CHARF "/>"
			"</query></iq>", 
			(match_arg(params[0], 'g', TEXT("get"), 2) && (config || bookmark) ) ? "config" : "private", 
			j->id++, namespace);
		return 0;
	}

	if (match_arg(params[0], 'p', TEXT("put"), 2)) {							/* put */
		list_t l;

		watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"private%d\">"
			"<query xmlns=\"jabber:iq:private\">"
			"<" CHARF ">", j->id++, namespace);

		if (config) {
			for (l = plugins; l; l = l->next) {
				plugin_t *p = l->data;
				list_t n;
				watch_write(j->send_watch, "<plugin xmlns=\"ekg2:plugin\" name=\"%s\" prio=\"%d\">", p->name, p->prio);
back:
				for (n = variables; n; n = n->next) {
					variable_t *v = n->data;
					CHAR_T *vname, *tname;
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
			watch_write(j->send_watch, params[2]);
		}
		{
			CHAR_T *beg = namespace;
			CHAR_T *end = xwcsstr(namespace, TEXT(" "));

/*			if (end) *end = '\0'; */	/* SEGV? where? why? :( */

			if (end) beg = xstrndup(namespace, end-namespace);
			else	 beg = xstrdup(namespace);

			watch_write(j->send_watch, "</" CHARF "></query></iq>", beg);

			xfree(beg);
		}
		return 0;
	}

	if (match_arg(params[0], 'c', TEXT("clear"), 2)) {						/* clear */
		if (bookmark) jabber_bookmarks_free(j);			/* let's destroy previously saved bookmarks */
		watch_write(j->send_watch,
			"<iq type=\"set\" id=\"private%d\">"
			"<query xmlns=\"jabber:iq:private\">"
			"<" CHARF "/></query></iq>", j->id++, namespace);
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

#if USE_UNICODE
#define LNULL NULL
#define command_add(x, y, par, a, b, c) command_add(x, y, L##par, a, b, c)
#endif
	command_add(&jabber_plugin, TEXT("jid:"), "?", jabber_command_inline_msg, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:_autoaway"), "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:_autoback"), "r", jabber_command_away,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:add"), "!U ?", jabber_command_modify, 	JABBER_FLAGS_TARGET, NULL); 
	command_add(&jabber_plugin, TEXT("jid:auth"), "!p !uU", jabber_command_auth, 	JABBER_FLAGS | COMMAND_ENABLEREQPARAMS, 
			"-a --accept -d --deny -r --request -c --cancel");
	command_add(&jabber_plugin, TEXT("jid:away"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:back"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:bookmark"), "!p ?", jabber_command_private, JABBER_ONLY | COMMAND_ENABLEREQPARAMS, 
			"-a --add -c --clear -d --display -m --modify -r --remove");
	command_add(&jabber_plugin, TEXT("jid:change"), "!p ? p ? p ? p ? p ? p ?", jabber_command_change, JABBER_FLAGS | COMMAND_ENABLEREQPARAMS , 
			"-f --fullname -c --city -b --born -d --description -n --nick -C --country");
	command_add(&jabber_plugin, TEXT("jid:chat"), "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, TEXT("jid:config"), "!p", jabber_command_private,	JABBER_ONLY | COMMAND_ENABLEREQPARAMS, 
			"-c --clear -d --display -g --get -p --put");
	command_add(&jabber_plugin, TEXT("jid:connect"), "r ?", jabber_command_connect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:dcc"), "p uU f ?", jabber_command_dcc,	JABBER_ONLY, 
			"send get resume voice close list");
	command_add(&jabber_plugin, TEXT("jid:del"), "!u", jabber_command_del, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, TEXT("jid:disconnect"), "r ?", jabber_command_disconnect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:dnd"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:invisible"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:ffc"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:msg"), "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, TEXT("jid:modify"), "!Uu !", jabber_command_modify,JABBER_FLAGS_TARGET, 
			"-n --nickname -g --group");
	command_add(&jabber_plugin, TEXT("jid:join"), "! ? ?", jabber_muc_command_join, JABBER_FLAGS_TARGET | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&jabber_plugin, TEXT("jid:part"), "! ?", jabber_muc_command_part, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, TEXT("jid:passwd"), "!", jabber_command_passwd, 	JABBER_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&jabber_plugin, TEXT("jid:privacy"), "? ?", jabber_command_privacy,	JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, TEXT("jid:private"), "!p ! ?", jabber_command_private,   JABBER_ONLY | COMMAND_ENABLEREQPARAMS, 
			"-c --clear -d --display -p --put");
	command_add(&jabber_plugin, TEXT("jid:reconnect"), NULL, jabber_command_reconnect, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:search"), "? ?", jabber_command_search, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, TEXT("jid:stats"), "? ?", jabber_command_stats, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, TEXT("jid:transpinfo"), "? ?", jabber_command_transpinfo, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, TEXT("jid:transports"), "? ?", jabber_command_transports, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, TEXT("jid:vacation"), "?", jabber_command_vacation, JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, TEXT("jid:ver"), "!u", jabber_command_ver, 	JABBER_FLAGS_TARGET, NULL); /* ??? ?? ? ?@?!#??#!@? */
	command_add(&jabber_plugin, TEXT("jid:userinfo"), "!u", jabber_command_userinfo, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, TEXT("jid:lastseen"), "!u", jabber_command_lastseen, JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, TEXT("jid:register"), "? ?", jabber_command_register, JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:xa"), "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, TEXT("jid:xml"), "!", jabber_command_xml, 	JABBER_ONLY | COMMAND_ENABLEREQPARAMS, NULL);
};

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
