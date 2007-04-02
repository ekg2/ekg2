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

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#include "feed.h"

static int feed_theme_init();
PLUGIN_DEFINE(feed, PLUGIN_PROTOCOL, feed_theme_init);

static QUERY(feed_validate_uid)
{
        char *uid = *(va_arg(ap, char **));
        int *valid = va_arg(ap, int *);

        if (!uid)
                return 0;
#ifdef HAVE_EXPAT
        if (!xstrncasecmp(uid, "rss:", 4)) {
                (*valid)++;
		return -1;
	}
#endif
	if (!xstrncasecmp(uid, "nntp:", 5)) {
		(*valid)++;
		return -1;
	}

        return 0;
}

static QUERY(feed_session) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

        if (!s)
                return -1;

	if (s->plugin != &feed_plugin)

		return 0;

	if (data && !s->priv) {
		feed_private_t *j 	= xmalloc(sizeof(feed_private_t));
#ifdef HAVE_EXPAT
		j->isrss		= !xstrncasecmp(session, "rss:", 4);
		if (j->isrss)		j->private = rss_protocol_init();
		else
#endif
					j->private = nntp_protocol_init();
		s->priv			= j;
		userlist_read(s);
	} else if (!data && s->priv) {
		feed_private_t *j 	= s->priv;
		userlist_write(s);
		s->priv			= NULL;
#ifdef HAVE_EXPAT
		if (j->isrss) 		rss_protocol_deinit(j->private);
		else
#endif
					nntp_protocol_deinit(j->private);
		xfree(j);
	}
	return 0;
}
	/* new: 
	 * 	0x0 - old
	 * 	0x1 - new
	 * 	0x2 - modified 
	 */

	/* mtags: (by default rss_message() won't display any messages if new == 0, but if user want to display again (?) news, we must allow him)
	 * 	0x0 - none
	 * 	0x8 - display all headers / sheaders
	 */

