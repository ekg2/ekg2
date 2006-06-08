/* $Id$ */

/*
 *  (C) Copyright 2003-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Tomasz Torcz <zdzichu@irc.pl>
 *                          Leszek Krupiñski <leafnode@pld-linux.org>
 *                          Piotr Paw³ow and other libtlen developers (http://libtlen.sourceforge.net/index.php?theme=teary&page=authors)
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
#define WITH_JABBER_DCC 0
#define WITH_JABBER_JINGLE 0

#define JABBER_DEFAULT_DCC_PORT 6000	/* XXX */

int jabber_dcc_port = 0;
char *jabber_dcc_ip = NULL;

static int jabber_theme_init();
PLUGIN_DEFINE(jabber, PLUGIN_PROTOCOL, jabber_theme_init);

#ifdef EKG2_WIN32_SHARED_LIB
	EKG2_WIN32_SHARED_LIB_HELPER
#endif

/*
 * jabber_private_destroy()
 *
 * inicjuje jabber_private_t danej sesji.
 */
static void jabber_private_init(session_t *s)
{
        const char *uid = session_uid_get(s);
        jabber_private_t *j = s->priv;

	if ((xstrncasecmp(uid, "tlen:", 5) && xstrncasecmp(uid, "jid:", 4)) || j)
		return;

        if (session_private_get(s))
                return;

        j = xmalloc(sizeof(jabber_private_t));
        j->fd = -1;

	j->istlen = !xstrncasecmp(uid, "tlen:", 5);

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

	if ((xstrncasecmp(uid, "tlen:", 5) && xstrncasecmp(uid, "jid:", 4)) || !j)
                return;

        xfree(j->server);
	xfree(j->resource);

        if (j->parser)
                XML_ParserFree(j->parser);
	jabber_bookmarks_free(j);

        xfree(j);

        session_private_set(s, NULL);
}

#if WITH_JABBER_DCC

WATCHER(jabber_dcc_handle_recv) {
	dcc_t *d = data;
	jabber_dcc_t *p;
	session_t *s;
	jabber_private_t *j;

/*	debug("jabber_dcc_handle_recv() data = %x type = %d\n", data, type); */
	if (type) {
		/* XXX, close DCC */
		return 0;
	}

	if (!d || !(p = d->priv)) return -1;
	if (!(s = p->session) || !(j = jabber_private(s))) return -1;

	switch (p->protocol) {
		case (JABBER_DCC_PROTOCOL_BYTESTREAMS): {
			jabber_dcc_bytestream_t *b = p->private.bytestream;
			char buf[16384];	/* dla data transfer */
			int len;

			if (b->validate != JABBER_DCC_PROTOCOL_BYTESTREAMS) return -1;	/* someone is doing mess with memory ? */

			if (b->step != SOCKS5_DATA) {
				char buf[200];		/* dla SOCKS5 */

				len = read(fd, &buf, sizeof(buf)-1);
				if (len == 0) { close(fd); return -1; }
				buf[len] = 0;

				if (buf[0] != 5) { debug("SOCKS5: protocol mishmash\n"); return -1; }
				if (buf[1] != 0) { debug("SOCKS5: reply error: %x\n", buf[1]); return -1; }

				switch (b->step) {
					case (SOCKS5_CONNECT): {
						char *ouruid;
						char req[47];
						char *digest;
						int i;

						req[0] = 0x05;	/* version */
						req[1] = 0x01;	/* req connect */
						req[2] = 0x00;	/* RSV */
						req[3] = 0x03;	/* ATYPE: 0x01-ipv4 0x03-DOMAINNAME 0x04-ipv6 */
						req[4] = 40;	/* length of hash. */

						/* generate SHA1 hash */
						ouruid = saprintf("%s/" CHARF, s->uid+4, j->resource);
						digest = jabber_dcc_digest(p->sid, d->uid+4, ouruid);
						for (i=0; i < 40; i++) req[5+i] = digest[i];
						xfree(ouruid);

						req[45] = 0x00;	req[46] = 0x00; /* port = 0 */

						write(fd, &req, sizeof(req));
						b->step = SOCKS5_AUTH;
						return 0;
					}
					case (SOCKS5_AUTH): {
						jabber_write(p->session, 
							"<iq type=\"result\" to=\"%s\" id=\"%s\">"
							"<query xmlns=\"http://jabber.org/protocol/bytestreams\">"
							"<streamhost-used jid=\"%s\"/>"
							"</query></iq>", d->uid+4, p->req, b->streamhost->jid);

						b->step = SOCKS5_DATA;
						d->active = 1;
						return 0;
					}
				default:
					debug("SOCKS5: UNKNOWN STATE: %x\n", b->step);
					close(fd);
					return -1;
				}
			}
			
			len = read(fd, &buf, sizeof(buf)-1);
			if (len == 0) { close(fd); return -1; }
			buf[len] = 0;

			/* XXX, write to file @ d->offset */
			FILE *f = fopen("temp.dump", d->offset == 0 ? "w" : "a");
			fseek(f, d->offset, SEEK_SET);
			fwrite(&buf, len, 1, f);
			fclose(f);

			d->offset += len;
			if (d->offset == d->size) {
				print("dcc_done_get", format_user(p->session, d->uid), d->filename);
				dcc_close(d);
				close(fd);
				return -1;
			}

			break;
		}
		default:
			debug("jabber_dcc_handle_recv() UNIMPLEMENTED PROTOTYPE: %x\n", p->protocol);
	}
	return 0;
}

WATCHER(jabber_dcc_handle_send) {  /* XXX, try merge with jabber_dcc_handle_recv() */
	dcc_t *d = data;

	char buf[16384];
	int flen, len;

	if (!d) return -1;

	if (type) {
		return -1;
	}

	if (!d->active) return 0; /* we must wait untill stream will be accepted... BLAH! awful */
	debug("jabber_dcc_handle_send() ready: %d type: %d fd: %d filename: %s --- data = 0x%x\n", d->active, type, fd, d->filename, data);


	flen = sizeof(buf);

	if (d->offset + flen > d->size) flen = d->size - d->offset;

/*
	FILE *f = fopen(d->filename, "r");
        fseek(f, d->offset, SEEK_SET); 
	flen = fread((char *) &buf, 16384, 1, f); 
*/

	len = write(fd, (char *) &buf, flen);

	debug("jabber_dcc_handle_send() write(): %d offset: %d ", len, d->offset);

	d->offset += len;
	debug("rest: %d\n", d->size-d->offset);

	if (d->offset /* == */ >= d->size) {
		close(fd);
		return -1;
	}

	return 0;
}

WATCHER(jabber_dcc_handle_accepted) { /* XXX, try merge with jabber_dcc_handle_recv() */
	char buf[200];
	int len;

	if (type) {
		return -1;
	}
	
	len = read(fd, &buf, sizeof(buf)-1);

	if (len < 1) return -1;
	buf[len] = 0;
	debug("jabber_dcc_handle_accepted() read: %d bytes data: %s\n", len, buf);

	if (buf[0] != 0x05) { debug("SOCKS5: protocol mishmash\n"); return -1; }

	if (buf[1] == 0x02 /* SOCKS5 AUTH */) {
		char req[2];
		req[0] = 0x05;
		req[1] = 0x00;
		write(fd, (char *) &req, sizeof(req));
	}

	if (buf[1] == 0x01 /* REQ CONNECT */ && buf[2] == 0x00 /* RSVD */ && buf[3] == 0x03 /* DOMAINNAME */ && len == 47 /* plen == 47 */ ) {
		char *sha1 = &buf[5];
		char req[47];

		dcc_t *d = NULL;
		list_t l;
		int i;

		for (l=dccs; l; l = l->next) {
			dcc_t *D = l->data;
			jabber_dcc_t *p = D->priv;
			char *this_sha1;

			if (xstrncmp(D->uid, "jid:", 4)) continue; /* we skip not jabber dccs */
			if (p->protocol != JABBER_DCC_PROTOCOL_BYTESTREAMS) continue; /* prottype must be JABBER_DCC_PROTOCOL_BYTESTREAMS */

			{
				list_t l;
				for (l = sessions; l; l = l->data) {
					session_t *s = l->data;
					jabber_private_t *j = s->priv;
					char *fulluid;

					if (!s->connected) continue;
					if (!(session_check(s, 1, "jid"))) continue;

					fulluid = saprintf("%s/" CHARF, s->uid+4, j->resource);

					/* XXX, take care about initiator && we		*/
					/* D->type == DCC_SEND initiator -- we 		*/
					/*            DCC_GET  initiator -- D->uid+4	*/
					this_sha1 = jabber_dcc_digest(p->sid, fulluid, D->uid+4);

					debug("[JABBER_DCC_ACCEPTED] SHA1: %s THIS: %s (session: %s)\n", sha1, this_sha1, fulluid);
					if (!xstrcmp(sha1, this_sha1)) {
						d = D;
						break;
					}
					xfree(fulluid);
				}
			}
		}

		if (!d) {
			debug("[JABBER_DCC_ACCEPTED] SHA1 HASH NOT FOUND: %s\n", sha1);
			/* XXX */
			return -1;
		}

	/* HEADER: */
		req[0] = 0x05; /* SOCKS5 */
		req[1] = 0x00; /* SUCC */
		req[2] = 0x00; /* RSVD */
		req[3] = 0x03; /* DOMAINNAME */
		req[4] = 40;   /* length of hash */
	/* SHA1 HASH: */
		for (i=0; i < 40; i++) req[5+i] = sha1[i];
	/* PORT: */
		req[45] = 0x00; 
		req[46] = 0x00;
	/* LET'S SEND IWIL (OK, AUTH NOT IWIL) PACKET: */
		write(fd, (char *) &req, sizeof(req));

		watch_add(&jabber_plugin, fd, WATCH_WRITE, 1, jabber_dcc_handle_send, d);
		return -1;
	}
	return 0;
}

WATCHER(jabber_dcc_handle_accept) {
	struct sockaddr_in sin;
	int newfd, sin_len = sizeof(sin);

	if (type) {
		return -1;
	}

	if ((newfd = accept(fd, (struct sockaddr *) &sin, &sin_len)) == -1) {
		debug("jabber_dcc_handle_accept() accept() FAILED (%s)\n", strerror(errno));
		return -1;
	}

	debug("jabber_dcc_handle_accept() accept() fd: %d\n", newfd);
	watch_add(&jabber_plugin, newfd, WATCH_READ, 1, jabber_dcc_handle_accepted, NULL);
	return 0;
}

