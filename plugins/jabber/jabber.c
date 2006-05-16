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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>

#ifdef HAVE_EXPAT_H
#  include <expat.h>
#endif

#ifdef __sun      /* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif

#include <ekg/debug.h>
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

#define jabberfix(x,a) ((x) ? x : a)

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

void jabber_dcc_close_handler(struct dcc_s *d) {
	jabber_dcc_t *p = d->priv;
	if (!d->active && d->type == DCC_GET) {
		session_t *s = p->session;
		jabber_private_t *j;
		
		if (!s || !(j= session_private_get(s))) return;

		jabber_write(j, "<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"403\">Declined</error></iq>", 
			d->uid+4, p->req);
	}
	if (p) {
		xfree(p->req);
		xfree(p->sid);
		xfree(p);
	}
	d->priv = NULL;
}

dcc_t *jabber_dcc_find(const char *uin, /* without jid: */ const char *id) {
	int number;
	if (sscanf(id, "offer%d", &number)) {
		list_t l;
		for (l = dccs; l; l = l->next) {
			dcc_t *d = l->data;
			if (d->id == number && d->type == DCC_SEND && !xstrncmp(d->uid, "jid:", 4) && !xstrncmp(d->uid+4, uin, xstrlen(d->uid+4))) {
				debug("jabber_dcc_find() %s %s founded: 0x%x\n", uin, id, d);
				return d;
				break;
			}
		}
	}
	debug("jabber_dcc_find() %s %s not founded. Possible abuse attempt?!\n", uin, id);
	return NULL;
}

/*
 * jabber_session()
 *
 * obs³uguje dodawanie i usuwanie sesji -- inicjalizuje lub zwalnia
 * strukturê odpowiedzialn± za wnêtrzno¶ci jabberowej sesji.
 */
QUERY(jabber_session)
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
QUERY(jabber_print_version)
{
        print("generic", XML_ExpatVersion());

        return 0;
}

/*
 * jabber_validate_uid()
 *
 * sprawdza, czy dany uid jest poprawny i czy plugin do obs³uguje.
 */
QUERY(jabber_validate_uid)
{
        char *uid = *(va_arg(ap, char **)), *m;
        int *valid = va_arg(ap, int *);

        if (!uid)
                return 0;

	/* minimum: jid:a@b */
        if (!xstrncasecmp(uid, "jid:", 4) && (m=xstrchr(uid, '@')) &&
			((uid+4)<m) && xstrlen(m+1)) {
                (*valid)++;
		return -1;
	}

        return 0;
}

QUERY(jabber_window_kill) 
{
	window_t        *w = *va_arg(ap, window_t **);
	jabber_private_t *j;

	char *status = NULL;

	if (w && w->id && w->target && w->userlist && session_check(w->session, 1, "jid") && 
			(j = jabber_private(w->session)) && session_connected_get(w->session))
		jabber_write(j, "<presence to=\"%s/%s\" type=\"unavailable\">%s</presence>", w->target+4, "darkjames", status ? status : "");

	return 0;
}

