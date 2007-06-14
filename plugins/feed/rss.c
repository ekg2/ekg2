/*
 *  (C) Copyright 2006 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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
#ifdef HAVE_EXPAT

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <netdb.h>

#include <string.h>

#include <ekg/dynstuff.h>
#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/sessions.h>
#include <ekg/userlist.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#ifdef HAVE_EXPAT_H
# include <expat.h>
#endif

#include <iconv.h>

#define RSS_DEFAULT_TIMEOUT 60

#include "feed.h"

#define rss_convert_string(text, encoding) \
	ekg_convert_string(text, encoding ? encoding : "UTF-8", NULL)

typedef enum {
	RSS_PROTO_UNKNOWN = 0,
	RSS_PROTO_HTTP,
	RSS_PROTO_HTTPS,
	RSS_PROTO_FTP,
	RSS_PROTO_FILE,
	RSS_PROTO_EXEC, 
} rss_proto_t;

typedef struct {
	char *session;
	int new;		/* is new? */

	char *url;		/* url */
	int hash_url;		/* ekg_hash of url */
	char *title;		/* title */
	int hash_title;		/* ekg_hash of title */
	char *descr;		/* descr */
	int hash_descr;		/* ekg_hash of descr */

	string_t other_tags;	/* place for other headers saved in format: (tag: value\n)
				 * sample:
				 * 	author: someone\n
				 * 	pubDate: someday\n
				 */
} rss_item_t;

typedef struct {
	char *session;
	int new;		/* is new? */

	char *url;		/* url */
	int hash_url;		/* ekg_hash of url */
	char *title;		/* title */
	int hash_title;		/* ekg_hash of title */
	char *descr;		/* descr */
	int hash_descr;		/* ekg_hash of descr */
	char *lang;		/* lang */
	int hash_lang;		/* ekg_hash of lang */

	list_t rss_items;	/* list of channel items, rss_item_t struct */
} rss_channel_t;

typedef struct {
	char *session;

	char *url;		/* url */
	char *uid;		/* rss:url */

	int resolving;		/* we wait for resolver ? */
	int connecting;		/* we wait for connect() ? */
	int getting;		/* we wait for read()	 ? */

	int headers_done;
	list_t rss_channels;
/* XXX headers_* */

	string_t headers;	/* headers */
	string_t buf;		/* buf with requested file */
/* PROTOs: */
	rss_proto_t proto;
	char *host;	/* protos: RSS_PROTO_HTTP, RSS_PROTO_HTTPS, RSS_PROTO_FTP			hostname 	*/
	char *ip;	/*		j/w								cached ip addr	*/
	int port;	/* 		j/w								port		*/
	char *file;	/* protos: 	j/w RSS_PROTO_FILE 						file		*/
} rss_feed_t;

static list_t feeds;			/* list of feeds, rss_feed_t struct */

static void rss_string_append(rss_feed_t *f, const char *str) {
	string_t buf		= f->buf;

	if (!buf) buf = f->buf = 	string_init(str);
	else				string_append(buf, str);
	string_append_c(buf, '\n');
}

static void rss_set_statusdescr(const char *uid, int status, char *descr) {
	list_t l;
	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;

		if (!xstrncmp(s->uid, "rss:", 4))
			feed_set_statusdescr(userlist_find(s, uid), status, descr);
	}
}

static void rss_set_status(const char *uid, int status) {
	list_t l;

	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;

		if (!xstrncmp(s->uid, "rss:", 4))
			feed_set_status(userlist_find(s, uid), status);
	}
}

static void rss_set_descr(const char *uid, char *descr) {
	list_t l;
	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;

		if (!xstrncmp(s->uid, "rss:", 4)) 
			feed_set_descr(userlist_find(s, uid), descr);
	}
}

