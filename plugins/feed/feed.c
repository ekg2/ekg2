#include "ekg2-config.h"

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/vars.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include "feed.h"

static int feed_theme_init();
PLUGIN_DEFINE(feed, PLUGIN_PROTOCOL, feed_theme_init);

QUERY(feed_validate_uid)
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

QUERY(feed_session) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

        if (!s)
                return -1;

	if (
		(xstrncasecmp(session, "nntp:", 5) 
#ifdef HAVE_EXPAT
		&& xstrncasecmp(session, "rss:", 4))
#endif
	   )
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
	} else if (s->priv) {
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

QUERY(rss_message) {
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

//	if (*new == 0) return 0;		XXX

	switch (dmode) {
		case 0:	 print("none", "new message");		/* only notify */
		case -1: return 0;				/* do nothing */

		case 2:	body		= NULL;			/* only headers */
		case 1:	if (dmode == 1) headers = NULL;		/* only body */
		default:					/* default: 3 (body+headers) */
		case 3:	sheaders = NULL;			/* headers+body */
		case 4:	break;					/* shreaders+headers+body */
	}
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
		char *str = xstrdup(headers);
		char *formated = NULL;
		while ((tmp = split_line(&str))) {
			char *value = NULL;
			char *formatka;

			if ((value = xstrchr(tmp, ' '))) *value = 0;
			if (dheaders && !xstrstr(dheaders, tmp)) continue;	/* jesli mamy display_headers a tego nie mamy na liscie to pomijamy */

			formatka = saprintf("feed_message_header_%s", tmp);
			if ((!xstrcmp(format_find(formatka), ""))) { xfree(formatka); formatka = NULL; }
	
			formated = format_string(format_find(formatka ? formatka : "feed_message_header_generic"), tmp, value ? value+1 : "");
			print_window(uid, s, 1, "feed_message_body", formated ? formated : tmp);

			xfree(formatka);
		}
		if (body) print_window(uid, s, 1, "feed_message_body", "");	/* rozdziel */
	}
	if (body) {
		if (session_check(s, 0, "nntp")) {
			int article_signature	= 0;
			char *str		= xstrdup(body);

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
		} else {
			print_window(uid, s, 1, "feed_message_body", body);
		}
	}

	print_window(uid, s, 1, "feed_message_footer");

	*new = 0;
	return 0;
}

void feed_set_status(userlist_t *u, char *status) {
	char *tmp;
	if (!u || !status) return;

	tmp 		= u->status;
	u->status	= status;
	xfree(tmp);
}

void feed_set_descr(userlist_t *u, char *descr) {
	char *tmp;
	if (!u || !descr) return;

	tmp 		= u->descr;
	u->descr	= descr;
	xfree(tmp);
}

void feed_set_statusdescr(userlist_t *u, char *status, char *descr) {
	feed_set_status(u, status);
	feed_set_descr(u, descr);
}

int feed_plugin_init(int prio) {
	plugin_register(&feed_plugin, prio);
			/* common */
	query_connect(&feed_plugin, "session-added", feed_session, (void*) 1);
	query_connect(&feed_plugin, "session-removed", feed_session, (void*) 0);
        query_connect(&feed_plugin, "protocol-validate-uid", feed_validate_uid, NULL);
			/* common - rss, nntp */
	query_connect(&feed_plugin, "rss-message", rss_message, NULL);
			/* common - vars */
	plugin_var_add(&feed_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&feed_plugin, "alias", VAR_STR, NULL, 0, NULL);

			/* (-1 - nothing; 0 - only notify; 1 - only body; 2 - only headers; 3 - headers+body 4 - sheaders+headers+ body)  default+else: 3 */
	plugin_var_add(&feed_plugin, "display_mode", VAR_INT, "3", 0, NULL);	
	plugin_var_add(&feed_plugin, "display_headers", VAR_STR, 
		/* RSS: */ 
			"pubDate: author: " 
		/* NNTP: */ 
			"From: Date: Newsgroups: Subject: User-Agent: NNTP-Posting-Host:", 
		0, NULL);
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
	format_add("feed_subcribe_already",	_("%) You already subscribe this group: %1"), 1);
	format_add("feed_subscribe_no",		_("%) Subscribtion not found, cannot unsubscribe"), 1);

	format_add("feed_message_header",	_("%g,+=%G-----%W  %1 %n(ID: %W%2%n)"), 1);
	format_add("feed_message_body",		_("%g||%n%| %1"), 1);
	format_add("feed_message_footer",	_("%g|+=%G----- End of message...%n\n"), 1);

		/* %1 - tag %2 - value */
	format_add("feed_message_header_generic",	_("%r %1 %W%2"), 1);
	format_add("feed_message_header_pubDate:",	_("%r Napisano: %W%2"), 1);
	format_add("feed_message_header_author:",	_("%r Autor: %W%2"), 1);

	format_add("feed_server_header_generic",	_("%m %1 %W%2"), 1);

	format_add("nntp_command_help_header",	_("%g,+=%G----- %2 %n(%T%1%n)"), 1);
	format_add("nntp_command_help_item",	_("%g|| %W%1: %n%2"), 1);
	format_add("nntp_command_help_footer",	_("%g`+=%G----- End of 100%n\n"), 1);

	format_add("nntp_message_quote_level1",	"%g%1", 1);
	format_add("nntp_message_quote_level2", "%y%1", 1);
	format_add("nntp_message_quote_level",	"%B%1", 1);	/* upper levels.. */
	format_add("nntp_message_signature",	"%B%1", 1);
	return 0;
}

