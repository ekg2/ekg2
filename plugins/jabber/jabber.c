/* $Id$ */

/*
 *  (C) Copyright 2003-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Tomasz Torcz <zdzichu@irc.pl>
 *                          Leszek Krupiñski <leafnode@pld-linux.org>
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
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>

#ifdef HAVE_EXPAT_H
#  include <expat.h>
#endif

#ifdef sun      /* Solaris, thanks to Beeth */
#include <sys/filio.h>
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

static int jabber_theme_init();

PLUGIN_DEFINE(jabber, PLUGIN_PROTOCOL, jabber_theme_init);

/*
 * jabber_private_destroy()
 *
 * inicjuje jabber_private_t danej sesji.
 */
static void jabber_private_init(session_t *s)
{
        const char *uid = session_uid_get(s);
        jabber_private_t *j;

        if (xstrncasecmp(uid, "jid:", 4))
                return;

        if (session_private_get(s))
                return;

        j = xmalloc(sizeof(jabber_private_t));
        memset(j, 0, sizeof(jabber_private_t));
        j->fd = -1;
        session_private_set(s, j);
}

/*
 * jabber_private_destroy()
 *
 * zwalnia jabber_private_t danej sesji.
 */
static void jabber_private_destroy(session_t *s)
{
        jabber_private_t *j = session_private_get(s);
        const char *uid = session_uid_get(s);

        if (xstrncasecmp(uid, "jid:", 4) || !j)
                return;

        xfree(j->server);
        xfree(j->stream_id);

        if (j->parser)
                XML_ParserFree(j->parser);

        xfree(j);

        session_private_set(s, NULL);
}

/*
 * jabber_session()
 *
 * obs³uguje dodawanie i usuwanie sesji -- inicjalizuje lub zwalnia
 * strukturê odpowiedzialn± za wnêtrzno¶ci jabberowej sesji.
 */
int jabber_session(void *data, va_list ap)
{
        char **session = va_arg(ap, char**);
        session_t *s = session_find(*session);

        if (!s)
                return -1;

        if (data)
                jabber_private_init(s);
        else
                jabber_private_destroy(s);

        return 0;
}

/*
 * jabber_print_version()
 *
 * wy¶wietla wersjê pluginu i biblioteki.
 */
int jabber_print_version(void *data, va_list ap)
{
        print("generic", XML_ExpatVersion());

        return 0;
}

/*
 * jabber_validate_uid()
 *
 * sprawdza, czy dany uid jest poprawny i czy plugin do obs³uguje.
 */
int jabber_validate_uid(void *data, va_list ap)
{
        char **__uid = va_arg(ap, char **), *uid = *__uid, *m;
        int *valid = va_arg(ap, int *);

        if (!uid)
                return 0;

	/* minimum: jid:a@b */
        if (!xstrncasecmp(uid, "jid:", 4) && (m=xstrchr(uid, '@')) &&
			((uid+4)<m) && xstrlen(m+1))
                (*valid)++;

        return 0;
}

int jabber_write_status(session_t *s)
{
        jabber_private_t *j = session_private_get(s);
        int priority = session_int_get(s, "priority");
        const char *status;
        char *descr;

        if (!s || !j)
                return -1;

        if (!session_connected_get(s))
                return 0;

        status = session_status_get(s);
        descr = jabber_escape(session_descr_get(s));

        if (!xstrcmp(status, EKG_STATUS_AVAIL)) {
                if (descr)
                        jabber_write(j, "<presence><status>%s</status><priority>%d</priority></presence>", descr, priority);
                else
                        jabber_write(j, "<presence><priority>%d</priority></presence>", priority);
        } else if (!xstrcmp(status, EKG_STATUS_INVISIBLE)) {
                if (descr)
                        jabber_write(j, "<presence type=\"invisible\"><status>%s</status><priority>%d</priority></presence>", descr, priority);
                else
                        jabber_write(j, "<presence type=\"invisible\"><priority>%d</priority></presence>", priority);
        } else {
                if (descr)
                        jabber_write(j, "<presence><show>%s</show><status>%s</status><priority>%d</priority></presence>", status, descr, priority);
                else
                        jabber_write(j, "<presence><show>%s</show><priority>%d</priority></presence>", status, priority);
        }

        xfree(descr);

        return 0;
}

void jabber_handle(void *data, xmlnode_t *n)
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        session_t *s = jdh->session;
        jabber_private_t *j;

        if (!s || !(j = jabber_private(s)) || !n) {
                debug("[jabber] jabber_handle() invalid parameters\n");
                return;
        }

        debug("[jabber] jabber_handle() <%s>\n", n->name);

        if (!xstrcmp(n->name, "message")) {
		jabber_handle_message(n, s, j);
	} else if (!xstrcmp(n->name, "iq")) {
		jabber_handle_iq(n, jdh);
	} else if (!xstrcmp(n->name, "presence")) {
		jabber_handle_presence(n, s);
	} else {
		debug("[jabber] what's that: %s ?\n", n->name);
	}
};