static rss_item_t *rss_item_find(rss_channel_t *c, const char *url, const char *title, const char *descr) {
	session_t *s	= session_find(c->session);

	int hash_url	= url	? ekg_hash(url)   : 0;
	int hash_title	= title ? ekg_hash(title) : 0;
	int hash_descr	= descr ? ekg_hash(descr) : 0;

	list_t l;
	rss_item_t *item;

	for (l = c->rss_items; l; l = l->next) {
		item = l->data;
		
//		debug("rss_item_find() %s %s (%d, %d)\n", item->url, url, item->hash_url, hash_url); 

		if (item->hash_url != hash_url || xstrcmp(url, item->url)) continue;
		if (session_int_get(s, "item_enable_title_checking") == 1 && (item->hash_title != hash_title || xstrcmp(title, item->title))) continue;
		if (session_int_get(s, "item_enable_descr_checking") == 1 && (item->hash_descr != hash_descr || xstrcmp(descr, item->descr))) continue;
		
//		debug("rss_item_find() FIND RETVAL = 0x%x\n", item);
		return item;
	}

	item 		= xmalloc(sizeof(rss_item_t));
	item->url 	= xstrdup(url);
	item->hash_url	= hash_url;
	item->title	= xstrdup(title);
	item->hash_title= hash_title;
	item->descr	= xstrdup(descr);
	item->hash_descr= hash_descr;
	
	item->other_tags= string_init(NULL);
	item->new	= 1;
	
	debug("rss_item_find() ADD RETVAL = 0x%x\n", item);
	list_add(&(c->rss_items), item, 0);
	return item;
}

static rss_channel_t *rss_channel_find(rss_feed_t *f, const char *url, const char *title, const char *descr, const char *lang) {
	session_t *s	= session_find(f->session);

	int hash_url	= url	? ekg_hash(url)   : 0;
	int hash_title	= title ? ekg_hash(title) : 0;
	int hash_descr	= descr ? ekg_hash(descr) : 0;
	int hash_lang	= lang	? ekg_hash(lang)  : 0;

	list_t l;
	rss_channel_t *channel;

	for (l = f->rss_channels; l; l = l->next) {
		channel = l->data;

//		debug("rss_channel_find() %s %s\n", channel->url, url); 

		if (channel->hash_url != hash_url || xstrcmp(url, channel->url)) continue;
		if (session_int_get(s, "channel_enable_title_checking") == 1 && (channel->hash_title != hash_title || xstrcmp(title, channel->title))) continue;
		if (session_int_get(s, "channel_enable_descr_checking") == 1 && (channel->hash_descr != hash_descr || xstrcmp(descr, channel->descr))) continue;
		if (session_int_get(s, "channel_enable_lang_checking")  == 1 && (channel->hash_lang  != hash_lang  || xstrcmp(lang, channel->lang))) continue;

//		debug("rss_channel_find() FIND RETVAL = 0x%x\n", channel);
		return channel;
	}

	channel			= xmalloc(sizeof(rss_channel_t));
	channel->session	= xstrdup(f->session);
	channel->url		= xstrdup(url);
	channel->hash_url	= hash_url;
	channel->title		= xstrdup(title);
	channel->hash_title	= hash_title;
	channel->descr		= xstrdup(descr);
	channel->hash_descr	= hash_descr;
	channel->lang		= xstrdup(lang);
	channel->hash_lang	= hash_lang;

	channel->new	= 1;

	debug("rss_channel_find() ADD RETVAL = 0x%x\n", channel);
	list_add(&(f->rss_channels), channel, 0);
	return channel;
}