/* zwraca strukture z fd / infem XXX */
int jabber_dcc_init(int port) {
	struct sockaddr_in sin;
	int fd;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("jabber_dcc_init() socket() FAILED (%s)\n", strerror(errno));
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	while (bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))) {
		debug("jabber_dcc_init() bind() port: %d FAILED (%s)\n", port, strerror(errno));
		port++;
		if (port > 65535) return -1;

		sin.sin_port = htons(port);
	}
	if (listen(fd, 10)) {
		debug("jabber_dcc_init() listen() FAILED (%s)\n", strerror(errno));
		return -1;
	}
	debug("jabber_dcc_init() SUCCESSED fd:%d port:%d\n", fd, port);

	watch_add(&jabber_plugin, fd, WATCH_READ, 1, jabber_dcc_handle_accept, NULL);

	jabber_dcc_port = port;
	return fd;
}

QUERY(jabber_dcc_postinit) {
#if WITH_JABBER_DCC
	jabber_dcc_init(JABBER_DEFAULT_DCC_PORT); /* XXX */
#endif
	return 0;
}

void jabber_dcc_close_handler(struct dcc_s *d) {
	jabber_dcc_t *p = d->priv;
	if (!d->active && d->type == DCC_GET) {
		session_t *s = p->session;
		jabber_private_t *j;
		
		if (!s || !(j= session_private_get(s))) return;

		watch_write(j->send_watch, "<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"403\">Declined</error></iq>", 
			d->uid+4, p->req);
	}
	if (p) {
		xfree(p->req);
		xfree(p->sid);
		xfree(p);
	}
	/* XXX, free ALL data */
	d->priv = NULL;
}

dcc_t *jabber_dcc_find(const char *uin, /* without jid: */ const char *id, const char *sid) {
#define DCC_RULE(x) (!xstrncmp(x->uid, "jid:", 4) && !xstrcmp(x->uid+4, uin))
	list_t l;
	if (!id && !sid) { debug("jabber_dcc_find() neither id nor sid passed.. Returning NULL\n"); return NULL; }

	for (l = dccs; l; l = l->next) {
		dcc_t *d = l->data;
		jabber_dcc_t *p = d->priv;

		if (DCC_RULE(d) && (!sid || !xstrcmp(p->sid, sid)) && (!id || !xstrcmp(p->req, id))) {
			debug("jabber_dcc_find() %s sid: %s id: %s founded: 0x%x\n", uin, sid, id, d);
			return d;
		}
	}
	debug("jabber_dcc_find() %s %s not founded. Possible abuse attempt?!\n", uin, sid);
	return NULL;
}
#endif

/* destory all previously saved bookmarks... we DON'T DELETE LIST on jabberd server... only list saved @ j->bookamarks */
int jabber_bookmarks_free(jabber_private_t *j) {
	list_t l;
	if (!j || !j->bookmarks) return -1;

	for (l = j->bookmarks; l; l = l->next) {
		jabber_bookmark_t *book = l->data;
		if (!book) continue;

		if (book->type == JABBER_BOOKMARK_URL) { xfree(book->private.url->name); xfree(book->private.url->url); }
		else if (book->type == JABBER_BOOKMARK_CONFERENCE) { 
			xfree(book->private.conf->name); xfree(book->private.conf->jid);
			xfree(book->private.conf->nick); xfree(book->private.conf->pass);
		}
		xfree(book->private.other);
		xfree(book);
		l->data = NULL;
	}
	list_destroy(j->bookmarks, 0); 
	j->bookmarks = NULL;
	return 0;
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

	if (!xstrncasecmp(uid, "tlen:", 5)) {
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
		watch_write(j->send_watch, "<presence to=\"%s/%s\" type=\"unavailable\">%s</presence>", w->target+4, "darkjames" /* XXX */, status ? status : "");

	return 0;
}

int jabber_write_status(session_t *s)
{
        jabber_private_t *j = session_private_get(s);
        int priority = session_int_get(s, "priority");
        const char *status;
        CHAR_T *descr;
	char *real = NULL;

#if WITH_JABBER_JINGLE
/* This is only to enable 'call' button in GTalk .... */
# define JINGLE_CAPS "<c xmlns=\"http://jabber.org/protocol/caps\" ext=\"voice-v1\" ver=\"0.1\" node=\"http://www.google.com/xmpp/client/caps\"/>"
#else 
# define JINGLE_CAPS ""
#endif

        if (!s || !j)
                return -1;

        if (!session_connected_get(s))
                return 0;

        status = session_status_get(s);
	if ((descr = jabber_escape(session_descr_get(s)))) {
		real = saprintf("<status>" CHARF "</status>", descr);
		xfree(descr);
	}

	if (!xstrcmp(status, EKG_STATUS_AVAIL))
		watch_write(j->send_watch, "<presence>%s<priority>%d</priority>%s</presence>", real ? real : "", priority, JINGLE_CAPS);
	else if (!xstrcmp(status, EKG_STATUS_INVISIBLE))
		watch_write(j->send_watch, "<presence type=\"invisible\">%s<priority>%d</priority></presence>", real ? real : "", priority);
	else
		watch_write(j->send_watch, "<presence><show>%s</show>%s<priority>%d</priority>%s</presence>", status, real ? real : "", priority, JINGLE_CAPS);
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
	char *uid;
	
	if (j->istlen)	uid = saprintf("tlen:%s", juid);
	else		uid = saprintf("jid:%s", juid);

	string_t body;
	time_t sent = 0;

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

	if ((nsubject = xmlnode_find_child(n, "subject")) && nsubject->data) {
		string_append(body, "Subject: ");
		string_append(body, nsubject->data);
		string_append(body, "\n\n");
	}

	if (nbody)
		string_append(body, nbody->data);

	for (xitem = n->children; xitem; xitem = xitem->next) {
		if (!xstrcmp(xitem->name, "x")) {
			const char *ns = jabber_attr(xitem->atts, "xmlns");
			
			if (!xstrcmp(ns, "jabber:x:encrypted")) {	/* JEP-0027 */
				/* XXX, encrypted message. It's only limited to OpenPGP */
				string_append(body, "\n\n[EKG2]: Encrypted message:\n");
				string_append(body, "XXX, Sorry, decrypting message not works now :(");
			} else if (!xstrncmp(ns, "jabber:x:event", 14)) {
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

					watch_write(j->send_watch, "<message to=\"%s\"><x xmlns=\"jabber:x:event\">", from);

					if (!xstrcmp(our_status, EKG_STATUS_INVISIBLE)) {
						watch_write(j->send_watch, "<offline/>");
					} else {
						if (acktype & 1)
							watch_write(j->send_watch, "<delivered/>");
						if (acktype & 2)
							watch_write(j->send_watch, "<displayed/>");
					};
					watch_write(j->send_watch, "<id>%s</id></x></message>", id);
				}
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
/* jabber:x:event */	} else if (!xstrncmp(ns, "jabber:x:oob", 12)) {
				xmlnode_t *xurl;
				xmlnode_t *xdesc;

				if ( ( xurl = xmlnode_find_child(xitem, "url") ) ) {
					string_append(body, "\n\n");
					string_append(body, "URL: ");
					string_append(body, xurl->data);
					if ((xdesc = xmlnode_find_child(xitem, "desc"))) {
						string_append(body, "(");
						string_append(body, xdesc->data);
						string_append(body, ")");
					}
					string_append(body, "\n");
				}
/* jabber:x:oob */	} else if (!xstrncmp(ns, "jabber:x:delay", 14)) {
				sent = jabber_try_xdelay(jabber_attr(xitem->atts, "stamp"));
			} else debug("[JABBER, MESSAGE]: <x xmlns=%s\n", ns);
		}	/* <x> */
	}
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

		if (!sent) sent = time(NULL);

		debug("[jabber,message] type = %s\n", type);
		if (!xstrcmp(type, "groupchat")) {
			char *tuid = xstrrchr(uid, '/');
			char *uid2 = (tuid) ? xstrndup(uid, tuid-uid) : xstrdup(uid);
			char *nick = (tuid) ? xstrdup(tuid+1) : NULL;
			char *formatted;
			/* w muc po resource jest nickname (?) always? */

			class	|= EKG_NO_THEMEBIT;
			ekgbeep	= EKG_NO_BEEP;

			formatted = format_string(format_find("jabber_muc"), session_name(s), uid2, nick ? nick : uid2+4, text);
			
			debug("[MUC,MESSAGE] uid2:%s uuid:%s message:%s\n", uid2, nick, text);
			query_emit(NULL, "protocol-message", &me, &uid, &rcpts, &formatted, &format, &sent, &class, &seq, &ekgbeep, &secure);

			xfree(uid2);
			xfree(nick);
			xfree(formatted);
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
void jabber_handle_xmldata_form(session_t *s, const char *uid, const char *command, xmlnode_t *form, const char *param) { /* JEP-0004: Data Forms */
	xmlnode_t *node;
	int fieldcount = 0;
/*	const char *FORM_TYPE = NULL; */

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
			
			if (!fieldcount) print("jabber_form_command", session_name(s), uid, command, param);

			for (child = node->children; child; child = child->next) {
				if (!xstrcmp(child->name, "required")) isreq = 1;
				else if (!xstrcmp(child->name, "value")) {
/* 
					if (!xstrcmp(var, "FORM_TYPE")) {
						FORM_TYPE = xstrdup(var); 
						if (!xstrcmp(FORM_TYPE, "http://jabber.org/protocol/rc")) {
							print("jabber_form_command_ext", session_name(s), uid, "control", 
						}
						goto next; 
					}
*/
					xfree(def_option); 
					def_option	= jabber_unescape(child->data); 
				} 
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
				} else debug("[FIELD->CHILD] %s\n", child->name);
			}

			print("jabber_form_item", session_name(s), uid, label, var, def_option, 
				isreq == -1 ? "X" : isreq == 1 ? "V" : " ");
			if (sub) {
				int len = xstrlen(sub->str);
				if (sub->str[len-1] == '\t' && sub->str[len-2] == '\n') sub->str[len-2] = 0;
				print("jabber_form_item_sub", session_name(s), uid, sub->str);
				string_free(sub, 1);
			}
next:
			fieldcount++;
			xfree(var);
			xfree(label);
		}
	}
	if (!fieldcount) print("jabber_form_command", session_name(s), uid, command);
	print("jabber_form_end", session_name(s), uid, command, param);
}