void jabber_handle_message(xmlnode_t *n, session_t *s, jabber_private_t *j) {
	xmlnode_t *nbody = xmlnode_find_child(n, "body");
	xmlnode_t *nsubject = xmlnode_find_child(n, "subject");
	xmlnode_t *xitem = xmlnode_find_child(n, "x");
	xmlnode_t *nerr = xmlnode_find_child(n, "error");
	xmlnode_t *xurl = NULL;
	xmlnode_t *xdesc = NULL;
	string_t body = string_init("");
	char *session, *sender, **rcpts = NULL, *text, *seq = NULL;
	const char *from = jabber_attr(n->atts, "from");
	time_t sent = time(NULL);
	int class = EKG_MSGCLASS_CHAT;
	int ekgbeep = EKG_TRY_BEEP;
	int secure = 0;
	uint32_t *format = NULL;
	char *tmp;

	if (nsubject && !nerr) {
		string_append(body, "Subject: ");
		string_append(body, nsubject->data);
		string_append(body, "\n\n");
	}

	if (nbody && !nerr)
		string_append(body, nbody->data);

	if (xitem && !nerr) {
		const char *ns;
		ns = jabber_attr(xitem->atts, "xmlns");

		/* try to parse timestamp */
		sent = jabber_try_xdelay(xitem, ns);

		if (ns && !xstrncmp(ns, "jabber:x:event", 14)) {
			/* jesli jest body, to mamy do czynienia z prosba o potwierdzenie */
			if (nbody && (xmlnode_find_child(xitem, "delivered") 
			    || xmlnode_find_child(xitem, "displayed")) ) {
				char *id = jabber_attr(n->atts, "id");
				const char *our_status = session_status_get(s);

				jabber_write(j, "<message to=\"%s\">", from);
				jabber_write(j, "<x xmlns=\"jabber:x:event\">");

				if (!xstrcmp(our_status, EKG_STATUS_INVISIBLE)) {
					jabber_write(j, "<offline/>");
				} else {
					if (xmlnode_find_child(xitem, "delivered"))
						jabber_write(j, "<delivered/>");
					if (xmlnode_find_child(xitem, "displayed"))
						jabber_write(j, "<displayed/>");
				};

				jabber_write(j, "<id>%s</id></x></message>",id);
			};

			/* je¶li body nie ma, to odpowiedz na nasza prosbe */
			if (!nbody && xmlnode_find_child(xitem, "delivered")
			    && (config_display_ack == 1 || config_display_ack == 2)) {
				char *tmp = jabber_unescape(from);

				print("ack_delivered", tmp);
				xfree(tmp);
			}

			if (!nbody && xmlnode_find_child(xitem, "offline")
			    && (config_display_ack == 1 || config_display_ack == 3)) {
				char *tmp = jabber_unescape(from);

				print("ack_queued", tmp);
                                        xfree(tmp);
                                }

			if (!nbody && xmlnode_find_child(xitem, "composing")) {
				char *tmp = jabber_unescape(from);

				if (session_int_get(s, "show_typing_notify"))
					print("jabber_typing_notify", tmp);

				xfree(tmp);
			} /* composing */
		} /* jabber:x:event */

		if (ns && !xstrncmp(ns, "jabber:x:oob", 12)) {
			if ( ( xurl = xmlnode_find_child(xitem, "url") ) ) {
				// TODO: unescape those
				string_append(body, "\n\n");
				string_append(body, "URL: ");
				string_append(body, xurl->data);
				string_append(body, "\n");
				xdesc = xmlnode_find_child(xitem, "desc");
				string_append(body, xdesc->data);
				string_append(body, "\n");
			}
		} /* jabber:x:oob */
	} /* if !nerr && <x>; TODO: split as functions */

	session = xstrdup(session_uid_get(s));
	tmp = jabber_unescape(from);
	sender = saprintf("jid:%s", tmp);
	xfree(tmp);
	text = jabber_unescape(body->str);
	string_free(body, 1);

	if ((nbody || nsubject) && !nerr)
		query_emit(NULL, "protocol-message", &session, &sender, &rcpts, &text, &format, &sent, &class, &seq, &ekgbeep, &secure);

	if (nerr) {
		char *recipient, *mbody, *tmp, *tmp2;
		char *ecode = jabber_attr(nerr->atts, "code");
		char *etext = jabber_unescape(nerr->data);

		tmp2 = jabber_unescape(from);
		tmp = saprintf("jid:%s", tmp2);
		xfree(tmp2);
		recipient = get_nickname(s, tmp);

		if (nbody && nbody->data) {
			tmp2 = jabber_unescape(nbody->data);
			mbody = xstrndup(tmp2, 15);
			xstrtr(mbody, '\n', ' ');

			print("jabber_msg_failed_long", recipient, ecode, etext, mbody);

			xfree(mbody);
			xfree(tmp2);
		} else
			print("jabber_msg_failed", recipient, ecode, etext);

		xfree(etext);
		xfree(tmp);
	} /* <error> */

	xfree(session);
	xfree(sender);
	array_free(rcpts);
	xfree(text);
	xfree(format);
	xfree(seq);
} /* */