static rss_feed_t *rss_feed_find(session_t *s, const char *url) {
	list_t newsgroups = feeds;
	list_t l;
	rss_feed_t *feed;

	if (!xstrncmp(url, "rss:", 4)) url += 4;

	for (l = newsgroups; l; l = l->next) {
		feed = l->data;

		debug("rss_feed_find() %s %s\n", feed->url, url);
		if (!xstrcmp(feed->url, url)) 
			return feed;
	}
	debug("rss_feed_find() 0x%x NEW %s\n", newsgroups, url);

	feed		= xmalloc(sizeof(rss_feed_t));
	feed->session	= xstrdup(s->uid);
	feed->uid	= saprintf("rss:%s", url);
	feed->url 	= xstrdup(url);

/*  URI: ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))? */

	if (!xstrncmp(url, "https://", 8)) {
		url += 8;				/* skip https:// */
		feed->proto = RSS_PROTO_HTTPS;
	} else if (!xstrncmp(url, "ftp://", 6)) {
		url += 6;				/* skip ftp://	*/
		feed->proto = RSS_PROTO_FTP;
	} else if (!xstrncmp(url, "file://", 7)) {
		url += 7;				/* skip file:// */
		feed->proto = RSS_PROTO_FILE;
	} else if (!xstrncmp(url, "http://", 7)) {
		url += 7;				/* skip http:// */
		feed->proto = RSS_PROTO_HTTP;
	} else if (!xstrncmp(url, "exec:", 5)) {
		url += 5;
		feed->proto = RSS_PROTO_EXEC;
	}

	if (feed->proto == RSS_PROTO_HTTP || feed->proto == RSS_PROTO_HTTPS || feed->proto == RSS_PROTO_FTP) {
		const char *req;
		char *host = NULL, *tmp;

		if ((req = xstrchr(url, '/')))	feed->host = xstrndup(url, req - url);
		else				feed->host = xstrdup(url);

		if ((tmp = xstrchr(host, ':'))) {	/* port http://www.cos:1234 */
			feed->port = atoi(tmp+1);
			*tmp = 0;
		} else {
			if (feed->proto == RSS_PROTO_FTP)	feed->port = 21;
			if (feed->proto == RSS_PROTO_HTTP)	feed->port = 80;
			if (feed->proto == RSS_PROTO_HTTPS)	feed->port = 443;
		}
		url = req;
	}
	if (feed->proto == RSS_PROTO_HTTP || feed->proto == RSS_PROTO_HTTPS || feed->proto == RSS_PROTO_FTP || feed->proto == RSS_PROTO_FILE || feed->proto == RSS_PROTO_EXEC) 
		feed->file = xstrdup(url);

	debug("[rss] proto: %d url: %s port: %d url: %s file: %s\n", feed->proto, feed->url, feed->port, feed->url, feed->file);

	list_add(&(feeds), feed, 0);
	return feed;
}

typedef struct xmlnode_s {
	char *name;
	string_t data;
	char **atts;

	struct xmlnode_s *parent;
	struct xmlnode_s *children;
	
	struct xmlnode_s *next;
} xmlnode_t;

typedef struct {
	rss_feed_t *f;
	xmlnode_t *node;
	char *no_unicode;
} rss_fetch_process_t;

static void rss_fetch_error(rss_feed_t *f, const char *str) {
	debug("rss_fetch_error() %s\n", str);
	rss_set_statusdescr(f->uid, EKG_STATUS_ERROR, xstrdup(str));
}

/* ripped from jabber plugin */
static void rss_handle_start(void *data, const char *name, const char **atts) {
	rss_fetch_process_t *j = data;
	xmlnode_t *n, *newnode;
	int arrcount;
	int i;

	if (!data || !name) {
		debug("[rss] xmlnode_handle_end() invalid parameters\n");
		return;
	}

	newnode = xmalloc(sizeof(xmlnode_t));
	newnode->name = xstrdup(name);
	newnode->data = string_init(NULL);

	if ((n = j->node)) {
		newnode->parent = n;

		if (!n->children) {
			n->children = newnode;
		} else {
			xmlnode_t *m = n->children;

			while (m->next)
				m = m->next;
			
			m->next = newnode;
			newnode->parent = n;
		}
	}
	arrcount = array_count((char **) atts);

	if (arrcount > 0) {
		newnode->atts = xmalloc((arrcount + 1) * sizeof(char *));
		for (i = 0; i < arrcount; i++) {
			const char *s = rss_convert_string(atts[i], j->no_unicode);
			newnode->atts[i] = (s ? s : xstrdup(atts[i]));
		}
	} else	newnode->atts = NULL; 

	j->node = newnode;
}

static void rss_handle_end(void *data, const char *name) {
	rss_fetch_process_t *j = data;
	xmlnode_t *n;

	if (!data || !name) {
		debug("[rss] xmlnode_handle_end() invalid parameters\n");
		return;
	}
	if (!(n = j->node)) return;

	if (n->parent) j->node = n->parent;
}

