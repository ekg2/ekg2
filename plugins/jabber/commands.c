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

static COMMAND(jabber_command_dcc) {
	jabber_private_t *j = session_private_get(session);

	if (!xstrncasecmp(params[0], "se", 2)) { /* send */
		struct stat st;
		userlist_t *u;
		dcc_t *d;
		FILE *fd;
		const char *fn;

		if (!params[1] || !params[2]) {
			wcs_printq("not_enough_params", name);
			return -1;
		}

		if (!(fn = prepare_path_user(params[2]))) {
			printq("generic_error", "path too long"); /* XXX? */
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

		if ((u->status <= EKG_STATUS_NA) || !u->resources) {
			printq("dcc_user_not_avail", format_user(session, u->uid));
			return -1;
		}

		if (!stat(fn, &st) && !S_ISREG(st.st_mode)) {
			printq("io_nonfile", params[2]);
			return -1;
		}

		if ((fd = fopen(fn, "r")) == NULL) {
			printq("io_cantopen", params[2], strerror(errno));
			return -1;
		}

		{
			string_t sid = NULL;
			jabber_dcc_t *p;
			char *filename;
			char *pathtmp;
			char *touid;

	/* XXX, introduce function jabber_get_resource(u, input_uid); */
			touid = saprintf("%s/%s", u->uid, ((ekg_resource_t *) (u->resources->data))->name);

			d 	= dcc_add(session, touid, DCC_SEND, NULL);
			d->filename 	= xstrdup(fn);
			d->size		= st.st_size;

			dcc_close_handler_set(d, jabber_dcc_close_handler);

			d->priv = p = xmalloc(sizeof(jabber_dcc_t));
			p->session 	= session;
			p->req		= saprintf("offer%d", dcc_id_get(d));
		
		/* copied from iris/jabber/s5b.cpp (C) 2003 Justin Karneges under LGPL 2.1 */
			do {
				/* generate hash like Psi do */
				int i;

				sid = string_init("s5b_");
				for (i = 0; i < 4; i++) {
					int word = rand() & 0xffff;
					int n;

					for (n = 0; n < 4; n++) {
						int dgst  = (word >> (n * 4)) & 0xf;	/* from 0..9 -> '0'..'9', 10..15 - 'A'..'F' */

						if (dgst < 10)	string_append_c(sid, dgst + '0');
						else		string_append_c(sid, dgst - 10 + 'a');
					}
				}
				debug_function("[jabber] jabber_command_dcc() hash generated: %s errors below are ok.\n", sid->str);
			} while (jabber_dcc_find(NULL, NULL, sid->str) && !string_free(sid, 1));	/* loop, [if sid exists] + free string if yes */

			p->sid		= string_free(sid, 0);
			p->sfd		= -1;
			p->fd		= fd;

				/* XXX: introduce prepare_filename() */
			if ((pathtmp = xstrrchr(fn, '/'))) 
				pathtmp++;			/* skip '/' */ 
			else 	pathtmp = (char*) fn;		/* no '/' ok.  */

			filename = jabber_escape(pathtmp);	/* escape string */

			watch_write(j->send_watch, "<iq type=\"set\" id=\"%s\" to=\"%s\">"
					"<si xmlns=\"http://jabber.org/protocol/si\" id=\"%s\" profile=\"http://jabber.org/protocol/si/profile/file-transfer\">"
					"<file xmlns=\"http://jabber.org/protocol/si/profile/file-transfer\" size=\"%d\" name=\"%s\">"
					"<range/></file>"
					"<feature xmlns=\"http://jabber.org/protocol/feature-neg\"><x xmlns=\"jabber:x:data\" type=\"form\">"
					"<field type=\"list-single\" var=\"stream-method\">"
					"<option><value>http://jabber.org/protocol/bytestreams</value></option>"
/*					"<option><value>http://jabber.org/protocol/ibb</value></option>" */
					"</field></x></feature></si></iq>", p->req, d->uid+5, p->sid, st.st_size, filename);
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

		if (xstrncmp(d->uid, "xmpp:", 5)) {
			debug_error("%s:%d /dcc command, incorrect `%s`!\n", __FILE__, __LINE__, __(d->uid));
			printq("generic_error", "Use /dcc on correct session, sorry");
			return -1;
		}

		{
			jabber_dcc_t *p = d->priv;
			session_t *s = p->session;
			jabber_private_t *j = jabber_private(s);
			char *filename;

			if (p->fd) {
				debug_error("[jabber] p->fd: 0x%x\n", p->fd);
				printq("generic_error", "Critical dcc error p->fd != NULL");
				return -1;
			}

			filename = saprintf("%s/%s", config_dcc_dir ? config_dcc_dir : prepare_path("download", 1), d->filename);
			debug("[jabber] DCC/GET Downloading file as: %s\n", filename);
			/* XXX, sanity d->filename */
		
		/* XXX, jesli to jest rget to plik moze istniec */
			while ((p->fd = fopen(filename, "r"))) {
				filename = xrealloc(filename, xstrlen(filename)+3);
				debug_error("[jabber] DCC/GET FILE ALREADY EXISTS APPENDING '.1': %s\n", filename);

				xstrcat(filename, ".1");

				fclose(p->fd);
			}
			
			if (!(p->fd = fopen(filename, "w"))) {
				int err = errno;
				debug_error("[jabber] DCC/GET CANNOT CREATE FILE: %s (%s)\n", filename, strerror(err));
				printq("dcc_get_cant_create", filename, strerror(err));
				return -1;
			}
			/* if resume fseek() to d->offset XXX */

			printq("dcc_get_getting", format_user(session, dcc_uid_get(d)), filename);

			watch_write(j->send_watch, "<iq type=\"result\" to=\"%s\" id=\"%s\">"
					"<si xmlns=\"http://jabber.org/protocol/si\"><feature xmlns=\"http://jabber.org/protocol/feature-neg\">"
					"<x xmlns=\"jabber:x:data\" type=\"submit\">"
					"<field var=\"stream-method\"><value>http://jabber.org/protocol/bytestreams</value></field>"
					"</x></feature></si></iq>", d->uid+5, p->req);
		}
		return 0;
	}

	if (!xstrncasecmp(params[0], "vo", 2)) { /* voice */
		printq("not_implemented");
		return -1;
	}
	return cmd_dcc(name, params, session, target, quiet);
}

static COMMAND(jabber_command_connect)
{
	const char *realserver	= session_get(session, "server"); 
	const char *resource	= session_get(session, "resource");
	const char *server;

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

/* XXX, get_uid() checks if this plugin is ok to handle it. so we have here tlen: or jid: but, we must check
 * 		if this uid match istlen.. However doesn't matter which protocol is, function should work...
 */
	if (((!istlen && xstrncasecmp(uid, "xmpp:", 5) && xstrncasecmp(uid, "jid:", 4)) || (istlen && xstrncasecmp(uid, "tlen:", 5)))) {
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
	int payload		= 4 + j->istlen;

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
	if (tolower(uid[0]) == 'x')
		payload++;

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
			( chat ? "<active xmlns=\"http://jabber.org/protocol/chatstates\"/>" : ""));

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

//	username = xstrndup(session->uid + 4, xstrchr(session->uid+4, '@') - session->uid+4);

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
	int payload = 4 + j->istlen;

	if (params[1])
		target = params[1];
	else if (!target) {
		printq("invalid_params", name);
		return -1;
	}

		/* XXX: shouldn't we use our magical jid_target2uid() here? */
	if (!(uid = get_uid(session, target)) || (j->istlen && (tolower(uid[0]) == 'j' || tolower(uid[0]) == 'x')) || (!j->istlen && tolower(uid[0]) == 't')) {
		printq("invalid_session");
		return -1;
	}
	if (tolower(uid[0]) == 'x')
		payload++;

	/* user jest OK, wiêc lepiej mieæ go pod rêk± */
	tabnick_add(uid);

	if (match_arg(params[0], 'r', ("request"), 2)) {
		action = "subscribe";
		printq("jabber_auth_request", uid+payload, session_name(session));
	} else if (match_arg(params[0], 'a', ("accept"), 2)) {
		action = "subscribed";
		printq("jabber_auth_accept", uid+payload, session_name(session));
	} else if (match_arg(params[0], 'c', ("cancel"), 2)) {
		action = "unsubscribe";
		printq("jabber_auth_unsubscribed", uid+payload, session_name(session));
	} else if (match_arg(params[0], 'd', ("deny"), 2)) {
		action = "unsubscribed";

		if (userlist_find(session, uid))  // mamy w rosterze
			printq("jabber_auth_cancel", uid+payload, session_name(session));
		else // nie mamy w rosterze
			printq("jabber_auth_denied", uid+payload, session_name(session));
	
	} else if (match_arg(params[0], 'p', ("probe"), 2)) {	/* TLEN ? */
	/* ha! undocumented :-); bo 
	   [Used on server only. Client authors need not worry about this.] */
		action = "probe";
		printq("jabber_auth_probe", uid+payload, session_name(session));
	} else {
		printq("invalid_params", name);
		return -1;
	}
	/* NOTE: libtlen send this without id */
	watch_write(j->send_watch, "<presence to=\"%s\" type=\"%s\" id=\"roster\"/>", uid+payload, action);
	return 0;
}

static COMMAND(jabber_command_modify) {
	jabber_private_t *j = session->priv;

	int addcom = !xstrcmp(name, ("add"));
	int payload = 4 + j->istlen;

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
	if (!(uid = get_uid(session, target)) || (j->istlen && (tolower(uid[0]) == 'j' || tolower(uid[0]) == 'x')) || (!j->istlen && tolower(uid[0]) == 't')) {
		printq("invalid_session");
		return -1;
	}
	if (tolower(uid[0]) == 'x')
		payload++;

	if (!u)	u = xmalloc(sizeof(userlist_t));		/* alloc temporary memory for /jid:add */

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
				command_exec_format(target, session, 0, ("/jid:privacy --set %s +pin"), uid);
				continue;
			}
			
			if (!j->istlen && match_arg(argv[i], 'O', ("offline"), 2)) {	/* only jabber:iq:privacy */
				command_exec_format(target, session, 0, ("/jid:privacy --set %s -pin"), uid);
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
	if (nickname)	watch_write(j->send_watch, "<item jid=\"%s\" name=\"%s\"%s>", uid+payload, nickname, (u->groups ? "" : "/"));
	else		watch_write(j->send_watch, "<item jid=\"%s\"%s>", uid+payload, (u->groups ? "" : "/"));

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
			if (!(uid = u->uid) || (j->istlen && (tolower(uid[0]) == 'j' || tolower(uid[0]) == 'x')) || (!j->istlen && tolower(uid[0]) == 't')) {
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

	if (!(uid = jid_target2uid(session, target, quiet)))
		return -1;

	if (!(ut = userlist_find(session, uid))) {
		print("user_not_found", session_name(session));
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
		jabber_private_t *j = session_private_get(session);
		ekg_resource_t *r = l->data;

		char *xquery_res = jabber_escape(r->name);
			/* XXX: in most functions we don't escape UIDs, should we do it here? */
		char *xuid = jabber_escape(uid + 5);
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
			/* XXX: like above */
		char *xuid = jabber_escape(uid + (tolower(uid[0]) == 'x' ? 5 : 4));
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
			/* XXX: like above, really worth escaping? */
		char *xuid = jabber_escape(uid + (tolower(uid[0]) == 'x' ? 5 : 4));
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
		return command_exec(target, session, ("/xmpp:register tuba"), quiet);
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
		if (issearch)	str = string_init(("/xmpp:search tuba "));
		else		str = string_init(("/xmpp:register tuba "));

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

typedef enum {
	PUBSUB_GENERIC = 0,	/* generic one */

	PUBSUB_GEO,		/* XEP-0080: User Geolocation 4.1 */
	PUBSUB_MOOD,		/* XEP-0107: User Mood 2.2 */
	PUBSUB_ACTIVITY,	/* XEP-0108: User Activity 2.2 */
	PUBSUB_USERTUNE,	/* XEP-0118: User Tune */			/* NOW PLAYING! YEAH! D-BUS!!! :-) */
	PUBSUB_NICKNAME,	/* XEP-0172: User Nickname 4.5 */
	PUBSUB_CHATTING,	/* XEP-0194: User Chatting */
	PUBSUB_BROWSING,	/* XEP-0195: User Browsing */
	PUBSUB_GAMING,		/* XEP-0196: User Gaming */
	PUBSUB_VIEWING,		/* XEP-0197: User Viewing */
} pubsub_type_t;

/* XXX, QUERY() */
static char *jabber_pubsub_publish(session_t *s, const char *server, pubsub_type_t type, const char *nodeid, const char *itemid, ...) {
	jabber_private_t *j;
	va_list ap;

	char *node;
	char *item;
	if (!s || !(j = s->priv)) return NULL;

	if (!nodeid) {			/* if !nodeid */
		switch (type) {			/* assume it's PEP, and use defaults */
			case PUBSUB_GEO:	node = xstrdup("http://jabber.org/protocol/geoloc");	break; /* ? */
			case PUBSUB_MOOD:	node = xstrdup("http://jabber.org/protocol/mood");	break; /* ? */
			case PUBSUB_ACTIVITY:	node = xstrdup("http://jabber.org/protocol/activity");	break;
			case PUBSUB_USERTUNE:	node = xstrdup("http://jabber.org/protocol/tune");	break;
			case PUBSUB_NICKNAME:	node = xstrdup("http://jabber.org/protocol/nick");	break;
			case PUBSUB_CHATTING:	node = xstrdup("http://jabber.org/protocol/chatting");	break;
			case PUBSUB_BROWSING:	node = xstrdup("http://jabber.org/protocol/browsing");	break;
			case PUBSUB_GAMING:	node = xstrdup("http://jabber.org/protocol/gaming");	break; /* ? */
			case PUBSUB_VIEWING:	node = xstrdup("http://jabber.org/protocol/viewing");	break; /* ? */

			default:	/* we MUST have node */
				debug_error("jabber_pubsub_publish() Unknown node... type: %d\n", type);
				return NULL;
		}
	} else node = jabber_escape(nodeid);

	if (!itemid) 
		item = saprintf("%s_%x%d%d", node, rand()*rand(), (int)time(NULL), rand());	/* some pseudo random itemid */
	else	item = jabber_escape(itemid);	

	if (j->send_watch) j->send_watch->transfer_limit = -1;
	
	va_start(ap, itemid);

	watch_write(j->send_watch,
		"<iq type=\"set\" to=\"%s\" id=\"pubsubpublish%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
		"<publish node=\"%s\"><item id=\"%s\">", server, j->id++, node, item);

	switch (type) {
		char *p[10];		/* different params */
		char *tmp;		/* for jabber_escape() */

		case PUBSUB_GENERIC:
			p[0] = va_arg(ap, char *);

			watch_write(j->send_watch, p[0]);
			break;

		case PUBSUB_GEO:	/* XEP-0080: User Geolocation */
			/* XXX a lot */
			break;

			watch_write(j->send_watch, "<geoloc xmlns=\"http://jabber.org/protocol/geoloc\"");
			watch_write(j->send_watch, "</geoloc>");
			break;

		case PUBSUB_MOOD:	/* XEP-0107: User Mood */
			p[0] = va_arg(ap, char *);	/* mood */
			p[1] = va_arg(ap, char *);	/* text */

			watch_write(j->send_watch, "<mood xmlns=\"http://jabber.org/protocol/mood\">");		/* header */
			watch_write(j->send_watch, "<%s/>", p[0]);								/* mood value */
			if (p[1]) { watch_write(j->send_watch, "<text>%s</text>", (tmp = jabber_escape(p[1])));	xfree(tmp); }	/* text */
			watch_write(j->send_watch, "</mood>");							/* footer */
			break;

		case PUBSUB_ACTIVITY:	/* XEP-0108: User Activity */
			p[0] = va_arg(ap, char *);	/* [REQ] general category (doing_chores, drinking, eating, exercising, grooming, ....) */
			p[1] = va_arg(ap, char *);	/* [OPT] specific category (...) */
			p[2] = va_arg(ap, char *);	/* [OPT] text */
			
			watch_write(j->send_watch, "<activity xmlns=\"http://jabber.org/protocol/activity\">");	/* activity header */

			if (p[1]) {
				watch_write(j->send_watch, "<%s><%s/></%s>", p[0], p[1], p[0]);						/* general + specific */
			} else	watch_write(j->send_watch, "<%s/>", p[0]);								/* only general */
			if (p[2]) { watch_write(j->send_watch, "<text>%s</text>", (tmp = jabber_escape(p[2]))); 	xfree(tmp); }	/* text */
			watch_write(j->send_watch, "</activity>");						/* activity footer */
			break;

		case PUBSUB_NICKNAME:	/* XEP-0172: User Nickname */
			p[0] = va_arg(ap, char *);	/* nickname */

			watch_write(j->send_watch, "<nick xmlns=\"http://jabber.org/protocol/nick\">%s</nick>", 
				(tmp = jabber_escape(p[0])));	xfree(tmp);								/* nickname */
			break;

		case PUBSUB_CHATTING:	/* XEP-0194: User Chatting */
			p[0] = va_arg(ap, char *);	/* [REQ] uri */
			p[1] = va_arg(ap, char *);	/* [OPT] name */
			p[2] = va_arg(ap, char *);	/* [OPT] topic */

			watch_write(j->send_watch, "<room xmlns=\"http://jabber.org/protocol/chatting\">");	/* header */
			watch_write(j->send_watch, "<uri>%s</uri>", p[0]);								/* uri */
			if (p[1]) watch_write(j->send_watch, "<name>%s</name>", p[1]);							/* name */
			if (p[2]) { watch_write(j->send_watch, "<topic>%s</topic>", (tmp = jabber_escape(p[2])));	xfree(tmp); }	/* topic */
			watch_write(j->send_watch, "</room>");							/* footer */
			break;

		case PUBSUB_USERTUNE:	/* XEP-0118: User Tune */
			p[0] = va_arg(ap, char *);	/* artist */
			p[1] = va_arg(ap, char *);	/* title */
			p[2] = va_arg(ap, char *);	/* source */
			p[3] = va_arg(ap, char *);	/* track */
			p[4] = va_arg(ap, char *);	/* length */

			watch_write(j->send_watch, "<tune xmlns=\"http://jabber.org/protocol/tune\">");		/* tune header */
			if (p[0]) { watch_write(j->send_watch, "<artist>%s</artist>", (tmp = jabber_escape(p[0])));	xfree(tmp); }	/* artist */
			if (p[1]) { watch_write(j->send_watch, "<title>%s</title>", (tmp = jabber_escape(p[1])));	xfree(tmp); }	/* title */
			if (p[2]) { watch_write(j->send_watch, "<source>%s</source>", (tmp = jabber_escape(p[2])));	xfree(tmp); }	/* source */
			if (p[3]) watch_write(j->send_watch, "<track>%s</track>", p[3]);						/* track # or URI */
			if (p[4]) watch_write(j->send_watch, "<length>%s</length>", p[4]);						/* len [seconds] */
			watch_write(j->send_watch, "</tune>");							/* tune footer */
			break;

		case PUBSUB_BROWSING:	/* XEP-0195: User Browsing */
			p[0] = va_arg(ap, char *);	/* [REQ] uri */
			p[1] = va_arg(ap, char *);	/* [OPT] title */
			p[2] = va_arg(ap, char *);	/* [OPT] description */
			p[3] = va_arg(ap, char *);	/* [OPT] keywords */

			watch_write(j->send_watch, "<page xmlns='http://jabber.org/protocol/browsing'>");	/* header */
			watch_write(j->send_watch, "<uri>%s</uri>", p[0]);									/* uri */
			if (p[1]) { watch_write(j->send_watch, "<title>%s</title>", (tmp = jabber_escape(p[1])));		xfree(tmp); }	/* title */
			if (p[2]) { watch_write(j->send_watch, "<description>%s</description>", (tmp = jabber_escape(p[2])));	xfree(tmp); }   /* descr */
			if (p[3]) { watch_write(j->send_watch, "<keywords>%s</keywords>", (tmp = jabber_escape(p[3])));		xfree(tmp); }   /* keywords */
			watch_write(j->send_watch, "</page>");							/* footer */
			break;

		case PUBSUB_GAMING:
			p[0] = va_arg(ap, char *);	/* [REQ] name */
		/* XXX */

			watch_write(j->send_watch, "<game xmlns=\"http://jabber.org/protocol/gaming\">");	/* header */
			watch_write(j->send_watch, (tmp = jabber_escape(p[0]))); xfree(tmp);				/* game name */
//			if (p[1]) watch_write(j->send_watch, 
			watch_write(j->send_watch, "</game>");							/* footer */
			break;

		case PUBSUB_VIEWING:	/* XEP-0197: User Viewing */
			break;
		/* XXX, not implemented cause of bug in XEP
		 */
			p[0] = va_arg(ap, char *);	/* program name */
	}

	va_end(ap);

	watch_write(j->send_watch, "</item></publish></pubsub></iq>");

	JABBER_COMMIT_DATA(j->send_watch);
#if 0
		char *title = NULL;
/* @ p[0]	if http://www.w3.org/2005/Atom ...
 * 	--itemid %s
 * 	--title %s
 * 	--summary %s
 * 	...
 * 	...
 */
		if (j->send_watch) j->send_watch->transfer_limit = -1;

		watch_write(j->send_watch, 
			"<iq type=\"set\" to=\"%s\" id=\"pubsubpublish%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
			"<publish node=\"%s\">"
				"<item id=\"%s\"><entry xmlns=\"http://www.w3.org/2005/Atom\">", server, j->id++, node, itemid);

		if (title) { watch_write(j->send_watch, "<title>%s</title>", (tmp = jabber_escape(title))); xfree(title); }
		/* XXX ... */
		JABBER_COMMIT_DATA(j->send_watch);
#endif 

	xfree(node);
	return item;
}

static COMMAND(jabber_command_pubsub) {
	jabber_private_t *j = session->priv;

	const char *server;
	const char *node;
	const char **p = &params[1];

	if (p[0] && p[1]) {
		server	= p[0];
		node	= p[1];
		p	= &p[2];
	} else {
		server	= jabber_default_pubsub_server ? jabber_default_pubsub_server : j->server;
		node	= p[0];
		p	= &p[1];
	}

/* XXX, escape node */

	if (match_arg(params[0], 'c', "create", 2)) {			/* CREATE NODE */
		if (j->send_watch) j->send_watch->transfer_limit = -1;

		watch_write(j->send_watch, "<iq type=\"set\" to=\"%s\" id=\"pusubcreatenode%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub\">", server, j->id++);
		if (!node) 
			watch_write(j->send_watch, "<create/><configure/>");
		else	watch_write(j->send_watch, "<create node=\"%s\"/><configure/>", node);

		watch_write(j->send_watch, "</pubsub></iq>");

		JABBER_COMMIT_DATA(j->send_watch);
		return 0;
	} else if (match_arg(params[0], 'C', "configure", 2)) {		/* CONFIGURE NODE (if node) || GET DEFAULT CONFIGURATION (if !node) */
/* XXX, about !node from XEP
 *
 * 	If the request did not specify a node, the service SHOULD return a <bad-request/> error. 
 * 	It is possible that by not including a NodeID, the requesting entity is asking to configure the root node; however, 
 * 	if the requesting entity is not a service-level admin, it makes sense to return <bad-request/> instead of <forbidden/>.
 *
 * 	We assume that if user didn't pass node, that we'll show default configuration...
 */
		if (j->send_watch) j->send_watch->transfer_limit = -1;

		watch_write(j->send_watch, "<iq type=\"set\" to=\"%s\" id=\"pubsubconfigure%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub#owner\">", server, j->id++);

		if (!node)
			watch_write(j->send_watch, "<default/>");
		else	watch_write(j->send_watch, "<configure node=\"%s\"/>", node);

		watch_write(j->send_watch, "</pubsub></iq>");

		JABBER_COMMIT_DATA(j->send_watch);
		return 0;
	} else if (match_arg(params[0], 'd', "delete", 2)) {		/* DELETE NODE */
		if (!node) {
			printq("not_enough_params", name);
			return -1;
		}

		watch_write(j->send_watch, 
				"<iq type=\"set\" to=\"%s\" id=\"pubsubdelete%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub#owner\">"
					"<delete node=\"%s\"/>"
				"</pubsub></iq>",
				server, j->id++, node);
		return 0;
	} else if (match_arg(params[0], 'P', "purge", 2)) {		/* PURGE NODE */
		if (!node) {
			printq("not_enough_params", name);
			return -1;
		}

		watch_write(j->send_watch,
				"<iq type=\"set\" to=\"%s\" id=\"pubsubdelete%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub#owner\">"
					"<purge node=\"%s\"/>"
				"</pubsub></iq>",
				server, j->id++, node);
		return 0;
	} else if (match_arg(params[0], 'm', "manage", 2)) {		/* MANAGE NODE */

	} else if (match_arg(params[0], 'g', "get", 2)) {		/* LIST NODES @ `server` || GET ITEMS @ `node` @ `server` */
		if (!node) {			/* if !node list nodes @ server */
			watch_write(j->send_watch, 
				"<iq type=\"get\" to=\"%s\" id=\"pubsublist%d\"><query xmlns=\"http://jabber.org/protocol/disco#items\"/></iq>",
				server, j->id++);
			return 0;
		}

		/* XXX, we can limit result using max_items */
		/* XXX, we can get only specified items using <item id= /> */

		watch_write(j->send_watch,
				"<iq type=\"get\" to=\"%s\" id=\"pubsubitems%d\"><pubsub xmlns=\"http://jabber.org/protocol/pubsub\">"
						"<items node=\"%s\"/>"
				"</pubsub></iq>", server, j->id++, node);
		return 0;
	} else if (match_arg(params[0], 'l', "list", 2)) {		/* LIST SUBSCRIPTION */

	} else if (match_arg(params[0], 'p', "publish", 2)) {		/* PUBLISH ITEM TO `node` @ `server` */
		char *itemid;

		if (!node || !p[0]) {
			printq("not_enough_params", name);
			return -1;
		}
//		itemid = jabber_pubsub_publish(session, server, PUBSUB_USERTUNE, node, "current", "Artist", "Title", NULL, "666", "123456");
		itemid = jabber_pubsub_publish(session, server, PUBSUB_GENERIC, node, NULL /* generate own itemid */, p[0]);

		xfree(itemid);
		return 0;
	} else if (match_arg(params[0], 'r', "remove", 2)) {		/* REMOVE ITEM */

	} else if (match_arg(params[0], 's', "subscribe", 2)) {		/* SUBSCRIBE TO `node` @ `server` */

	} else if (match_arg(params[0], 'S', "status", 2)) {		/* DISPLAY (SUBSCIPTION) | (AFFILITATIONS) STATUS @ `node` @ `server` */
		

	} else {
		printq("invalid_params", name);
		return -1;
	}

	printq("generic_error", "STUB FUNCTION");

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
		printq("not_implemented");
		return -1;
		needsync = 1;
	}

	if (!xstrcmp(params[0], "--delete")) {		/* delete list's entry */
		printq("not_implemented");
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
		else if (!xstrncmp(params[1], "xmpp:", 5)) { type = "jid";	value = params[1]+5; }
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
	char *username = (params[1]) ? xstrdup(params[1]) : (tmp = xstrchr(session->uid, '@')) ? xstrndup(session->uid+5, tmp-session->uid-5) : NULL;
	char *password = (params[1] && params[2]) ? saprintf("<password>%s</password>", params[2]) : NULL;

	if (!username) { /* rather impossible */
		wcs_printq("invalid_params", name);
		return -1;
	}

	if (!xstrncmp(target, "jid:", 4)) target += 4; /* remove jid: */
	else if (!xstrncmp(target, "xmpp:", 5)) target += 5;

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

		if (!xstrncmp(jid, "jid:", 4)) jid += 4;
		else if (!xstrncmp(jid, "xmpp:", 5)) jid += 5;

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
		uid		= xstrdup(session->uid+5);
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

	ret = command_exec_format(target, session, 0, "/jid:%smsg %s %s %s%s",
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
		FILE *f = fopen(listfile, "r");
		char line[512];

		if (!f) {
			printq("io_cantopen", listfile, strerror(errno));
			return -1;
		}

		while (fgets(line, sizeof(line), f)) {
			const int istlen = jabber_private(session)->istlen;
			char *uid = &line[2];
			char *nickname;

			if (!xstrchr(line, 10)) /* discard line if too long */
				continue;

			if (xstrncmp(line, "+,", 2)) { /* XXX: '-'? */
				debug_error("jabber_command_userlist(), unknown op on '%s'\n", line);
				continue;
			}

			if ((nickname = xstrchr(uid, ','))) {
				char *p;

				*(nickname++) = 0;
				if ((p = xstrchr(nickname, ',')))
					*p = 0;
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
#if 0 /* disabled until 'jid:' support removal */
	commands_lock = &commands;	/* keep it sorted or die */
#endif

	/* XXX: VERY, VERY, VERY BIG, FAT WARNING:
	 * Follow macro is used for the time that legacy 'jid:' prefix still allowed
	 * I know we already change session UID, but some users may still like to explicitly use 'jid:'
	 */
#define COMMAND_ADD_J(a, b, c...) command_add(a, "jid:" b, c); command_add(a, "xmpp:" b, c);

	COMMAND_ADD_J(&jabber_plugin, "", "?", jabber_command_inline_msg, 	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "_autoaway", "r", jabber_command_away,	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "_autoxa", "r", jabber_command_away,	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "_autoback", "r", jabber_command_away,	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "add", "U ?", jabber_command_modify, 	JABBER_FLAGS, NULL); 
	COMMAND_ADD_J(&jabber_plugin, "admin", "! ?", jabber_muc_command_admin, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "auth", "!p uU", jabber_command_auth, 	JABBER_FLAGS_REQ,
			"-a --accept -d --deny -r --request -c --cancel");
	COMMAND_ADD_J(&jabber_plugin, "away", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "back", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "ban", "! ? ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "bookmark", "!p ?", jabber_command_private, JABBER_FLAGS_REQ, 
			"-a --add -c --clear -d --display -m --modify -r --remove");
	COMMAND_ADD_J(&jabber_plugin, "change", "!p ? p ? p ? p ? p ? p ?", jabber_command_change, JABBER_FLAGS_REQ, 
			"-f --fullname -c --city -b --born -d --description -n --nick -C --country");
	COMMAND_ADD_J(&jabber_plugin, "chat", "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "config", "!p", jabber_command_private,	JABBER_FLAGS_REQ, 
			"-c --clear -d --display -g --get -p --put");
	COMMAND_ADD_J(&jabber_plugin, "connect", NULL, jabber_command_connect, JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "control", "! ? ?", jabber_command_control, JABBER_FLAGS_REQ, NULL);
	COMMAND_ADD_J(&jabber_plugin, "conversations", NULL, jabber_command_conversations,	JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "dcc", "p uU f ?", jabber_command_dcc,	JABBER_ONLY, 
			"send get resume voice close list");
	COMMAND_ADD_J(&jabber_plugin, "del", "!u", jabber_command_del, 	JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "disconnect", "r", jabber_command_disconnect, JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "dnd", "r", jabber_command_away, 	JABBER_ONLY, NULL);
//	COMMAND_ADD_J(&jabber_plugin, "ignore", "uUC I", jabber_command_ignore,	JABBER_ONLY, "status descr notify msg dcc events *");
	COMMAND_ADD_J(&jabber_plugin, "ffc", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "find", "?", jabber_command_find, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "invisible", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "join", "! ? ?", jabber_muc_command_join, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "kick", "! ! ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "lastseen", "!u", jabber_command_lastseen, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "modify", "!Uu ?", jabber_command_modify,JABBER_FLAGS_REQ, 
			"-n --nickname -g --group");
	COMMAND_ADD_J(&jabber_plugin, "msg", "!uU !", jabber_command_msg, 	JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "part", "! ?", jabber_muc_command_part, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "passwd", "?", jabber_command_passwd, 	JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "privacy", "? ? ?", jabber_command_privacy,	JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "private", "!p ! ?", jabber_command_private,   JABBER_FLAGS_REQ, 
			"-c --clear -d --display -p --put");
	COMMAND_ADD_J(&jabber_plugin, "pubsub", "!p ? ? ?", jabber_command_pubsub, JABBER_FLAGS, 
			"-c --create -C --configure -d --delete -P --purge -m --manage -g --get -l --list -p --publish -r --remove -s --subscribe -S --status");
	COMMAND_ADD_J(&jabber_plugin, "reconnect", NULL, jabber_command_reconnect, JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "register", "? ?", jabber_command_register, JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "reply", "! !", jabber_command_reply, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "search", "? ?", jabber_command_search, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "stats", "? ?", jabber_command_stats, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "tmsg", "!uU ! !", jabber_command_msg, JABBER_FLAGS_TARGET, NULL); /* threaded msg */
	COMMAND_ADD_J(&jabber_plugin, "topic", "! ?", jabber_muc_command_topic, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "transpinfo", "? ?", jabber_command_transpinfo, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "transports", "? ?", jabber_command_transports, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "unban", "! ?", jabber_muc_command_ban, JABBER_FLAGS_TARGET, NULL);
//	COMMAND_ADD_J(&jabber_plugin, "unignore", "i ?", jabber_command_ignore, JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "unregister", "?", jabber_command_register, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "userinfo", "!u", jabber_command_userinfo, JABBER_FLAGS_TARGET, NULL);
	COMMAND_ADD_J(&jabber_plugin, "userlist", "! ?", jabber_command_userlist, JABBER_FLAGS_REQ,
			"-g --get -p --put"); /* BFW: it is unlike GG, -g gets userlist from file, -p writes it into it */
	COMMAND_ADD_J(&jabber_plugin, "vacation", "?", jabber_command_vacation, JABBER_FLAGS, NULL);
	COMMAND_ADD_J(&jabber_plugin, "ver", "!u", jabber_command_ver, 	JABBER_FLAGS_TARGET, NULL); /* ??? ?? ? ?@?!#??#!@? */
	COMMAND_ADD_J(&jabber_plugin, "xa", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	COMMAND_ADD_J(&jabber_plugin, "xml", "!", jabber_command_xml, 	JABBER_ONLY, NULL);

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
	command_add(&jabber_plugin, "tlen:change", "?",	tlen_command_pubdir, 		JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "tlen:connect", "r ?",	jabber_command_connect,		JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:disconnect", "r ?",	jabber_command_disconnect,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:del", "!u", jabber_command_del, 	JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "tlen:dnd", "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:ffc", "r",	jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:find", "?",	tlen_command_pubdir, 		JABBER_FLAGS, NULL);
	command_add(&jabber_plugin, "tlen:invisible", "r", jabber_command_away, 	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:modify", "!Uu ?",	jabber_command_modify,		JABBER_FLAGS_REQ, 
			"-n --nickname -g --group");
	command_add(&jabber_plugin, "tlen:msg", "!uU !",	jabber_command_msg, 		JABBER_FLAGS_TARGET, NULL);
	command_add(&jabber_plugin, "tlen:reconnect", NULL,	jabber_command_reconnect,	JABBER_ONLY, NULL);
	command_add(&jabber_plugin, "tlen:search", "?",	tlen_command_pubdir, 		JABBER_FLAGS, NULL);

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