/* handluje <x xmlns=jabber:x:data type=submit */
int jabber_handle_xmldata_form_submit(session_t *s, xmlnode_t *form, const char *FORM_TYPE, int alloc, ...) {
	char **atts	= NULL;
	int valid	= 0;
	va_list ap;
	char **vatmp;

	va_start(ap, alloc);
	if (alloc) atts = *(va_arg(ap, char ***));
	while ((vatmp = va_arg(ap, char **))) { debug("JABBER, RC, ADDING ATTR TO ATTS: 0x%x\n", vatmp); } 
	va_end(ap);

	for (; form; form = form->next) {
		if (!xstrcmp(form->name, "field")) {
			const char *vartype	= jabber_attr(form->atts, "type"); 
			const char *varname	= jabber_attr(form->atts, "var");
			char *value		= jabber_unescape(form->children ? form->children->data : NULL);
			char *tmp; 
							
			if (FORM_TYPE && (!xstrcmp(varname, "FORM_TYPE") && !xstrcmp(vartype, "hidden") && !xstrcmp(value, FORM_TYPE))) 
				valid = 1;
			if ((tmp = jabber_attr(atts, varname))) {
				xfree(tmp);
				tmp	= value;
				value	= NULL;
			} else if (alloc) { 
				debug("JABBER, RC, FORM_TYPE: %s ADDING ATTR TO ATTS: %s\n", FORM_TYPE, varname);
			} else 	debug("JABBER, RC, FORM_TYPE: %s ATTR NOT IN ATTS: %s (SOMEONE IS DOING MESS WITH FORM_TYPE?)\n", FORM_TYPE, varname);
		}
	}
	return valid;
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

	if (!xstrcmp(type, "error")) {
		xmlnode_t *e = xmlnode_find_child(n, "error");
		char *reason = (e) ? jabber_unescape(e->data) : NULL;

		if (!xstrncmp(id, "register", 8)) {
			print("register_failed", jabberfix(reason, "?"));
		} else if (!xstrcmp(id, "auth")) {
			if (reason) {
				print("conn_failed", reason, session_name(s));
			} else {
				print("jabber_generic_conn_failed", session_name(s));
			}
			j->connecting = 0;
		} else if (!xstrncmp(id, "passwd", 6)) {
			print("passwd_failed", jabberfix(reason, "?"));
			session_set(s, "__new_password", NULL);
		} else if (!xstrncmp(id, "search", 6)) {
			debug("[JABBER] search failed: %s\n", reason);
		}
#if WITH_JABBER_DCC
		else if (!xstrncmp(id, "offer", 5)) {
			char *uin = jabber_unescape(from);
			if (dcc_close(jabber_dcc_find(uin, id, NULL))) {
				/* XXX, possible abuse attempt */
			}
			xfree(uin);
		}
#endif
		else debug("[JABBER] GENERIC IQ ERROR: %s\n", reason);

		xfree(reason);
		return;			/* we don't need to go down */
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
			watch_write(j->send_watch, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
			jabber_write_status(s);

			if (session_int_get(s, "auto_bookmark_sync") != 0) command_exec(NULL, s, TEXT("/jid:bookmark --get"), 1);
		} 
	}

	if (!xstrncmp(id, "passwd", 6)) {
		if (!xstrcmp(type, "result")) {
			char *new_passwd = (char *) session_get(s, "__new_password");

			session_set(s, "password", new_passwd);
			session_set(s, "__new_password", NULL);
			wcs_print("passwd");
		} 
		session_set(s, "__new_password", NULL);
	}

#if WITH_JABBER_DCC
	if ((q = xmlnode_find_child(n, "si"))) { /* JEP-0095: Stream Initiation */
/* dj, I think we don't need to unescape rows (tags) when there should be int, do we?  ( <size> <offset> <length>... )*/
		xmlnode_t *p;

		if (!xstrcmp(type, "result")) {
			char *uin = jabber_unescape(from);
			dcc_t *d;

			if ((d = jabber_dcc_find(uin, id, NULL))) {
				xmlnode_t *node;
				jabber_dcc_t *p = d->priv;

				for (node = q->children; node; node = node->next) {
					/* sprawdzic co my mamy w tym fjuczer... itd............... XXX XXX XXX !!!!
						<feature xmlns='http://jabber.org/protocol/feature-neg'>
							<x xmlns='jabber:x:data' type='submit'>
							<field var='stream-method'>
								<value>http://jabber.org/protocol/bytestreams</value>
							</field>
							</x>
						</feature>
					*/
				}
				p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS; /* XXX jw */

				if (p->protocol == JABBER_DCC_PROTOCOL_BYTESTREAMS) {
					struct jabber_streamhost_item streamhost;
					jabber_dcc_bytestream_t *b;
					list_t l;

					b = p->private.bytestream = xmalloc(sizeof(jabber_dcc_bytestream_t));
					b->validate = JABBER_DCC_PROTOCOL_BYTESTREAMS;

					if (jabber_dcc_ip) {
						/* basic streamhost, our ip, default port, our jid. check if we enable it. XXX*/
						streamhost.jid	= saprintf("%s/" CHARF, s->uid+4, j->resource);
						streamhost.ip	= xstrdup(jabber_dcc_ip);
						streamhost.port	= jabber_dcc_port;
						list_add(&(b->streamlist), &streamhost, sizeof(struct jabber_streamhost_item));
					}

/* 		... other, proxy, etc, etc..
					streamhost.ip = ....
					streamhost.port = ....
					list_add(...);
 */

					xfree(p->req);
					p->req = xstrdup(itoa(j->id++));

					watch_write(j->send_watch, "<iq type=\"set\" to=\"%s\" id=\"%s\">"
						"<query xmlns=\"http://jabber.org/protocol/bytestreams\" mode=\"tcp\" sid=\"%s\">", 
						d->uid+4, p->req, p->sid);

					for (l = b->streamlist; l; l = l->next) {
						struct jabber_streamhost_item *item = l->data;
						watch_write(j->send_watch, "<streamhost port=\"%d\" host=\"%s\" jid=\"%s\"/>", item->port, item->ip, item->jid);
					}
					watch_write(j->send_watch, "<fast xmlns=\"http://affinix.com/jabber/stream\"/></query></iq>");

				}
			} else /* XXX */;
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
#endif	/* FILETRANSFER */

	if ((q = xmlnode_find_child(n, "command"))) {
		const char *ns		= jabber_attr(q->atts, "xmlns");
		const char *node	= jabber_attr(q->atts, "node");

		char *uid 	= jabber_unescape(from);

		if (!xstrcmp(type, "result") && !xstrcmp(ns, "http://jabber.org/protocol/commands")) {
			const char *status	= jabber_attr(q->atts, "status");

			if (!xstrcmp(status, "completed")) { 
				print("jabber_remotecontrols_completed", session_name(s), uid, node);
			} else if (!xstrcmp(status, "executing")) {
				const char *set  = node;
				xmlnode_t *child;

				if (!xstrncmp(node, "http://jabber.org/protocol/rc#", 30)) set = node+30;

				for (child = q->children; child; child = child->next) {
					if (	!xstrcmp(child->name, "x") && 
						!xstrcmp(jabber_attr(child->atts, "xmlns"), "jabber:x:data") && 
						!xstrcmp(jabber_attr(child->atts, "type"), "form")) {

						jabber_handle_xmldata_form(s, uid, "control", child->children, node);
					}
				}
			} else debug("[JABBER, UNKNOWN STATUS: %s\n", status);
		} else if (!xstrcmp(type, "set") && !xstrcmp(ns, "http://jabber.org/protocol/commands") && xstrcmp(jabber_attr(q->atts, "action"), "cancel") /* XXX */) {
			int not_handled = 0;
			int not_allowed = 0;
			int is_valid = 0;
			xmlnode_t *x = xmlnode_find_child(q, "x");
			
			int iscancel = !xstrcmp(jabber_attr(q->atts, "action"), "cancel");


			if (x && !xstrcmp(jabber_attr(x->atts, "xmlns"), "jabber:x:data") && !xstrcmp(jabber_attr(x->atts, "type"), "submit"))
				goto rc_invalid;
			
			/* CHECK IF HE CAN DO IT */
			switch (session_int_get(s, "allow_remote_control"))  {
				case 666: break;
				case 1: if (!xstrncmp(from, s->uid+4, xstrlen(s->uid+4)) /* jesli poczatek jida sie zgadza. */
						&& from[xstrlen(s->uid+4)] == '/' /* i jesli na koncu tej osoby jida, gdzie sie konczy nasz zaczyna sie resource... */) 
						break; /* to jest raczej ok */
				case 2:	/* XXX */
				case 0:
				default:
					not_allowed = 1;
			}
			if (not_allowed) goto rc_forbidden;

#define EXECUTING_HEADER(title, instr, FORM_TYPE) \
	if (j->send_watch) j->send_watch->transfer_limit = -1;	/* read plugins.h for more details */ \
	watch_write(j->send_watch,\
		"<iq to=\"%s\" type=\"result\" id=\"%s\">"\
		"<command xmlns=\"http://jabber.org/protocol/commands\" node=\"%s\" status=\"executing\">"\
		"<x xmlns=\"jabber:x:data\" type=\"form\">"\
		"<title>%s</title>"\
		"<instructions>%s</instructions>"\
		"<field var=\"FORM_TYPE\" type=\"hidden\"><value>%s</value></field>", from, id, node, title, instr, FORM_TYPE)

#define EXECUTING_FIELD_BOOL(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"boolean\"><value>%d</value></field>", name, label, value)

#define EXECUTING_FIELD_STR_MULTI(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"text-multi\"><value>%s</value></field>", name, label, value)

#define EXECUTING_FIELD_STR(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"text-single\"><value>%s</value></field>", name, label, value)

#define EXECUTING_FIELD_STR_INT(name, label, value) \
	watch_write(j->send_watch, "<field var=\"%s\" label=\"%s\" type=\"text-single\"><value>%d</value></field>", name, label, value)

#define EXECUTING_SUBOPTION_STR(label, value) \
	watch_write(j->send_watch, "<option label=\"%s\"><value>%s</value></option>")

#define EXECUTING_FOOTER() watch_write(j->send_watch, "</x></command></iq>")

			if (!xstrcmp(node, "http://jabber.org/protocol/rc#set-status")) {
				if (!x) {
					char *status = s->status;
					char *descr = jabber_escape(s->descr);
					/* XXX, w/g http://jabber.org/protocol/rc#set-status nie mamy 'avail' tylko 'online' */

					if (!xstrcmp(status, EKG_STATUS_AUTOAWAY))	status = (s->autoaway ? "away" : "online");
					else if (!xstrcmp(status, EKG_STATUS_AVAIL))	status = "online";

					EXECUTING_HEADER("Set Status", "Choose the status and status message", "http://jabber.org/protocol/rc");
					watch_write(j->send_watch, "<field var=\"status\" label=\"Status\" type=\"list-single\"><required/>");
						EXECUTING_SUBOPTION_STR("Chat", EKG_STATUS_FREE_FOR_CHAT);
						EXECUTING_SUBOPTION_STR("Online", "online");			/* EKG_STATUS_AVAIL */
						EXECUTING_SUBOPTION_STR("Away", EKG_STATUS_AWAY);
						EXECUTING_SUBOPTION_STR("Extended Away", EKG_STATUS_XA);
						EXECUTING_SUBOPTION_STR("Do Not Disturb", EKG_STATUS_DND);
						EXECUTING_SUBOPTION_STR("Invisible", EKG_STATUS_INVISIBLE); 
						EXECUTING_SUBOPTION_STR("Offline", EKG_STATUS_NA);
						watch_write(j->send_watch, "<value>%s</value></field>", status);
					EXECUTING_FIELD_STR_INT("status-priority", "Priority", session_int_get(s, "priority"));
					EXECUTING_FIELD_STR_MULTI("status-message", "Message", descr ? descr : "");
					EXECUTING_FOOTER();
					xfree(descr);
				} else {
					if (!xstrcmp(jabber_attr(x->atts, "xmlns"), "jabber:x:data") && !xstrcmp(jabber_attr(x->atts, "type"), "submit")) {
						char *status	= NULL;
						char *descr	= NULL;
						char *prio	= NULL;
						int priority	= session_int_get(s, "priority");

						is_valid = jabber_handle_xmldata_form_submit(s, x->children, "http://jabber.org/protocol/rc", 0,
								"status-priority", &status, "status-message", &descr, "status-priority", &prio);
						if (is_valid) {
							if (prio) priority = atoi(prio);
							if (!xstrcmp(status, "online")) { xfree(status); status = xstrdup(EKG_STATUS_AVAIL); } 

							if (status)	session_status_set(s, status);
							if (descr)	session_descr_set(s, descr);
							session_int_set(s, "priority", priority);
							print("jabber_remotecontrols_commited_status", session_name(s), uid, status, descr, itoa(priority));
							jabber_write_status(s);
						}
						xfree(status); xfree(descr);
					}
				}
			} else if (!xstrcmp(node, "http://jabber.org/protocol/rc#set-options") || !xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-set-all-options")) {
				int alloptions = !xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-set-all-options");

				if (!x) {
					EXECUTING_HEADER("Set Options", "Set the desired options", alloptions ? "http://ekg2.org/jabber/rc" : "http://jabber.org/protocol/rc");
					if (alloptions) {
						/* send all ekg2 options? *g* :> */
						list_t l;
						for (l = variables; l; l = l->next) {
							variable_t *v = l->data;
							if (v->type == VAR_STR || v->type == VAR_FOREIGN || v->type == VAR_FILE || v->type == VAR_DIR || v->type == VAR_THEME) {
								char *value = jabber_unescape(*(char **) v->ptr);
								EXECUTING_FIELD_STR(v->name, v->name, value ? value : "");
								xfree(value);
							} else if (v->type == VAR_INT || v->type == VAR_BOOL) {
								EXECUTING_FIELD_BOOL(v->name, v->name, itoa(*(int *) v->ptr));
							} else { 	/* XXX */
								continue;
							}
						}
					} else {
						EXECUTING_FIELD_BOOL("sounds", "Play sounds", config_beep);
						EXECUTING_FIELD_BOOL("auto-offline", "Automatically Go Offline when Idle", (s->autoaway || s->status == EKG_STATUS_AUTOAWAY));
						EXECUTING_FIELD_BOOL("auto-msg", "Automatically Open New Messages", 0);		/* XXX */
						EXECUTING_FIELD_BOOL("auto-files", "Automatically Accept File Transfers", 0);	/* XXX */
						EXECUTING_FIELD_BOOL("auto-auth", "Automatically Authorize Contacts", 0);	/* XXX */
					}
					EXECUTING_FOOTER();
				} else {
					if (alloptions) {
						char **atts = NULL;
						is_valid = jabber_handle_xmldata_form_submit(s, x->children, "http://ekg2.org/jabber/rc", 1, &atts, NULL);
						if (is_valid && atts) {
							int i;
							for (i=0; (atts[i] && atts[i+1]); i+=2) {
								variable_set(atts[i], atts[i+1], 0);
							}
						}
						array_free(atts);
					} else { 
						char *sounds 		= NULL;
						char *auto_offline	= NULL;
						char *auto_msg		= NULL;
						char *auto_files	= NULL;
						char *auto_auth		= NULL;
						is_valid = jabber_handle_xmldata_form_submit(s, x->children, "http://jabber.org/protocol/rc", 0, 
							"sounds", &sounds, "auto-offline", &auto_offline, "auto-msg", &auto_msg, "auto-files", &auto_files, 
							"auto-auth", &auto_auth);
						/* parse */
						debug("[JABBER, RC] sounds: %s [AUTO] off: %s msg: %s files: %s auth: %s\n", sounds, 
							auto_offline, auto_msg, auto_files, auto_auth);
						xfree(sounds); xfree(auto_offline); xfree(auto_msg); xfree(auto_files); xfree(auto_auth);
					} 
				}
			} else if (!xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-manage-plugins")) {
				if (!x) {
					EXECUTING_HEADER("Manage ekg2 plugins", "Do what you want :)", "http://ekg2.org/jabber/rc");
					EXECUTING_FOOTER();
				} else {
					not_handled = 1;
				}
			} else if (!xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-manage-sesions")) {
				if (!x) {
					EXECUTING_HEADER("Manage ekg2 sessions", "Do what you want :)", "http://ekg2.org/jabber/rc");
					EXECUTING_FOOTER();
				} else {
					not_handled = 1;
				}
			} else if (!xstrcmp(node, "http://ekg2.org/jabber/rc#ekg-command-execute")) {
				if (!x) {
					list_t l;
					char *session_cur = jabber_escape(session_current->uid);

					EXECUTING_HEADER("EXECUTE COMMAND IN EKG2", "Do what you want, but be carefull", "http://ekg2.org/jabber/rc");
					watch_write(j->send_watch, 
						"<field label=\"Command name\" type=\"list-single\" var=\"command-name\">");	/* required */

					for (l = commands; l; l = l->next) {
						command_t *c = l->data;
						watch_write(j->send_watch, "<option label=\"%s\"><value>%s</value></option>", c->name, c->name);
					}
					watch_write(j->send_watch,
						"<value>help</value></field>"
						"<field label=\"Params:\" type=\"text-multi\" var=\"params-line\"></field>"
						"<field label=\"Session name (empty for current)\" type=\"list-single\" var=\"session-name\">");
	
					for (l = sessions; l; l = l->next) {
						session_t *s = l->data;
						char *alias	= jabber_escape(s->alias);
						char *uid	= jabber_escape(s->uid);
						watch_write(j->send_watch, "<option label=\"%s\"><value>%s</value></option>", alias ? alias : uid, uid);
						xfree(alias); xfree(uid);
					}
					watch_write(j->send_watch, 
						"<value>%s</value></field>"
						"<field label=\"Window name (leave empty for current)\" type=\"list-single\" var=\"window-name\">", session_cur);

					for (l = windows; l; l = l->next) {
						window_t *w = l->data;
						char *target 	= jabber_escape(window_target(w));
						watch_write(j->send_watch, "<option label=\"%s\"><value>%d</value></option>", target ? target : itoa(w->id), w->id);
						xfree(target);
					} 
					watch_write(j->send_watch, "<value>%d</value></field>", window_current->id);
					EXECUTING_FIELD_BOOL("command-quiet", "Quiet mode? (Won't display *results of command handler* on screen)", 0);
					EXECUTING_FOOTER();
					xfree(session_cur);
				} else {
					xmlnode_t *node; 
					char *sessionname	= NULL;
					char *command		= NULL;
					char *params		= NULL;
					char *window		= NULL;
					char *quiet		= 0;

					int windowid = 1, isquiet = 0;

					is_valid = jabber_handle_xmldata_form_submit(s, x->children, "http://ekg2.org/jabber/rc", 0, 
							"command-name", &command, "params-line", &params, "session-name", &sessionname,
							"window-name", &window, "command-quiet", &quiet);
					if (quiet)	isquiet  = atoi(quiet);
					if (window)	windowid = atoi(window);

					if (is_valid) { 
						char *fullcommand = saprintf("/%s %s", command, params ? params : "");
						window_t  *win;
						session_t *ses;
						int ret;

						ses	= session_find(sessionname);
						win	= window_exist(windowid);
	
						if (!ses) ses = session_current;
						if (!win) win = window_current;

						ret = command_exec(win->target, ses, fullcommand, isquiet);
						print("jabber_remotecontrols_commited_command", session_name(s), uid, fullcommand, session_name(ses), win->target, itoa(isquiet));
						xfree(fullcommand);
					}
					xfree(sessionname); xfree(params); xfree(command);
					xfree(window); xfree(quiet);
				}
			} else {
				debug("YOU WANT TO CONTROL ME with unknown node? (%s) Sorry I'm not fortune-teller\n", node);
				not_handled = 1;
			}
rc_invalid:
			if (x && !not_handled) {
				if (is_valid) {
					watch_write(j->send_watch, 
						"<iq to=\"%s\" type=\"result\" id=\"%s\">"
						"<command xmlns=\"http://jabber.org/protocol/commands\" node=\"%s\" status=\"completed\"/>"
						"</iq>", from, id, node);
				} else {
					debug("JABBER, REMOTECONTROL: He didn't send valid FORM_TYPE!!!! He's bu.\n");
					/* error <bad-request/> ? */
				}
			}
rc_forbidden:
			if (not_handled || not_allowed) {
				watch_write(j->send_watch, 
					"<iq to=\"%s\" type=\"error\" id=\"%s\">"
						"<error type=\"cancel\"><%s xmlns=\"urn:ietf:params:xml:ns:xmpp-stanzas\"/></error>"
					"</iq>", from, id,
					not_allowed ? "forbidden" : "item-not-found");
			}
			JABBER_COMMIT_DATA(j->send_watch); /* let's write data to jabber socket */
		}
		xfree(uid);
	}

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
#if WITH_JABBER_JINGLE
			if (!xstrcmp(ns, "session")) {
				/*   id == still the same through the session */
				/* type == [initiate, candidates, terminate] */

			} else 
#endif
			if (!xstrcmp(ns, "jabber:iq:privacy")) { /* RFC 3921 (10th: privacy) */
				xmlnode_t *node;
				int i = 0;

				for (node = q->children; node; node = node->next) {
					if (!xstrcmp(node->name, "list")) {
						if (node->children) {	/* Display all details */
							xmlnode_t *temp;
							i = -1;
							print("jabber_privacy_item_header", session_name(s), j->server, jabber_attr(node->atts, "name"));

							for (temp=node->children; temp; temp = temp->next) {	
								if (!xstrcmp(temp->name, "item")) {
									/* (we should sort it by order! XXX) (server send like it was set) */
									/* a lot TODO */
									char *value = jabber_attr(temp->atts, "value");
									char *type  = jabber_attr(temp->atts, "type");
									char *action = jabber_attr(temp->atts, "action");
									char *jid, *formated;

									if (!xstrcmp(type, "jid")) 		jid = saprintf("jid:%s", value);
									else if (!xstrcmp(type, "group")) 	jid = saprintf("@%s", value);
									else					jid = xstrdup(value);

									formated = format_string(
										format_find(!xstrcmp(action,"allow") ? "jabber_privacy_item_allow" : "jabber_privacy_item_deny"), jid);

									print("jabber_privacy_item", session_name(s), j->server, 
										jabber_attr(temp->atts, "order"), formated, 
										xmlnode_find_child(temp, "message")		? "X" : " ",
										xmlnode_find_child(temp, "presence-in")		? "X" : " ", 
										xmlnode_find_child(temp, "presence-out")	? "X" : " ", 
										xmlnode_find_child(temp, "iq") 			? "X" : " ");

									xfree(formated);
									xfree(jid);
								}
							}
							{	/* just help... */
								char *allowed = format_string(format_find("jabber_privacy_item_allow"), "jid:allowed - JID ALLOWED");
								char *denied  = format_string(format_find("jabber_privacy_item_deny"), "@denied - GROUP DENIED");
								print("jabber_privacy_item_footer", session_name(s), j->server, allowed, denied);
								xfree(allowed); xfree(denied);
							}
						} else { 		/* Display only name */
							/* rekurencja, ask for this item (?) */
							if (!i) print("jabber_privacy_list_begin", session_name(s), j->server);
							if (i != -1) i++;

							print("jabber_privacy_list_item", session_name(s), j->server, itoa(i), jabber_attr(node->atts, "name")); 
						}
					}
				}
				if (i > 0)  print("jabber_privacy_list_end", session_name(s), j->server);
				if (i == 0) print("jabber_privacy_list_noitem", session_name(s), j->server);

			} else if (!xstrcmp(ns, "jabber:iq:private")) {
				xmlnode_t *node;

				for (node = q->children; node; node = node->next) {
					char *lname	= jabber_unescape(node->name);
					char *ns	= jabber_attr(node->atts, "xmlns");
					xmlnode_t *child;

					int config_display = 1;
					int bookmark_display = 1;
					int quiet = 0;

					if (!xstrcmp(node->name, "ekg2")) {
						if (!xstrcmp(ns, "ekg2:prefs") && !xstrncmp(id, "config", 6)) 
							config_display = 0;	/* if it's /jid:config --get (not --display) we don't want to display it */ 
						/* XXX, other */
					}
					if (!xstrcmp(node->name, "storage")) {
						if (!xstrcmp(ns, "storage:bookmarks") && !xstrncmp(id, "config", 6))
							bookmark_display = 0;	/* if it's /jid:bookmark --get (not --display) we don't want to display it (/jid:bookmark --get performed @ connect) */
					}

					if (!config_display || !bookmark_display) quiet = 1;

					if (node->children)	printq("jabber_private_list_header", session_name(s), lname, ns);
					if (!xstrcmp(node->name, "ekg2") && !xstrcmp(ns, "ekg2:prefs")) { 	/* our private struct, containing `full` configuration of ekg2 */
						for (child = node->children; child; child = child->next) {
							char *cname	= jabber_unescape(child->name);
							char *cvalue	= jabber_unescape(child->data);
							if (!xstrcmp(child->name, "plugin") && !xstrcmp(jabber_attr(child->atts, "xmlns"), "ekg2:plugin")) {
								xmlnode_t *temp;
								printq("jabber_private_list_plugin", session_name(s), lname, ns, jabber_attr(child->atts, "name"), jabber_attr(child->atts, "prio"));
								for (temp = child->children; temp; temp = temp->next) {
									char *snname = jabber_unescape(temp->name);
									char *svalue = jabber_unescape(temp->data);
									printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
									xfree(snname);
									xfree(svalue);
								}
							} else if (!xstrcmp(child->name, "session") && !xstrcmp(jabber_attr(child->atts, "xmlns"), "ekg2:session")) {
								xmlnode_t *temp;
								printq("jabber_private_list_session", session_name(s), lname, ns, jabber_attr(child->atts, "uid"));
								for (temp = child->children; temp; temp = temp->next) {
									char *snname = jabber_unescape(temp->name);
									char *svalue = jabber_unescape(temp->data);
									printq("jabber_private_list_subitem", session_name(s), lname, ns, snname, svalue);
									xfree(snname);
									xfree(svalue);
								}
							} else	printq("jabber_private_list_item", session_name(s), lname, ns, cname, cvalue);
							xfree(cname); xfree(cvalue);
						}
					} else if (!xstrcmp(node->name, "storage") && !xstrcmp(ns, "storage:bookmarks")) { /* JEP-0048: Bookmark Storage */
						/* destroy previously-saved list */
						jabber_bookmarks_free(j);

						/* create new-one */
						for (child = node->children; child; child = child->next) {
							jabber_bookmark_t *book = xmalloc(sizeof(jabber_bookmark_t));

							debug("[JABBER:IQ:PRIVATE BOOKMARK item=%s\n", child->name);
							if (!xstrcmp(child->name, "conference")) {
								xmlnode_t *temp;

								book->type	= JABBER_BOOKMARK_CONFERENCE;
								book->private.conf		= xmalloc(sizeof(jabber_bookmark_conference_t));
								book->private.conf->name	= jabber_unescape(jabber_attr(child->atts, "name"));
								book->private.conf->jid		= jabber_unescape(jabber_attr(child->atts, "jid"));
								book->private.conf->autojoin	= !xstrcmp(jabber_attr(child->atts, "autojoin"), "true");

								book->private.conf->nick	= jabber_unescape( (temp = xmlnode_find_child(child, "nick")) ? temp->data : NULL);
								book->private.conf->pass        = jabber_unescape( (temp = xmlnode_find_child(child, "password")) ? temp->data : NULL);

								printq("jabber_bookmark_conf", session_name(s), book->private.conf->name, book->private.conf->jid,
									book->private.conf->autojoin ? "X" : " ", book->private.conf->nick, book->private.conf->pass);

							} else if (!xstrcmp(child->name, "url")) {
								book->type	= JABBER_BOOKMARK_URL;
								book->private.url	= xmalloc(sizeof(jabber_bookmark_url_t));
								book->private.url->name	= jabber_unescape(jabber_attr(child->atts, "name"));
								book->private.url->url	= jabber_unescape(jabber_attr(child->atts, "url"));

								printq("jabber_bookmark_url", session_name(s), book->private.url->name, book->private.url->url);

							} else { debug("[JABBER:IQ:PRIVATE:BOOKMARK UNKNOWNITEM=%s\n", child->name); xfree(book); book = NULL; }

							if (book) list_add(&j->bookmarks, book, 0);
						}
					} else {
						/* DISPLAY IT ? w jakim formacie?
						 * + CHILD item=value item2=value2
						 *  - SUBITEM .....
						 *  - SUBITEM........
						 *  + SUBCHILD ......
						 *   - SUBITEM
						 * ? XXX
						 */
					}
					if (node->children)	printq("jabber_private_list_footer", session_name(s), lname, ns);
					else 			printq("jabber_private_list_empty", session_name(s), lname, ns);
					xfree(lname);
				}
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/vacation")) { /* JEP-0109: Vacation Messages */
				xmlnode_t *temp;

				char *message	= jabber_unescape( (temp = xmlnode_find_child(q, "message")) ? temp->data : NULL);
				char *begin	= (temp = xmlnode_find_child(q, "start")) && temp->data ? temp->data : _("begin");
				char *end	= (temp = xmlnode_find_child(q, "end")) && temp->data  ? temp->data : _("never");

				print("jabber_vacation", session_name(s), message, begin, end);

				xfree(message);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				const char *wezel = jabber_attr(q->atts, "node");
				char *uid = jabber_unescape(from);
				xmlnode_t *node;

				print((wezel) ? "jabber_transinfo_begin_node" : "jabber_transinfo_begin", session_name(s), uid, wezel);

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
						else if (!xstrcmp(var, "http://jabber.org/protocol/disco"))		tvar = "/jid:transpinfo && /jid:transports";
						else if (!xstrcmp(var, "http://jabber.org/protocol/muc"))		tvar = "/jid:mucpart && /jid:mucjoin";
						else if (!xstrcmp(var, "http://jabber.org/protocol/stats"))		tvar = "/jid:stats";
						else if (!xstrcmp(var, "jabber:iq:register"))		    		tvar = "/jid:register";
						else if (!xstrcmp(var, "jabber:iq:search"))				tvar = "/jid:search";

						else if (!xstrcmp(var, "http://jabber.org/protocol/bytestreams")) { user_command = 1; tvar = "/jid:dcc (PROT: BYTESTREAMS)"; }
						else if (!xstrcmp(var, "http://jabber.org/protocol/si/profile/file-transfer")) { user_command = 1; tvar = "/jid:dcc"; }
						else if (!xstrcmp(var, "http://jabber.org/protocol/commands")) { user_command = 1; tvar = "/jid:control"; } 
						else if (!xstrcmp(var, "jabber:iq:version"))	{ user_command = 1;	tvar = "/jid:ver"; }
						else if (!xstrcmp(var, "jabber:iq:last"))	{ user_command = 1;	tvar = "/jid:lastseen"; }
						else if (!xstrcmp(var, "vcard-temp"))		{ user_command = 1;	tvar = "/jid:change && /jid:userinfo"; }
						else if (!xstrcmp(var, "message"))		{ user_command = 1;	tvar = "/jid:msg"; }

						else if (!xstrcmp(var, "http://jabber.org/protocol/vacation"))	{ user_command = 2;	tvar = "/jid:vacation"; }
						else if (!xstrcmp(var, "presence-invisible"))	{ user_command = 2;	tvar = "/invisible"; } /* we ought use jabber:iq:privacy */
						else if (!xstrcmp(var, "jabber:iq:privacy"))	{ user_command = 2;	tvar = "/jid:privacy"; }
						else if (!xstrcmp(var, "jabber:iq:private"))	{ user_command = 2;	tvar = "/jid:private && /jid:config && /jid:bookmark"; }

						if (tvar)	print(	user_command == 2 ? "jabber_transinfo_comm_not" : 
									user_command == 1 ? "jabber_transinfo_comm_use" : "jabber_transinfo_comm_ser", 

									session_name(s), uid, tvar, var);
						else		print("jabber_transinfo_feature", session_name(s), uid, var, var);
					} else if (!xstrcmp(node->name, "x") && !xstrcmp(jabber_attr(node->atts, "xmlns"), "jabber:x:data")) {
/* XXX, ESCAPE IT AND DISPLAY. */
						xmlnode_t *q;
						debug("FORMULARZYK ;)\n");
#if 1
						for (q = node->children; q; q = q->next) {
							if (!xstrcmp(q->name, "field")) {
								xmlnode_t *child = xmlnode_find_child(q, "value");
								/* label: value (var) */
								debug("%s: %s\n", jabber_attr(q->atts, "label"), child && child->data ? child->data : "MMH?");
							}
						}
#endif
					}
				}
				print("jabber_transinfo_end", session_name(s), uid);
				xfree(uid);
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				xmlnode_t *item = xmlnode_find_child(q, "item");
				char *uid = jabber_unescape(from);

				int iscontrol = !xstrncmp(id, "control", 7);

				if (item) {
					int i = 1;
					print(iscontrol ? "jabber_remotecontrols_list_begin" : "jabber_transport_list_begin", session_name(s), uid);
					for (; item; item = item->next, i++) {
						char *sjid  = jabber_unescape(jabber_attr(item->atts, "jid"));
						char *sdesc = jabber_unescape(jabber_attr(item->atts, "name"));
						char *snode = jabber_unescape(jabber_attr(item->atts, "node"));
						
						if (!iscontrol)	print("jabber_transport_list_item", session_name(s), uid, sjid, sdesc, itoa(i));
						else		print("jabber_remotecontrols_list_item",  session_name(s), uid, sjid, snode, sdesc, itoa(i));
						xfree(sdesc);
						xfree(sjid);
						xfree(snode);
					}
					print(iscontrol ? "jabber_remotecontrols_list_end"	: "jabber_transport_list_end", session_name(s), uid);
				} else	print(iscontrol ? "jabber_remotecontrols_list_nolist" : "jabber_transport_list_nolist", session_name(s), uid);
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
								jabber_handle_xmldata_form(s, uid, "search", reg->children, NULL);
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
						jabber_handle_xmldata_form(s, from_str, "register", reg->children, NULL);
						break;
					}
				}
				if (!done) {
					xmlnode_t *instr = xmlnode_find_child(q, "instructions");
					print("jabber_form_title", session_name(s), from_str, from_str);

					if (instr && instr->data) {
						char *instr_str = jabber_unescape(instr->data);
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
#if WITH_JABBER_DCC
			} else if (!xstrcmp(ns, "http://jabber.org/protocol/bytestreams")) { /* JEP-0065: SOCKS5 Bytestreams */
				char *uid = jabber_unescape(from);		/* jid */
				char *sid = jabber_attr(q->atts, "sid");	/* session id */
				char *smode = jabber_attr(q->atts, "mode"); 	/* tcp, udp */
				dcc_t *d = NULL;

				if (!xstrcmp(type, "set") && (d = jabber_dcc_find(uid, NULL, sid)) && d->type == DCC_GET /* XXX */) {
					/* w sumie jak nie mamy nawet tego dcc.. to mozemy kontynuowac ;) */
					/* problem w tym czy user chce ten plik.. etc.. */
					/* i tak to na razie jest jeden wielki hack, trzeba sprawdzac czy to dobry typ dcc. etc, XXX */
					xmlnode_t *node;
					jabber_dcc_t *p = d->priv;
					jabber_dcc_bytestream_t *b = NULL;

					list_t host_list = NULL;
					struct jabber_streamhost_item *streamhost = NULL;

					p->protocol = JABBER_DCC_PROTOCOL_BYTESTREAMS;

					xfree(p->req);
					p->req = xstrdup(id);

					for (node = q->children; node; node = node->next) {
						if (!xstrcmp(node->name, "streamhost")) {
							struct jabber_streamhost_item *newstreamhost = xmalloc(sizeof(struct jabber_streamhost_item));

							newstreamhost->ip	= xstrdup(jabber_attr(node->atts, "host"));	/* XXX in host can be hostname */
							newstreamhost->port	= atoi(jabber_attr(node->atts, "port"));
							newstreamhost->jid	= xstrdup(jabber_attr(node->atts, "jid"));
							list_add(&host_list, newstreamhost, 0);
						}
					}

					if ((list_count(host_list)) == 1) streamhost = host_list->data;
					else {
						streamhost = host_list->data;	/* XXX, select best one/chosen one */
					}
					
					if (streamhost) {
						struct sockaddr_in sin;
						int fd;
						char socks5[4];
						fd = socket(AF_INET, SOCK_STREAM, 0);

						sin.sin_family = AF_INET;
						sin.sin_port	= htons(streamhost->port);
						inet_pton(AF_INET, streamhost->ip, &(sin.sin_addr));

						connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
						watch_add(&jabber_plugin, fd, WATCH_READ, 1, jabber_dcc_handle_recv, d);
						
						p->private.bytestream = b = xmalloc(sizeof(jabber_dcc_bytestream_t));
						b->validate	= JABBER_DCC_PROTOCOL_BYTESTREAMS;
						b->step		= SOCKS5_CONNECT;
						b->streamhost	= streamhost;

						socks5[0] = 0x05;	/* socks version 5 */
						socks5[1] = 0x02;	/* number of methods */
						socks5[2] = 0x00;	/* no auth */
						socks5[3] = 0x02;	/* username */
						write(fd, (char *) &socks5, sizeof(socks5));
					}
				} else if (!xstrcmp(type, "result")) {
					xmlnode_t *used = xmlnode_find_child(q, "streamhost-used");
					jabber_dcc_t *p;
					jabber_dcc_bytestream_t *b;
					list_t l;

					if ((d = jabber_dcc_find(uid, id, NULL))) {
						char *usedjid = (used) ? jabber_attr(used->atts, "jid") : NULL;
						p = d->priv;
						b = p->private.bytestream;

						for (l = b->streamlist; l; l = l->next) {
							struct jabber_streamhost_item *item = l->data;
							if (!xstrcmp(item->jid, usedjid)) {
								b->streamhost = item;
							}
						}
						debug("[STREAMHOST-USED] stream: 0x%x\n", b->streamhost);
						d->active	= 1;
					}
				}
				debug("[FILE - BYTESTREAMS] 0x%x\n", d);
#endif
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
					userlist_t *u;
					char *uid;
					if (j->istlen)	uid = saprintf("tlen:%s", jabber_attr(item->atts, "jid"));
					else 		uid = saprintf("jid:%s",jabber_attr(item->atts, "jid"));

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

			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#items")) {
				if (!xstrcmp(jabber_attr(q->atts, "node"), "http://jabber.org/protocol/commands")) {
					/* XXX, check if $uid can do it */
					watch_write(j->send_watch, 
						"<iq to=\"%s\" type=\"result\" id=\"%s\">"
						"<query xmlns=\"http://jabber.org/protocol/disco#items\" node=\"http://jabber.org/protocol/commands\">"
						"<item jid=\"%s/%s\" name=\"Set Status\" node=\"http://jabber.org/protocol/rc#set-status\"/>"
						"<item jid=\"%s/%s\" name=\"Forward Messages\" node=\"http://jabber.org/protocol/rc#forward\"/>"
						"<item jid=\"%s/%s\" name=\"Set Options\" node=\"http://jabber.org/protocol/rc#set-options\"/>"
						"<item jid=\"%s/%s\" name=\"Set ALL ekg2 Options\" node=\"http://ekg2.org/jabber/rc#ekg-set-all-options\"/>"
						"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-plugins\"/>"
						"<item jid=\"%s/%s\" name=\"Manage ekg2 plugins\" node=\"http://ekg2.org/jabber/rc#ekg-manage-sessions\"/>"
						"<item jid=\"%s/%s\" name=\"Execute ANY command in ekg2\" node=\"http://ekg2.org/jabber/rc#ekg-command-execute\"/>"
						"</query></iq>", from, id, 
						s->uid+4, j->resource, s->uid+4, j->resource, 
						s->uid+4, j->resource, s->uid+4, j->resource,
						s->uid+4, j->resource, s->uid+4, j->resource,
						s->uid+4, j->resource);
				}
			}
			if (!xstrcmp(ns, "http://jabber.org/protocol/disco#info")) {
				watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">"
						"<query xmlns=\"http://jabber.org/protocol/disco#info\">"
						"<feature var=\"http://jabber.org/protocol/commands\"/>"
#if WITH_JABBER_DCC
						"<feature var=\"http://jabber.org/protocol/bytestreams\"/>"
						"<feature var=\"http://jabber.org/protocol/si\"/>"
						"<feature var=\"http://jabber.org/protocol/si/profile/file-transfer\"/>"
#endif
						"</query></iq>", from, id);

			} else if (!xstrncmp(ns, "jabber:iq:version", 17) && id && from) {
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

				watch_write(j->send_watch, "<iq to=\"%s\" type=\"result\" id=\"%s\">" 
						"<query xmlns=\"jabber:iq:version\">"
						"<name>"CHARF"</name>"
						"<version>"CHARF "</version>"
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
	time_t when = 0;
	xmlnode_t *q;
	int ismuc = 0;

	int na = !xstrcmp(type, "unavailable");

	jid = jabber_unescape(from);

	if (jabber_private(s)->istlen)	uid = saprintf("tlen:%s", jid);
	else				uid = saprintf("jid:%s", jid);

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
		char *ns	= jabber_attr(q->atts, "xmlns");

		if (!xstrcmp(q->name, "x")) {
			if (!xstrcmp(ns, "http://jabber.org/protocol/muc#user")) {
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

						debug("[MUC, PRESENCE] NEWITEM: %s ROLE:%s AFF:%s\n", jid, role, affiliation);

						xfree(jid); xfree(role); xfree(affiliation);
					} else {
						debug("[MUC, PRESENCE] child->name: %s\n", child->name);
					}
				}
				ismuc = 1;
			} else if (!xstrcmp(ns, "jabber:x:signed")) {	/* JEP-0027 */
				debug("[JABBER] XXX, SIGNED PRESENCE? uid: %s data: %s\n", mucuid, q->data);
			} else if (!xstrncmp(ns, "jabber:x:delay", 14)) {
				when = jabber_try_xdelay(jabber_attr(q->atts, "stamp"));
			} else debug("[JABBER, PRESENCE]: <x xmlns=%s\n", ns);
		}		/* <x> */
		xfree(mucuid);
	}
	if (!ismuc && (!type || ( na || !xstrcmp(type, "error") || !xstrcmp(type, "available")))) {
		xmlnode_t *nshow, *nstatus, *nerr, *temp;
		char *status = NULL, *descr = NULL;
		char *jstatus = NULL;
		char *tmp2;

		int prio = (temp = xmlnode_find_child(n, "priority")) ? atoi(temp->data) : 10;

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
			descr = saprintf("(%s) %s", ecode, etext);
			xfree(etext);

			xfree(status);
			status = xstrdup(EKG_STATUS_ERROR);
		} 
		if ((nstatus = xmlnode_find_child(n, "status"))) { /* opisowy */
			xfree(descr);
			descr = jabber_unescape(nstatus->data);
		}

		if ((tmp2 = xstrchr(uid, '/'))) {
			char *tmp = xstrndup(uid, tmp2-uid);
			userlist_t *ut;
#if 0
			if (!xstrcmp(tmp, s->uid)) print("jabber_resource_another", session_name(s), tmp, tmp2+1, itoa(prio), status ? status : jstatus, descr); /* makes it more colorful ? */
#endif
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
			char *host 	= NULL;
			int port 	= 0;

			if (!when) when = time(NULL);
			query_emit(NULL, "protocol-status", &session, &uid, &status, &descr, &host, &port, &when, NULL);
			
			xfree(session);
/*			xfree(host); */
		}
		xfree(status);
		xfree(descr);
	}
	xfree(uid);
} /* <presence> */