static void rss_handle_cdata(void *data, const char *text, int len) {
	rss_fetch_process_t *j = data;
	xmlnode_t *n;
	char *recode;
	int i;
	int rlen;

	if (!j || !text) {
		debug("[rss] xmlnode_handle_cdata() invalid parameters\n");
		return;
	}

	if (!(n = j->node)) return;

	recode	= xmalloc(len+1);
	rlen	= 0;

	for (i = 0; i < len;) {
		unsigned int znak = (unsigned char) text[i];

		if (znak > 0x7F && j->no_unicode) {
			int ucount = 0;
			unsigned char znaczek = 0;

			/* mapowanie takie samo jak iso-8859-1 <==> utf-8 */
			/* stolen from linux/drivers/char/vt.c do_con_write() */

			if ((znak & 0xe0) == 0xc0) 	{ ucount = 1; znaczek = (znak & 0x1f); }
			else if ((znak & 0xf0) == 0xe0) { ucount = 2; znaczek = (znak & 0x0f); }
			else if ((znak & 0xf8) == 0xf0) { ucount = 3; znaczek = (znak & 0x07); }
			else if ((znak & 0xfc) == 0x78) { ucount = 4; znaczek = (znak & 0x03); }
			else if ((znak & 0xfe) == 0xfc) { ucount = 5; znaczek = (znak & 0x01); }	/* shouldn't happen in utf-8 */
			i++;		/* next */

			if (i+ucount > len || ucount == 5 || !ucount) {
				debug("invalid utf-8 char\n");	/* shouldn't happen */
				recode[rlen++] = '?';
				i += ucount;
				continue;
			}

			while (ucount && ((text[i] & 0xc0) == 0x80)) {
				ucount--;
				znaczek = (znaczek << 6) | (((unsigned char) text[i]) & 0x3f);
				i++;
			}
			recode[rlen++] = znaczek;
			continue;
		}
		recode[rlen++] = znak;
		i++;
	}
	{		/* I think old version leaked, if I were wrong, let mi know */
		char *tmp = recode;

		recode = rss_convert_string(recode, j->no_unicode);
		if (!recode)
			recode = tmp;
		else
			xfree(tmp);
	}

	string_append(n->data, recode);

	xfree(recode);
}

static int rss_handle_encoding(void *data, const char *name, XML_Encoding *info) {
	rss_fetch_process_t      *j = data;
	int i;

	debug("rss_handle_encoding() %s\n", name);

	for(i=0; i<256; i++)
		info->map[i] = i;

	info->convert	= NULL;
	info->data	= NULL;
	info->release	= NULL;
	j->no_unicode	= xstrdup(name); 
	return 1;
}

static void rss_parsexml_atom(rss_feed_t *f, xmlnode_t *node) {
	debug("rss_parsexml_atom() sorry, atom not implemented\n");
}

static void rss_parsexml_rdf(rss_feed_t *f, xmlnode_t *node) {
	rss_channel_t *chan;

	debug("rss_parsexml_rdf (channels oldcount: %d)\n", list_count(f->rss_channels));
	debug_error("XXX http://web.resource.org/rss/1.0/");

	chan = rss_channel_find(f, /* chanlink, chantitle, chandescr, chanlang */ "", "", "", "");

	for (; node; node = node->next) {
		if (!xstrcmp(node->name, "channel")) {
			/* DUZE XXX */


		} else if (!xstrcmp(node->name, "item")) {
			const char *itemtitle   = NULL;
			const char *itemdescr   = NULL;
			const char *itemlink    = NULL;

			xmlnode_t *subnode;
			rss_item_t *item;
			string_t    tmp		= string_init(NULL);

			for (subnode = node->children; subnode; subnode = subnode->next) {
				if (!xstrcmp(subnode->name, "title"))		itemtitle	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "link"))	itemlink	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "description"))itemdescr	= subnode->data->str;
				else {  /* other, format tag: value\n */
/*					debug("rss_parsexml_rdf RDF->ITEMS: %s\n", subnode->name); */
					string_append(tmp, subnode->name);
					string_append(tmp, ": ");
					string_append(tmp, subnode->data->str);
					string_append_c(tmp, '\n');
				}
			}
			item = rss_item_find(chan, itemlink, itemtitle, itemdescr);

			string_free(item->other_tags, 1);
			item->other_tags = tmp;


		} else debug("rss_parsexml_rdf RSS: %s\n", node->name);
	}
}