void jabber_handle_iq(xmlnode_t *n, jabber_handler_data_t *jdh) {
	const char *type = jabber_attr(n->atts, "type");
	const char *id = jabber_attr(n->atts, "id");
	const char *from = jabber_attr(n->atts, "from");
	session_t *s = jdh->session;
	jabber_private_t *j = jabber_private(s);

	if (!type) {
		debug("[jabber] <iq> without type!\n");
		return;
	}

	if (id && !xstrcmp(id, "auth")) {
		s->last_conn = time(NULL);
		j->connecting = 0;

		if (!xstrcmp(type, "result")) {
			char *__session = xstrdup(session_uid_get(s));
			session_connected_set(s, 1);
			session_unidle(s);
			query_emit(NULL, "protocol-connected", &__session);
			xfree(__session);
			jdh->roster_retrieved = 0;
			userlist_free(s);
			jabber_write(j, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
			jabber_write_status(s);
		}

		/* TODO: try to merge with <message>'s <error> parsing */
		if (!xstrcmp(type, "error")) {
			xmlnode_t *e = xmlnode_find_child(n, "error");

			if (e && e->data) {
				char *data = jabber_unescape(e->data);
				print("conn_failed", data, session_name(s));
			} else
				print("jabber_generic_conn_failed", session_name(s));
		}
	}

	if (id && !xstrncmp(id, "passwd", 6)) {
		if (!xstrcmp(type, "result")) {
			char *new_passwd = (char *) session_get(s, "__new_password");

			session_set(s, "password", new_passwd);
			xfree(new_passwd);
			print("passwd");
		}

		if (!xstrcmp(type, "error")) {
			xmlnode_t *e = xmlnode_find_child(n, "error");
			char *reason = (e && e->data) ? jabber_unescape(e->data) : xstrdup("?");

			print("passwd_failed", reason);
			xfree(reason);
		}

		session_set(s, "__new_password", NULL);

	}

	/* XXX: temporary hack: roster przychodzi jako typ 'set' (przy dodawaniu), jak
	        i typ "result" (przy za¿±daniu rostera od serwera) */
	if (type && (!xstrncmp(type, "result", 6) || !xstrncmp(type, "set", 3)) ) {
		/* First we check if there is vCard...                */
		xmlnode_t *q = xmlnode_find_child(n, "vCard");
		if (q) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (ns && !xstrncmp(ns, "vcard-temp", 10)) {
				xmlnode_t *fullname = xmlnode_find_child(q, "FN");
				xmlnode_t *nickname = xmlnode_find_child(q, "NICKNAME");
				xmlnode_t *birthday = xmlnode_find_child(q, "BDAY");
				xmlnode_t *adr = xmlnode_find_child(q, "ADR");
				xmlnode_t *city = xmlnode_find_child(adr, "LOCALITY");
				xmlnode_t *desc = xmlnode_find_child(q, "DESC");

				char *from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
				char *fullname_str = (fullname && fullname->data) ? jabber_unescape(fullname->data) : xstrdup(_("unknown"));
				char *nickname_str = (nickname && nickname->data) ? jabber_unescape(nickname->data) : xstrdup(_("unknown"));
				char *bday_str = (birthday && birthday->data) ? jabber_unescape(birthday->data) : xstrdup(_("unknown"));
				char *city_str = (city && city->data) ? jabber_unescape(city->data) : xstrdup(_("unknown"));
				char *desc_str = (desc && desc->data) ? jabber_unescape(desc->data) : xstrdup(_("unknown"));

				print("jabber_userinfo_response", from_str, fullname_str,
					nickname_str, bday_str, city_str,desc_str);

				xfree(desc_str);
				xfree(city_str);
				xfree(bday_str);
				xfree(nickname_str);
				xfree(fullname_str);
				xfree(from_str);
			}
		}

		q = 0;
		q = xmlnode_find_child(n, "query");
		if (q) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (ns && !xstrncmp(ns, "jabber:iq:version", 17)) {
				xmlnode_t *name = xmlnode_find_child(q, "name");
				xmlnode_t *version = xmlnode_find_child(q, "version");
				xmlnode_t *os = xmlnode_find_child(q, "os");

				char *from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
				char *name_str = (name && name->data) ? jabber_unescape(name->data) : xstrdup(_("unknown"));
				char *version_str = (version && version->data) ? jabber_unescape(version->data) : xstrdup(_("unknown"));
				char *os_str = (os && os->data) ? jabber_unescape(os->data) : xstrdup(_("unknown"));

				print("jabber_version_response", from_str, name_str, version_str, os_str);

				xfree(os_str);
				xfree(version_str);
				xfree(name_str);
				xfree(from_str);
			}

			if (ns && !xstrncmp(ns, "jabber:iq:last", 14)) {
				int seconds = 0;
				const char *last = jabber_attr(q->atts, "seconds");
				char buff[21];
				char *from_str, *lastseen_str;

				seconds = atoi(last);

				/*TODO If user is online: display user's status; */

				if ((seconds>=0) && (seconds < 999 * 24 * 60 * 60  - 1) )
					/* days, hours, minutes, seconds... */
					snprintf (buff, 21, _("%03dd %02dh %02dm %02ds ago"),seconds / 86400 , \
						(seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60);
				else
					strcpy (buff, _("very long ago"));

				from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
				lastseen_str = xstrdup(buff);

				print("jabber_lastseen_response", from_str, lastseen_str);
				xfree(lastseen_str);
				xfree(from_str);
			}

			if (ns && !xstrncmp(ns, "jabber:iq:roster", 16)) {
				userlist_t u, *tmp;
				char *ctmp;
				xmlnode_t *group;

				xmlnode_t *item = xmlnode_find_child(q, "item");
				for (; item ; item = item->next) {
					memset(&u, 0, sizeof(u));
					u.uid = saprintf("jid:%s",jabber_attr(item->atts, "jid"));
					u.nickname = jabber_unescape(jabber_attr(item->atts, "name"));

					if (!u.nickname)
						u.nickname = xstrdup(u.uid);

					u.status = xstrdup(EKG_STATUS_NA);
					
					group = xmlnode_find_child(item,"group");
					for (; group ; group = group->next ) {
					    ekg_group_add(&u, jabber_unescape(group->data));
					}


					/* je¶li element rostera ma subscription = remove to tak naprawde u¿ytkownik jest usuwany;
					w przeciwnym wypadku - nalezy go dopisaæ do userlisty; dodatkowo, jesli uzytkownika
					mamy ju¿ w liscie, to przyszla do nas zmiana rostera; usunmy wiec najpierw, a potem
					sprawdzmy, czy warto dodawac :) */
					if (jdh->roster_retrieved && (tmp = userlist_find(s, u.uid)) )
						userlist_remove(s, tmp);

					if (jabber_attr(item->atts, "subscription") &&
					    !xstrncmp(jabber_attr(item->atts, "subscription"), "remove", 6)) {
						/* nic nie robimy, bo juz usuniete */
					} else {
						if (jabber_attr(item->atts, "subscription"))
							u.authtype = xstrdup(jabber_attr(item->atts, "subscription"));
						list_add_sorted(&(s->userlist), &u, sizeof(u), userlist_compare);
						if (jdh->roster_retrieved) {
							ctmp = saprintf("/auth --probe %s", u.uid);
							command_exec(NULL, s, ctmp, 1);
							xfree(ctmp);
						}
					}
				}; /* for */
				jdh->roster_retrieved = 1;
			} /* jabber:iq:roster */
		} /* if query */
	} /* type == set */

	if (type && !xstrncmp(type, "get", 3)) {
		xmlnode_t *q = xmlnode_find_child(n, "query");

		if (q) {
			const char *ns;
			ns = jabber_attr(q->atts, "xmlns");

			if (ns && !xstrncmp(ns, "jabber:iq:version", 17)) {

				/* 'id' powinno byc w <iq/> */
				if (id && from) {
					const char *ver_client_name, *ver_client_version, *ver_os;
					char *escaped_client_name;
					char *escaped_client_version;

					if (!(ver_client_name = session_get(s, "ver_client_name")))
						ver_client_name = DEFAULT_CLIENT_NAME;
					if (!(ver_client_version = session_get(s, "ver_client_version")))
					ver_client_version = VERSION;

					escaped_client_name = jabber_escape(ver_client_name);
					escaped_client_version = jabber_escape(ver_client_version);

					jabber_write(j, "<iq to=\"%s\" type=\"result\" id=\"%s\">", from, id);
					jabber_write(j, "<query xmlns=\"jabber:iq:version\"><name>%s</name>",
						escaped_client_name);
					jabber_write(j, "<version>%s</version>",
						escaped_client_version);

					xfree(escaped_client_name);
					xfree(escaped_client_version);

					if (!(ver_os = session_get(s, "ver_os"))) {
						struct utsname buf;

						if (uname(&buf) == 0) {
							char *escaped_sysname;
							char *escaped_release;
							char *escaped_machine;

							escaped_sysname = jabber_escape(buf.sysname);
							escaped_release = jabber_escape(buf.release);
							escaped_machine = jabber_escape(buf.machine);

							jabber_write(j, "<os>%s %s %s</os>", 
								escaped_sysname,
								escaped_release,
								escaped_machine);

							xfree(escaped_sysname);
							xfree(escaped_release);
							xfree(escaped_machine);
						} else {
							jabber_write(j, "<os>unknown</os>");
						}
							
					} else {
						char *escaped_ver_os;

						escaped_ver_os = jabber_escape(ver_os);
						jabber_write(j, "<os>%s</os>", jabber_escape(ver_os));
						xfree(escaped_ver_os);
					}

					jabber_write(j, "</query></iq>");
				}
			}; /* jabber:iq:version */

		} /* if query */
	} /* type == get */
} /* iq */

