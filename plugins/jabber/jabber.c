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

#include "config.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

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
#include <ekg/xmalloc.h>

#include "jabber.h"

static int jabber_plugin_destroy();

plugin_t jabber_plugin = {
	name: "jabber",
	pclass: PLUGIN_PROTOCOL,
	destroy: jabber_plugin_destroy
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

	if (strncasecmp(uid, "jid:", 4))
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

	if (strncasecmp(uid, "jid:", 4) || !j)
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
void jabber_session(void *data, va_list ap)
{
	char **session = va_arg(ap, char**);
	session_t *s = session_find(*session);

	if (!s)
		return;

	if (data)
		jabber_private_init(s);
	else
		jabber_private_destroy(s);
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

	if (!strncasecmp(*uid, "jid:", 4) && strchr(*uid, '@'))
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

	if (!strcmp(status, EKG_STATUS_AVAIL)) {
		if (descr)
			jabber_write(j, "<presence><status>%s</status></presence>", descr);
		else
			jabber_write(j, "<presence/>");
	} else if (!strcmp(status, EKG_STATUS_INVISIBLE)) {
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

void jabber_handle(session_t *s, xmlnode_t *n)
{
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

	if (!strcmp(n->name, "message")) {
		xmlnode_t *nbody = xmlnode_find_child(n, "body");
		xmlnode_t *nsubject = xmlnode_find_child(n, "subject");
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

		session = xstrdup(session_uid_get(s));
		sender = saprintf("jid:%s", from);
		text = jabber_unescape(body->str);
		string_free(body, 1);

		query_emit(NULL, "protocol-message", &session, &sender, &rcpts, &text, &format, &sent, &class, &seq, NULL);

		xfree(session);
		xfree(sender);
		array_free(rcpts);
		xfree(text);
		xfree(format);
		xfree(seq);
	}

	if (!strcmp(n->name, "iq") && type) {
		if (id && !strcmp(id, "auth")) {
			j->connecting = 0;

			if (!strcmp(type, "result")) {
				print("generic", "Po³±czono siê z Jabberem");
				session_connected_set(s, 1);
				session_unidle(s);
				jabber_write(j, "<iq type=\"get\" id=\"roster\"><query xmlns=\"jabber:iq:roster\"/></iq>");
				jabber_write_status(s);
			}

			if (!strcmp(type, "error")) {
				xmlnode_t *e = xmlnode_find_child(n, "error");

				if (e && e->data) {
					char *data = jabber_unescape(e->data), *tmp = saprintf("B³±d ³±czenia siê z Jabberem: %s", data);
					print("generic_error", tmp);
					xfree(data);
					xfree(tmp);
				} else
					print("generic_error", "B³±d ³±czenia siê z Jabberem");
			}
		}

		if (id && !strncmp(id, "passwd", 6)) {
			if (!strcmp(type, "result")) {
				session_set(s, "password", session_get(s, "__new_password"));
				print("passwd");
			}

			if (!strcmp(type, "error")) {
				xmlnode_t *e = xmlnode_find_child(n, "error");
				char *reason = (e && e->data) ? jabber_unescape(e->data) : xstrdup("?");

				print("passwd_failed", reason);
				xfree(reason);
			}

			session_set(s, "__new_password", NULL);
	
		}
		
		if (id && !strncmp(id, "roster", 7)) {
			userlist_t u;
			xmlnode_t *q = xmlnode_find_child(n, "query");
			xmlnode_t *item = xmlnode_find_child(q, "item");
			for (; item ; item = item->next) {
				memset(&u, 0, sizeof(u));
				u.uid = saprintf("jid:%s",jabber_attr(item->atts, "jid"));
				u.nickname = jabber_unescape(jabber_attr(item->atts, "name"));
		
				if (!u.nickname) 
					u.nickname = strdup(u.uid); 				

				u.status = xstrdup(EKG_STATUS_NA);
				//XXX grupy
				if (userlist_find(s, u.uid)) 
					userlist_replace(s, &u);
			 	else 
					list_add_sorted(&(s->userlist), &u, sizeof(u), NULL);
			};
			
		}
	} /* if iq */

	if (!strcmp(n->name, "presence")) {
		if (type && !strcmp(type, "subscribe") && from) {
			char *tmp = saprintf("%s prosi o autoryzacjê dodania. U¿yj "
				"\"/auth -a %s\" aby zaakceptowaæ, \"/auth -d %s\" "
				"aby odrzuciæ.", from, from, from);
			print("generic", tmp);
			xfree(tmp);

			return;
		}

		if (type && !strcmp(type, "unsubscribe") && from) {
			char *tmp = saprintf("%s prosi o autoryzacjê usuniêcia. U¿yj "
				"\"/auth -c %s\" aby usun±æ.", from, from);
			print("generic", tmp);
			xfree(tmp);

			return;
		}

 		if (type && !strcmp(type, "unsubscribed") && from) {
			userlist_t u;
			char *err;
			u.uid = xstrdup(from);
			if (userlist_find(s, u.uid))
				userlist_remove(s, &u);
			else {
				err = saprintf("Ej, %s czego¶ od nas chce. Znasz go, Zenek?", u.uid);
				print("generic", err);
				xfree(err);
			}
                        return;
                }


		if (!type) {
			xmlnode_t *nshow = xmlnode_find_child(n, "show");
			xmlnode_t *nstatus = xmlnode_find_child(n, "status");
			char *session, *uid, *status = NULL, *descr = NULL, *host = NULL;
			int port = 0;

			if (nshow)
				status = jabber_unescape(nshow->data);
			else
				status = xstrdup(EKG_STATUS_AVAIL);

			if (!strcmp(status, "na")) {
				xfree(status);
				status = xstrdup(EKG_STATUS_NA);
			}

			if (nstatus)
				descr = jabber_unescape(nstatus->data);

			session = xstrdup(session_uid_get(s));
			uid = saprintf("jid:%s", from);
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

	if (!j)
		return;

	if (j->obuf || j->connecting)
		watch_remove(&jabber_plugin, j->fd, WATCH_WRITE);

	if (j->obuf) {
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
	}

	session_connected_set(s, 0);
	j->connecting = 0;
	XML_ParserFree(j->parser);
	j->parser = NULL;
	close(j->fd);
	j->fd = -1;
}

static void jabber_handle_start(void *data, const char *name, const char **atts)
{
	jabber_private_t *j = session_private_get(data);
	session_t *s = data;
	
	if (!strcmp(name, "stream:stream")) {
		const char *password = session_get(s, "password");
		const char *resource = session_get(s, "resource");
		const char *uid = session_uid_get(s);
		char *username;

		username = xstrdup(uid + 4);
		*(strchr(username, '@')) = 0;

		if (!resource)
			resource = "ekg2";

		j->stream_id = xstrdup(jabber_attr((char **) atts, "id"));

		jabber_write(j, "<iq type=\"set\" id=\"auth\" to=\"%s\"><query xmlns=\"jabber:iq:auth\"><username>%s</username><digest>%s</digest><resource>%s</resource></query></iq>", j->server, username, jabber_digest(j->stream_id, password), resource);
		
		xfree(username);

		return;
	}

	xmlnode_handle_start(data, name, atts);
}

void jabber_handle_stream(int type, int fd, int watch, void *data)
{
	session_t *s = (session_t*) data;
	jabber_private_t *j = session_private_get(s);
	char *buf;
	int len;

	/* ojej, roz³±czy³o nas */
	if (type == 1) {
		jabber_handle_disconnect(s);
		return;
	}

	debug("[jabber] jabber_handle_stream()\n");

	if (!(buf = XML_GetBuffer(j->parser, 4096))) {
		print("generic_error", "XML_GetBuffer failed");
		goto fail;
	}

	if ((len = read(fd, buf, 4095)) < 1) {
		print("generic_error", strerror(errno));
		goto fail;
	}

	buf[len] = 0;

	debug("[jabber] recv %s\n", buf);

	if (!XML_ParseBuffer(j->parser, len, (len == 0))) {
		print("generic_error", "B³±d parsowania XMLa");
		goto fail;
	}

	return;

fail:
	watch_remove(&jabber_plugin, fd, WATCH_READ);
}

void jabber_handle_connect(int type, int fd, int watch, void *data)
{
	int res = 0, res_size = sizeof(res);
	jabber_private_t *j = session_private_get(data);

	debug("[jabber] jabber_handle_connect()\n");

	if (type != 0)
		return;

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		print("generic_error", strerror(res));
		return;
	}

	watch_add(&jabber_plugin, fd, WATCH_READ, 1, jabber_handle_stream, data);

	jabber_write(j, "<?xml version=\"1.0\" encoding=\"utf-8\"?><stream:stream to=\"%s\" xmlns=\"jabber:client\" xmlns:stream=\"http://etherx.jabber.org/streams\">", j->server);

	j->id = 1;
	j->parser = XML_ParserCreate("UTF-8");
	XML_SetUserData(j->parser, data);	
	XML_SetElementHandler(j->parser, (XML_StartElementHandler) jabber_handle_start, (XML_EndElementHandler) xmlnode_handle_end);
	XML_SetCharacterDataHandler(j->parser, (XML_CharacterDataHandler) xmlnode_handle_cdata);
}

void jabber_handle_resolver(int type, int fd, int watch, void *data)
{
	session_t *s = data;
	jabber_private_t *j = jabber_private(data);
	struct in_addr a;
	int one = 1, res;
	const char *port_s = session_get(s, "port");
	int port = (port_s) ? atoi(port_s) : 5222;
	struct sockaddr_in sin;

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
		return;
	}

	debug("[jabber] resolved to %s\n", inet_ntoa(a));

	close(fd);

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[jabber] socket() failed: %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		return;
	}

	debug("[jabber] socket() = %d\n", fd);

	j->fd = fd;

	if (ioctl(fd, FIONBIO, &one) == -1) {
		debug("[jabber] ioctl() failed: %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		return;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = a.s_addr;

	debug("[jabber] connecting to %s:%d\n", inet_ntoa(sin.sin_addr), port);
	
	connect(fd, (struct sockaddr*) &sin, sizeof(sin));

	if (errno != EINPROGRESS) {
		debug("[jabber] connect() failed: %s\n", strerror(errno));
		print("generic_error", strerror(errno));
		return;
	}

	watch_add(&jabber_plugin, fd, WATCH_WRITE, 0, jabber_handle_connect, data);
}

COMMAND(jabber_command_connect)
{
	const char *password = session_get(session, "password");
	const char *server;
	int res, fd[2];
	jabber_private_t *j = session_private_get(session);
	
	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (j->connecting) {
		printq("generic_error", "Ale ³±czenie ju¿ trwa, szefie");
		return -1;
	}

	if (session_connected_get(session)) {
		printq("generic_error", "Jeste¶ ju¿ po³±czony, ziomu¶");
		return -1;
	}

	if (!password) {
		printq("generic_error", "Nale¿y ustawiæ has³o sesji");
		return -1;
	}

	debug("session->uid = %s\n", session->uid);

	if (!(server = strchr(session->uid, '@'))) {
		printq("generic_error", "Z³y id sesji. Nie ma serwera.");
		return -1;
	}

	xfree(j->server);
	j->server = xstrdup(++server);

	debug("[jabber] resolving %s\n", server);

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
			struct hostent *he = gethostbyname(server);

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

		/* XXX dodaæ dzieciaka do przegl±dania */

		watch_add(&jabber_plugin, fd[0], WATCH_READ, 0, jabber_handle_resolver, session);
	}

	j->connecting = 1;

	printq("generic", "£±czê siê z Jabberem, czekaj no...");

	if (!strcmp(session_status_get(session), EKG_STATUS_NA))
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
		printq("not_connected");
		return -1;
	}

	/* je¶li jest /reconnect, nie mieszamy z opisami */
	if (strcmp(name, "reconnect")) {
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
		printq("conn_stopped");
	} else
		printq("disconnected");

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
	
	if (j->connecting || session_connected_get(session))
		jabber_command_disconnect(name, params, session, target, quiet);

	return jabber_command_connect(name, params, session, target, quiet);
}

COMMAND(jabber_command_msg)
{
	jabber_private_t *j = session_private_get(session);
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

		if (strchr(uid, '@') && strchr(uid, '@') < strchr(uid, '.')) {			
			printq("user_not_found", params[0]);
			return -1;
		}
	} else {
		if (strncasecmp(uid, "jid:", 4)) {
			printq("invalid_session");
			return -1;
		}

		uid += 4;
	}
	
	/* czy wiadomo¶æ ma mieæ temat? */
	if (config_subject_prefix && !strncmp(params[1], config_subject_prefix, strlen(config_subject_prefix))) {
		/* obcinamy prefix tematu */
		subtmp = xstrdup((params[1]+strlen(config_subject_prefix)));

		/* je¶li ma wiêcej linijek, zostawiamu tylko pierwsz± */
		if (strchr(subtmp, 10)) *(strchr(subtmp, 10)) = 0;

		subject = jabber_escape(subtmp);
		/* body of wiadomo¶æ to wszystko po koñcu pierwszej linijki */
		msg = jabber_escape(strchr(params[1], 10)); 
		xfree(subtmp);
	} else 
		msg = jabber_escape(params[1]); /* bez tematu */

	jabber_write(j, "<message %sto=\"%s\">", (!strcasecmp(name, "chat")) ? "type=\"chat\" " : "", uid);

	if (subject) jabber_write(j, "<subject>%s</subject>", subject);

	jabber_write(j, "<body>%s</body></message>", msg);

	xfree(msg);
	xfree(subject);

	if (config_display_sent) {
		char *tmp = saprintf("jid:%s", uid);
		const char *rcpts[2] = { tmp, NULL };
		message_print(session_uid_get(session), session_uid_get(session), rcpts, params[1], NULL, time(NULL), EKG_MSGCLASS_SENT, NULL);
		xfree(tmp);
	}

	session_unidle(session);

	return 0;
}

COMMAND(jabber_command_inline_msg)
{
	const char *p[2] = { target, params[0] };

	return jabber_command_msg("chat", p, session, target, quiet);
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
	const char *descr;
	
	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
	}

	if (params[0])
		session_descr_set(session, (!strcmp(params[0], "-")) ? NULL : params[0]);

	descr = session_descr_get(session);

	if (!strcmp(name, "_autoback")) {
		printq("generic", "Automagicznie wracamy do ¿ywych");
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
		goto change;
	}

	if (!strcmp(name, "back")) {
		printq("generic", "Wracamy do ¿ywych");
		session_status_set(session, EKG_STATUS_AVAIL);
		session_unidle(session);
		goto change;
	}

	if (!strcmp(name, "_autoaway")) {
		printq("generic", "Automagicznie zajêty");
		session_status_set(session, EKG_STATUS_AUTOAWAY);
		goto change;
	}

	if (!strcmp(name, "away")) {
		printq("generic", "Zajêty");
		session_status_set(session, EKG_STATUS_AWAY);
		session_unidle(session);
		goto change;
	}

	if (!strcmp(name, "dnd")) {
		printq("generic", "Nie przeszkadzaæ");
		session_status_set(session, EKG_STATUS_DND);
		session_unidle(session);
		goto change;
	}

	if (!strcmp(name, "xa")) {
		printq("generic", "Ekstended e³ej");
		session_status_set(session, EKG_STATUS_XA);
		session_unidle(session);
		goto change;
	}

	if (!strcmp(name, "invisible")) {
		printq("generic", "Niewidoczny");
		session_status_set(session, EKG_STATUS_INVISIBLE);
		session_unidle(session);
		goto change;
	}

	printq("generic_error", "Ale o so chozi?");
	return -1;

change:
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
		printq("not_connected");
		return -1;
	}

        if (!params[0]) {
                printq("not_enough_params", name);
                return -1;
        }

	username = xstrdup(session->uid + 4);
	*(strchr(username, '@')) = 0;

	passwd = jabber_escape(params[0]);
	jabber_write(j, "<iq type=\"set\" to=\"%s\" id=\"passwd%d\"><query xmlns=\"jabber:iq:register\"><username>%s</username><password>%s</password></query></iq>", j->server, j->id++, username, passwd);
	xfree(passwd);
	
	session_set(session, "__new_password", params[0]);

	return 0;
}