static void rss_parsexml_rss(rss_feed_t *f, xmlnode_t *node) {
	debug("rss_parsexml_rss (channels oldcount: %d)\n", list_count(f->rss_channels));

	for (; node; node = node->next) {
		if (!xstrcmp(node->name, "channel")) {
			const char *chantitle	= NULL;
			const char *chanlink	= NULL;
			const char *chandescr	= NULL;
			const char *chanlang	= NULL;
			rss_channel_t *chan;

			xmlnode_t *subnode;

			for (subnode = node->children; subnode; subnode = subnode->next) {
				if (!xstrcmp(subnode->name, "title")) 		chantitle 	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "link")) 	chanlink	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "description"))chandescr	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "language"))	chanlang	= subnode->data->str;
				else if (!xstrcmp(subnode->name, "item"))	; /* later */
				else debug("rss_parsexml_rss RSS->CHANNELS: %s\n", subnode->name);
			}

			chan = rss_channel_find(f, chanlink, chantitle, chandescr, chanlang);
			debug("rss_parsexml_rss (items oldcount: %d)\n", list_count(chan->rss_items));

			for (subnode = node->children; subnode; subnode = subnode->next) {
				if (!xstrcmp(subnode->name, "item")) {
					const char *itemtitle	= NULL;
					const char *itemdescr	= NULL;
					const char *itemlink	= NULL;
					rss_item_t *item;
					string_t    tmp		= string_init(NULL);

					xmlnode_t *items;
					for (items = subnode->children; items; items = items->next) {
						if (!xstrcmp(items->name, "title"))		itemtitle = items->data->str;
						else if (!xstrcmp(items->name, "description"))	itemdescr = items->data->str;
						else if (!xstrcmp(items->name, "link")) 	itemlink  = items->data->str;
						else {	/* other, format tag: value\n */
							string_append(tmp, items->name);
							string_append(tmp, ": ");
							string_append(tmp, items->data->str);
							string_append_c(tmp, '\n');
						}
					}
					item = rss_item_find(chan, itemlink, itemtitle, itemdescr);

					string_free(item->other_tags, 1);
					item->other_tags = tmp;
				}
			}
		} else debug("rss_parsexml_rss RSS: %s\n", node->name);
	}
}

static void xmlnode_free(xmlnode_t *n) {
	xmlnode_t *m;

	if (!n)
		return;

	for (m = n->children; m;) {
		xmlnode_t *cur = m;
		m = m->next;
		xmlnode_free(cur);
	}

	xfree(n->name);
	string_free(n->data, 1);
	array_free(n->atts);
	xfree(n);
}

static void rss_fetch_process(rss_feed_t *f, const char *str) {
	int new_items = 0;
	list_t l;

	rss_fetch_process_t *priv = xmalloc(sizeof(rss_fetch_process_t));
	xmlnode_t *node;
	XML_Parser parser = XML_ParserCreate(NULL);

        XML_SetUserData(parser, (void*) priv);
        XML_SetElementHandler(parser, (XML_StartElementHandler) rss_handle_start, (XML_EndElementHandler) rss_handle_end);
        XML_SetCharacterDataHandler(parser, (XML_CharacterDataHandler) rss_handle_cdata);

//	XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_ALWAYS);
	XML_SetUnknownEncodingHandler(parser, (XML_UnknownEncodingHandler) rss_handle_encoding, priv);

	rss_set_descr(f->uid, xstrdup("Parsing..."));

	if (XML_Parse(parser, str, xstrlen(str), 1) == XML_STATUS_OK) {
		for (node = priv->node; node; node = node->next) {
			if (!xstrcmp(node->name, "rss")) rss_parsexml_rss(f, node->children);
			else if (!xstrcmp(node->name, "feed")) rss_parsexml_atom(f, node->children); /* xmlns */
			else if (!xstrcmp(node->name, "rdf:RDF")) rss_parsexml_rdf(f, node->children);
			else {
				debug("UNKNOWN node->name: %s\n", node->name);
				goto fail;
			}
		}
	} else {
		char *tmp = saprintf("XML_Parse: %s", XML_ErrorString(XML_GetErrorCode(parser)));
		rss_fetch_error(f, tmp);
		xfree(tmp);
		goto fail;
			//		for (node = priv->node; node; node = node->parent); /* going up on error */
	}

	for (l = f->rss_channels; l; l = l->next) {
		rss_channel_t *channel = l->data;
		list_t k;

		for (k = channel->rss_items; k; k = k->next) {
			rss_item_t *item 	= k->data;
			char *proto_headers	= f->headers->len	? f->headers->str	: NULL;
			char *headers		= item->other_tags->len	? item->other_tags->str : NULL;
			int modify		= 0;			/* XXX */

//			if (channel->new)	item->new = 0;
			if (item->new)		new_items++;

			query_emit_id(NULL, RSS_MESSAGE, 
				&(f->session), &(f->uid), &proto_headers, &headers, &(item->title), 
				&(item->url),  &(item->descr), &(item->new), &modify);
		}
		channel->new = 0;
	}

	if (!new_items)
		rss_set_statusdescr(f->uid, EKG_STATUS_DND, xstrdup("Done, no new messages"));
	else	rss_set_statusdescr(f->uid, EKG_STATUS_AVAIL, saprintf("Done, %d new messages", new_items));
fail:
	xmlnode_free(priv->node);
	XML_ParserFree(parser);
	return;
}