time_t jabber_try_xdelay(const char *stamp) {
	/* try to parse timestamp */
	if (stamp) {
        	struct tm tm;
       	        memset(&tm, 0, sizeof(tm));
               	sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d",
                        &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                       	&tm.tm_hour, &tm.tm_min, &tm.tm_sec);
       	        tm.tm_year -= 1900;
               	tm.tm_mon -= 1;
                return mktime(&tm);
        }
	return time(NULL);

}

void jabber_handle_disconnect(session_t *s, const char *reason, int type)
{
        jabber_private_t *j = jabber_private(s);

        if (!j)
                return;

        if (j->connecting)
                watch_remove(&jabber_plugin, j->fd, WATCH_WRITE);

	if (j->send_watch) {
		j->send_watch->type = WATCH_NONE;
		watch_free(j->send_watch);
		j->send_watch = NULL;
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

        if (!session_connected_get(s) && ((j->istlen && !xstrcmp(name, "s")) || (!j->istlen && !xstrcmp(name, "stream:stream")))) {
		CHAR_T *passwd		= jabber_escape(session_get(s, "password"));
                CHAR_T *resource	= jabber_escape(session_get(s, "resource"));
                char *username;
		char *authpass;
		char *stream_id;
		if (!j->istlen) username = xstrdup(s->uid + 4);
		else 		username = xstrdup(s->uid + 5);
		*(xstrchr(username, '@')) = 0;
	
		if (session_get(s, "__new_acount")) {
			watch_write(j->send_watch, 
				"<iq type=\"set\" to=\"%s\" id=\"register%d\">"
				"<query xmlns=\"jabber:iq:register\"><username>%s</username><password>" CHARF "</password></query></iq>", 
				j->server, j->id++, username, passwd ? passwd : TEXT("foo"));
		}

                if (!resource)
                        resource = xwcsdup(JABBER_DEFAULT_RESOURCE);

		stream_id = jabber_attr((char **) atts, 
					j->istlen ? "i" : "id");

		/* stolen from libtlen function calc_passcode() Copyrighted by libtlen's developer and Piotr Paw³ow */
		if (j->istlen) {
			const char *tmp = session_get(s, "password");
			int     magic1 = 0x50305735, magic2 = 0x12345671, sum = 7;
			char    z;
			while ((z = *tmp++) != 0) {
				if (z == ' ' || z == '\t') continue;
				magic1 ^= (((magic1 & 0x3f) + sum) * z) + (magic1 << 8);
				magic2 += (magic2 << 8) ^ magic1;
				sum += z;
			}
			magic1 &= 0x7fffffff;
			magic2 &= 0x7fffffff;

			xfree(passwd);
			passwd = saprintf("%08x%08x", magic1, magic2);
		}

		authpass = (!j->istlen && session_int_get(s, "plaintext_passwd")) ? 
			saprintf("<password>" CHARF "</password>", passwd) :  				/* plaintext */
			saprintf("<digest>%s</digest>", jabber_digest(stream_id, passwd));		/* hash */
		watch_write(j->send_watch, 
			"<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username>%s<resource>" CHARF"</resource></query></iq>", 
			j->server, username, authpass, resource);
                xfree(username);
		xfree(authpass);
		xfree(passwd);

		xfree(j->resource);
		j->resource = resource;
        } else
		xmlnode_handle_start(data, name, atts);
}

WATCHER(jabber_handle_stream)
{
        jabber_handler_data_t *jdh	= (jabber_handler_data_t *) data;
        session_t *s			= (session_t *) jdh->session;
        jabber_private_t *j		= session_private_get(s);
        char *buf;
        int len;

        s->activity = time(NULL);

        /* ojej, roz³±czy³o nas */
        if (type == 1) {
                debug("[jabber] jabber_handle_stream() type == 1, exitting\n");

		if (s && session_connected_get(s))  /* hack to avoid reconnecting when we do /disconnect */
			jabber_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK);

		xfree(data);
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
#ifdef NO_POSIX_SYSTEM
                if ((len = recv(fd, buf, 4095, 0)) < 1) {
#else
                if ((len = read(fd, buf, 4095)) < 1) {
#endif
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

	if (type == 1) {
		xfree(data);
		return 0;
	}

	if (!s || !session_connected_get(s)) {
		return -1;
	}

	if (jabber_private(s)->istlen) {
		jabber_write(s, "  \t  ");	/* ping according to libtlen */
		return 0;
	}
	
	if (session_int_get(s, "ping-server") == 0) return -1;

	jabber_write(s, "<iq/>"); /* leafnode idea */
	return 0;
}

WATCHER(jabber_handle_connect_tlen_hub) {	/* tymczasowy */
	int res = 0;
	int res_size = sizeof(res);

	session_t *s = (session_t *) data;

	if (type) {
		return 0;
	}
	
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		jabber_handle_disconnect(s, strerror(res), EKG_DISCONNECT_FAILURE);
		return -1;
	}
	
	/* XXX */
	debug("Connecting to HUB, currectly not works ;/");
	jabber_handle_disconnect(s, "Unimplemented do: /eval \"/session server s1.tlen.pl\" \"/session port 443\" \"/connect\" sorry.", EKG_DISCONNECT_FAILURE);
	return -1;
}

WATCHER(jabber_handle_connect) /* tymczasowy */
{
	session_t *s = (session_t *) data;
        jabber_private_t *j = session_private_get(s);
       	jabber_handler_data_t *jdh;

        int res = 0, res_size = sizeof(res);
	char *tname;

        debug("[jabber] jabber_handle_connect()\n");

        if (type) {
                return 0;
        }

        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
                jabber_handle_disconnect(s, strerror(res), EKG_DISCONNECT_FAILURE);
                return -1;
        }

	jdh = xmalloc(sizeof(jabber_handler_data_t));
	jdh->session = s;
/*	jdh->roster_retrieved = 0; */
	watch_add(&jabber_plugin, fd, WATCH_READ, jabber_handle_stream, jdh);

#ifdef HAVE_GNUTLS
	j->send_watch = watch_add(&jabber_plugin, fd, WATCH_WRITE_LINE, j->using_ssl ? jabber_handle_write : NULL, j);
#else
	j->send_watch = watch_add(&jabber_plugin, fd, WATCH_WRITE_LINE, NULL, NULL);
#endif
	if (!(j->istlen))
		watch_write(j->send_watch, 
			"<?xml version=\"1.0\" encoding=\"utf-8\"?><stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">", 
			j->server);
	else {
		watch_write(j->send_watch, "<s v=\'2\'>");
	}

        j->id = 1;
        j->parser = XML_ParserCreate("UTF-8");
        XML_SetUserData(j->parser, (void*) jdh);
        XML_SetElementHandler(j->parser, (XML_StartElementHandler) jabber_handle_start, (XML_EndElementHandler) xmlnode_handle_end);
        XML_SetCharacterDataHandler(j->parser, (XML_CharacterDataHandler) xmlnode_handle_cdata);

	if (j->istlen || (session_int_get(s, "ping-server") != 0)) {
		tname = saprintf("ping-%s", s->uid+4);
		timer_add(&jabber_plugin, tname, j->istlen ? 60 : 180, 1, jabber_ping_timer_handler, xstrdup(s->uid));
		/* w/g dokumentacji do libtlen powinnismy wysylac pinga co 60 sekund */
		xfree(tname);
	}

	return -1;
}

WATCHER(jabber_handle_resolver) /* tymczasowy watcher */
{
	session_t *s = (session_t *) data;
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
	int tlenishub = !session_get(s, "server") && j->istlen;
        if (type) {
                return 0;
	}

        debug("[jabber] jabber_handle_resolver()\n", type);
#ifdef NO_POSIX_SYSTEM
	int ret = ReadFile(fd, &a, sizeof(a), &res, NULL);
#else
	res = read(fd, &a, sizeof(a));
#endif
	if ((res != sizeof(a)) || (res && a.s_addr == INADDR_NONE /* INADDR_NONE kiedy NXDOMAIN */)) {
                if (res == -1)
                        debug("[jabber] unable to read data from resolver: %s\n", strerror(errno));
                else
                        debug("[jabber] read %d bytes from resolver. not good\n", res);
                close(fd);
                print("conn_failed", format_find("conn_failed_resolving"), session_name(s));
                /* no point in reconnecting by jabber_handle_disconnect() */
                j->connecting = 0;
                return -1;
        }

        debug("[jabber] resolved to %s\n", inet_ntoa(a));
#ifdef NO_POSIX_SYSTEM
	CloseHandle((HANDLE) fd);
#else
        close(fd);
#endif

        if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                debug("[jabber] socket() failed: %s\n", strerror(errno));
                jabber_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_FAILURE);
                return -1;
        }

        debug("[jabber] socket() = %d\n", fd);

        j->fd = fd;

        if (ioctl(fd, FIONBIO, &one) == -1) {
                debug("[jabber] ioctl() failed: %s\n", strerror(errno));
                jabber_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_FAILURE);
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

	if (tlenishub) j->port = 80; 

	sin.sin_port = htons(j->port);

        debug("[jabber] connecting to %s:%d\n", inet_ntoa(sin.sin_addr), j->port);

        res = connect(fd, (struct sockaddr*) &sin, sizeof(sin));

        if (res == -1 &&
#ifdef NO_POSIX_SYSTEM
		(WSAGetLastError() != WSAEWOULDBLOCK)
#else
		errno != EINPROGRESS
#endif
		) {
                debug("[jabber] connect() failed: %s (errno=%d)\n", strerror(errno), errno);
                jabber_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_FAILURE);
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
			jabber_handle_disconnect(s, gnutls_strerror(ret), EKG_DISCONNECT_FAILURE);
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

		watch_add(&jabber_plugin, fd, WATCH_WRITE, jabber_handle_connect_tls, s);

		return -1;
        } // use_ssl
