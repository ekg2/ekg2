/*
 *  (C) Copyright 2006 Jakub Zawadzki <darkjames@darkjames.ath.cxl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/vars.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include "feed.h"

typedef enum {
	NNTP_IDLE = 0,
	NNTP_CHECKING,
	NNTP_DOWNLOADING,
} nntp_newsgroup_state_t;

typedef struct {
	int artid;
	char *msgid;
	int new;
	string_t header;
	string_t body;
} nntp_article_t;

typedef struct {
	char *uid;
	char *name;
	nntp_newsgroup_state_t state;

	int article;	/* current */

	int fart;	/* first article in the group		*/
	int cart;	/* current artcile (downloading)	*/
	int lart;	/* last article 			*/
	list_t articles;/* list of articles, nntp_article_t	*/
} nntp_newsgroup_t;

typedef struct {
	int connecting;
	int fd;
	int lock;
	int authed;

	int last_code;			/* last code */
	nntp_newsgroup_t *newsgroup;	/* current newsgroup */

	string_t buf;
	list_t newsgroups;

	watch_t *send_watch;
} nntp_private_t;

static nntp_article_t *nntp_article_find(nntp_newsgroup_t *group, int articleid, char *msgid) {
	nntp_article_t *article;
	list_t l;

	for (l = group->articles; l; l = l->next) {
		article = l->data;

		if (article->artid == articleid) {
			if (!article->msgid && msgid) article->msgid = xstrdup(msgid);
			return article;
		}
	}
	article		= xmalloc(sizeof(nntp_article_t));
	article->new	= 1;
	article->artid 	= articleid;
	article->msgid	= xstrdup(msgid);
	article->header	= string_init(NULL);
	article->body	= string_init(NULL);

	list_add(&group->articles, article, 0);
	return article;
}

static nntp_newsgroup_t *nntp_newsgroup_find(session_t *s, const char *name) {
	nntp_private_t *j = feed_private(s);
	list_t l;
	nntp_newsgroup_t *newsgroup;

	for (l = j->newsgroups; l; l = l->next) {
		newsgroup = l->data;

		debug("nntp_newsgroup_find() %s %s\n", newsgroup->name, name);
		if (!xstrcmp(newsgroup->name, name)) 
			return newsgroup;
	}
	debug("nntp_newsgroup_find() 0x%x NEW %s\n", j->newsgroups, name);

	newsgroup	= xmalloc(sizeof(nntp_newsgroup_t));
	newsgroup->uid	= saprintf("nntp:%s", name);
	newsgroup->name = xstrdup(name);

	list_add(&(j->newsgroups), newsgroup, 0);
	return newsgroup;
}

static void nntp_handle_disconnect(session_t *s, const char *reason, int type) {
	nntp_private_t *j = feed_private(s);

	if (!j)
		return;

	if (j->connecting)
		watch_remove(&feed_plugin, j->fd, WATCH_WRITE);

	if (j->send_watch) {
		j->send_watch->type = WATCH_NONE;
		watch_free(j->send_watch);
		j->send_watch = NULL;
	}

	j->newsgroup = NULL;

	j->last_code	= -1;
	j->authed	= 0;

	session_connected_set(s, 0);
	j->connecting = 0;
	close(j->fd);
	j->fd = -1;

	{
		char *__session = xstrdup(session_uid_get(s));
		char *__reason = xstrdup(reason);

		query_emit(NULL, "protocol-disconnected", &__session, &__reason, &type, NULL);

		xfree(__session);
		xfree(__reason);
	}
}

typedef struct {
	char *session;
	char *filename;
	char *newsgroup;
	char *subject;

	time_t last_mtime;
} nntp_children_t;