int jabber_write_status(session_t *s)
{
        jabber_private_t *j = session_private_get(s);
        int priority = session_int_get(s, "priority");
        const char *status;
        CHAR_T *descr;
	char *real = NULL;

        if (!s || !j)
                return -1;

        if (!session_connected_get(s))
                return 0;

        status = session_status_get(s);
	if ((descr = jabber_escape(session_descr_get(s)))) {
		real = saprintf("<status>" CHARF "</status>", descr);
		xfree(descr);
	}

	if (!xstrcmp(status, EKG_STATUS_AVAIL))			jabber_write(j, "<presence>%s<priority>%d</priority></presence>", 			real ? real : "", priority);
	else if (!xstrcmp(status, EKG_STATUS_INVISIBLE))	jabber_write(j, "<presence type=\"invisible\">%s<priority>%d</priority></presence>", 	real ? real : "", priority);
	else							jabber_write(j, "<presence><show>%s</show>%s<priority>%d</priority></presence>", 	status, real ? real : "", priority);

        xfree(real);
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
	xmlnode_t *nerr = xmlnode_find_child(n, "error");
	xmlnode_t *nsubject, *xitem;
	
	const char *from = jabber_attr(n->atts, "from");

	char *juid 	= jabber_unescape(from); /* was tmp */
	char *uid 	= saprintf("jid:%s", juid);

	string_t body;
	time_t sent;

	xfree(juid);

	if (nerr) {
		char *ecode = jabber_attr(nerr->atts, "code");
		char *etext = jabber_unescape(nerr->data);
		char *recipient = get_nickname(s, uid);

		if (nbody && nbody->data) {
			char *tmp2 = jabber_unescape(nbody->data);
			char *mbody = xstrndup(tmp2, 15);
			xstrtr(mbody, '\n', ' ');

			print("jabber_msg_failed_long", recipient, ecode, etext, mbody);

			xfree(mbody);
			xfree(tmp2);
		} else
			print("jabber_msg_failed", recipient, ecode, etext);

		xfree(etext);
		xfree(uid);
		return;
	} /* <error> */

	body = string_init("");

	if ((nsubject = xmlnode_find_child(n, "subject"))) {
		string_append(body, "Subject: ");
		string_append(body, nsubject->data);
		string_append(body, "\n\n");
	}

	if (nbody)
		string_append(body, nbody->data);

	if ((xitem = xmlnode_find_child(n, "x"))) {
		const char *ns = jabber_attr(xitem->atts, "xmlns");

		/* try to parse timestamp */
		sent = jabber_try_xdelay(xitem, ns);

		if (!xstrncmp(ns, "jabber:x:event", 14)) {
			int acktype = 0; /* bitmask: 2 - queued ; 1 - delivered */
			int isack;

			if (xmlnode_find_child(xitem, "delivered"))	acktype |= 1;	/* delivered */
			if (xmlnode_find_child(xitem, "offline"))	acktype	|= 2;	/* queued */
			if (xmlnode_find_child(xitem, "composing"))	acktype |= 4;	/* composing */

			isack = (acktype & 1) || (acktype & 2);

			/* jesli jest body, to mamy do czynienia z prosba o potwierdzenie */
			if (nbody && isack) {
				char *id = jabber_attr(n->atts, "id");
				const char *our_status = session_status_get(s);

				jabber_write(j, "<message to=\"%s\">", from);
				jabber_write(j, "<x xmlns=\"jabber:x:event\">");

				if (!xstrcmp(our_status, EKG_STATUS_INVISIBLE)) {
					jabber_write(j, "<offline/>");
				} else {
					if (acktype & 1)
						jabber_write(j, "<delivered/>");
					if (acktype & 2)
						jabber_write(j, "<displayed/>");
				};
				jabber_write(j, "<id>%s</id></x></message>",id);
			};
			/* je¶li body nie ma, to odpowiedz na nasza prosbe */
			if (!nbody && isack) {
				char *__session = xstrdup(session_uid_get(s));
				char *__rcpt	= xstrdup(uid); /* was uid+4 */
				CHAR_T *__status  = xwcsdup(
						(acktype & 1) ? EKG_ACK_DELIVERED : 
						(acktype & 2) ? EKG_ACK_QUEUED : 
/* TODO: wbudowac composing w protocol-message-ack ? */
/*						(acktype & 4) ? "compose" :  */
						NULL);
				CHAR_T *__seq	= NULL; /* id ? */

				/* protocol_message_ack; sesja ; uid + 4 ; seq (NULL ? ) ; status - delivered ; queued ) */
				{
					CHAR_T *session = normal_to_wcs(__session);
					CHAR_T *rcpt = normal_to_wcs(__rcpt);
					query_emit(NULL, "protocol-message-ack", &__session, &rcpt, &__seq, &__status);
					free_utf(session);
					free_utf(rcpt);
				}
				
				xfree(__session);
				xfree(__rcpt);
				xfree(__status);
				/* xfree(__seq); */
			}

			if (!nbody && (acktype & 4) && session_int_get(s, "show_typing_notify")) {
					print("jabber_typing_notify", uid+4);
			} /* composing */
		} /* jabber:x:event */

		if (!xstrncmp(ns, "jabber:x:oob", 12)) {
			xmlnode_t *xurl;
			xmlnode_t *xdesc;

			if ( ( xurl = xmlnode_find_child(xitem, "url") ) ) {
				string_append(body, "\n\n");
				string_append(body, "URL: ");
				string_append(body, xurl->data);
				string_append(body, "\n");
				if ((xdesc = xmlnode_find_child(xitem, "desc"))) {
					string_append(body, xdesc->data);
					string_append(body, "\n");
				}
			}
		} /* jabber:x:oob */
	} /* if !nerr && <x>; TODO: split as functions */
	else sent = time(NULL);

	if (nbody || nsubject) {
		const char *type = jabber_attr(n->atts, "type");

		char *me	= xstrdup(session_uid_get(s));
		int class 	= EKG_MSGCLASS_CHAT;
		int ekgbeep 	= EKG_TRY_BEEP;
		int secure	= 0;
		char **rcpts 	= NULL;
		char *seq 	= NULL;
		uint32_t *format = NULL;

		char *text = jabber_unescape(body->str);

		debug("[jabber,message] type = %s\n", type);
		if (!xstrcmp(type, "groupchat")) {
			char *tuid = xstrrchr(uid, '/');
			int isour = 0;
			char *proto = xstrdup("jabber_");
			char *proto_ext = NULL;
			int proto_imp = 0;
			int priv = 0;
			int tous = 0;

			char *uid2 = (tuid) ? xstrndup(uid, tuid-uid) : xstrdup(uid);
			char *uuid = (tuid) ? xstrdup(tuid+1) : uid;

			rcpts = xcalloc(2, sizeof(char *));
			rcpts[0] = xstrdup(uid2);
			
			query_emit(NULL, "multi-protocol-message", &me, &uuid, &rcpts, &uid2, &proto, &proto_ext, &proto_imp, 
					&isour, &priv, &tous, &sent, &secure, &text, NULL);
			xfree(proto);
		} else {
			query_emit(NULL, "protocol-message", &me, &uid, &rcpts, &text, &format, &sent, &class, &seq, &ekgbeep, &secure);
		}


		xfree(me);
		xfree(text);
		array_free(rcpts);
/*
		xfree(seq);
		xfree(format);
*/
	}
	string_free(body, 1);
	xfree(uid);
} /* */
/* idea and some code copyrighted by Marek Marczykowski jid:marmarek@jabberpl.org */
void jabber_handle_xmldata_form(session_t *s, const char *uid, const char *command, xmlnode_t *form) { /* JEP-0004: Data Forms */
	xmlnode_t *node;
	int fieldcount = 0;
	for (node = form; node; node = node->next) {
		if (!xstrcmp(node->name, "title")) {
			char *title = jabber_unescape(node->data);
			print("jabber_form_title", session_name(s), uid, title);
			xfree(title);
		} else if (!xstrcmp(node->name, "instructions")) {
			char *inst = jabber_unescape(node->data);
			print("jabber_form_instructions", session_name(s), uid, inst);
			xfree(inst);
		} else if (!xstrcmp(node->name, "field")) {
			xmlnode_t *child;
			char *label	= jabber_unescape(jabber_attr(node->atts, "label"));
			char *var	= jabber_unescape(jabber_attr(node->atts, "var"));
			char *def_option = NULL;
			string_t sub = NULL;
			int subcount = 0;

			int isreq = 0;	/* -1 - optional; 1 - required */
			
			if (!fieldcount) print("jabber_form_command", session_name(s), uid, command);

			for (child = node->children; child; child = child->next) {
				if (!xstrcmp(child->name, "required")) isreq = 1;
				else if (!xstrcmp(child->name, "value")) { xfree(def_option); def_option = jabber_unescape(child->data); } 
				else if (!xstrcmp(child->name, "option")) {
					xmlnode_t *tmp;
					char *opt_value = jabber_unescape( (tmp = xmlnode_find_child(child, "value")) ? tmp->data : NULL);
					char *opt_label = jabber_unescape(jabber_attr(child->atts, "label"));
					char *fritem;

					fritem = format_string(format_find("jabber_form_item_val"), session_name(s), uid, opt_value, opt_label);
					if (!sub)	sub = string_init(fritem);
					else		string_append(sub, fritem);
					xfree(fritem);

/*					print("jabber_form_item_sub", session_name(s), uid, opt_label, opt_value); */
/*					debug("[[option]] [value] %s [label] %s\n", opt_value, opt_label); */

					xfree(opt_value);
					xfree(opt_label);
					subcount++;
					if (!(subcount % 4)) string_append(sub, "\n\t");
				} 
				else debug("[FIELD->CHILD] %s\n", child->name);

			}
			print("jabber_form_item", session_name(s), uid, label, var, def_option, 
				isreq == -1 ? "X" : isreq == 1 ? "V" : " ");
			if (sub) {
				int len = xstrlen(sub->str);
				if (sub->str[len-1] == '\t' && sub->str[len-2] == '\n') sub->str[len-2] = 0;
				print("jabber_form_item_sub", session_name(s), uid, sub->str);
				string_free(sub, 1);
			}
			fieldcount++;

			xfree(var);
			xfree(label);
		}
	}
	if (!fieldcount) print("jabber_form_command", session_name(s), uid, command);
	print("jabber_form_end", session_name(s), uid, command);
}