void jabber_handle_presence(xmlnode_t *n, session_t *s) {
	const char *from = jabber_attr(n->atts, "from");
	const char *type = jabber_attr(n->atts, "type");

	if (type && !xstrcmp(type, "subscribe") && from) {
		print("jabber_auth_subscribe", from, session_name(s));
		return;
	}

	if (type && !xstrcmp(type, "unsubscribe") && from) {
		char *tmp = jabber_unescape(from);
		print("jabber_auth_unsubscribe", tmp, session_name(s));
		xfree(tmp);
		return;
	}

	if (!type || (type && (!xstrcmp(type, "unavailable") || !xstrcmp(type, "error")
	    || !xstrcmp(type, "available"))) ) {
		xmlnode_t *nshow = xmlnode_find_child(n, "show"); /* typ */
		xmlnode_t *nstatus = xmlnode_find_child(n, "status"); /* opisowy */
		xmlnode_t *xitem = xmlnode_find_child(n, "x");
		xmlnode_t *nerr = xmlnode_find_child(n, "error");
		char *session, *uid, *status = NULL, *descr = NULL, *host = NULL, *tmp;
		int port = 0;
		time_t when = xitem ? jabber_try_xdelay(xitem, jabber_attr(xitem->atts, "xmlns")) : time(NULL);
		char **res_arr = array_make(from, "/", 2, 0, 0);
		char known_status = 0;

		if (nshow)
			status = jabber_unescape(nshow->data);
		else {
			status = xstrdup(EKG_STATUS_AVAIL);
			known_status = 1;
		}

		if (!xstrcmp(status, "na") || !xstrcmp(type, "unavailable")) {
			xfree(status);
			status = xstrdup(EKG_STATUS_NA);
			known_status = 1;
		}

		if (nstatus)
		descr = jabber_unescape(nstatus->data);

		if (nerr) {
			char *ecode = jabber_attr(nerr->atts, "code");
			char *etext = jabber_unescape(nerr->data);

			descr = saprintf("(%s) %s", ecode, etext);
			xfree(etext);
			xfree(status);
			status = xstrdup(EKG_STATUS_ERROR);
			known_status = 1;
		}

		session = xstrdup(session_uid_get(s));
		tmp = jabber_unescape(from);
		uid = saprintf("jid:%s", tmp);
		xfree(tmp);
		host = NULL;
		port = 0;

		if (res_arr[0] && res_arr[1]) {
			char *tmp = saprintf("jid:%s", res_arr[0]);
			userlist_t *ut = userlist_find(s, tmp);
			xfree(tmp);
			if (ut)
				ut->resource = xstrdup(res_arr[1]);
		}
		array_free(res_arr);

		if (!known_status && 
				xstrcasecmp(status, EKG_STATUS_AWAY) &&
				xstrcasecmp(status, EKG_STATUS_INVISIBLE) &&
				xstrcasecmp(status, EKG_STATUS_XA) &&
				xstrcasecmp(status, EKG_STATUS_DND) &&
				xstrcasecmp(status, EKG_STATUS_FREE_FOR_CHAT) &&
				xstrcasecmp(status, EKG_STATUS_BLOCKED)) {
			debug("[jabber] Unknown presence: %s from %s. Please report!\n", status, uid);
			xfree(status);
			status = xstrdup(EKG_STATUS_AVAIL);
		};

		query_emit(NULL, "protocol-status", &session, &uid, &status, &descr, &host, &port, &when, NULL);

		xfree(session);
		xfree(uid);
		xfree(status);
		xfree(descr);
		xfree(host);
	}
} /* <presence> */