static WATCHER_LINE(rss_fetch_handler) {
	rss_feed_t      *f = data;

	if (type) {
		if (f->buf) 
			rss_fetch_process(f, f->buf->str);
		else	rss_fetch_error(f, "[INTERNAL ERROR] Null f->buf");
		f->getting = 0;
		f->headers_done = 0;
		return 0;
	}

	if (f->headers_done) {
		rss_set_descr(f->uid, xstrdup("Getting data..."));
		if (xstrcmp(watch, ""))
			rss_string_append(f, watch);
	} else {
		if (!xstrcmp(watch, "\r")) {
			f->headers_done = 1;
			return 1;
		}
	/* append headers */
		if (!f->headers)	f->headers = string_init(watch);
		else			string_append(f->headers, watch);
		string_append_c(f->headers, '\n');

		/* XXX, parse some headers */
	}
	return 0;
}

/* handluje polaczenie, wysyla to co ma wyslac, dodaje łocza do odczytu */
static WATCHER(rss_fetch_handler_connect) {
	int		res = 0; 
	socklen_t	res_size = sizeof(res);
	rss_feed_t	*f = data;

	f->connecting = 0;

	string_clear(f->headers);
	string_clear(f->buf);

	if (type == 1) {
		debug ("[rss] handle_connect(): type %d\n", type);
		return 0;
	}

	if (type || getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		if (type) 
			debug("[rss] handle_connect(): SO_ERROR %s\n", strerror(res));
		if (type == 2); /* connection timeout */
		close(fd);
		return -1; /* ? */
	}
	
	if (f->proto == RSS_PROTO_HTTP) {
		rss_set_descr(f->uid, xstrdup("Requesting..."));
		char *request = saprintf(
			"GET %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"User-Agent: Ekg2 - evilny klient gnu (rss feeder)\r\n"
			/* XXX, other headers */
			"Connection: close\r\n"
			"\r\n", f->file, f->host);
		write(fd, request, xstrlen(request));
		xfree(request);
	} else {	/* unknown proto here ? */
		close(fd);
		return -1;
	}
	f->getting = 1;
	f->headers_done = 0;
	watch_add_line(&feed_plugin, fd, WATCH_READ_LINE, rss_fetch_handler, f);
	return -1;
}

typedef struct {
	char *session;
	char *uid;
} rss_resolver_t;

static int rss_url_fetch(rss_feed_t *f, int quiet);

static WATCHER(rss_url_fetch_resolver) {
	rss_resolver_t *b = data;
	rss_feed_t *f;
	char buf[25];	/* v4 */
	int len;

	debug("rss_url_fetch_resolver() fd: %d type: %d\n", fd, type);

	f = rss_feed_find(session_find(b->session), b->uid);

	if (type) {
		f->resolving = 0;
		if (f->ip)
			rss_url_fetch(f, 0);

		if (type == 2) 
			rss_set_statusdescr(b->uid, EKG_STATUS_ERROR, saprintf("Resolver tiemout..."));

		xfree(b->session);
		xfree(b->uid);
		xfree(b);
		return 0;
	}

	if ((len = read(fd, &buf[0], sizeof(buf)-1)) > 0) {

		buf[len] = 0;

		rss_set_descr(b->uid, saprintf("Resolved to: %s (read: %d bytes)", buf, len));

		f->ip = xstrdup(buf);
	} else {
		rss_set_statusdescr(b->uid, EKG_STATUS_ERROR, 
			saprintf("Resolver ERROR read: %d bytes (%s)", len, len == -1 ? strerror(errno) : ""));
	}
	return -1;
}

