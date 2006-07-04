#include "ekg2-config.h"

#include <ekg/commands.h>
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

QUERY(rss_message) {
	char *session	= *(va_arg(ap, char **));
	char *uid	= *(va_arg(ap, char **));

	char *headers	= *(va_arg(ap, char **));
	char *title	= *(va_arg(ap, char **));
	char *url	= *(va_arg(ap, char **));
	char *body	= *(va_arg(ap, char **));

	int *new	= va_arg(ap, int *); 

	session_t *s	= session_find(session);
	char *tmp;

	print_window(uid, s, 1, "feed_message_header", title, url);

	if (headers) {
		char *str = xstrdup(headers);
		char *formated = NULL;
		while ((tmp = split_line(&str))) {
			formated = format_string(format_find("feed_message_headers"), tmp);
			print_window(uid, s, 1, "feed_message_body", formated ? formated : tmp);
		}
	}

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

//				if (i > 0 && tmp[i] == ' ') 		/* normal clients quote >>>> aaaa */
				if (i > 0) 				/* buggy clients quote  >>>>>aaaa */
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
//	query_connect(&feed_plugin, "nntp-message", nntp_message, NULL);
			/* common - vars */
	plugin_var_add(&feed_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&feed_plugin, "alias", VAR_STR, NULL, 0, NULL);
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
	format_add("feed_message_header",	_("%g,+=%G-----%W  %1 %n(ID: %W%2%n)"), 1);
	format_add("feed_message_body",		_("%g||%n%| %1"), 1);
	format_add("feed_message_footer",	_("%g|+=%G----- End of message...%n\n"), 1);

//	format_add("feed_message_headers",	_("%g|| %r %1"), 1);
	format_add("feed_message_headers",	_("%r %1"), 1);

	format_add("nntp_command_help_header",	_("%g,+=%G----- %2 %n(%T%1%n)"), 1);
	format_add("nntp_command_help_item",	_("%g|| %W%1: %n%2"), 1);
	format_add("nntp_command_help_footer",	_("%g`+=%G----- End of 100%n\n"), 1);

	format_add("nntp_message_quote_level1",	"%g%1", 1);
	format_add("nntp_message_quote_level2", "%y%1", 1);
	format_add("nntp_message_quote_level",	"%B%1", 1);	/* upper levels.. */
	format_add("nntp_message_signature",	"%B%1", 1);
	return 0;
}