static void nntp_children_died(struct child_s *c, int pid, const char *name, int status, void *data) {
	nntp_children_t *d = data;
	session_t *s = session_find(d->session);
	nntp_private_t *j;
	struct stat st;
	FILE *f;

	if (!s || !s->priv) {
		print("nntp_posting_failed", session_name(s), d->newsgroup, "session not found", d->filename);
		goto fail;
	}

	if ((stat(d->filename, &st) != 0)) {
		print("nntp_posting_failed", session_name(s), d->newsgroup, "fstat() failed", d->filename);
		goto fail;
	}

	if (st.st_ctime <= d->last_mtime) {
		print("nntp_posting_failed", session_name(s), d->newsgroup, "mtime not changed", d->filename);
		goto fail;
	}

	print("nntp_posting", session_name(s), d->newsgroup, d->subject);

	j = feed_private(s);

fail:
	xfree(d->session);
	xfree(d->filename);
	xfree(d->newsgroup);
	xfree(d->subject);
	xfree(d);
}

#define NNTP_HANDLER(x) static int x(session_t *s, int code, char *str, void *data) 
typedef int (*nntp_handler) (session_t *, int, char *, void *);


NNTP_HANDLER(nntp_help_process) {			/* 100 */
	debug("nntp_help_process() %s\n", str);

//	format_add("nntp_command_help_header",	_("%g,+=%G----- %2 %n(%T%1%n)"), 1);
//	format_add("nntp_command_help_item",	_("%g|| %W%1: %n%2"), 1);
//	format_add("nntp_command_help_footer",	_("%g`+=%G----- End of 100%n\n"), 1);
	return 0;
}

static char hextochar(char t) {
	if (t >= '0' && t <= '9')
		return t - '0';
	else if (t >= 'A' && t <= 'F')
		return 10+(t - 'A');
	else if (t >= 'a' && t <= 'f') 
		return 10+(t - 'a');
	debug("hextochar() invalid char: %d\n", t);
	return 0;
}

NNTP_HANDLER(nntp_message_process) {			/* 220, 221, 222 */
	nntp_private_t *j 	= feed_private(s);
	int article_headers	= (code == 220 || code == 221);
	int article_body	= (code == 220 || code == 222);
	char *mbody, **tmpbody;

	nntp_article_t *art = NULL;
	
	if (!(mbody = split_line(&str))) return -1;

	tmpbody = array_make(mbody, " ", 3, 1, 0);		/* header [id <message-id> type] */
	
	if (!tmpbody || !tmpbody[0] || !tmpbody[1] || !tmpbody[2]) {
		debug("nntp_message_process() tmpbody? mbody: %s\n", mbody);
		array_free(tmpbody);
		return -1;
	}

	if (!(art = nntp_article_find(j->newsgroup, atoi(tmpbody[0]), tmpbody[1]))) {
		debug("nntp_message_process nntp_article_find() failed\n");
		array_free(tmpbody);
		return -1;
	}
	
	if (article_headers) 	string_clear(art->header);
	if (article_body)	string_clear(art->body);

	if (article_headers && article_body) {
		char *tmp;
		if ((tmp = xstrchr(str, '\r'))) {
			string_append_n(art->header, str, tmp-str-1);
			str = tmp + 2;		/* +\r\n */
		} else {
			debug("ERROR, It's really article_headers with article_body?!\n");
		}
	} else if (article_headers) 
		string_append_n(art->header, str, xstrlen(str)-1);	/* don't add ending \n */

	if (article_body)
		string_append_n(art->body, str, xstrlen(str)-1);	/* don't add ending \n */


	if (article_body && article_headers) do {
		enum {
			ENCODING_UNKNOWN = 0,
			ENCODING_BASE64,
			ENCODING_QUOTEDPRINTABLE,
			ENCODING_8BIT,
		} cte = ENCODING_UNKNOWN;

		char *encoding;
		char *tmp; 

		char *text;
		int i = 0;

		/* Content-Transfer-Encoding */
		if ((tmp = xstrstr(art->header->str, "Content-Transfer-Encoding: "))) { /* base64 || quoted-printable || 8bit || .... */
			char *value = xstrchr(tmp, ' ')+1;

			if (!xstrncmp(value, "8bit", 4)) 		cte = ENCODING_8BIT;
			if (!xstrncmp(value, "base64", 6))		cte = ENCODING_BASE64;
			if (!xstrncmp(value, "quoted-printable", 16))	cte = ENCODING_QUOTEDPRINTABLE;
		}
		debug("encoding type: %d\n", cte);
/* XXX, console_charset + iconv... */
		if (cte == ENCODING_UNKNOWN);

		text = string_free(art->body, 0);

		art->body = string_init(NULL);

		while (text[i]) {
			switch (cte) {
				case ENCODING_QUOTEDPRINTABLE:
					if (text[i] == '=' && text[i+1] && text[i+2]) {
						string_append_c(art->body, hextochar(text[i+1]) * 16 | hextochar(text[i+2]));
						i += 2;
					} else	string_append_c(art->body, text[i]);
					break;
				case ENCODING_8BIT:
				default:
					string_append_c(art->body, text[i]);
			}
			i++;
		}
		xfree(text);
	} while(0);

	
	{
		char *uid	= j->newsgroup		? j->newsgroup->uid 	: NULL;
		char *sheaders	= NULL;
		char *headers	= article_headers	? art->header->str 	: NULL;
		char *body	= article_body		? art->body->str	: NULL;
		char *artid	= (char *) itoa(art->artid);
		int modify	= 0;						/* XXX */

		query_emit(NULL, "rss-message", &(s->uid), &uid, &sheaders, &headers, &artid, &(art->msgid), &body, &(art->new), &modify);
	}

	if (j->newsgroup) {
		j->newsgroup->state = NNTP_IDLE;
	} else debug("nntp_message_process() j->newsgroup == NULL!!!!\n");

	array_free(tmpbody);
	return 0;
}