static int rss_url_fetch(rss_feed_t *f, int quiet) {
	int fd = -1;

	debug("rss_url_fetch() f: 0x%x\n", f);

	if (f->connecting || f->resolving) {
		printq("rss_during_connect", session_name(session_find(f->session)), f->url);
		return -1;
	}

	if (f->getting) {
		printq("rss_during_getting", session_name(session_find(f->session)), f->url);
		return -1;
	}

	if (f->proto == RSS_PROTO_HTTPS) {
		printq("generic_error", "Currently we don't support https protocol, sorry");
		return -1;
	}

	if (f->proto == RSS_PROTO_FTP) {
		printq("generic_error", "Currently we don't support ftp protocol, sorry");
		return -1;
	}

	if (f->proto == RSS_PROTO_FILE) {
		fd = open(f->file, O_RDONLY);

		if (fd == -1) {
			debug("rss_url_fetch FILE: %s (error: %s,%d)", f->file, strerror(errno), errno);
			return -1;
		}
	}

	if (f->proto == RSS_PROTO_EXEC) {
		int fds[2];
		int pid;
		f->headers_done = 1;

		pipe(fds);
		
		if (!(pid = fork())) {

			dup2(open("/dev/null", O_RDONLY), 0);
			dup2(fds[1], 1);
			dup2(fds[1], 2);

			close(fds[0]);
			close(fds[1]);

			execl("/bin/sh", "sh", "-c", f->file, (void *) NULL);
			exit(1);
		}

		if (pid < 1) {
			close(fds[0]);
			close(fds[1]);
			return -1;
		}

		close(fds[1]);

		fd = fds[0];
		watch_add_line(&feed_plugin, fd, WATCH_READ_LINE, rss_fetch_handler, f);
	}

	if (f->proto == RSS_PROTO_HTTP) {
		debug("rss_url_fetch HTTP: host: %s port: %d file: %s\n", f->host, f->port, f->file);

		if (f->port <= 0 || f->port >= 65535) return -1;
		
		if (!f->ip) {	/* if we don't have ip, maybe it's v4 address? */
			if (inet_addr(f->host) != INADDR_NONE)
				f->ip = xstrdup(f->host);
		}

		if (f->ip) {
			struct sockaddr_in sin;
			int ret;
			int one = 1;

			debug("rss_url_fetch %s using previously cached IP address: %s\n", f->host, f->ip);

			fd = socket(AF_INET, SOCK_STREAM, 0);

			sin.sin_addr.s_addr 	= inet_addr(f->ip);
			sin.sin_port		= htons(f->port);
			sin.sin_family		= AF_INET;

			rss_set_descr(f->uid, saprintf("Connecting to: %s (%s)", f->host, f->ip));
			f->connecting = 1;

			ioctl(fd, FIONBIO, &one);

			ret = connect(fd, (struct sockaddr *) &sin, sizeof(sin));

			watch_add(&feed_plugin, fd, WATCH_WRITE, rss_fetch_handler_connect, f);
		} else {
			int fd[2];
			int res;

			if (pipe(fd) == -1) {
				rss_set_statusdescr(f->uid, EKG_STATUS_ERROR, saprintf("Resolver error @ pipe() %s\n", strerror(errno)));
				return -1;
			}

			f->resolving = 1;
			if ((res = fork()) == -1) {
				rss_set_statusdescr(f->uid, EKG_STATUS_ERROR, saprintf("Resolver error @ fork() %s\n", strerror(errno)));
				close(fd[0]);
				close(fd[1]);
				f->resolving = 0;
				return -1;
			}
			
			if (res) {
				rss_resolver_t *b = xmalloc(sizeof(rss_resolver_t));
				watch_t *w; 

				close(fd[1]);

				b->session 	= xstrdup(f->session);
				b->uid		= saprintf("rss:%s", f->url);

				rss_set_descr(f->uid, xstrdup("Resolving..."));
	
				w = watch_add(&feed_plugin, fd[0], WATCH_READ, rss_url_fetch_resolver, b);
				watch_timeout_set(w, 10);	/* 10 sec resolver timeout */
			} else {
				struct hostent *he;
				close(fd[0]);
				if ((he = gethostbyname(f->host))) {
					struct in_addr a;
					char *ip;

					memcpy(&a, he->h_addr, sizeof(a));
					ip = inet_ntoa(a);

					write(fd[1], ip, xstrlen(ip));
				}
				sleep(2);
				exit(0);
			}
		}
		return fd;
	}
	return -1;
}


