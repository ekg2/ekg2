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
	/* ... */

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

int feed_plugin_init(int prio) {
	plugin_register(&feed_plugin, prio);
			/* common */
	query_connect(&feed_plugin, "session-added", feed_session, (void*) 1);
	query_connect(&feed_plugin, "session-removed", feed_session, (void*) 0);
        query_connect(&feed_plugin, "protocol-validate-uid", feed_validate_uid, NULL);
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
	return 0;
}

static int feed_theme_init() {
	format_add("nntp_command_help_header",	_("%g,+=%G----- %2 %n(%T%1%n)"), 1);
	format_add("nntp_command_help_item",	_("%g|| %W%1: %n%2"), 1);
	format_add("nntp_command_help_footer",	_("%g`+=%G----- End of 100%n\n"), 1);

//	format_add("nntp_message_header",	_("%g|| %r %1"), 1);
	format_add("nntp_message_header",	_("%r %1"), 1);

	format_add("nntp_message_body_header",	_("%g,+=%G-----%W  %1 %n(ID: %W%2%n)"), 1);
	format_add("nntp_message_body",		_("%g|| %n%1"), 1);
	format_add("nntp_message_body_end",	_("%g|+=%G----- End of message...%n\n"), 1);
	
	format_add("nntp_message_quote_level1",	"%g%1", 1);
	format_add("nntp_message_quote_level2", "%y%1", 1);
	format_add("nntp_message_quote_level",	"%B%1", 1);	/* upper levels.. */
	format_add("nntp_message_signature",	"%B%1", 1);
	return 0;
}

