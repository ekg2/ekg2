
/*
 *  (C) Copyright 2007	Michał Górny & EKG2 authors
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

#ifdef HAVE_EXPAT_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <expat.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/queries.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

	/* jogger.c */
extern plugin_t jogger_plugin;

typedef enum {
	JOGGER_FINISHED		= -2,	/* like below, but happy suicide */
	JOGGER_DISABLED		= -1,	/* parser is going to suicide */
	JOGGER_RESPONSE		= 0,	/* waiting for response */
	JOGGER_HEADERS,			/* seeking through headers */
	JOGGER_XML,			/* parsing XML */
	JOGGER_TITLE,			/* precaching <title/> */
	JOGGER_FOUND,			/* inside right <entry/>! */
	JOGGER_CONTENT,			/* inside <content/> of right <entry/> */
} jogger_feed_mode_t;

typedef struct {
	session_t	*session;
	int		eid;
	char		*url;

	jogger_feed_mode_t mode;

	watch_t		*watch;		/* we could use union right here, but is it worth it? */
	XML_Parser	parser;
	string_t	title,
			content,
			categories;
} jogger_feed_t;

list_t jogger_feeds = NULL;

	/* hardcoded IP address is not such a good thing, but first we need
	 * to implemented resolver-delayed message queuing, so that if we
	 * receive new entry notifications before resolver finishes, then
	 * we don't skip entry fetching */
static struct in_addr jogger_ip = { 0xfb7f6ed9 };

	/* here we will look for data associated with specific session
	 * and set them to disabled */
void jogger_feeds_cleanup(session_t *s) {
	list_t l;

	for (l = jogger_feeds; l; l = l->next) {
		jogger_feed_t *priv = l->data;

		if (priv->session == s)
			priv->mode = JOGGER_DISABLED; /* removed */
	}
}

void jogger_feed_send(jogger_feed_t *priv) {
	char *title		= string_free(priv->title, 0);
	char *categories	= string_free(priv->categories, 0);
	char *content		= string_free(priv->content, 0);
	char *msg		= saprintf("[ %s ]\n( %s )\n\n%s", title ? title : _("(untitled)"), categories ? categories+2 : _("[no categories]"), content);
	char *uid		= saprintf("jogger:%d", priv->eid);

	const char *suid	= session_uid_get(priv->session);
	const char **rcpts	= NULL;
	const uint32_t *fmt	= NULL;
	const time_t sent	= time(NULL);
	const int class		= EKG_MSGCLASS_MESSAGE;
	const char *seq		= NULL;
	const int dobeep	= 0;
	const int secure	= 0;

	xfree(title);
	xfree(categories);
	xfree(content);

	query_emit_id(NULL, PROTOCOL_MESSAGE, &suid, &uid, &rcpts, &msg, &fmt, &sent, &class, &seq, &dobeep, &secure);

	xfree(msg);
	xfree(uid);
}

	/* here we have expat handlers */
void jogger_feed_elem(jogger_feed_t *priv, const char *name, const char **attrs) {
	if (priv->mode == JOGGER_TITLE || priv->mode == JOGGER_CONTENT)
		priv->mode--;

	if (priv->mode >= JOGGER_FOUND) { /* inside right entry */
		if (!xstrcmp(name, "content")) {
			string_clear(priv->content);
			priv->content = string_init(NULL);
			priv->mode = JOGGER_CONTENT;
		} else if (!xstrcmp(name, "category")) {
			while (attrs[0]) {
				if (!xstrcmp(attrs[0], "label")) {
						/* instead of checking someway if it's first category every time,
						 * we just use +2 when fetching content */
					string_append(priv->categories, "; ");
					string_append(priv->categories, attrs[1]);
					break;
				}
				
				attrs += 2;
			}
		} else if (!xstrcmp(name, "entry")) {
			jogger_feed_send(priv);

			priv->mode = JOGGER_FINISHED;
			XML_StopParser(priv->parser, XML_FALSE);
		}
	} else {
		if (!xstrcmp(name, "title")) { /* we have to precache title */
			string_clear(priv->title);
			priv->mode = JOGGER_TITLE;
		} else if (!xstrcmp(name, "link")) { /* compare link */
			while (attrs[0]) {
				if (!xstrcmp(attrs[0], "href")) {
					if (!xstrcmp(attrs[1], priv->url)) { /* found! */
						priv->mode = JOGGER_FOUND;
						break;
					}
				}

				attrs += 2;
			}
		} else if (!xstrcmp(name, "entry"))
			string_clear(priv->title);
	}
}

void jogger_feed_char(jogger_feed_t *priv, const char *text, int len) {
	string_t s = NULL;

	if (priv->mode == JOGGER_TITLE)
		s = priv->title;
	else if (priv->mode == JOGGER_CONTENT)
		s = priv->content;

	if (s)
		string_append_n(s, text, len);
}

	/* here we will try to read & parse server response */