void jabber_handle_iq(xmlnode_t *n, jabber_handler_data_t *jdh) {
	const char *type = jabber_attr(n->atts, "type");
	const char *id   = jabber_attr(n->atts, "id");
	const char *from = jabber_attr(n->atts, "from");

	session_t *s = jdh->session;
	jabber_private_t *j = jabber_private(s);
	xmlnode_t *q;

	if (!type) {
		debug("[jabber] <iq> without type!\n");
		return;
	}

	if (!xstrcmp(id, "auth")) {
		s->last_conn = time(NULL);
		j->connecting = 0;

		if (!xstrcmp(type, "result")) {
			session_connected_set(s, 1);
			session_unidle(s);
			{
				char *__session = xstrdup(session_uid_get(s));
				query_emit(NULL, "protocol-connected", &__session);
				xfree(__session);
			}
			if (session_get(s, "__new_acount")) {
				print("register", session_uid_get(s));
				if (!xstrcmp(session_get(s, "password"), "foo")) print("register_change_passwd", session_uid_get(s), "foo");
				session_set(s, "__new_acount", NULL);
			}

			jdh->roster_retrieved = 0;
			userlist_free(s);
			jabber_write(j, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
			jabber_write_status(s);
		} else if (!xstrcmp(type, "error")) { /* TODO: try to merge with <message>'s <error> parsing */
			xmlnode_t *e = xmlnode_find_child(n, "error");

			if (e && e->data) {
				char *data = jabber_unescape(e->data);
				print("conn_failed", data, session_name(s));
			} else
				print("jabber_generic_conn_failed", session_name(s));
		}
	}

	if (!xstrncmp(id, "passwd", 6)) {
		if (!xstrcmp(type, "result")) {
			char *new_passwd = (char *) session_get(s, "__new_password");

			session_set(s, "password", new_passwd);
			session_set(s, "__new_password", NULL);
			print("passwd");
		} else if (!xstrcmp(type, "error")) {
			xmlnode_t *e = xmlnode_find_child(n, "error");
			char *reason = (e) ? jabber_unescape(e->data) : NULL;

			print("passwd_failed", jabberfix(reason, "?"));
			xfree(reason);
		}
		session_set(s, "__new_password", NULL);
	}

	if (!xstrncmp(id, "register", 8)) {
		if (!xstrcmp(type, "error")) {
			print("register_failed", "NOERROR"); /* XXX, TODO */
		}
	}


	if ((q = xmlnode_find_child(n, "si"))) { /* JEP-0095: Stream Initiation */
/* dj, I think we don't need to unescape rows (tags) when there should be int, do we?  ( <size> <offset> <length>... )*/
		xmlnode_t *p;
		if (!xstrcmp(type, "result")) {
/* Eh... */
#if 0
			char *uin = jabber_unescape(from);
			dcc_t *d;
			if ((d = jabber_dcc_find(uin, id))) {
				jabber_dcc_t *p = d->priv;
				char *protstr;

				switch(p->protocol) {
					case(1): protstr = saprintf(
							"<query xmlns=\"http://jabber.org/protocol/bytestreams\" mode=\"tcp\" sid=\"%s\">",
							p->sid); 
						break;
					case(2): ;
				}

				jabber_write(j, "<iq type=\"set\" to=\"%s\" id=\"%d\">"
						"<query xmlns=%s>",
						"<streamhost port=\"8010\" host=\"ip\" jid=\"%s/%s\"/>"
						"<fast xmlns=\"http://affinix.com/jabber/stream\"/></query></iq>",
						d->uid+4, j->id++, p->sid);
				xfree(protstr);
			}
#endif
		} 

		if (!xstrcmp(type, "set") && ((p = xmlnode_find_child(q, "file")))) {  /* JEP-0096: File Transfer */
			dcc_t *D;
			char *uin = jabber_unescape(from);
			char *uid;
			char *filename	= jabber_unescape(jabber_attr(p->atts, "name"));
			char *size 	= jabber_attr(p->atts, "size");
			xmlnode_t *range;
			jabber_dcc_t *jdcc;

			uid = saprintf("jid:%s", uin);

			jdcc = xmalloc(sizeof(jabber_dcc_t));
			jdcc->session	= s;
			jdcc->req 	= xstrdup(id);
			jdcc->sid	= jabber_unescape(jabber_attr(q->atts, "id"));

			D = dcc_add(uid, DCC_GET, NULL);
			dcc_filename_set(D, filename);
			dcc_size_set(D, atoi(size));
			dcc_private_set(D, jdcc);
			dcc_close_handler_set(D, jabber_dcc_close_handler);
/* XXX, result
			if ((range = xmlnode_find_child(p, "range"))) {
				char *off = jabber_attr(range->atts, "offset");
				char *len = jabber_attr(range->atts, "length");
				if (off) dcc_offset_set(D, atoi(off));
				if (len) dcc_size_set(D, atoi(len));
			}
*/
			print("dcc_get_offer", format_user(s, uid), filename, size, itoa(dcc_id_get(D))); 

			xfree(uin);
			xfree(uid);
			xfree(filename);
		}
	}
	if (!xstrcmp(type, "error") && !xstrncmp(id, "offer", 5)) {
		char *uin = jabber_unescape(from);
		if (dcc_close(jabber_dcc_find(uin, id))); /* possible abuse attempt */
		/* XXX, informujemy usera o zamknieciu po³±czenia? */

		xfree(uin);
	}
/* FILETRANSFER */
	
	/* XXX: temporary hack: roster przychodzi jako typ 'set' (przy dodawaniu), jak
	        i typ "result" (przy za¿±daniu rostera od serwera) */
	if (!xstrncmp(type, "result", 6) || !xstrncmp(type, "set", 3)) {
		xmlnode_t *q;

		/* First we check if there is vCard... */
		if ((q = xmlnode_find_child(n, "vCard"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrncmp(ns, "vcard-temp", 10)) {
				xmlnode_t *fullname = xmlnode_find_child(q, "FN");
				xmlnode_t *nickname = xmlnode_find_child(q, "NICKNAME");
				xmlnode_t *birthday = xmlnode_find_child(q, "BDAY");
				xmlnode_t *adr  = xmlnode_find_child(q, "ADR");
				xmlnode_t *city = xmlnode_find_child(adr, "LOCALITY");
				xmlnode_t *desc = xmlnode_find_child(q, "DESC");

				char *from_str     = (from)	? jabber_unescape(from) : NULL;
				char *fullname_str = (fullname) ? jabber_unescape(fullname->data) : NULL;
				char *nickname_str = (nickname) ? jabber_unescape(nickname->data) : NULL;
				char *bday_str     = (birthday) ? jabber_unescape(birthday->data) : NULL;
				char *city_str     = (city)	? jabber_unescape(city->data) : NULL;
				char *desc_str     = (desc)	? jabber_unescape(desc->data) : NULL;

				print("jabber_userinfo_response", 
						jabberfix(from_str, _("unknown")), 	jabberfix(fullname_str, _("unknown")),
						jabberfix(nickname_str, _("unknown")),	jabberfix(bday_str, _("unknown")),
						jabberfix(city_str, _("unknown")),	jabberfix(desc_str, _("unknown")));
				xfree(desc_str);
				xfree(city_str);
				xfree(bday_str);
				xfree(nickname_str);
				xfree(fullname_str);
				xfree(from_str);
			}
		}

		if ((q = xmlnode_find_child(n, "query"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");
			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				xmlnode_t *node;
				char *uid = jabber_unescape(from);
				print("jabber_transinfo_begin", session_name(s), uid);

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "identity")) {
						char *cat	= jabber_attr(node->atts, "category");			/* server, gateway, directory */
						char *name	= jabber_unescape(jabber_attr(node->atts, "name"));	/* nazwa */
						char *type	= jabber_attr(node->atts, "type");			/* typ: im */
						
						if (name) /* jesli nie ma nazwy don't display it. */
							print("jabber_transinfo_identify" /* _server, _gateway... ? */, session_name(s), uid, name);

						xfree(name);
					} else if (!xstrcmp(node->name, "feature")) {
						char *var = jabber_attr(node->atts, "var");
						char *tvar = NULL; /* translated */
						int user_command = 0;

/* dj, jakas glupota... ale ma ktos pomysl zeby to inaczej zrobic?... jeszcze istnieje pytanie czy w ogole jest sens to robic.. */
						if (!xstrcmp(var, "http://jabber.org/protocol/disco#info")) 		tvar = "/jid:transpinfo";
						else if (!xstrcmp(var, "http://jabber.org/protocol/disco#items")) 	tvar = "/jid:transports";
						else if (!xstrcmp(var, "http://jabber.org/protocol/disco"))		tvar = "/jid:transports && /jid:transpinfo";
						else if (!xstrcmp(var, "jabber:iq:register"))		    		tvar = "/jid:register";
						else if (!xstrcmp(var, "jabber:iq:search"))				tvar = "/jid:search";
						else if (!xstrcmp(var, "http://jabber.org/protocol/muc"))		tvar = "/jid:mucjoin && /jid:mucpart";

						else if (!xstrcmp(var, "jabber:iq:version"))	{ user_command = 1;	tvar = "/jid:ver"; }
						else if (!xstrcmp(var, "message"))		{ user_command = 1;	tvar = "/jid:msg"; }
						else if (!xstrcmp(var, "jabber:iq:last"))	{ user_command = 1;	tvar = "/jid:lastseen"; }
/*						else if (!xstrcmp(var, "vcard-temp"))		{ user_command = 1;	tvar = "/jid:change && /jid:userinfo"; } */

						if (tvar)	print(user_command ? "jabber_transinfo_comm_use" : "jabber_transinfo_comm_ser", 
									session_name(s), uid, tvar, var);
						else		print("jabber_transinfo_feature", session_name(s), uid, var, var);
					}

				}
				print("jabber_transinfo_end", session_name(s), uid);
				xfree(uid);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				xmlnode_t *item = xmlnode_find_child(q, "item");
				char *uid = jabber_unescape(from);

				if (item) {
					int i = 1;
					print("jabber_transport_list_begin", session_name(s), uid);
					for (; item; item = item->next, i++) {
						char *sdesc = jabber_unescape(jabber_attr(item->atts, "name"));
						char *sjid  = jabber_unescape(jabber_attr(item->atts, "jid"));
						print("jabber_transport_list_item", session_name(s), uid, sjid, sdesc, itoa(i));
						xfree(sdesc);
						xfree(sjid);
					}
					print("jabber_transport_list_end", session_name(s), uid);
				} else	print("jabber_transport_list_nolist", session_name(s), uid);
				xfree(uid);
			} else if (!xstrcmp(ns, "jabber:iq:search")) {
				xmlnode_t *node;
				int rescount = 0;
				char *uid = jabber_unescape(from);
				int formdone = 0;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) rescount++;
				}
				if (rescount > 1) print("jabber_search_begin", session_name(s), uid);

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "item")) {
						xmlnode_t *tmp;
						char *jid 	= jabber_attr(node->atts, "jid");
						char *nickname	= jabber_unescape( (tmp = xmlnode_find_child(node, "nick"))  ? tmp->data : NULL);
						char *fn	= jabber_unescape( (tmp = xmlnode_find_child(node, "first")) ? tmp->data : NULL);
						char *lastname	= jabber_unescape( (tmp = xmlnode_find_child(node, "last"))  ? tmp->data : NULL);
						char *email	= jabber_unescape( (tmp = xmlnode_find_child(node, "email")) ? tmp->data : NULL);

						/* idea about displaink user in depend of number of users founded gathered from gg plugin */
						print(rescount > 1 ? "jabber_search_items" : "jabber_search_item", 
							session_name(s), uid, jid, nickname, fn, lastname, email);
						xfree(nickname);
						xfree(fn);
						xfree(lastname);
						xfree(email);
					} else {
						xmlnode_t *reg;
						if (rescount == 0) rescount = -1;
						if (formdone) continue;

						for (reg = q->children; reg; reg = reg->next) {
							if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(reg->atts, "xmlns")) && !xstrcmp("form", jabber_attr(reg->atts, "type")))
							{
								formdone = 1;
								jabber_handle_xmldata_form(s, uid, "search", reg->children);
								break;
							}
						}

						if (!formdone) {
							/* XXX */
						}
					}
				}
				if (rescount > 1) print("jabber_search_end", session_name(s), uid);
				if (rescount == 0) print("search_not_found"); /* not found */
				xfree(uid);
			} else if (!xstrcmp(ns, "jabber:iq:register")) {
				xmlnode_t *reg;
				char *from_str = (from) ? jabber_unescape(from) : xstrdup(_("unknown"));
				int done = 0;

				for (reg = q->children; reg; reg = reg->next) {
					if (!xstrcmp(reg->name, "x") && !xstrcmp("jabber:x:data", jabber_attr(reg->atts, "xmlns")) && !xstrcmp("form", jabber_attr(reg->atts, "type")))
					{
						done = 1;
						jabber_handle_xmldata_form(s, from_str, "register", reg->children);
						break;
					}
				}
				if (!done) {
					xmlnode_t *instr = xmlnode_find_child(q, "instructions");
					print("jabber_form_title", session_name(s), from_str, from_str);

					if (instr->data) {
						char *instr_str = (instr) ? jabber_unescape(instr->data) : NULL;
						print("jabber_form_instructions", session_name(s), from_str, instr_str);
						xfree(instr_str);
					}
					print("jabber_form_command", session_name(s), from_str, "register");

					for (reg = q->children; reg; reg = reg->next) {
						char *jname, *jdata;
						if (!xstrcmp(reg->name, "instructions")) continue;

						jname = jabber_unescape(reg->name);
						jdata = jabber_unescape(reg->data);
						print("jabber_registration_item", session_name(s), from_str, jname, jdata);
						xfree(jname);
						xfree(jdata);
					}
					print("jabber_form_end", session_name(s), from_str, "register");
				}
				xfree(from_str);
			} else if (!xstrncmp(ns, "jabber:iq:version", 17)) {
				xmlnode_t *name = xmlnode_find_child(q, "name");
				xmlnode_t *version = xmlnode_find_child(q, "version");
				xmlnode_t *os = xmlnode_find_child(q, "os");

				char *from_str	= (from) ?	jabber_unescape(from) : NULL;
				char *name_str	= (name) ?	jabber_unescape(name->data) : NULL;
				char *version_str = (version) ? jabber_unescape(version->data) : NULL;
				char *os_str	= (os) ?	jabber_unescape(os->data) : NULL;

				print("jabber_version_response",
						jabberfix(from_str, "unknown"), jabberfix(name_str, "unknown"), 
						jabberfix(version_str, "unknown"), jabberfix(os_str, "unknown"));
				xfree(os_str);
				xfree(version_str);
				xfree(name_str);
				xfree(from_str);
			} else if (!xstrncmp(ns, "jabber:iq:last", 14)) {
				const char *last = jabber_attr(q->atts, "seconds");
				int seconds = 0;
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

				from_str = (from) ? jabber_unescape(from) : NULL;
				lastseen_str = xstrdup(buff);

				print("jabber_lastseen_response", jabberfix(from_str, "unknown"), lastseen_str);
				xfree(lastseen_str);
				xfree(from_str);
			} else if (!xstrncmp(ns, "jabber:iq:roster", 16)) {
				xmlnode_t *item = xmlnode_find_child(q, "item");

				for (; item ; item = item->next) {
					char *uid 	= saprintf("jid:%s",jabber_attr(item->atts, "jid"));
					userlist_t *u;

					/* je¶li element rostera ma subscription = remove to tak naprawde u¿ytkownik jest usuwany;
					w przeciwnym wypadku - nalezy go dopisaæ do userlisty; dodatkowo, jesli uzytkownika
					mamy ju¿ w liscie, to przyszla do nas zmiana rostera; usunmy wiec najpierw, a potem
					sprawdzmy, czy warto dodawac :) */
					if (jdh->roster_retrieved && (u = userlist_find(s, uid)) )
						userlist_remove(s, u);

					if (!xstrncmp(jabber_attr(item->atts, "subscription"), "remove", 6)) {
						/* nic nie robimy, bo juz usuniete */
					} else {
						char *nickname 	= jabber_unescape(jabber_attr(item->atts, "name"));
						xmlnode_t *group = xmlnode_find_child(item,"group");
						/* czemu sluzy dodanie usera z nickname uid jesli nie ma nickname ? */
						u = userlist_add(s, uid, nickname ? nickname : uid); 

						if (jabber_attr(item->atts, "subscription"))
							u->authtype = xstrdup(jabber_attr(item->atts, "subscription"));
						
						for (; group ; group = group->next ) {
							char *gname = jabber_unescape(group->data);
							ekg_group_add(u, gname);
							xfree(gname);
						}

						if (jdh->roster_retrieved) {
							command_exec_format(NULL, s, 1, TEXT("/auth --probe %s"), uid);
						}
						xfree(nickname); 
					}
					xfree(uid);
				}; /* for */
				jdh->roster_retrieved = 1;
			} /* jabber:iq:roster */
		} /* if query */
	} /* type == set */

	if (!xstrncmp(type, "get", 3)) {
		xmlnode_t *q;

		if ((q = xmlnode_find_child(n, "query"))) {
			const char *ns = jabber_attr(q->atts, "xmlns");

			if (!xstrncmp(ns, "jabber:iq:version", 17) && id && from) {
				const char *ver_os;
				const char *tmp;

				CHAR_T *escaped_client_name	= jabber_escape( jabberfix((tmp = session_get(s, "ver_client_name")), DEFAULT_CLIENT_NAME) );
				CHAR_T *escaped_client_version	= jabber_escape( jabberfix((tmp = session_get(s, "ver_client_version")), VERSION) );
				CHAR_T *osversion;

				if (!(ver_os = session_get(s, "ver_os"))) {
					struct utsname buf;

					if (uname(&buf) != -1) {
						char *osver = saprintf("%s %s %s", buf.sysname, buf.release, buf.machine);
						osversion = jabber_escape(osver);
						xfree(osver);
					} else {
						osversion = xwcsdup(TEXT("unknown")); /* uname failed and not ver_os session variable */
					}
				} else {
					osversion = jabber_escape(ver_os);	/* ver_os session variable */
				}

				jabber_write(j, "<iq to=\"%s\" type=\"result\" id=\"%s\">" 
						"<query xmlns=\"jabber:iq:version\">"
						"<name>"CHARF"/name>"
						"<version>"CHARF"/version>"
						"<os>"CHARF"</os></query></iq>", 
						from, id, 
						escaped_client_name, escaped_client_version, osversion);

				xfree(escaped_client_name);
				xfree(escaped_client_version);
				xfree(osversion);
			} /* jabber:iq:version */
		} /* if query */
	} /* type == get */
} /* iq */