/* 
   zarz±dzanie autoryzacjami
   s³u¿y równie¿ do usuwania u¿ytkowników (gdy wywo³ane z nazw± "jdel")
*/

COMMAND(jabber_command_auth) 
{
	jabber_private_t *j = session_private_get(session);
	const char *action;
	char *uid, *descr;

	if (!session_check(session, 1, "jid")) {
		printq("invalid_session");
		return -1;
        }

	if (!session_connected_get(session)) {
		printq("not_connected");
		return -1;
	}

	if (!params[1]) {
		printq("not_enough_params", name);
		return -1;
	}
	
	if (!(uid = get_uid(session, params[1]))) {
		uid = params[1];

		if (!(strchr(uid,'@') && strchr(uid, '@') < strchr(uid, '.'))) {
			printq("user_not_found", params[1]);
			return -1;
		}
	} else {
		if (strncasecmp(uid, "jid:", 4)) {
			printq("invalid_session");
			return -1;
		}

		/* user jest OK, wiêc lepiej mieæ go pod rêk± */
		tabnick_add(uid);

		uid += 4;
	};

	if (params[0] && match_arg(params[0], 'r', "request", 2)) {
		action = "subscribe";
		descr = saprintf("Wys³ano ¿±danie autoryzacji do %s", uid);
		goto success;
	}

	if (params[0] && match_arg(params[0], 'a', "accept", 2)) {
		action = "subscribed";
		descr = saprintf("Autoryzowano %s", uid);
		goto success;
	}

	if (params[0] && match_arg(params[0], 'c', "cancel", 2)) {
		action = "unsubscribed";
		descr = saprintf("Wys³ano pro¶bê o cofniêcie autoryzacji do %s", uid);
		goto success;
	}

	if (params[0] && (match_arg(params[0], 'd', "deny", 2) || !strcasecmp(name, "jdel")) ) {
		action = "unsubscribe";
		/* TODO: sprawdziæ, czy cofamy autoryzacje czy odmawiamy - czy mamy w rosterze*/
		descr = saprintf("Cofniêto/odmówiono autoryzacji %s", uid);
		goto success;
	};

	/* ha! undocumented :-); bo 
	   [Used on server only. Client authors need not worry about this.] */
	if (params[0] && match_arg(params[0], 'p', "probe", 2)) {
		action = "probe";
		descr = saprintf("Wys³ano pytanie o obecno¶æ do %s", uid);
		goto success;
	};

	goto  fail;
fail:
	printq("invalid_params", name);
	return -1;

success:
	jabber_write(j, "<presence to=\"%s\" type=\"%s\" id=\"roster\"/>", uid, action);
	printq("generic", descr);
	xfree(descr);
	return 0;
}

