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

#include "jabber.h"

static int jabber_plugin_destroy();

plugin_t jabber_plugin = {
	name: "jabber",
	pclass: PLUGIN_PROTOCOL,
	destroy: jabber_plugin_destroy,
};

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
	char **uid = va_arg(ap, char **);
	int *valid = va_arg(ap, int *);

	if (!*uid)
		return 0;

	if (!xstrncasecmp(*uid, "jid:", 4) && xstrchr(*uid, '@'))
		(*valid)++;
	
	return 0;
}

int jabber_write_status(session_t *s)
{
	jabber_private_t *j = session_private_get(s);
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
			jabber_write(j, "<presence><status>%s</status></presence>", descr);
		else
			jabber_write(j, "<presence/>");
	} else if (!xstrcmp(status, EKG_STATUS_INVISIBLE)) {
		if (descr)
			jabber_write(j, "<presence type=\"invisible\"><status>%s</status></presence>", descr);
		else
			jabber_write(j, "<presence type=\"invisible\"/>");
	} else {
		if (descr)
			jabber_write(j, "<presence><show>%s</show><status>%s</status></presence>", status, descr);
		else
			jabber_write(j, "<presence><show>%s</show></presence>", status);
	}

	xfree(descr);

	return 0;
}

void jabber_handle(void *data, xmlnode_t *n)
{
	jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
	session_t *s = jdh->session;
	jabber_private_t *j;
	const char *id, *type, *to, *from;


	if (!s || !(j = jabber_private(s)) || !n) {
		debug("[jabber] jabber_handle() invalid parameters\n");
		return;
	}

	debug("[jabber] jabber_handle() <%s>\n", n->name);

	id = jabber_attr(n->atts, "id");
	type = jabber_attr(n->atts, "type");
	to = jabber_attr(n->atts, "to");
	from = jabber_attr(n->atts, "from");

	if (!xstrcmp(n->name, "message")) {
		xmlnode_t *nbody = xmlnode_find_child(n, "body");
		xmlnode_t *nsubject = xmlnode_find_child(n, "subject");
		xmlnode_t *xitem = xmlnode_find_child(n, "x");
		string_t body = string_init("");
		char *session, *sender, **rcpts = NULL, *text, *seq = NULL;
		time_t sent = time(NULL);
		int class = EKG_MSGCLASS_CHAT;
		uint32_t *format = NULL;

		if (nsubject) {
			string_append(body, "Subject: ");
			string_append(body, nsubject->data);
			string_append(body, "\n\n");
		}

		if (nbody)
			string_append(body, nbody->data);
	
		if (xitem) {
			const char *ns, *stamp;
			ns = jabber_attr(xitem->atts, "xmlns");
			stamp  = jabber_attr(xitem->atts, "stamp");
			
			if (ns && !xstrncmp(ns, "jabber:x:delay", 14) && stamp) {
				struct tm tm;
				memset(&tm, 0, sizeof(tm));
				sscanf(stamp, "%4d%2d%2dT%2d:%2d:%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
				tm.tm_year -= 1900;
				tm.tm_mon -= 1;
				sent = mktime(&tm);
			};
		
			if (ns && !xstrncmp(ns, "jabber:x:event", 14)) {
				/* jesli jest body, to mamy do czynienia z prosba o potwierdzenie */
				if (nbody && (xmlnode_find_child(xitem, "delivered") || xmlnode_find_child(xitem, "displayed")) ) {
					char *id = jabber_attr(n->atts, "id");

					jabber_write(j, "<message to=\"%s\">", from);
					jabber_write(j, "<x xmlns=\"jabber:x:event\">");

					if (xmlnode_find_child(xitem, "delivered"))
						jabber_write(j, "<delivered/>");
					if (xmlnode_find_child(xitem, "displayed")) 
						jabber_write(j, "<displayed/>");

					jabber_write(j, "<id>%s</id></x></message>",id);
				};
				
				/* je¶li body nie ma, to odpowiedz na nasza prosbe */
				if (!nbody && xmlnode_find_child(xitem, "delivered") && (config_display_ack == 1 || config_display_ack == 2))
					print("ack_delivered", jabber_unescape(from));
					
				if (!nbody && xmlnode_find_child(xitem, "offline") && (config_display_ack == 1 || config_display_ack == 3))
					print("ack_queued", jabber_unescape(from));

#if 1
				if (!nbody && xmlnode_find_child(xitem, "composing")) {
					char *tmp;

					// TODO: usun±æ, albo zrobiæ prawdziwy format_string
					tmp = saprintf("%s co¶ do nas pisze ...", jabber_unescape(from));
					print("generic2", tmp);
					xfree(tmp);
				}; /* composing */
#endif

			}; /* jabber:x:event */
		}

		session = xstrdup(session_uid_get(s));
		sender = saprintf("jid:%s", jabber_unescape(from));
		text = jabber_unescape(body->str);
		string_free(body, 1);

		if (nbody || nsubject)
			query_emit(NULL, "protocol-message", &session, &sender, &rcpts, &text, &format, &sent, &class, &seq, NULL);

		xfree(session);
		xfree(sender);
		array_free(rcpts);
		xfree(text);
		xfree(format);
		xfree(seq);
	}

	if (!xstrcmp(n->name, "iq") && type) {
		if (id && !xstrcmp(id, "auth")) {
			s->last_conn = time(NULL);
			j->connecting = 0;

			if (!xstrcmp(type, "result")) {
				print("connected", session_name(s)); 
				session_connected_set(s, 1);
				session_unidle(s);
				jabber_write(j, "<iq type=\"get\"><query xmlns=\"jabber:iq:roster\"/></iq>");
				jabber_write_status(s);
			}

			if (!xstrcmp(type, "error")) {
				xmlnode_t *e = xmlnode_find_child(n, "error");

				if (e && e->data) {
					char *data = jabber_unescape(e->data);
					print("conn_failed", data, session_name(s));
					xfree(data);
				} else
					print("jabber_generic_conn_failed", session_name(s));
			}
		}

		if (id && !xstrncmp(id, "passwd", 6)) {
			if (!xstrcmp(type, "result")) {
				session_set(s, "password", session_get(s, "__new_password"));
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
			xmlnode_t *q = xmlnode_find_child(n, "query");
			if (q) {
				const char *ns;
				ns = jabber_attr(q->atts, "xmlns");
				
				if (ns && !xstrncmp(ns, "jabber:iq:roster", 16)) {
					userlist_t u, *tmp;

					xmlnode_t *item = xmlnode_find_child(q, "item");
					for (; item ; item = item->next) {
						memset(&u, 0, sizeof(u));
						u.uid = saprintf("jid:%s",jabber_attr(item->atts, "jid"));
						u.nickname = jabber_unescape(jabber_attr(item->atts, "name"));
		
						if (!u.nickname) 
							u.nickname = xstrdup(u.uid);

						u.status = xstrdup(EKG_STATUS_NA);
						//XXX grupy


						/* je¶li element rostera ma subscription = remove to tak naprawde u¿ytkownik jest usuwany;
						   w przeciwnym wypadku - nalezy go dopisaæ do userlisty; dodatkowo, jesli uzytkownika
						   mamy ju¿ w liscie, to przyszla do nas zmiana rostera; usunmy wiec najpierw, a potem
						   sprawdzmy, czy warto dodawac :) */
						if ((tmp = userlist_find(s, u.uid)) )
							userlist_remove(s, tmp);

						if (jabber_attr(item->atts, "subscription") && !xstrncmp(jabber_attr(item->atts, "subscription"), "remove", 6)) {
							/* nic nie robimy, bo juz usuniete */
						} else { 
							if (jabber_attr(item->atts, "subscription"))
								u.authtype = xstrdup(jabber_attr(item->atts, "subscription"));
							list_add_sorted(&(s->userlist), &u, sizeof(u), NULL);
						}
					}; /* for */
				} /* jabber:iq:roster */
			} /* if query */
		} /* type == set */
	
		if (type && !xstrncmp(type, "get", 3)) {
			xmlnode_t *q = xmlnode_find_child(n, "query");

			if (q) {
				const char *ns;
				ns = jabber_attr(q->atts, "xmlns");

				if (ns && !xstrncmp(ns, "jabber:iq:version", 17)) {
					struct utsname buf;

					uname(&buf);

					/* 'id' powinno byc w <iq/> */
					if (id && from) {
						jabber_write(j, "<iq to=\"%s\" type=\"result\" id=\"%s\">", from, id);
						jabber_write(j, "<query xmlns=\"jabber:iq:version\"><name>Application Platform and Instant Messaging System - EKG-NG</name>");
						jabber_write(j, "<version>%s</version>", VERSION);
						jabber_write(j, "<os>%s %s %s</os>", jabber_escape(buf.sysname), jabber_escape(buf.release), jabber_escape(buf.machine));
						jabber_write(j, "</query></iq>");
					}
				}; /* jabber:iq:version */

			} /* if query */
		} /* type == get */ 
	} /* if iq */

	if (!xstrcmp(n->name, "presence")) {
		if (type && !xstrcmp(type, "subscribe") && from) {
			print("jabber_auth_subscribe", from, session_name(s));
			return;
		}

		if (type && !xstrcmp(type, "unsubscribe") && from) {
			print("jabber_auth_unsubscribe", jabber_unescape(from), session_name(s));
			return;
		}

		if (!type || (type && !xstrcmp(type, "unavailable")) ) {
			xmlnode_t *nshow = xmlnode_find_child(n, "show"); /* typ */
			xmlnode_t *nstatus = xmlnode_find_child(n, "status"); /* opisowy */
			char *session, *uid, *status = NULL, *descr = NULL, *host = NULL;
			int port = 0;

			if (nshow)
				status = jabber_unescape(nshow->data);
			else
				status = xstrdup(EKG_STATUS_AVAIL);

			if (!xstrcmp(status, "na") || !xstrcmp(type, "unavailable")) {
				xfree(status);
				status = xstrdup(EKG_STATUS_NA);
			}

			if (nstatus)
				descr = jabber_unescape(nstatus->data);

			session = xstrdup(session_uid_get(s));
			uid = saprintf("jid:%s", jabber_unescape(from));
			host = NULL;
			port = 0;

			query_emit(NULL, "protocol-status", &session, &uid, &status, &descr, &host, &port, NULL);

			xfree(session);
			xfree(uid);
			xfree(status);
			xfree(descr);
			xfree(host);
		}
	}
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
	if (j->using_ssl)
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
	if (j->using_ssl) {
		gnutls_deinit(j->ssl_session);
		gnutls_global_deinit();
	}
#endif
	reconnect_delay = session_int_get(s, "auto_reconnect");
	if (reconnect_delay && reconnect_delay != -1) 
		timer_add(&jabber_plugin, "reconnect", reconnect_delay, 0, jabber_reconnect_handler, xstrdup(s->uid));

}

void jabber_reconnect_handler(int type, void *data)
{
	session_t *s = session_find((char*) data);

	if (!s || session_connected_get(s) == 1)
		return;

	command_exec(NULL, s, xstrdup("/connect"), 0);
}

static void jabber_handle_start(void *data, const char *name, const char **atts)
{
	jabber_handler_data_t *jdh = (jabber_handler_data_t*) data;
	jabber_private_t *j = session_private_get(jdh->session);
	session_t *s = jdh->session;
	
	if (!xstrcmp(name, "stream:stream")) {
		const char *password = session_get(s, "password");
		const char *resource = session_get(s, "resource");
		const char *uid = session_uid_get(s);
		char *username;
		int plaintext = session_int_get(s, "plaintext_passwd");
		
		username = xstrdup(uid + 4);
		*(xstrchr(username, '@')) = 0;

		if (!resource)
			resource = "ekg2";

		j->stream_id = xstrdup(jabber_attr((char **) atts, "id"));

		if (plaintext)
			jabber_write(j, "<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><password>%s</password><resource>%s</resource></query></iq>", j->server, username, password, resource);
		else
		  	jabber_write(j, "<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>", j->server, username, jabber_digest(j->stream_id, password), resource);
		xfree(username);

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
	if (j->using_ssl && ((len = gnutls_record_recv(j->ssl_session, buf, 4095))<1)) {
		print("generic_error", strerror(errno));
		goto fail;
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
		debug("wrong type: %d\n", type);
		return;
	}

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		print("generic_error", strerror(res));
		j->connecting = 0;
		return;
	}

	watch_add(&jabber_plugin, fd, WATCH_READ, 1, jabber_handle_stream, data);

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
	gnutls_certificate_credentials xcred;
	/* Allow connections to servers that have OpenPGP keys as well. */
	const int cert_type_priority[3] = {GNUTLS_CRT_X509,
		GNUTLS_CRT_OPENPGP, 0};
#endif

	if (type != 0)
		return;

	debug("[jabber] jabber_handle_resolver()\n", type);
	
	if (!port)
		port = 5222;

	if ((res = read(fd, &a, sizeof(a))) != sizeof(a)) {
		if (res == -1)
			debug("[jabber] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug("[jabber] read %d bytes from resolver. not good\n", res);
		close(fd);
		print("generic_error", "Nie znaleziono serwera, sorry");
		j->connecting = 0;
		return;
	}

	debug("[jabber] resolved to %s\n", inet_ntoa(a));

	close(fd);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[jabber] socket() failed: %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		j->connecting = 0;
		return;
	}

	debug("[jabber] socket() = %d\n", fd);

	j->fd = fd;

	if (ioctl(fd, FIONBIO, &one) == -1) {
		debug("[jabber] ioctl() failed: %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		j->connecting = 0;
		return;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = a.s_addr;
#ifdef HAVE_GNUTLS
	if (use_ssl)
		sin.sin_port = htons(ssl_port);
	else
#endif
		sin.sin_port = htons(port);

	debug("[jabber] connecting to %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
	
	connect(fd, (struct sockaddr*) &sin, sizeof(sin));

	if (errno != EINPROGRESS) {
		debug("[jabber] connect() failed: %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		j->connecting = 0;
		return;
	}

#ifdef HAVE_GNUTLS
	j->using_ssl = 0;
	if (use_ssl) {
		int ret, retrycount = 65535; // insane
		gnutls_global_init();
		gnutls_certificate_allocate_credentials(&xcred);
		/* XXX - ~/.ekg/certs/server.pem */
		gnutls_certificate_set_x509_trust_file(xcred, "brak", GNUTLS_X509_FMT_PEM);
		gnutls_init(&(j->ssl_session), GNUTLS_CLIENT);
		gnutls_set_default_priority(j->ssl_session);
		gnutls_certificate_type_set_priority(j->ssl_session, cert_type_priority);
		gnutls_credentials_set(j->ssl_session, GNUTLS_CRD_CERTIFICATE, xcred);

		gnutls_transport_set_ptr(j->ssl_session, (gnutls_transport_ptr)(j->fd));

		do { 
			ret = gnutls_handshake(j->ssl_session);
			retrycount--;
		} while (((ret == GNUTLS_E_INTERRUPTED) || (ret == GNUTLS_E_AGAIN)) && retrycount);

		if (ret < 0) {
			debug("[jabber] ssl handshake failed: %d - %s\n", ret, gnutls_strerror(ret));
			print("generic_error", gnutls_strerror(ret));
			j->connecting = 0;
			gnutls_deinit(j->ssl_session);
			gnutls_certificate_free_credentials(xcred);
			gnutls_global_deinit();
			return;
		}
		j->using_ssl = 1;
	} // use_ssl
#endif

	watch_add(&jabber_plugin, fd, WATCH_WRITE, 0, jabber_handle_connect, data);
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
	struct tm *t;
	time_t n;
	int now_days;
	char buf[100], *tmp;

	if (!s || !j) 
		return -1;

	print("show_status_header");

	// nasz stan
	if ((u = userlist_find(s, s->uid)) && u->nickname)
		print("show_status_uid_nick", s->uid, u->nickname);
	else
		print("show_status_uid", s->uid);

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
	strftime(buf, sizeof(buf), format_find((t->tm_yday == now_days) ? "show_status_last_conn_event_today" : "show_status_last_conn_event"), t);

	if (s->connected)
		print("show_status_connected_since", buf);
	else
		print("show_status_disconnected_since", buf);

	// nasz status
	if (s->connected)
		tmp = format_string(format_find(ekg_status_label(s->status, s->descr, "show_status_")),s->descr, "");
	else
		tmp = format_string(format_find("show_status_notavail"));

	print("show_status_status_simple", tmp);

	print("show_status_footer");

	return 0;
}

COMMAND(jabber_command_connect)
{
	const char *password = session_get(session, "password");
	const char *server, *realserver = session_get(session, "server"); 
	int res, fd[2];
	jabber_private_t *j = session_private_get(session);
	
	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (!password) {
		printq("no_config");
		return -1;
	}

	debug("session->uid = %s\n", session->uid);
	
	if (!(server = xstrchr(session->uid, '@'))) {
		printq("wrong_id", session->uid);
		return -1;
	}

	xfree(j->server);
	j->server = xstrdup(++server) ;

	debug("[jabber] resolving %s\n", (realserver ? realserver : server));

	if (pipe(fd) == -1) {
		printq("generic_error", strerror(errno));
		return -1;
	}

	debug("[jabber] resolver pipes = { %d, %d }\n", fd[0], fd[1]);

	if ((res = fork()) == -1) {
		printq("generic_error", strerror(errno));
		return -1;
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
		close(fd[1]);

		jabber_handler_data_t *jdta = xmalloc(sizeof(jabber_handler_data_t));
		jdta->session = session;
		/* XXX dodaæ dzieciaka do przegl±dania */

		watch_add(&jabber_plugin, fd[0], WATCH_READ, 0, jabber_handle_resolver, jdta);
	}

	j->connecting = 1;

	printq("connecting", session_name(session));

	if (!xstrcmp(session_status_get(session), EKG_STATUS_NA))
		session_status_set(session, EKG_STATUS_AVAIL);
	
	return 0;
}

COMMAND(jabber_command_disconnect)
{
	jabber_private_t *j = session_private_get(session);
	char *descr = NULL;


	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
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

	if (j->connecting) {
		j->connecting = 0;
		printq("conn_stopped", session_name(session));
	} else
		printq("disconnected", session_name(session));

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
	const char *uid;

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		return -1;
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

	jabber_write(j, "<body>%s</body>", msg);

	jabber_write(j, "<x xmlns=\"jabber:x:event\">%s%s<displayed/><composing/></x>", 
		( config_display_ack == 1 || config_display_ack == 2 ? "<delivered/>" : ""),
		( config_display_ack == 1 || config_display_ack == 3 ? "<offline/>"   : "") );
	jabber_write(j, "</message>");

	xfree(msg);
	xfree(subject);

	if (config_display_sent && !quiet) {
		char *tmp = saprintf("jid:%s", uid);
		const char *rcpts[2] = { tmp, NULL };
		message_print(session_uid_get(session), session_uid_get(session), rcpts, params[1], NULL, time(NULL), (chat) ? EKG_MSGCLASS_SENT : EKG_MSGCLASS_MESSAGE, NULL);
		xfree(tmp);
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
	};

	descr = session_descr_get(session);

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
	ekg_update_status(session);
	
	if (descr) {
		char *f = saprintf("%s_descr", format);
		printq(f, descr, "", session_name(session));
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

	if (!params[1]) {
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
		action = "unsubscribed";
		printq("jabber_auth_unsubscribed", uid, session_name(s));
		goto success;
	}

	if (params[0] && match_arg(params[0], 'd', "deny", 2)) {
		char *tmp;
		action = "unsubscribe";

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

COMMAND(jabber_command_add)
{
	jabber_private_t *j = session_private_get(session);
	char *uid, *tmp;
	int ret;

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
	
	uid = (char *) params[0]; 
	if (!xstrncasecmp(uid, "jid:", 4))
		uid += 4;
	else if (xstrchr(uid, ':')) { 
                printq("invalid_uid");
                return -1;
	}

	jabber_write(j, "<iq type=\"set\" id=\"roster\"><query xmlns=\"jabber:iq:roster\">");
	if (params[1])
		jabber_write(j, "<item jid=\"%s\" name=\"%s\"/>", uid, jabber_escape(params[1]));
	else
		jabber_write(j, "<item jid=\"%s\"/>", uid);
	jabber_write(j, "</query></iq>");

	tmp = saprintf("/auth --request jid:%s", uid);
	ret = command_exec(target, session, tmp, 0);
	xfree(tmp);
	
	return ret;
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


int jabber_plugin_init()
{
	list_t l;

	plugin_register(&jabber_plugin);

	query_connect(&jabber_plugin, "protocol-validate-uid", jabber_validate_uid, NULL);
	query_connect(&jabber_plugin, "plugin-print-version", jabber_print_version, NULL);
	query_connect(&jabber_plugin, "session-added", jabber_session, (void*) 1);
	query_connect(&jabber_plugin, "session-removed", jabber_session, (void*) 0);
	query_connect(&jabber_plugin, "status-show", jabber_status_show_handle, NULL);


#define possibilities(x) array_make(x, " ", 0, 1, 1)
#define params(x) array_make(x, " ", 0, 1, 1)

	command_add(&jabber_plugin, "jid:connect", params("?"), jabber_command_connect, 0, "", "³±czy siê z serwerem", "", NULL);
	command_add(&jabber_plugin, "jid:disconnect", params("?"), jabber_command_disconnect, 0, " [powód/-]", "roz³±cza siê od serwera", "", NULL);
	command_add(&jabber_plugin, "jid:reconnect", params(""), jabber_command_reconnect, 0, "", "roz³±cza i ³±czy siê ponownie", "", NULL);
	command_add(&jabber_plugin, "jid:msg", params("uU ?"), jabber_command_msg, 0, "", "wysy³a pojedyncz± wiadomo¶æ", "\nPoprzedzenie wiadomo¶ci wielolinijkowej ci±giem zdefiniowanym w zmiennej subject_string spowoduje potraktowanie pierwszej linijki jako tematu.", NULL);
	command_add(&jabber_plugin, "jid:chat", params("uU ?"), jabber_command_msg, 0, "", "wysy³a wiadomo¶æ w ramach rozmowy", "", NULL);
	command_add(&jabber_plugin, "jid:", params("?"), jabber_command_inline_msg, 0, "", "wysy³a wiadomo¶æ", "", NULL);
	command_add(&jabber_plugin, "jid:xml", params("?"), jabber_command_xml, 0, "", "wysy³a polecenie xml", "\nPolecenie musi byæ zakodowanie w UTF-8, a wszystkie znaki specjalne u¿ywane w XML (\" ' & < >) musz± byæ zamienione na odpowiadaj±ce im sekwencje.", NULL);
	command_add(&jabber_plugin, "jid:away", params("r"), jabber_command_away, 0, "", "zmienia stan na zajêty", "", NULL);
	command_add(&jabber_plugin, "jid:_autoaway", params("r"), jabber_command_away, 0, "", "zmienia stan na zajêty", "", NULL);
	command_add(&jabber_plugin, "jid:back", params("r"), jabber_command_away, 0, "", "zmienia stan na dostêpny", "", NULL);
	command_add(&jabber_plugin, "jid:_autoback", params("r"), jabber_command_away, 0, "", "zmienia stan na dostêpny", "", NULL);
	command_add(&jabber_plugin, "jid:invisible", params("r"), jabber_command_away, 0, "", "zmienia stan na zajêty", "", NULL);
	command_add(&jabber_plugin, "jid:dnd", params("r"), jabber_command_away, 0, "", "zmienia stan na nie przeszkadzaæ","", NULL);
	command_add(&jabber_plugin, "jid:xa", params("r"), jabber_command_away, 0, "", "zmienia stan na bardzo zajêty", "", NULL);
	command_add(&jabber_plugin, "jid:passwd", params("?"), jabber_command_passwd, 0, "", "zmienia has³o", "", NULL);
	command_add(&jabber_plugin, "jid:auth", params("p uU"), jabber_command_auth, 0, "", "obs³uga autoryzacji", 
	  "<akcja> <JID> \n"
	  "  -a, --accept <JID>    autoryzuje JID\n"
	  "  -d, --deny <JID>      odmawia udzielenia autoryzacji\n"
 	  "  -r, --request <JID>   wysy³a ¿±danie autoryzacji\n"
	  "  -c, --cancel <JID>    wysy³a ¿±danie cofniêcia autoryzacji\n",
	  possibilities("-a --accept -d --deny -r --request -c --cancel") );
	command_add(&jabber_plugin, "jid:add", params("U ?"), jabber_command_add, 0, "", "dodaje u¿ytkownika do naszego rostera, jednocze¶nie prosz±c o autoryzacjê", "<JID> [nazwa]", NULL ); 
	command_add(&jabber_plugin, "jid:del", params("u"), jabber_command_del, 0, "", "usuwa z naszego rostera", "", NULL);

#undef possibilities 
#undef params
        
	plugin_var_add(&jabber_plugin, "alias", VAR_STR, 0, 0);
	plugin_var_add(&jabber_plugin, "auto_away", VAR_INT, "0", 0);
        plugin_var_add(&jabber_plugin, "auto_back", VAR_INT, "0", 0);
        plugin_var_add(&jabber_plugin, "auto_connect", VAR_INT, "0", 0);
        plugin_var_add(&jabber_plugin, "auto_find", VAR_INT, "0", 0);
        plugin_var_add(&jabber_plugin, "auto_reconnect", VAR_INT, "0", 0);
        plugin_var_add(&jabber_plugin, "display_notify", VAR_INT, "0", 0);
	plugin_var_add(&jabber_plugin, "log_formats", VAR_STR, "xml,simple", 0);
	plugin_var_add(&jabber_plugin, "password", VAR_STR, "foo", 1);
        plugin_var_add(&jabber_plugin, "plaintext_passwd", VAR_INT, "0", 0);
	plugin_var_add(&jabber_plugin, "port", VAR_INT, itoa(5222), 0);
	plugin_var_add(&jabber_plugin, "resource", VAR_STR, 0, 0);
	plugin_var_add(&jabber_plugin, "server", VAR_STR, 0, 0);
	plugin_var_add(&jabber_plugin, "ssl_port", VAR_INT, itoa(5223), 0);
	plugin_var_add(&jabber_plugin, "use_ssl", VAR_INT, itoa(1), 0);

	format_add("jabber_auth_subscribe", "%> (%2) %1 prosi o autoryzacjê dodania. U¿yj \"/auth -a %1\" aby zaakceptowaæ, \"/auth -d %1\" aby odrzuciæ.%n\n", 1);
	format_add("jabber_auth_unsubscribe", "%> (%2) %1 prosi o autoryzacjê usuniêcia. U¿yj \"/auth -c %1\" aby usun±æ.%n\n", 1);
	format_add("jabber_xmlerror", "%> (%1) B³±d parsowania XMLa%n\n", 1);
	format_add("jabber_auth_request", "%> (%2) Wys³ano ¿±danie autoryzacji do %1.%n%\n", 1);
	format_add("jabber_auth_accept", "%> (%2) Autoryzowano %1. %n\n", 1);
	format_add("jabber_auth_unsubscribed", "%> (%2) Wys³ano pro¶bê o cofniêcie autoryzacji do %1.%n\n", 1);
	format_add("jabber_auth_cancel", "%> (%2) Cofniêto autoryzacjê %1.%n%\n", 1);
	format_add("jabber_auth_denied", "%> (%2) Odmówiona autoryzacji %1. %n\n", 1);
	format_add("jabber_auth_probe", "%> (%2) Wys³ano pytanie o obecno¶æ do %1.%n\n", 1);
	format_add("jabber_generic_conn_failed", "%> (%1) B³±d ³±czenia siê z serwerem Jabbera%n\n", 1);

	for (l = sessions; l; l = l->next)
		jabber_private_init((session_t*) l->data);

	return 0;
}

static int jabber_plugin_destroy()
{
	list_t l;

	for (l = sessions; l; l = l->next)
		jabber_private_destroy((session_t*) l->data);

	plugin_unregister(&jabber_plugin);

	return 0;
}