static QUERY(rss_message) {
	char *session	= *(va_arg(ap, char **));
	char *uid	= *(va_arg(ap, char **));
	char *sheaders	= *(va_arg(ap, char **));
	char *headers	= *(va_arg(ap, char **));
	char *title	= *(va_arg(ap, char **));
	char *url	= *(va_arg(ap, char **));
	char *body	= *(va_arg(ap, char **));

	int *new	= va_arg(ap, int *); 		/* 0 - old; 1 - new; 2 - modified */
	int mtags	= *(va_arg(ap, int *));		

	session_t *s	= session_find(session);
	char *tmp;

	const char *dheaders	= session_get(s, "display_headers");
	const char *dsheaders	= session_get(s, "display_server_headers");
	int dmode		= session_int_get(s, "display_mode");
	int mw			= session_int_get(s, "make_window");

	char *target		= NULL;

	if (*new == 0) return 0;

	switch (dmode) {
		case 0:	 print_window(uid, s, 1, "none", "new message");	/* only notify */
		case -1: return 0;						/* do nothing */

		case 2:	body		= NULL;					/* only headers */
		case 1:	if (dmode == 1) headers = NULL;				/* only body */
		default:							/* default: 3 (body+headers) */
		case 3:	sheaders = NULL;					/* headers+body */
		case 4:	break;							/* shreaders+headers+body */
	}

	switch (mw) {			/* XXX, __current ? */
		case 0: 
			target = "__status";
			break;
		case 1:
			target = session;
			break;
		case 2:
		default:
			target = uid;
			break;
	}
	if (mw) window_new(uid, s, 0);
	print_window(uid, s, 1, "feed_message_header", title, url);

	if (sheaders) {
		char *str = xstrdup(sheaders);
		char *formated = NULL;
		while ((tmp = split_line(&str))) {
			char *value = NULL;
			char *formatka;

			if ((value = xstrchr(tmp, ' '))) *value = 0;
			if (dsheaders && !xstrstr(dsheaders, tmp)) continue;	/* jesli mamy display_server_headers a tego nie mamy na liscie to pomijamy */

			formatka = saprintf("feed_server_header_%s", tmp);
			if ((!xstrcmp(format_find(formatka), ""))) { xfree(formatka); formatka = NULL; }
	
			formated = format_string(format_find(formatka ? formatka : "feed_server_header_generic"), tmp, value ? value+1 : "");
			print_window(uid, s, 1, "feed_message_body", formated ? formated : tmp);

			xfree(formatka);
		}
		if (headers || body) print_window(uid, s, 1, "feed_message_body", "");	/* rozdziel */
	}
	if (headers) {
		char *str, *org;
		str = org = xstrdup(headers);
		char *formated = NULL;
		while ((tmp = split_line(&str))) {
			char *value = NULL;
			char *formatka;

			if ((value = xstrchr(tmp, ' '))) *value = 0;
			if (dheaders && !xstrstr(dheaders, tmp)) {
/*				debug("DHEADER: %s=%s\n", tmp, value+1);  */
				continue;	/* jesli mamy display_headers a tego nie mamy na liscie to pomijamy */
			}

			formatka = saprintf("feed_message_header_%s", tmp);
			if ((!xstrcmp(format_find(formatka), ""))) { xfree(formatka); formatka = NULL; }
	
			formated = format_string(format_find(formatka ? formatka : "feed_message_header_generic"), tmp, value ? value+1 : "");
			print_window(uid, s, 1, "feed_message_body", formated ? formated : tmp);
			
			xfree(formated);
			xfree(formatka);
		}
		if (body) print_window(uid, s, 1, "feed_message_body", "");	/* rozdziel */
		xfree(org);
	}
	if (body) {
		if (session_check(s, 0, "nntp")) {
			int article_signature	= 0;
			char *str, *org;
			str = org = xstrdup(body);

			while ((tmp = split_line(&str))) {
				char *formated = NULL;

				if (!xstrcmp(tmp, "-- ")) article_signature = 1;
				if (article_signature) {
					formated = format_string(format_find("nntp_message_signature"), tmp);
				} else {
					int i;
					char *quote_name = NULL;
					const char *f = NULL;
					for (i = 0; i < xstrlen(tmp) && tmp[i] == '>'; i++);

//					if (i > 0 && tmp[i] == ' ') 	/* normal clients quote:  >>>> aaaa */
					if (i > 0) 			/* buggy clients quote:   >>>>>aaaa */
					{
						quote_name = saprintf("nntp_message_quote_level%d", i+1);
						if (!xstrcmp(f = format_find(quote_name), "")) {
							debug("[NNTP, QUOTE] format: %s not found, using global one...\n", quote_name);
							f = format_find("nntp_message_quote_level");
						}
						xfree(quote_name);
					}
					if (f)	formated = format_string(f, tmp);
				}

				print_window(uid, s, 1, "feed_message_body", formated ? formated : tmp);
				xfree(formated);
			}
			xfree(org);
		} else {
			print_window(uid, s, 1, "feed_message_body", body);
		}
	}

	print_window(uid, s, 1, "feed_message_footer");

	*new = 0;
	return 0;
}

void feed_set_status(userlist_t *u, int status) {
	if (!u || !status) return;

/*	if (xstrcmp(u->status, status)) print("feed_status", u->uid, status, u->descr); */
	u->status	= status;
}

void feed_set_descr(userlist_t *u, char *descr) {
	char *tmp;
	if (!u || !descr) return;

/*	if (xstrcmp(u->descr, descr)) print("feed_status", u->uid, u->status, descr); */
	tmp 		= u->descr;
	u->descr	= descr;
	xfree(tmp);
}

void feed_set_statusdescr(userlist_t *u, int status, char *descr) {
	feed_set_status(u, status);
	feed_set_descr(u, descr);
}