NNTP_HANDLER(nntp_auth_process) {
	nntp_private_t *j 	= feed_private(s);
	char *tmp;

	switch(code) {
		case 200: 
		case 201: 
			tmp = s->status;
			if (code == 200)	s->status = xstrdup(EKG_STATUS_AVAIL);
			else			s->status = xstrdup(EKG_STATUS_AWAY);
			xfree(tmp);

			tmp = s->descr;
			s->descr = xstrdup(str);
			xfree(tmp);

			if (!j->authed && session_get(s, "username"))
				watch_write(j->send_watch, "AUTHINFO USER %s\r\n", session_get(s, "username"));
			break;
		case 381:	
			watch_write(j->send_watch, "AUTHINFO PASS %s\r\n", session_get(s, "password"));		
			break;
		case 281:	
			j->authed = 1; 
			break;
		case 480:		/* XXX, auth required */
			break;
	}
	return 0;
}

NNTP_HANDLER(nntp_null_process) {
	debug("nntp_null_process() `%s`\n... %s\n", data, str);
	return 0;
}

NNTP_HANDLER(nntp_group_process) {
	nntp_private_t *j 	= feed_private(s);
	char **p = array_make(str, " ", 4, 1, 0);
	nntp_newsgroup_t *group; 
	userlist_t *u;

	if (!p) return -1;
		/* 211 n f l s group selected */
	debug("nntp_group_process() str:%s p[0]: %s p[1]: %s p[2]: %s p[3]: %s p[4]: %s\n", str, p[0], p[1], p[2], p[3], p[4]);

	group		= nntp_newsgroup_find(s, p[3]);
	group->fart	= atoi(p[1]);
	group->lart	= atoi(p[2]);
	if (!group->cart) group->cart = group->lart;

	if ((u = userlist_find(s, group->uid))) {
		if (!xstrcmp(u->status, EKG_STATUS_AWAY)) {
			feed_set_descr(u, saprintf("First article: %d Last article: %d", group->fart, group->lart));
		}
	}

	j->newsgroup	= group;
	group->state	= NNTP_IDLE;

	array_free(p);
	return 0;
}

NNTP_HANDLER(nntp_message_error) {
	nntp_private_t *j       = feed_private(s);

	if (!j->newsgroup) 	return -1;

	j->newsgroup->state	= NNTP_IDLE;
	return 0;
}

NNTP_HANDLER(nntp_group_error) {
	nntp_private_t *j       = feed_private(s);

	if (!j->newsgroup) return -1;

	feed_set_statusdescr(userlist_find(s, j->newsgroup->uid), xstrdup(EKG_STATUS_ERROR), saprintf("Generic error %d: %s", code, str));

	j->newsgroup->state	= NNTP_IDLE;
	j->newsgroup		= NULL;

	return 0;
}

NNTP_HANDLER(nntp_xover_process) {
	debug("xover: %s\n", str);
	return 0;
}