COMMAND(jabber_command_add)
{
 	jabber_private_t *j = session_private_get(session);
        char *uid, *tmp;

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
	
	uid = params[0]; 
	if (!strncasecmp(uid, "jid:", 4))
		uid += 4;

	jabber_write(j, "<iq type=\"set\" id=\"roster\"><query xmlns=\"jabber:iq:roster\">");
	if (params[1])
		jabber_write(j, "<item jid=\"%s\" name=\"%s\"/>", uid, jabber_escape(params[1]));
	else
		jabber_write(j, "<item jid=\"%s\"/>", uid);
	jabber_write(j, "</query></iq>");

	tmp = saprintf("/auth --request jid:%s", uid);
	command_exec(target, session, tmp, 0);
	xfree(tmp);
	
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

	command_add(&jabber_plugin, "jid:connect", "?", jabber_command_connect, 0, "", "³±czy siê z serwerem", "");
	command_add(&jabber_plugin, "jid:disconnect", "?", jabber_command_disconnect, 0, " [powód/-]", "roz³±cza siê od serwera", "");
	command_add(&jabber_plugin, "jid:reconnect", "", jabber_command_reconnect, 0, "", "roz³±cza i ³±czy siê ponownie", "");
	command_add(&jabber_plugin, "jid:msg", "??", jabber_command_msg, 0, "", "wysy³a pojedyncz± wiadomo¶æ", "\nPoprzedzenie wiadomo¶ci wielolinijkowej ci±giem zdefiniowanym w zmiennej subject_string spowoduje potraktowanie pierwszej linijki jako tematu.");
	command_add(&jabber_plugin, "jid:chat", "??", jabber_command_msg, 0, "", "wysy³a wiadomo¶æ w ramach rozmowy", "");
	command_add(&jabber_plugin, "jid:", "?", jabber_command_inline_msg, 0, "", "wysy³a wiadomo¶æ", "");
	command_add(&jabber_plugin, "jid:xml", "?", jabber_command_xml, 0, "", "wysy³a polecenie xml", "\nPolecenie musi byæ zakodowanie w UTF-8, a wszystkie znaki specjalne u¿ywane w XML (\" ' & < >) musz± byæ zamienione na odpowiadaj±ce im sekwencje.");
	command_add(&jabber_plugin, "jid:away", "?", jabber_command_away, 0, "", "zmienia stan na zajêty", "");
	command_add(&jabber_plugin, "jid:_autoaway", "?", jabber_command_away, 0, "", "zmienia stan na zajêty", "");
	command_add(&jabber_plugin, "jid:back", "?", jabber_command_away, 0, "", "zmienia stan na dostêpny", "");
	command_add(&jabber_plugin, "jid:_autoback", "?", jabber_command_away, 0, "", "zmienia stan na dostêpny", "");
	command_add(&jabber_plugin, "jid:invisible", "?", jabber_command_away, 0, "", "zmienia stan na zajêty", "");
	command_add(&jabber_plugin, "jid:dnd", "?", jabber_command_away, 0, "", "zmienia stan na dostêpny", "");
	command_add(&jabber_plugin, "jid:xa", "?", jabber_command_away, 0, "", "zmienia stan na dostêpny", "");
	command_add(&jabber_plugin, "jid:passwd", "?", jabber_command_passwd, 0, "", "zmienia has³o", "");
	command_add(&jabber_plugin, "jid:auth", "??", jabber_command_auth, 0, "", "obs³uga autoryzacji", 
	  "<akcja> <JID> \n"
	  "  -a, --accept <JID>    autoryzuje JID\n"
	  "  -d, --deny <JID>      odmawia udzielenia autoryzacji\n"
 	  "  -r, --request <JID>   wysy³a ¿±danie autoryzacji\n"
	  "  -c, --cancel <JID>    wysy³a ¿±danie cofniêcia autoryzacji\n");
	command_add(&jabber_plugin, "jid:jadd", "??", jabber_command_add, 0, "", "dodaje u¿ytkownika do naszego rostera, jednocze¶nie prosz±c o autoryzacjê", "<JID> [nazwa]");
	command_add(&jabber_plugin, "jid:jdel", "u", jabber_command_auth, 0, "", "usuwa z naszego rostera", "");

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