time_t jabber_try_xdelay(xmlnode_t *xitem, const char* ns)
{
        struct tm tm;

        const char *stamp = jabber_attr(xitem->atts, "stamp");

        if (ns && !xstrncmp(ns, "jabber:x:delay", 14) && stamp) {
                memset(&tm, 0, sizeof(tm));
                sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d",
                        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                        &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
                tm.tm_year -= 1900;
                tm.tm_mon -= 1;
                return mktime(&tm);
        } else
                return time(NULL);

}

static void jabber_handle_disconnect(session_t *s)
{
        jabber_private_t *j = jabber_private(s);
        int reconnect_delay;

        if (!j)
                return;

        if (j->obuf || j->connecting)
                watch_remove(&jabber_plugin, j->fd, WATCH_WRITE);

        if (j->obuf) {
                xfree(j->obuf);
                j->obuf = NULL;
                j->obuf_len = 0;
        }

#ifdef HAVE_GNUTLS
        if (j->using_ssl && j->ssl_session)
                gnutls_bye(j->ssl_session, GNUTLS_SHUT_RDWR);
#endif
        session_connected_set(s, 0);
        j->connecting = 0;
        if (j->parser)
                XML_ParserFree(j->parser);
        j->parser = NULL;
        close(j->fd);
        j->fd = -1;

#ifdef HAVE_GNUTLS
        if (j->using_ssl && j->ssl_session)
                gnutls_deinit(j->ssl_session);
#endif

        userlist_clear_status(s, NULL);

        reconnect_delay = session_int_get(s, "auto_reconnect");
        if (reconnect_delay && reconnect_delay != -1)
                timer_add(&jabber_plugin, "reconnect", reconnect_delay, 0, jabber_reconnect_handler, xstrdup(s->uid));

}

void jabber_reconnect_handler(int type, void *data)
{
        session_t *s = session_find((char*) data);
        char *tmp;

        if (!s || session_connected_get(s) == 1)
                return;

        tmp = xstrdup("/connect");
        command_exec(NULL, s, tmp, 0);
        xfree(tmp);
}