typedef	struct {
	int 		num;
	nntp_handler	handler;
	int is_multi;
	void *data;
} nntp_handler_t;

nntp_handler_t nntp_handlers[] = {
	{100, nntp_help_process,	1, NULL}, 
	{200, nntp_auth_process,	0, NULL},
	{201, nntp_auth_process,	0, NULL},
	{281, nntp_auth_process,	0, NULL}, 
	{381, nntp_auth_process,	0, NULL}, 
	{480, nntp_auth_process,	0, NULL}, 

	{220, nntp_message_process, 	1, NULL},
	{221, nntp_message_process,	1, NULL}, 
	{222, nntp_message_process,	1, NULL},
	{423, nntp_message_error,	0, NULL}, 

	{211, nntp_group_process,	0, NULL}, 
	{411, nntp_group_error,		0, NULL}, 

	{224, nntp_xover_process,	1, "xover"}, 

	{282, nntp_null_process,	1, "xgitle"}, 
	{-1, NULL, 			0, NULL},
}; 

static void nntp_string_append(session_t *s, const char *str) {
	nntp_private_t *j       = feed_private(s);
	string_t buf            = j->buf;

	string_append(buf, str);
	string_append_c(buf, '\n');
}

static nntp_handler_t *nntp_handler_find(int code) {
	int i;
	for (i = 0; nntp_handlers[i].num != -1; i++) {
		if (nntp_handlers[i].num == code) return &(nntp_handlers[i]);
	}
	return NULL;
}

static WATCHER_LINE(nntp_handle_stream) {
	session_t *s = session_find(data);
	nntp_private_t *j = feed_private(s);

	char **p;

	if (type == 1) {
		nntp_handle_disconnect(s, strerror(errno), EKG_DISCONNECT_NETWORK);
		return 0;
	}

	if (!watch || !s) return -1;

	if (j->last_code != -1) {
		nntp_handler_t *handler = nntp_handler_find(j->last_code);

		if (!xstrcmp(watch, ".")) {
			int res = -1;

			if (handler && handler->is_multi) res = handler->handler(s, j->last_code, j->buf->str, handler->data);

			debug("nntp_handlers() retval: %d code: %d\n", res, j->last_code);

			string_clear(j->buf);
			j->last_code = -1;
			if (res != -1) return 0;
		}

		if (handler && handler->is_multi) {
			nntp_string_append(s, watch);
			return 0;
		}
	} 

	if ((p = array_make(watch, " ", 2, 1, 0)) && p[0] && atoi(p[0])) {
		int code = atoi(p[0]);

		nntp_handler_t *handler = nntp_handler_find(code);

		if (handler && handler->is_multi) {
			nntp_string_append(s, p[1]);
			j->last_code = code;
		} else if (handler) {
			handler->handler(s, code, p[1], handler->data);
			j->last_code = code;
		} else {
			debug("nntp_handle_stream() unhandled: %d (%s)\n", code, p[1]);
		}
	} else {
		debug("nntp_handle_stream() buf: %s (last: %d)\n", watch, j->last_code);
	}
	array_free(p);

	return 0;
}