static plugins_params_t feed_plugin_vars[] = {
#warning "Sort it"
/* common vars. */
	PLUGIN_VAR_ADD("auto_connect", 		SESSION_VAR_AUTO_CONNECT, VAR_BOOL, "1", 0, NULL),
	PLUGIN_VAR_ADD("alias",			SESSION_VAR_ALIAS, VAR_STR, NULL, 0, NULL),
	/* (-1 - nothing; 0 - only notify; 1 - only body; 2 - only headers; 3 - headers+body 4 - sheaders+headers+ body)  default+else: 3 */
	PLUGIN_VAR_ADD("display_mode",		0, VAR_INT, "3", 0, NULL),	

	PLUGIN_VAR_ADD("display_headers",	0, VAR_STR, 
			/* RSS: */ 
				"pubDate: author: dc:creator: dc:date:" 
			/* NNTP: */ 
				"From: Date: Newsgroups: Subject: User-Agent: NNTP-Posting-Host:", 
			0, NULL),

	/* 0 - status; 1 - all in one window (s->uid) 2 - seperate windows per feed / group. default+else: 2 */
	PLUGIN_VAR_ADD("make_window", 		0, VAR_INT, "2", 0, NULL),
/* rss vars. */
#ifdef HAVE_EXPAT
	PLUGIN_VAR_ADD("display_server_headers", 0, VAR_STR, 
	/* display some basic server headers */
		"HTTP/1.1 "	/* rcode? */
		"Server: "
		"Date: ",
		0, NULL),
#endif
/* nntp vars. */
	PLUGIN_VAR_ADD("username",		0, VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("password", 		SESSION_VAR_PASSWORD, VAR_STR, "foo", 1, NULL),
	PLUGIN_VAR_ADD("port", 			SESSION_VAR_PORT, VAR_INT, "119", 0, NULL),
	PLUGIN_VAR_ADD("server", 		SESSION_VAR_SERVER, VAR_STR, 0, 0, NULL),

	PLUGIN_VAR_END()
};

int feed_plugin_init(int prio) {
	feed_plugin.params = feed_plugin_vars;
	plugin_register(&feed_plugin, prio);
			/* common */
	query_connect_id(&feed_plugin, SESSION_ADDED, feed_session, (void*) 1);
	query_connect_id(&feed_plugin, SESSION_REMOVED, feed_session, (void*) 0);
	query_connect_id(&feed_plugin, PROTOCOL_VALIDATE_UID, feed_validate_uid, NULL);
			/* common - rss, nntp */
	query_connect_id(&feed_plugin, RSS_MESSAGE, rss_message, NULL);

#ifdef HAVE_EXPAT
	rss_init();	/* rss */
#endif
	nntp_init();	/* nntp */
	return 0;
}

static int feed_plugin_destroy() {
	plugin_unregister(&feed_plugin);
#ifdef HAVE_EXPAT
	rss_deinit();	/* rss */
#endif
	return 0;
}

static int feed_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("feed_status",		_("%> Newstatus: %1 (%2) %3"), 1);	/* XXX */

	format_add("feed_added", 		_("%> (%2) Added %T%1%n to subscription\n"), 1);
	format_add("feed_exists_other", 	_("%! (%3) %T%1%n already subscribed as %2\n"), 1);
	format_add("feed_not_found",		_("%) Subscription %1 not found, cannot unsubscribe"), 1);
	format_add("feed_deleted", 		_("%) (%2) Removed from subscription %T%1%n\n"), 1);

	format_add("feed_message_header",	_("%g,+=%G-----%W  %1 %n(ID: %W%2%n)"), 1);
	format_add("feed_message_body",		_("%g||%n %|%1"), 1);
	format_add("feed_message_footer",	_("%g|+=%G----- End of message...%n\n"), 1);

		/* %1 - tag %2 - value */
/* rss: */
	format_add("feed_message_header_generic",	_("%r %1 %W%2"), 1);
	format_add("feed_message_header_pubDate:",	_("%r Napisano: %W%2"), 1);
	format_add("feed_message_header_author:",	_("%r Autor: %W%2"), 1);
/* rdf: */
	format_add("feed_message_header_dc:date:",	_("%r Napisano: %W%2"), 1);
	format_add("feed_message_header_dc:creator:",	_("%r Autor: %W%2"), 1);

	format_add("feed_server_header_generic",	_("%m %1 %W%2"), 1);

	format_add("nntp_command_help_header",	_("%g,+=%G----- %2 %n(%T%1%n)"), 1);
	format_add("nntp_command_help_item",	_("%g|| %W%1: %n%2"), 1);
	format_add("nntp_command_help_footer",	_("%g`+=%G----- End of 100%n\n"), 1);

	format_add("nntp_message_quote_level1",	"%g%1", 1);
	format_add("nntp_message_quote_level2", "%y%1", 1);
	format_add("nntp_message_quote_level",	"%B%1", 1);	/* upper levels.. */
	format_add("nntp_message_signature",	"%B%1", 1);

	format_add("nntp_posting_failed",	_("(%1) Posting to group: %2 failed: %3 (post saved in: %4)"), 1);
	format_add("nntp_posting",		_("(%1) Posting to group: %2 Subject: %3...."), 1);
#endif
	return 0;
}