void jabber_handle_presence(xmlnode_t *n, session_t *s) {
	const char *from = jabber_attr(n->atts, "from");
	const char *type = jabber_attr(n->atts, "type");
	char *jid, *uid;
	xmlnode_t *q;
	int ismuc = 0;

	int na = !xstrcmp(type, "unavailable");

	jid = jabber_unescape(from);
	uid = saprintf("jid:%s", jid);
	xfree(jid);

	if (from && !xstrcmp(type, "subscribe")) {
		print("jabber_auth_subscribe", uid, session_name(s));
		xfree(uid);
		return;
	}

	if (from && !xstrcmp(type, "unsubscribe")) {
		print("jabber_auth_unsubscribe", uid, session_name(s));
		xfree(uid);
		return;
	}

	for (q = n->children; q; q = q->next) {
		char *tmp	= xstrrchr(uid, '/');
		char *mucuid	= xstrndup(uid, tmp ? tmp - uid : xstrlen(uid));
		if (!xstrcmp(q->name, "x") && !xstrcmp(jabber_attr(q->atts, "xmlns"), "http://jabber.org/protocol/muc#user")) {
			xmlnode_t *child;

			for (child = q->children; child; child = child->next) {
				if (!xstrcmp(child->name, "item")) { /* lista userow */
					char *jid	  = jabber_unescape(jabber_attr(child->atts, "jid"));		/* jid */
					char *role	  = jabber_unescape(jabber_attr(child->atts, "role"));		/* ? */
					char *affiliation = jabber_unescape(jabber_attr(child->atts, "affiliation"));	/* ? */

					char *uid; 
					char *tmp;

					window_t *w;
					userlist_t *ulist;
	
					if (!(w = window_find_s(s, mucuid))) /* co robimy jak okno == NULL ? */
						w = window_new(mucuid, s, 0); /* tworzymy ? */
					uid = saprintf("jid:%s", jid);

					if (!(ulist = userlist_find_u(&(w->userlist), uid)))
						ulist = userlist_add_u(&(w->userlist), uid, jid);

					if (ulist && na) {
							userlist_remove_u(&(w->userlist), ulist);
							ulist = NULL;
					}
					if (ulist) {
						tmp = ulist->status;
						ulist->status = xstrdup(EKG_STATUS_AVAIL);
						xfree(tmp);

					}
					xfree(uid);

					xfree(jid); xfree(role); xfree(affiliation);
				} else { /* debug pursuit only */
					char *s = saprintf("\tMUC: %s", child->name);
					print("generic", s);
					xfree(s);
				}
			}
			ismuc = 1;
		}
		xfree(mucuid);
	}
	if (!ismuc && (!type || ( na || !xstrcmp(type, "error") || !xstrcmp(type, "available")))) {
		xmlnode_t *nshow, *nstatus, *nerr;
		char *status = NULL, *descr = NULL;
		char *jstatus = NULL;
		char *tmp2;

		if ((nshow = xmlnode_find_child(n, "show"))) {	/* typ */
			jstatus = jabber_unescape(nshow->data);
			if (!xstrcmp(jstatus, "na") || na) {
				status = xstrdup(EKG_STATUS_NA);
			}
		} else {
			if (na)
				status = xstrdup(EKG_STATUS_NA);
			else	status = xstrdup(EKG_STATUS_AVAIL);
		}

		if ((nerr = xmlnode_find_child(n, "error"))) { /* bledny */
			char *ecode = jabber_attr(nerr->atts, "code");
			char *etext = jabber_unescape(nerr->data);
			xfree(status);
			status = xstrdup(EKG_STATUS_ERROR);
			descr = saprintf("(%s) %s", ecode, etext);
			xfree(etext);
		} 
		if ((nstatus = xmlnode_find_child(n, "status"))) { /* opisowy */
			xfree(descr);
			descr = jabber_unescape(nstatus->data);
		}

		if ((tmp2 = xstrchr(uid, '/'))) {
			char *tmp = xstrndup(uid, tmp2-uid);
			userlist_t *ut;
			if ((ut = userlist_find(s, tmp)))
				ut->resource = xstrdup(tmp2+1);
			xfree(tmp);
		}
		if (status) {
			xfree(jstatus);
		} else if (jstatus && (!xstrcasecmp(jstatus, EKG_STATUS_AWAY)		|| !xstrcasecmp(jstatus, EKG_STATUS_INVISIBLE)	||
					!xstrcasecmp(jstatus, EKG_STATUS_XA)		|| !xstrcasecmp(jstatus, EKG_STATUS_DND) 	|| 
					!xstrcasecmp(jstatus, EKG_STATUS_FREE_FOR_CHAT) || !xstrcasecmp(jstatus, EKG_STATUS_BLOCKED))) {
			status = jstatus;
		} else {
			debug("[jabber] Unknown presence: %s from %s. Please report!\n", jstatus, uid);
			xfree(jstatus);
			status = xstrdup(EKG_STATUS_AVAIL);
		}
		
		{
			char *session 	= xstrdup(session_uid_get(s));
			time_t when 	= jabber_try_xdelay(q, NULL);
			char *host 	= NULL;
			int port 	= 0;

			query_emit(NULL, "protocol-status", &session, &uid, &status, &descr, &host, &port, &when, NULL);
			
			xfree(session);
/*			xfree(host); */
		}
		xfree(status);
		xfree(descr);
	}
	xfree(uid);
} /* <presence> */