#endif
	if (j->istlen && tlenishub)	watch_add(&jabber_plugin, fd, WATCH_WRITE, jabber_handle_connect_tlen_hub, s);
	else				watch_add(&jabber_plugin, fd, WATCH_WRITE, jabber_handle_connect, s);
	return -1;
}

#ifdef HAVE_GNUTLS
WATCHER(jabber_handle_connect_tls) /* tymczasowy */
{
        session_t *s = (session_t *) data;
        jabber_private_t *j = session_private_get(s);
	int ret;

	if (type) {
		return 0;
	}

	ret = gnutls_handshake(j->ssl_session);

	if ((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN)) {

		watch_add(&jabber_plugin, (int) gnutls_transport_get_ptr(j->ssl_session),
			gnutls_record_get_direction(j->ssl_session) ? WATCH_WRITE : WATCH_READ,
			jabber_handle_connect_tls, s);

		ekg_yield_cpu();
		return -1;

	} else if (ret < 0) {
		gnutls_deinit(j->ssl_session);
		gnutls_certificate_free_credentials(j->xcred);
		jabber_handle_disconnect(s, gnutls_strerror(ret), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	// handshake successful
	j->using_ssl = 1;

	watch_add(&jabber_plugin, fd, WATCH_WRITE, jabber_handle_connect, s);
	return -1;
}
#endif

QUERY(jabber_status_show_handle)
{
        char *uid	= *(va_arg(ap, char**));
        session_t *s	= session_find(uid);
        jabber_private_t *j = session_private_get(s);
        userlist_t *u;
        char *fulluid;
        char *tmp;

        if (!s || !j)
                return -1;

        fulluid = saprintf("%s/" CHARF, uid, j->resource);

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
	format_add("jabber_resource_another", _("%> (%1) Another resource: %2/%3 (%4) is %5: %6"), 1); /* %1 session, %2: jid:cos %3: resource %4 priority %5 statustype %6 descr */
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

	/* %1 - session_name, %2 - server/ uid */
	format_add("jabber_privacy_list_begin",   _("%g,+=%G----- Privacy list on %T%2%n"), 1);
	format_add("jabber_privacy_list_item",	  _("%g|| %n %3 - %W%4%n"), 1);					/* %3 - lp %4 - itemname */
	format_add("jabber_privacy_list_end",	  _("%g`+=%G----- End of the privacy list%n"), 1);
	format_add("jabber_privacy_list_noitem",  _("%! No privacy list in %T%2%n"), 1);

	format_add("jabber_privacy_item_header", _("%g,+=%G----- Details for: %T%3%n\n%g||%n JID\t\t\t\t\t  MSG  PIN POUT IQ%n"), 1);
	format_add("jabber_privacy_item",	   "%g||%n %[-44]4 \t%K|%n %[2]5 %K|%n %[2]6 %K|%n %[2]7 %K|%n %[2]8\n", 1);
	format_add("jabber_privacy_item_footer", _("%g`+=%G----- Legend: %n[%3] [%4]%n"), 1);

	/* %1 - item [group, jid, subscri*] */
	format_add("jabber_privacy_item_allow",  "%G%1%n", 1);
	format_add("jabber_privacy_item_deny",   "%R%1%n", 1);

	/* %1 - session_name %2 - list_name %3 xmlns */
	format_add("jabber_private_list_header",  _("%g,+=%G----- Private list: %T%2/%3%n"), 1);

	/* BOOKMARKS */
	format_add("jabber_bookmark_url",	_("%g|| %n URL: %W%3%n (%2)"), 1);		/* %1 - session_name, bookmark  url item: %2 - name %3 - url */
	format_add("jabber_bookmark_conf",	_("%g|| %n MUC: %W%3%n (%2)"), 1);	/* %1 - session_name, bookmark conf item: %2 - name %3 - jid %4 - autojoin %5 - nick %6 - password */

	/* XXX not private_list but CONFIG ? */
	format_add("jabber_private_list_item",	    "%g|| %n %4: %W%5%n",  1);			/* %4 - item %5 - value */
	format_add("jabber_private_list_session",   "%g|| + %n Session: %W%4%n",  1);		/* %4 - uid */
	format_add("jabber_private_list_plugin",    "%g|| + %n Plugin: %W%4 (%5)%n",  1);	/* %4 - name %5 - prio*/
	format_add("jabber_private_list_subitem",   "%g||  - %n %4: %W%5%n",  1);               /* %4 - item %5 - value */

	format_add("jabber_private_list_footer",  _("%g`+=%G----- End of the private list%n"), 1);
	format_add("jabber_private_list_empty",	  _("%! No list: %T%2/%3%n"), 1);

	/* %1 - session_name, %2 - uid (*_item: %3 - agent uid %4 - description %5 - seq id) */
	format_add("jabber_transport_list_begin", _("%g,+=%G----- Avalible agents on: %T%2%n"), 1);
	format_add("jabber_transport_list_item",  _("%g|| %n %5 - %W%3%n (%4)"), 1);
	format_add("jabber_transport_list_end",   _("%g`+=%G----- End of the agents list%n\n"), 1);
	format_add("jabber_transport_list_nolist", _("%! No agents @ %T%2%n"), 1);

	format_add("jabber_remotecontrols_list_begin", _("%g,+=%G----- Avalible remote controls on: %T%2%n"), 1);
	format_add("jabber_remotecontrols_list_item",  _("%g|| %n %6 - %W%4%n (%5)"), 1);		/* %3 - jid %4 - node %5 - descr %6 - seqid */
	format_add("jabber_remotecontrols_list_end",   _("%g`+=%G----- End of the remote controls list%n\n"), 1);
	format_add("jabber_remotecontrols_list_nolist", _("%! No remote controls @ %T%2%n"), 1);
	format_add("jabber_remotecontrols_executing",	_("%> (%1) Executing command: %3 @ %2 (%4)"), 1);
	format_add("jabber_remotecontrols_completed",	_("%> (%1) Command: %3 @ %2 completed"), 1);

	format_add("jabber_remotecontrols_commited_status", _("%> (%1) RC %2: requested changing status to: %3 %4 with priority: %5"), 1);	/* %3 - status %4 - descr %5 - prio */
	format_add("jabber_remotecontrols_commited_command",_("%> (%1) RC %2: requested command: %3 @ session: %4 window: %5 quiet: %6"), 1);	/* %3 - command+params %4 - sessionname %5 - target %6 - quiet */

	format_add("jabber_transinfo_begin",	_("%g,+=%G----- Information about: %T%2%n"), 1);
	format_add("jabber_transinfo_begin_node",_("%g,+=%G----- Information about: %T%2%n (%3)"), 1);
	format_add("jabber_transinfo_identify",	_("%g|| %G --== %g%3 %G==--%n"), 1);
		/* %4 - real fjuczer name  %3 - translated fjuczer name. */
	format_add("jabber_transinfo_feature",	_("%g|| %n %W%2%n feature: %n%3"), 1);
	format_add("jabber_transinfo_comm_ser",	_("%g|| %n %W%2%n can: %n%3 %2 (%4)"), 1);
	format_add("jabber_transinfo_comm_use",	_("%g|| %n %W%2%n can: %n%3 $uid (%4)"), 1);
	format_add("jabber_transinfo_comm_not",	_("%g|| %n %W%2%n can: %n%3 (%4)"), 1);
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

	format_add("jabber_form_command",	_("%g|| %nType %W/%3 %g%2 %W%4%n"), 1); 
	format_add("jabber_form_instructions", 	  "%g|| %n%|%3", 1);
	format_add("jabber_form_end",		_("%g`+=%G----- End of this %3 form ;)%n"), 1);

	format_add("jabber_registration_item", 	  "%g|| %n            --%3 %4%n", 1); /* %3 - keyname %4 - value */ /* XXX, merge */
	
	/* %1 - session %2 - message %3 - start %4 - end */
	format_add("jabber_vacation", _("%> You'd set up your vacation status: %g%2%n (since: %3 expires@%4)"), 1);

	/* %1 - sessionname %2 - mucjid %3 - nickname %4 - text */
	format_add("jabber_muc", "%B<%G%3%B>%n %4", 1);
#if 0
	format_add("jabber_send_chan", _("%B<%W%2%B>%n %5"), 1);
	format_add("jabber_send_chan_n", _("%B<%W%2%B>%n %5"), 1);

	format_add("jabber_recv_chan", _("%b<%w%2%b>%n %5"), 1);
	format_add("jabber_recv_chan_n", _("%b<%w%2%b>%n %5"), 1);
#endif
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
#if WITH_JABBER_DCC
	query_connect(&jabber_plugin, "config-postinit", jabber_dcc_postinit, NULL);
#endif

	variable_add(&jabber_plugin, TEXT("dcc_ip"), VAR_STR, 1, &jabber_dcc_ip, NULL, NULL, NULL);

        jabber_register_commands();

        plugin_var_add(&jabber_plugin, "alias", VAR_STR, 0, 0, NULL);
		/* '666' enabled for everyone (DON'T TRY IT!); '0' - disabled; '1' - enabled for the same id (allow from diffrent resources); '2' - enabled for allow_remote_control_jids (XXX) */
	plugin_var_add(&jabber_plugin, "allow_remote_control", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_away", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_back", VAR_INT, "0", 0, NULL);
	plugin_var_add(&jabber_plugin, "auto_bookmark_sync", VAR_BOOL, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_connect", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_find", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "auto_reconnect", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "display_notify", VAR_INT, "0", 0, NULL);
        plugin_var_add(&jabber_plugin, "log_formats", VAR_STR, "xml,simple", 0, NULL);
        plugin_var_add(&jabber_plugin, "password", VAR_STR, "foo", 1, NULL);
        plugin_var_add(&jabber_plugin, "plaintext_passwd", VAR_INT, "0", 0, NULL);
	plugin_var_add(&jabber_plugin, "ping-server", VAR_BOOL, "0", 0, NULL);
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