static COMMAND(rss_command_check) {
	list_t l;

	if (params[0]) {
		userlist_t *u = userlist_find(session, params[0]);

		if (!u) {
			printq("user_not_found", params[0]);
			/* && try /rss:get ? */
			return -1;	
		}
		
		return rss_url_fetch(rss_feed_find(session, u->uid), quiet);
	}

	/* if param not given, check all */
	for (l = session->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		rss_feed_t *f = rss_feed_find(session, u->uid);

		rss_url_fetch(f, quiet);
	}
	return 0;
}

static COMMAND(rss_command_get) {
	return rss_url_fetch(rss_feed_find(session, target), quiet);
}

static COMMAND(rss_command_connect) {
	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	session_connected_set(session, 1);
	query_emit_id(NULL, PROTOCOL_CONNECTED, &session->uid);
	session->status = EKG_STATUS_AVAIL;

	return 0;
}

static COMMAND(rss_command_subscribe) {
	const char *nick;
	const char *uidnoproto;
	userlist_t *u;

	if ((u = userlist_find(session, target))) {
		printq("feed_exists_other", target, format_user(session, u->uid), session_name(session));
		return -1;
	}

	if (target[0] == 'n' || valid_plugin_uid(session->plugin, target) != 1) {
		printq("invalid_session");
		return -1;
	}

	uidnoproto = target + 4;

	if (!xstrncmp(uidnoproto, "http://", 7)) 	uidnoproto += 7;
	else if (!xstrncmp(uidnoproto, "file://", 7))	uidnoproto += 7;
	else if (!xstrncmp(uidnoproto, "exec:", 5))	uidnoproto += 5;
	else {
		debug_error("rss_command_subscribe() uidnoproto: %s\n", uidnoproto);
		printq("generic_error", "Protocol not implemented, sorry");
		return -1;
	}

	nick = (params[0] && params[1]) ? params[1] : uidnoproto;

	if (!(u = userlist_add(session, target, nick))) {
		debug_error("rss_command_subscribe() userlist_add(%s, %s, %s) failed\n", session->uid, target, nick);
		printq("generic_error", "IE, userlist_add() failed.");
		return -1;
	}

	printq("feed_added", format_user(session, target), session_name(session));
	return 0;
}

static COMMAND(rss_command_unsubscribe) {
	userlist_t *u; 
	if (!(u = userlist_find(session, target))) {
		printq("feed_not_found", target);
		return -1;
	}

	printq("feed_deleted", target, session_name(session));
	userlist_remove(session, u);
	return 0;
}

void rss_protocol_deinit(void *priv) {
	return;
}

void *rss_protocol_init() {
	return NULL;
}

void rss_deinit() {
	list_t l;
	for (l = feeds; l; l = l->next) {
		list_t k;
		rss_feed_t *f = l->data;
		
		xfree(f->session);
		xfree(f->url);
		xfree(f->uid);

		for (k = f->rss_channels; k; k = k->next) {
			rss_channel_t *channel = k->data;
			list_t j;

			xfree(channel->session);
			xfree(channel->url);
			xfree(channel->title);
			xfree(channel->descr);
			xfree(channel->lang);

			for (j = channel->rss_items; j; j = j->next) {
				rss_item_t *item = j->data;

				xfree(item->session);
				xfree(item->url);
				xfree(item->title);
				xfree(item->descr);
				
				xfree(item);
				j->data = NULL;
			}
			list_destroy(channel->rss_items, 0);
			xfree(channel);
			k->data = NULL;
		}
		list_destroy(f->rss_channels, 0);

		string_free(f->buf, 1);
		string_free(f->headers, 1);
		xfree(f->host);
		xfree(f->ip);
		xfree(f->file);
		xfree(f);

		l->data = NULL;
	}
	list_destroy(feeds, 0);
}

void rss_init() {
	command_add(&feed_plugin, ("rss:connect"), "?", rss_command_connect, RSS_ONLY, NULL);
	command_add(&feed_plugin, ("rss:check"), "u", rss_command_check, RSS_FLAGS, NULL);
	command_add(&feed_plugin, ("rss:get"), "!u", rss_command_get, RSS_FLAGS_TARGET, NULL);

	command_add(&feed_plugin, ("rss:subscribe"), "! ?",	rss_command_subscribe, RSS_FLAGS_TARGET, NULL); 
	command_add(&feed_plugin, ("rss:unsubscribe"), "!u",rss_command_unsubscribe, RSS_FLAGS_TARGET, NULL);
}
#endif