time_t jabber_try_xdelay(xmlnode_t *xitem, const char *ns_)
{
        if (xitem) {
		const char *ns, *stamp;

		ns = ns_ ? ns_ : jabber_attr(xitem->atts, "xmlns");
		stamp = jabber_attr(xitem->atts, "stamp");

		if (stamp && !xstrncmp(ns, "jabber:x:delay", 14)) {
	        	struct tm tm;
        	        memset(&tm, 0, sizeof(tm));
                	sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d",
	                        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                        	&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        	        tm.tm_year -= 1900;
                	tm.tm_mon -= 1;
	                return mktime(&tm);
		}
        }
	return time(NULL);

}

void jabber_handle_disconnect(session_t *s, const char *reason, int type)
{
        jabber_private_t *j = jabber_private(s);

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
	{
		char *__session = xstrdup(session_uid_get(s));
		char *__reason = xstrdup(reason);
		
		query_emit(NULL, "protocol-disconnected", &__session, &__reason, &type, NULL);

		xfree(__session);
		xfree(__reason);
	}

}

static void jabber_handle_start(void *data, const char *name, const char **atts)
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        jabber_private_t *j = session_private_get(jdh->session);
        session_t *s = jdh->session;

        if (!xstrcmp(name, "stream:stream")) {
		CHAR_T *passwd		= jabber_escape(session_get(s, "password"));
                CHAR_T *resource	= jabber_escape(session_get(s, "resource"));
                char *username;
		char *authpass;

		username = xstrdup(s->uid + 4);
		*(xstrchr(username, '@')) = 0;
	
		if (session_get(s, "__new_acount")) {
			jabber_write(j, "<iq type=\"set\" to=\"%s\" id=\"register%d\"><query xmlns=\"jabber:iq:register\"><username>%s</username><password>" CHARF "</password></query></iq>", 
				j->server, j->id++, username, passwd ? passwd : TEXT("foo"));
		}

                if (!resource)
                        resource = xwcsdup(JABBER_DEFAULT_RESOURCE);

                j->stream_id = xstrdup(jabber_attr((char **) atts, "id"));

		authpass = (session_int_get(s, "plaintext_passwd")) ? 
			saprintf("<password>" CHARF "</password>", passwd) :  				/* plaintext */
			saprintf("<digest>%s</digest>", jabber_digest(j->stream_id, passwd));		/* hash */
		jabber_write(j, "<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username>%s<resource>" CHARF"</resource></query></iq>", j->server, username, authpass, resource);
                xfree(username);
		xfree(resource);
		xfree(authpass);
		xfree(passwd);
        } else
		xmlnode_handle_start(data, name, atts);
}