static WATCHER(nntp_handle_connect) {
	session_t *s = session_find(data);
	nntp_private_t *j = feed_private(s);
	int res = 0, res_size = sizeof(res);

	debug("nntp_handle_connect() type: %d\n", type);

	if (type) {
		xfree(data);
		return 0;
	}

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		nntp_handle_disconnect(s, strerror(res), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	j->connecting = 0;
	session_connected_set(s, 1);
	query_emit(NULL, "protocol-connected", &data);

	watch_add_line(&feed_plugin, fd, WATCH_READ_LINE, nntp_handle_stream, xstrdup(data));
	j->send_watch = watch_add_line(&feed_plugin, fd, WATCH_WRITE_LINE, NULL, NULL);
	return -1;
}

static COMMAND(nntp_command_disconnect)
{
	nntp_private_t	*j = feed_private(session);

	if (!j->connecting && !session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (session_connected_get(session))
		watch_write(j->send_watch, "QUIT\r\n");

	if (j->connecting)
		nntp_handle_disconnect(session, NULL, EKG_DISCONNECT_STOPPED);
	else
		nntp_handle_disconnect(session, NULL, EKG_DISCONNECT_USER);

	return 0;
}

static COMMAND(nntp_command_connect) {
	nntp_private_t *j = feed_private(session);
	struct sockaddr_in sin;
	/* just proof of concpect... no checking for errors... no resolving etc... it's boring */
	int fd, res;
	const char *ip = session_get(session, "server");
	int one = 1;

	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}
	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	j->fd = fd = socket(AF_INET, SOCK_STREAM, 0);

        sin.sin_family		= AF_INET;
        sin.sin_addr.s_addr	= inet_addr(ip);
	sin.sin_port		= ntohs(session_int_get(session, "port"));

        ioctl(fd, FIONBIO, &one);

	j->connecting = 1;
        res = connect(fd, (struct sockaddr*) &sin, sizeof(sin));

	if (res && (errno != EINPROGRESS)) {
		nntp_handle_disconnect(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	watch_add(&feed_plugin, fd, WATCH_WRITE, nntp_handle_connect, xstrdup(session->uid));
	return 0;
}

static COMMAND(nntp_command_raw) {
	nntp_private_t *j = feed_private(session);
	watch_write(j->send_watch, "%s\r\n", params[0]);
	return 0;
}

static COMMAND(nntp_command_nextprev) {
	nntp_private_t *j = feed_private(session);
	int mode = session_int_get(session, "display_mode");

	if (!j->newsgroup) {
		wcs_printq("invalid_params", name);
		return -1;
	}
	if (!xstrcmp(name, "next")) 	j->newsgroup->article++;
	else				j->newsgroup->article--;

	if (mode == 2)				watch_write(j->send_watch, "HEAD %d\r\n", j->newsgroup->article);
	else if (mode == 3 || mode == 4)	watch_write(j->send_watch, "ARTICLE %d\r\n", j->newsgroup->article);
	else if (mode == 0 || mode == -1)	;
	else					watch_write(j->send_watch, "BODY %d\r\n", j->newsgroup->article);

	return 0;
}

static COMMAND(nntp_command_get) {
	nntp_private_t *j = feed_private(session);
	const char *comm = "ARTICLE";
	const char *group = NULL, *article = NULL;
	nntp_article_t *art = NULL;

	if (params[0] && params[1])	{ group = params[0]; article = params[1]; }
	else 				{ article = params[0]; }

	if (!group && target) 		group = target;
	if (!group && j->newsgroup)	group = j->newsgroup->uid;

	if (!article) {
		wcs_printq("invalid_params", name);
		return -1;
	}

	if (!group) {
		/* no group */
		wcs_printq("invalid_params", name);
		return -1;
	}

	if (!xstrncmp(group, "nntp:", 5)) group = group+5;	/* skip nntp: if exists */

	if (!j->newsgroup || xstrcmp(j->newsgroup->name, group)) {
/* zmienic grupe na target jesli != aktualnej .. */
		j->newsgroup = nntp_newsgroup_find(session, group);
		watch_write(j->send_watch, "GROUP %s\r\n", group);
	}

	j->newsgroup->article = atoi(article);

				art = nntp_article_find(j->newsgroup, j->newsgroup->article, NULL);
	if (!art->new)		art->new = 3;	/* turn on display flag. */
			/* XXX, wyswietlic artykul z kesza ? */

	if (!xstrcmp(name, "body")) comm = "BODY";

	watch_write(j->send_watch, "%s %s\r\n", comm, article);
	return 0;
}

static COMMAND(nntp_command_check) {
	extern void ekg_loop();

	nntp_private_t *j = feed_private(session);

	list_t l;
	if (j->lock) {
		debug("nntp_command_check() j->lock = 1\n");	/* XXX, usleep ? czy please try again later ? */
		return 0;
	}
	j->lock = 1;

	for (l = session->userlist; l; l = l->next) {
		userlist_t *u 		= l->data;
		nntp_newsgroup_t *n;
		int i;

		if (params[0] && xstrcmp(params[0], u->uid)) continue;

		n = nntp_newsgroup_find(session, u->uid+5);
	
		feed_set_statusdescr(u, xstrdup(EKG_STATUS_AWAY), xstrdup("Checking..."));

		j->newsgroup	= n;
		n->state 	= NNTP_CHECKING;
		watch_write(j->send_watch, "GROUP %s\r\n", n->name);

		while (n->state == NNTP_CHECKING) ekg_loop();
		if (!xstrcmp(u->status, EKG_STATUS_ERROR)) continue;

		if (n->cart == n->lart) {	/* nothing new */
			feed_set_status(u, xstrdup(EKG_STATUS_DND));
			continue;
		}

		for (i = n->cart+1; i <= n->lart; i++) {
			int mode = session_int_get(session, "display_mode");

			n->state	= NNTP_DOWNLOADING;
			j->newsgroup	= n;
			feed_set_descr(u, saprintf("Downloading %d article from %d", i, n->lart));

			if (mode == 2)				watch_write(j->send_watch, "HEAD %d\r\n", i);
			else if (mode == 3 || mode == 4)	watch_write(j->send_watch, "ARTICLE %d\r\n", i);
			else if (mode == 0 || mode == -1)	;
			else					watch_write(j->send_watch, "BODY %d\r\n", i);

			while (n->state == NNTP_DOWNLOADING) ekg_loop();
		}
		n->state		= NNTP_IDLE;
		
		feed_set_statusdescr(u, xstrdup(EKG_STATUS_AVAIL), saprintf("%d new articles", n->lart - n->cart));
		j->newsgroup->cart = n->lart;

		if (params[0]) break;
	}
	j->lock = 0;
	return 0;
}

static COMMAND(nntp_command_subscribe) {
	userlist_t *u;

	if ((u = userlist_find(session, target))) {
		printq("feed_subcribe_already", target);
		return -1;
	}

	userlist_add(session, target, target);
	return 0;
}

static COMMAND(nntp_command_unsubscribe) {
	userlist_t *u; 
	if (!(u = userlist_find(session, target))) {
		printq("feed_subscribe_no", target);
		return -1;
	}
	userlist_remove(session, u);
	return 0;
}

void *nntp_protocol_init() {
	nntp_private_t *p 	= xmalloc(sizeof(nntp_private_t));
	p->buf			= string_init(NULL);
	return p;
}

void nntp_protocol_deinit(void *priv) {

}

void nntp_init() {
	plugin_var_add(&feed_plugin, "username", VAR_STR, 0, 0, NULL);
        plugin_var_add(&feed_plugin, "password", VAR_STR, "foo", 1, NULL);
	plugin_var_add(&feed_plugin, "server", VAR_STR, 0, 0, NULL);
	plugin_var_add(&feed_plugin, "port", VAR_INT, "119", 0, NULL);

/*XXX,  :msg -- wysylanie wiadomosc na serwer... BE CAREFULL cause news aren't IM ;) */

	command_add(&feed_plugin, ("nntp:connect"), "?",	nntp_command_connect, RSS_ONLY, NULL);
	command_add(&feed_plugin, ("nntp:disconnect"), "?", nntp_command_disconnect, RSS_ONLY, NULL);

	command_add(&feed_plugin, ("nntp:subscribe"), "!",	nntp_command_subscribe, RSS_FLAGS_TARGET, NULL); 
	command_add(&feed_plugin, ("nntp:unsubscibe"), "!", nntp_command_unsubscribe, RSS_FLAGS_TARGET, NULL);

	command_add(&feed_plugin, ("nntp:check"), "u", 	nntp_command_check, RSS_FLAGS, NULL);

	command_add(&feed_plugin, ("nntp:article"), "? ?",	nntp_command_get, RSS_FLAGS, NULL);
	command_add(&feed_plugin, ("nntp:body"),	"? ?",	nntp_command_get, RSS_FLAGS, NULL);
	command_add(&feed_plugin, ("nntp:raw"), "?", 	nntp_command_raw, RSS_FLAGS, NULL);

	command_add(&feed_plugin, ("nntp:next"), "?", 	nntp_command_nextprev, RSS_FLAGS, NULL);
	command_add(&feed_plugin, ("nntp:prev"), "?", 	nntp_command_nextprev, RSS_FLAGS, NULL);
}