WATCHER_LINE(jogger_feed_response) {
	jogger_feed_t *priv	= data;

	if (type) {
		close(fd);
		if (priv->parser) {
			if (priv->mode != JOGGER_FINISHED && XML_Parse(priv->parser, NULL, 0, 1) != XML_STATUS_OK)
				debug_error("[jogger] jogger_feed_response(), finalization error: %s\n", XML_ErrorString(XML_GetErrorCode(priv->parser)));
			else
				debug("[jogger] jogger_feed_response(), parsing finished\n");
			XML_ParserFree(priv->parser);
		}
		xfree(priv->url);
		if (priv->mode != JABBER_FINISHED) {
			string_free(priv->title, 1);
			string_free(priv->categories, 1);
			string_free(priv->content, 1);
		}
		list_remove(&jogger_feeds, priv, 1);

		return -1;
	}

	switch (priv->mode) {
		case JOGGER_FINISHED:
		case JOGGER_DISABLED:
			close(fd);
			return -1;
		case JOGGER_RESPONSE:
			debug("[jogger] jogger_feed_response(), got response\n");
			priv->watch->type = WATCH_NONE;
			watch_free(priv->watch);

			if (!xstrstr(watch, "200"))
				debug_error("[jogger] jogger_feed_response(), unexpected response: %s\n", watch);

			priv->mode++;
			break;
		case JOGGER_HEADERS:
				/* XXX: check Content-Type? */
			if (!*watch || *watch == '\r') {
				priv->parser	= XML_ParserCreate(NULL);
				priv->title	= string_init(NULL);
				priv->categories= string_init(NULL);
				priv->content	= string_init(NULL);
				priv->mode++;
				XML_SetUserData(priv->parser, priv);
				XML_SetStartElementHandler(priv->parser, (XML_StartElementHandler) &jogger_feed_elem);
				XML_SetCharacterDataHandler(priv->parser, (XML_CharacterDataHandler) &jogger_feed_char);

				debug("[jogger] jogger_feed_response(), parsing response\n");
			}
			break;
		default: { /* data */
			const int len = xstrlen(watch);
			((char*) watch)[len] = '\n'; /* yeah, that's dirty & evil, but I know we can do this for a moment */

			if (XML_Parse(priv->parser, watch, len + 1, 0) != XML_STATUS_OK && priv->mode != JOGGER_FINISHED) {
				((char*) watch)[len] = 0;
				debug_error("[jogger] jogger_feed_response(), parsing error: %s, in:\n%s\n", XML_ErrorString(XML_GetErrorCode(priv->parser)), watch);
				return -1;
			}
			((char*) watch)[len] = 0;
		}
	}
	
	return 0;
}

	/* here we put request into the socket */
WATCHER(jogger_feed_request) {
	jogger_feed_t *priv	= data;
	char *p, *q;

	if (type) {
		if (type == 2) { /* timeout */
			debug_error("[jogger] jogger_feed_request(), timeout\n");
			xfree(priv->url);
			list_remove(&jogger_feeds, priv, 1);
		}
		return -1;
	}

	if (!(p = xstrchr(priv->url, '/')) || !(q = xstrchr((p += 2),  '/'))) {
		debug_error("[jogger] jogger_feed_request(), wrong url: %s, wtf?!\n", priv->url);
		return -1;
	}

	{
		const int etf = session_int_get(priv->session, "entries_try_fetch");
		if (!etf) /* user can change etf value between commencing socket open and its' opening */
			return -1;

		priv->watch = watch_add_line(&jogger_plugin, fd, WATCH_WRITE_LINE, NULL, priv);

		*q = 0;
			/* use of HTTP/1.1 makes Jogger use some funky transfer encoding,
			 * so we just use 1.0 with some 1.1 headers */
		watch_write(priv->watch, "GET /atom/content/%d HTTP/1.0\r\n"
				"Host: %s\r\nUser-Agent: EKG2 (Mozilla-incompatible, Opera rox)\r\n"
				"Accept: application/atom+xml, application/xml; q=0.8, "
				"text/xml; q=0.5, */*; q=0.1\r\nAccept-Charset: UTF-8\r\n\r\n",
				etf, p);
		*q = '/';

		watch_add_line(&jogger_plugin, fd, WATCH_READ_LINE, &jogger_feed_response, priv);
	}

	return -1;
}

	/* we really need to delay querying at least 30 seconds to get feed updated */
TIMER(jogger_feed_timer) {
	
	if (type)
		return -1;

	{
		jogger_feed_t *priv	= data;
		const int fd		= socket(PF_INET, SOCK_STREAM, 0);

		{
			struct sockaddr_in sin;

			sin.sin_family	= PF_INET;
			sin.sin_addr	= jogger_ip;
			sin.sin_port	= htons(80);

			int one = 1;
				/* ok, all those magic stolen from jabber plugin */

			if ((fd == -1) || (ioctl(fd, FIONBIO, &one) == -1)
					|| (connect(fd, (struct sockaddr*) &sin, sizeof(sin)) == 1 && errno != EINPROGRESS)) {
				debug_error("[jogger] jogger_feed_timer(), connecting failed: %s\n", strerror(errno));
				xfree(priv->url);
				xfree(priv);
				return -1;
			}

			setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
		}

		{
			watch_t *w = watch_add(&jogger_plugin, fd, WATCH_WRITE, &jogger_feed_request, priv);
			watch_timeout_set(w, 30);
			list_add(&jogger_feeds, priv, 0);
		}
	}

	return -1;
}

	/* here we check whether user wants entry fetching and queue it */
void jogger_feed_init(session_t *s, const char *url, const int eid) {
	if (session_int_get(s, "entries_try_fetch")) {
		jogger_feed_t *priv	= xmalloc(sizeof(jogger_feed_t));

		priv->url	= xstrdup(url);
		priv->eid	= eid;
		priv->session	= s;

		timer_add(&jogger_plugin, priv->url, 30, 0, &jogger_feed_timer, priv);
	}
}

#endif