WATCHER(jabber_handle_stream)
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
		/* todo, xfree data ? */
		if (s && session_connected_get(s))  /* hack to avoid reconnecting when we do /disconnect */
			jabber_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK);
                return 0;
        }

        debug("[jabber] jabber_handle_stream()\n");

        if (!(buf = XML_GetBuffer(j->parser, 4096))) {
                print("generic_error", "XML_GetBuffer failed");
                return -1;
        }

#ifdef HAVE_GNUTLS
        if (j->using_ssl && j->ssl_session) {
		len = gnutls_record_recv(j->ssl_session, buf, 4095);

		if ((len == GNUTLS_E_INTERRUPTED) || (len == GNUTLS_E_AGAIN)) {
			// will be called again
			ekg_yield_cpu();
			return 0;
		}

                if (len < 0) {
                        print("generic_error", gnutls_strerror(len));
                        return -1;
                }
        } else
#endif
                if ((len = read(fd, buf, 4095)) < 1) {
                        print("generic_error", strerror(errno));
                        return -1;
                }

        buf[len] = 0;

	debug("[jabber] recv %s\n", buf);

        if (!XML_ParseBuffer(j->parser, len, (len == 0))) {
                print("jabber_xmlerror", session_name(s));
                return -1;
        }

        return 0;
}

TIMER(jabber_ping_timer_handler) {
	session_t *s = session_find((char*) data);
	jabber_private_t *j;

	if (type == 1) {
		xfree(data);
		return 0;
	}

	if (!s || !session_connected_get(s)) {
		return -1;
	}
	
	if ((j = session_private_get(s))) {
		jabber_write(j, "<iq/>"); /* leafnode idea */
	}
	return 0;
}