static void jabber_handle_start(void *data, const char *name, const char **atts)
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        jabber_private_t *j = session_private_get(jdh->session);
        session_t *s = jdh->session;

        if (!xstrcmp(name, "stream:stream")) {
                char *password = (char *) session_get(s, "password");
                char *resource = jabber_escape(session_get(s, "resource"));
                const char *uid = session_uid_get(s);
                char *username;
                int plaintext = session_int_get(s, "plaintext_passwd");

                username = xstrdup(uid + 4);
                *(xstrchr(username, '@')) = 0;

                if (!resource)
                        resource = xstrdup(JABBER_DEFAULT_RESOURCE);

                j->stream_id = xstrdup(jabber_attr((char **) atts, "id"));

                if (plaintext)
                        jabber_write(j, "<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><password>%s</password><resource>%s</resource></query></iq>", j->server, username, password, resource);
                else
                        jabber_write(j, "<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>", j->server, username, jabber_digest(j->stream_id, password), resource);
                xfree(username);
		xfree(resource);
                return;
        }

        xmlnode_handle_start(data, name, atts);
}

void jabber_handle_stream(int type, int fd, int watch, void *data)
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        session_t *s = (session_t*) jdh->session;
        jabber_private_t *j = session_private_get(s);
        char *buf;
        int len;

        s->activity = time(NULL);

        /* ojej, roz³±czy³o nas */
        if (type == 1) {
                debug("[jabber] jabber_handle_stream() type == 1, exitting\n");
                jabber_handle_disconnect(s);
                return;
        }

        debug("[jabber] jabber_handle_stream()\n");

        if (!(buf = XML_GetBuffer(j->parser, 4096))) {
                print("generic_error", "XML_GetBuffer failed");
                goto fail;
        }

#ifdef HAVE_GNUTLS
        if (j->using_ssl && j->ssl_session) {
                do {
                        len = gnutls_record_recv(j->ssl_session, buf, 4095);
#ifdef _POSIX_PRIORITY_SCHEDULING
			sched_yield();
#endif
                } while ((len == GNUTLS_E_INTERRUPTED) || (len == GNUTLS_E_AGAIN));

                if (len < 0) {
                        print("generic_error", gnutls_strerror(len));
                        goto fail;
                }
        } else
#endif
                if ((len = read(fd, buf, 4095)) < 1) {
                        print("generic_error", strerror(errno));
                        goto fail;
                }

        buf[len] = 0;

        debug("[jabber] recv %s\n", buf);

        if (!XML_ParseBuffer(j->parser, len, (len == 0))) {
                print("jabber_xmlerror", session_name(s));
                goto fail;
        }

        return;

fail:
        watch_remove(&jabber_plugin, fd, WATCH_READ);
}

void jabber_handle_connect(int type, int fd, int watch, void *data)
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        int res = 0, res_size = sizeof(res);
        jabber_private_t *j = session_private_get(jdh->session);

        debug("[jabber] jabber_handle_connect()\n");

        if (type != 0) {
                return;
        }

        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
                print("generic_error", strerror(res));
                jabber_handle_disconnect(jdh->session);
		xfree(data);
                return;
        }

        watch_add(&jabber_plugin, fd, WATCH_READ, 1, jabber_handle_stream, jdh);

        jabber_write(j, "<?xml version=\"1.0\" encoding=\"utf-8\"?><stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">", j->server);

        j->id = 1;
        j->parser = XML_ParserCreate("UTF-8");
        XML_SetUserData(j->parser, (void*)data);
        XML_SetElementHandler(j->parser, (XML_StartElementHandler) jabber_handle_start, (XML_EndElementHandler) xmlnode_handle_end);
        XML_SetCharacterDataHandler(j->parser, (XML_CharacterDataHandler) xmlnode_handle_cdata);
}

void jabber_handle_resolver(int type, int fd, int watch, void *data)
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        session_t *s = jdh->session;
        jabber_private_t *j = jabber_private(s);
        struct in_addr a;
        int one = 1, res;
        int port_s = session_int_get(s, "port");
        int port = (port_s != -1 ) ? port_s : 5222;
        struct sockaddr_in sin;
#ifdef HAVE_GNUTLS
        const int use_ssl = session_int_get(s, "use_ssl");
        const int ssl_port_s = session_int_get(s, "ssl_port");
        int ssl_port = (ssl_port_s != -1) ? ssl_port_s : 5223;
        /* Allow connections to servers that have OpenPGP keys as well. */
        const int cert_type_priority[3] = {GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0};
        const int comp_type_priority[3] = {GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0};
#endif

        if (type != 0) 
                return;

        debug("[jabber] jabber_handle_resolver()\n", type);

        if (!port)
                port = 5222;

        if ((res = read(fd, &a, sizeof(a))) != sizeof(a) || (res && !xstrcmp(inet_ntoa(a), "255.255.255.255"))) {
                if (res == -1)
                        debug("[jabber] unable to read data from resolver: %s\n", strerror(errno));
                else
                        debug("[jabber] read %d bytes from resolver. not good\n", res);
                close(fd);
                print("conn_failed", format_find("conn_failed_resolving"), session_name(jdh->session));
                /* no point in reconnecting by jabber_handle_disconnect() */
                j->connecting = 0;
                return;
        }

        debug("[jabber] resolved to %s\n", inet_ntoa(a));

        close(fd);

        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                debug("[jabber] socket() failed: %s\n", strerror(errno));
                print("conn_failed", strerror(errno), session_name(jdh->session));
                jabber_handle_disconnect(jdh->session);
                return;
        }

        debug("[jabber] socket() = %d\n", fd);

        j->fd = fd;

        if (ioctl(fd, FIONBIO, &one) == -1) {
                debug("[jabber] ioctl() failed: %s\n", strerror(errno));
                print("conn_failed", strerror(errno), session_name(jdh->session));
                jabber_handle_disconnect(jdh->session);
                return;
        }

        /* failure here isn't fatal, don't bother with checking return code */
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = a.s_addr;
#ifdef HAVE_GNUTLS
	j->using_ssl = 0;
        if (use_ssl)
                sin.sin_port = htons(ssl_port);
        else
#endif
                sin.sin_port = htons(port);

        debug("[jabber] connecting to %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

        connect(fd, (struct sockaddr*) &sin, sizeof(sin));

        if (errno != EINPROGRESS && errno != 0) {
                debug("[jabber] connect() failed: %s (errno=%d)\n", strerror(errno), errno);
                print("conn_failed", strerror(errno), session_name(jdh->session));
                jabber_handle_disconnect(jdh->session);
                return;
        }

#ifdef HAVE_GNUTLS
        if (use_ssl) {
                int ret = 0;
                gnutls_certificate_allocate_credentials(&(j->xcred));
                /* XXX - ~/.ekg/certs/server.pem */
                gnutls_certificate_set_x509_trust_file(j->xcred, "brak", GNUTLS_X509_FMT_PEM);
		if (gnutls_init(&(j->ssl_session), GNUTLS_CLIENT)) {
			print("conn_failed_tls");
			print("conn_failed", gnutls_strerror(ret), session_name(jdh->session));
			jabber_handle_disconnect(jdh->session);
			return;
		}
                gnutls_set_default_priority(j->ssl_session);
                gnutls_certificate_type_set_priority(j->ssl_session, cert_type_priority);
                gnutls_credentials_set(j->ssl_session, GNUTLS_CRD_CERTIFICATE, j->xcred);
                gnutls_compression_set_priority(j->ssl_session, comp_type_priority);

                /* we use read/write instead of recv/send */
                gnutls_transport_set_pull_function(j->ssl_session, (gnutls_pull_func)read);
                gnutls_transport_set_push_function(j->ssl_session, (gnutls_push_func)write);
                gnutls_transport_set_ptr(j->ssl_session, (gnutls_transport_ptr)(j->fd));

                do {
                        ret = gnutls_handshake(j->ssl_session);
#ifdef _POSIX_PRIORITY_SCHEDULING
			sched_yield();
#endif
                } while ((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN));

                if (ret < 0) {
                        debug("[jabber] ssl handshake failed: %d - %s\n", ret, gnutls_strerror(ret));
                        print("conn_failed", gnutls_strerror(ret), session_name(jdh->session));
                        gnutls_deinit(j->ssl_session);
                        gnutls_certificate_free_credentials(j->xcred);
                        jabber_handle_disconnect(jdh->session);
                        return;
                }
                j->using_ssl = 1;
        } // use_ssl
#endif
        watch_add(&jabber_plugin, fd, WATCH_WRITE, 0, jabber_handle_connect, jdh);
}

int jabber_status_show_handle(void *data, va_list ap)
{
        char **uid = va_arg(ap, char**);
        session_t *s = session_find(*uid);
        jabber_private_t *j = session_private_get(s);
        userlist_t *u;
        int port_s = session_int_get(s, "port");
        int port = (port_s != -1) ? port_s : 5222;
#ifdef HAVE_GNUTLS
        const int ssl_port_s = session_int_get(s, "ssl_port");
        int ssl_port = (ssl_port_s != -1) ? ssl_port_s : 5223;
#endif
        const char *resource = session_get(s, "resource");
        char *fulluid;
        struct tm *t;
        time_t n;
        int now_days;
        char buf[100], *tmp;
	const char *format;

        if (!s || !j)
                return -1;

        fulluid = saprintf("%s/%s", s->uid, (resource ? resource : JABBER_DEFAULT_RESOURCE));

        // nasz stan
        if ((u = userlist_find(s, s->uid)) && u->nickname)
                print("show_status_uid_nick", fulluid, u->nickname);
        else
                print("show_status_uid", fulluid);

        xfree(fulluid);

        // nasz status
        if (s->connected)
                tmp = format_string(format_find(ekg_status_label(s->status, s->descr, "show_status_")),s->descr, "");
        else
                tmp = format_string(format_find("show_status_notavail"));

        print("show_status_status_simple", tmp);

        // serwer
#ifdef HAVE_GNUTLS
        if (j->using_ssl)
                print("show_status_server_tls", j->server, itoa(ssl_port));
        else
#endif
                print("show_status_server", j->server, itoa(port));

        if (j->connecting)
                print("show_status_connecting");

        // kiedy ostatnie polaczenie/rozlaczenie
        n = time(NULL);
        t = localtime(&n);
        now_days = t->tm_yday;

        t = localtime(&s->last_conn);
        format = format_find((t->tm_yday == now_days) ? "show_status_last_conn_event_today" : "show_status_last_conn_event");
        if (!strftime(buf, sizeof(buf), format, t) && xstrlen(format)>0)
                xstrcpy(buf, "TOOLONG");

        if (s->connected)
                print("show_status_connected_since", buf);
        else
                print("show_status_disconnected_since", buf);

        return 0;
}