WATCHER(jabber_handle_connect) /* tymczasowy */
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        int res = 0, res_size = sizeof(res);
	session_t *s = jdh->session;
        jabber_private_t *j = session_private_get(s);
	char *tname;

        debug("[jabber] jabber_handle_connect()\n");

        if (type) {
                return 0;
        }

        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
                jabber_handle_disconnect(s, strerror(res), EKG_DISCONNECT_FAILURE);
		xfree(data);
                return -1;
        }

        watch_add(&jabber_plugin, fd, WATCH_READ, 1, jabber_handle_stream, jdh);

        jabber_write(j, "<?xml version=\"1.0\" encoding=\"utf-8\"?><stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">", j->server);

        j->id = 1;
        j->parser = XML_ParserCreate("UTF-8");
        XML_SetUserData(j->parser, (void*)data);
        XML_SetElementHandler(j->parser, (XML_StartElementHandler) jabber_handle_start, (XML_EndElementHandler) xmlnode_handle_end);
        XML_SetCharacterDataHandler(j->parser, (XML_CharacterDataHandler) xmlnode_handle_cdata);

	tname = saprintf("ping-%s", s->uid+4);
	timer_add(&jabber_plugin, tname, 180, 1, jabber_ping_timer_handler, xstrdup(s->uid));
	xfree(tname);

	return -1;
}

WATCHER(jabber_handle_resolver) /* tymczasowy watcher */
{
	jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
	session_t *s = jdh->session;
	jabber_private_t *j = jabber_private(s);
	struct in_addr a;
	int one = 1, res;
	struct sockaddr_in sin;
	const int port = session_int_get(s, "port");
#ifdef HAVE_GNUTLS
	/* Allow connections to servers that have OpenPGP keys as well. */
	const int cert_type_priority[3] = {GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0};
	const int comp_type_priority[3] = {GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0};
	int ssl_port = session_int_get(s, "ssl_port");
	int use_ssl = session_int_get(s, "use_ssl");
#endif
        if (type) {
                return 0;
	}

        debug("[jabber] jabber_handle_resolver()\n", type);

        if ((res = read(fd, &a, sizeof(a))) != sizeof(a) || (res && a.s_addr == INADDR_NONE /* INADDR_NONE kiedy NXDOMAIN */)) {
                if (res == -1)
                        debug("[jabber] unable to read data from resolver: %s\n", strerror(errno));
                else
                        debug("[jabber] read %d bytes from resolver. not good\n", res);
                close(fd);
                print("conn_failed", format_find("conn_failed_resolving"), session_name(jdh->session));
                /* no point in reconnecting by jabber_handle_disconnect() */
                j->connecting = 0;
                return -1;
        }

        debug("[jabber] resolved to %s\n", inet_ntoa(a));

        close(fd);

        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                debug("[jabber] socket() failed: %s\n", strerror(errno));
                jabber_handle_disconnect(jdh->session, strerror(errno), EKG_DISCONNECT_FAILURE);
                return -1;
        }

        debug("[jabber] socket() = %d\n", fd);

        j->fd = fd;

        if (ioctl(fd, FIONBIO, &one) == -1) {
                debug("[jabber] ioctl() failed: %s\n", strerror(errno));
                jabber_handle_disconnect(jdh->session, strerror(errno), EKG_DISCONNECT_FAILURE);
                return -1;
        }

        /* failure here isn't fatal, don't bother with checking return code */
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = a.s_addr;
#ifdef HAVE_GNUTLS
	j->using_ssl = 0;
        if (use_ssl)
                j->port = ssl_port < 1 ? 5223 : ssl_port;
        else
#endif
		j->port = port < 1 ? 5222 : port;
	sin.sin_port = htons(j->port);

        debug("[jabber] connecting to %s:%d\n", inet_ntoa(sin.sin_addr), j->port);

        res = connect(fd, (struct sockaddr*) &sin, sizeof(sin));

        if (res == -1 && errno != EINPROGRESS) {
                debug("[jabber] connect() failed: %s (errno=%d)\n", strerror(errno), errno);
                jabber_handle_disconnect(jdh->session, strerror(errno), EKG_DISCONNECT_FAILURE);
                return -1;
        }

#ifdef HAVE_GNUTLS
        if (use_ssl) {
		int ret = 0;
                gnutls_certificate_allocate_credentials(&(j->xcred));
                /* XXX - ~/.ekg/certs/server.pem */
                gnutls_certificate_set_x509_trust_file(j->xcred, "brak", GNUTLS_X509_FMT_PEM);

		if ((ret = gnutls_init(&(j->ssl_session), GNUTLS_CLIENT)) ) {
			print("conn_failed_tls");
			jabber_handle_disconnect(jdh->session, gnutls_strerror(ret), EKG_DISCONNECT_FAILURE);
			return -1;
		}

		gnutls_set_default_priority(j->ssl_session);
                gnutls_certificate_type_set_priority(j->ssl_session, cert_type_priority);
                gnutls_credentials_set(j->ssl_session, GNUTLS_CRD_CERTIFICATE, j->xcred);
                gnutls_compression_set_priority(j->ssl_session, comp_type_priority);

                /* we use read/write instead of recv/send */
                gnutls_transport_set_pull_function(j->ssl_session, (gnutls_pull_func)read);
                gnutls_transport_set_push_function(j->ssl_session, (gnutls_push_func)write);
                gnutls_transport_set_ptr(j->ssl_session, (gnutls_transport_ptr)(j->fd));

		watch_add(&jabber_plugin, fd, WATCH_WRITE, 0, jabber_handle_connect_tls, jdh);

		return -1;
        } // use_ssl
#endif
        watch_add(&jabber_plugin, fd, WATCH_WRITE, 0, jabber_handle_connect, jdh);
	return -1;
}