static int jabber_theme_init()
{
        format_add("jabber_auth_subscribe", _("%> (%2) %T%1%n asks for authorisation. Use \"/auth -a %1\" to accept, \"/auth -d %1\" to refuse.%n\n"), 1);
        format_add("jabber_auth_unsubscribe", _("%> (%2) %T%1%n asks for removal. Use \"/auth -d %1\" to delete.%n\n"), 1);
        format_add("jabber_xmlerror", _("%! (%1) Error parsing XML%n\n"), 1);
        format_add("jabber_auth_request", _("%> (%2) Sent authorisation request to %T%1%n.\n"), 1);
        format_add("jabber_auth_accept", _("%> (%2) Authorised %T%1%n.\n"), 1);
        format_add("jabber_auth_unsubscribed", _("%> (%2) Asked %T%1%n to remove authorisation.\n"), 1);
        format_add("jabber_auth_cancel", _("%> (%2) Authorisation for %T%1%n revoked.\n"), 1);
        format_add("jabber_auth_denied", _("%> (%2) Authorisation for %T%1%n denied.\n"), 1);
        format_add("jabber_auth_probe", _("%> (%2) Sent presence probe to %T%1%n.\n"), 1);
        format_add("jabber_generic_conn_failed", _("%! (%1) Error connecting to Jabber server%n\n"), 1);
        format_add("jabber_msg_failed", _("%! Message to %T%1%n can't be delivered: %R(%2) %r%3%n\n"),1);
        format_add("jabber_msg_failed_long", _("%! Message to %T%1%n %y(%n%K%4(...)%y)%n can't be delivered: %R(%2) %r%3%n\n"),1);
        format_add("jabber_version_response", _("%> Jabber ID: %T%1%n\n%> Client name: %T%2%n\n%> Client version: %T%3%n\n%> Operating system: %T%4%n\n"), 1);
        format_add("jabber_userinfo_response", _("%> Jabber ID: %T%1%n\n%> Full Name: %T%2%n\n%> Nickname: %T%3%n\n%> Birthday: %T%4%n\n%> City: %T%5%n\n%> Desc: %T%6%n\n"), 1);
        format_add("jabber_lastseen_response", _("%> Jabber ID:  %T%1%n\n%> Logged out: %T%2%n\n"), 1);
        format_add("jabber_unknown_resource", _("%! (%1) User's resource unknown%n\n\n"), 1);
        format_add("jabber_status_notavail", _("%! (%1) Unable to check version, because %2 is unavailable%n\n"), 1);
        format_add("jabber_typing_notify", _("%> %T%1%n is typing to us ...%n\n"), 1);
	format_add("jabber_charset_init_error", _("%! Error initialising charset conversion (%1->%2): %3"), 3);

        return 0;
}

int jabber_plugin_init(int prio)
{
        list_t l;

        plugin_register(&jabber_plugin, prio);

        query_connect(&jabber_plugin, "protocol-validate-uid", jabber_validate_uid, NULL);
        query_connect(&jabber_plugin, "plugin-print-version", jabber_print_version, NULL);
        query_connect(&jabber_plugin, "session-added", jabber_session, (void*) 1);
        query_connect(&jabber_plugin, "session-removed", jabber_session, (void*) 0);
        query_connect(&jabber_plugin, "status-show", jabber_status_show_handle, NULL);

        jabber_register_commands();

        plugin_var_add(&jabber_plugin, "alias", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_away", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_back", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_connect", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_find", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_reconnect", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "display_notify", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "log_formats", VAR_STR, "xml,simple", 0, NULL);
        plugin_var_add(&jabber_plugin, "password", VAR_STR, "foo", 1, NULL);
        plugin_var_add(&jabber_plugin, "plaintext_passwd", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "port", VAR_INT, itoa(5222), 0, NULL);
        plugin_var_add(&jabber_plugin, "priority", VAR_INT, itoa(5), 0, NULL);
        plugin_var_add(&jabber_plugin, "resource", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "server", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "ssl_port", VAR_INT, itoa(5223), 0, NULL);
        plugin_var_add(&jabber_plugin, "show_typing_notify", VAR_INT, "1", 0, NULL);
        plugin_var_add(&jabber_plugin, "use_ssl", VAR_INT, itoa(0), 0, NULL);
        plugin_var_add(&jabber_plugin, "ver_client_name", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "ver_client_version", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "ver_os", VAR_STR, 0, 0, NULL);

	config_jabber_console_charset = xstrdup("iso-8859-2");
	variable_add(&jabber_plugin, "console_charset", VAR_STR, 1, &config_jabber_console_charset, NULL, NULL, NULL);

        for (l = sessions; l; l = l->next)
                jabber_private_init((session_t*) l->data);

#ifdef HAVE_GNUTLS
        gnutls_global_init();
#endif

        return 0;
}

static int jabber_plugin_destroy()
{
        list_t l;

#ifdef HAVE_GNUTLS
        gnutls_global_deinit();
#endif

        for (l = sessions; l; l = l->next)
                jabber_private_destroy((session_t*) l->data);

        plugin_unregister(&jabber_plugin);

        return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