#ifdef HAVE_GNUTLS
WATCHER(jabber_handle_connect_tls) /* tymczasowy */
{
        jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
        jabber_private_t *j = session_private_get(jdh->session);
	int ret;

	if (type) {
		return 0;
	}

	ret = gnutls_handshake(j->ssl_session);

	if ((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN)) {

		watch_add(&jabber_plugin, (int) gnutls_transport_get_ptr(j->ssl_session),
			gnutls_record_get_direction(j->ssl_session) ? WATCH_WRITE : WATCH_READ,
			0, jabber_handle_connect_tls, jdh);

		ekg_yield_cpu();
		return -1;

	} else if (ret < 0) {
		gnutls_deinit(j->ssl_session);
		gnutls_certificate_free_credentials(j->xcred);
		jabber_handle_disconnect(jdh->session, gnutls_strerror(ret), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	// handshake successful
	j->using_ssl = 1;

	watch_add(&jabber_plugin, fd, WATCH_WRITE, 0, jabber_handle_connect, jdh);
	return -1;
}
#endif

QUERY(jabber_status_show_handle)
{
        char *uid	= *(va_arg(ap, char**));
        session_t *s	= session_find(uid);
        jabber_private_t *j = session_private_get(s);
        const char *resource = session_get(s, "resource");
        userlist_t *u;
        char *fulluid;
        char *tmp;

        if (!s || !j)
                return -1;

        fulluid = saprintf("%s/%s", uid, (resource ? resource : JABBER_DEFAULT_RESOURCE));

        // nasz stan
	if ((u = userlist_find(s, uid)) && u->nickname)
		print("show_status_uid_nick", fulluid, u->nickname);
	else
		print("show_status_uid", fulluid);

	xfree(fulluid);

        // nasz status
	tmp = (s->connected) ? 
		format_string(format_find(ekg_status_label(s->status, s->descr, "show_status_")),s->descr, "") :
		format_string(format_find("show_status_notavail"));

	print("show_status_status_simple", tmp);
	xfree(tmp);

        // serwer
#ifdef HAVE_GNUTLS
	print(j->using_ssl ? "show_status_server_tls" : "show_status_server", j->server, itoa(j->port));
#else
	print("show_status_server", j->server, itoa(j->port));
#endif
			
        if (j->connecting)
                print("show_status_connecting");
	
	{
		// kiedy ostatnie polaczenie/rozlaczenie
        	time_t n = time(NULL);
        	struct tm *t = localtime(&n);
		int now_days = t->tm_yday;
		char buf[100];
		const char *format;

		t = localtime(&s->last_conn);
		format = format_find((t->tm_yday == now_days) ? "show_status_last_conn_event_today" : "show_status_last_conn_event");
		if (!strftime(buf, sizeof(buf), format, t) && xstrlen(format)>0)
			xstrcpy(buf, "TOOLONG");

		print( (s->connected) ? "show_status_connected_since" : "show_status_disconnected_since", buf);
	}
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
	format_add("jabber_charset_init_error", _("%! Error initialising charset conversion (%1->%2): %3"), 1);
	format_add("register_change_passwd", _("%> Your password for acount %T%1 is '%T%2%n' change it as fast as you can using command /jid:passwd <newpassword>"), 1);

	/* %1 - session_name, %2 - uid (*_item: %3 - agent uid %4 - description %5 - seq id) */
	format_add("jabber_transport_list_begin", _("%g,+=%G----- Avalible agents on: %T%2%n"), 1);
	format_add("jabber_transport_list_item",  _("%g|| %n %5 - %W%3%n (%4)"), 1);
	format_add("jabber_transport_list_end",   _("%g`+=%G----- End of the agents list%n\n"), 1);
	format_add("jabber_transport_list_nolist", _("%! No agents @ %T%2%n"), 1);

	format_add("jabber_transinfo_begin",	_("%g,+=%G----- Information about: %T%2%n"), 1);
	format_add("jabber_transinfo_identify",	_("%g|| %G --== %g%3 %G==--%n"), 1);
		/* %4 - real fjuczer name  %3 - translated fjuczer name. */
	format_add("jabber_transinfo_feature",	_("%g|| %n %W%2%n feauture: %n%3"), 1);
	format_add("jabber_transinfo_comm_ser",	_("%g|| %n %W%2%n can: %n%3 %2 (%4)"), 1);
	format_add("jabber_transinfo_comm_use",	_("%g|| %n %W%2%n can: %n%3 $uid (%4)"), 1);
	format_add("jabber_transinfo_end",	_("%g`+=%G----- End of the infomations%n\n"), 1);

	format_add("jabber_search_item",	_("%) JID: %T%3%n\n%) Nickname:  %T%4%n\n%) Name: %T%5 %6%n\n%) Email: %T%7%n\n"), 1);	/* like gg-search_results_single */
		/* %3 - jid %4 - nickname %5 - firstname %6 - surname %7 - email */
	format_add("jabber_search_begin",	_("%g,+=%G----- Search on %T%2%n"), 1);
//	format_add("jabber_search_items", 	  "%g||%n %[-24]3 %K|%n %[10]5 %K|%n %[10]6 %K|%n %[12]4 %K|%n %[16]7\n", 1);		/* like gg-search_results_multi. TODO */
	format_add("jabber_search_items",	  "%g||%n %3 - %5 '%4' %6 <%7>", 1);
	format_add("jabber_search_end",		_("%g`+=%G-----"), 1);
	format_add("jabber_search_error",	_("%! Error while searching: %3\n"), 1);

	format_add("jabber_form_title",		  "%g,+=%G----- %3 %n(%T%2%n)", 1);
	format_add("jabber_form_item",		  "%g|| %n%(21)3 (%6) %K|%n --%4 %(20)5", 1); 	/* %3 - label %4 - keyname %5 - value %6 - req; optional */

	format_add("jabber_form_item_val",	  "%K[%b%3%n %g%4%K]%n", 1);			/* %3 - value %4 - label */
	format_add("jabber_form_item_sub",        "%g|| %|%n\t%3", 1);			/* %3 formated jabber_form_item_val */

	format_add("jabber_form_command",	_("%g|| %nType %T/%3 %g%2%n"), 1);
	format_add("jabber_form_instructions", 	  "%g|| %n%|%3", 1);
	format_add("jabber_form_end",		_("%g`+=%G----- End of this %3 form ;)%n"), 1);

	format_add("jabber_registration_item", 	  "%g|| %n            --%3 %4%n", 1); /* %3 - keyname %4 - value */ /* XXX, merge */

	format_add("jabber_send_chan", _("%B<%W%2%B>%n %5"), 1);
	format_add("jabber_send_chan_n", _("%B<%W%2%B>%n %5"), 1);

	format_add("jabber_recv_chan", _("%b<%w%2%b>%n %5"), 1);
	format_add("jabber_recv_chan_n", _("%b<%w%2%b>%n %5"), 1);
        return 0;
}

int jabber_plugin_init(int prio)
{
        plugin_register(&jabber_plugin, prio);

        query_connect(&jabber_plugin, "protocol-validate-uid", jabber_validate_uid, NULL);
        query_connect(&jabber_plugin, "plugin-print-version", jabber_print_version, NULL);
        query_connect(&jabber_plugin, "session-added", jabber_session, (void*) 1);
        query_connect(&jabber_plugin, "session-removed", jabber_session, (void*) 0);
        query_connect(&jabber_plugin, "status-show", jabber_status_show_handle, NULL);
	query_connect(&jabber_plugin, "ui-window-kill", jabber_window_kill, NULL);

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
        plugin_var_add(&jabber_plugin, "port", VAR_INT, "5222", 0, NULL);
        plugin_var_add(&jabber_plugin, "priority", VAR_INT, "5", 0, NULL);
        plugin_var_add(&jabber_plugin, "resource", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "server", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "ssl_port", VAR_INT, "5223", 0, NULL);
        plugin_var_add(&jabber_plugin, "show_typing_notify", VAR_INT, "1", 0, NULL);
        plugin_var_add(&jabber_plugin, "use_ssl", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "ver_client_name", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "ver_client_version", VAR_STR, 0, 0, NULL);
        plugin_var_add(&jabber_plugin, "ver_os", VAR_STR, 0, 0, NULL);
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
 * vim: sts=8 sw=8
 */
