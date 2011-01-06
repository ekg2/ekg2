#include <stdio.h>

#include "queries.h"

/* list of known queries */

static const query_def_t core_query_list[] = {
	{ NULL, "mail-count", 0, {
		QUERY_ARG_INT,			/* mail count */
		QUERY_ARG_END } },

	{ NULL, "day-changed", 0, {
		/* XXX: struct tm *, struct tm * */
		QUERY_ARG_END } },

	{ NULL, "status-show", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ NULL, "plugin-print-version", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "set-vars-default", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "variable-changed", 0, {
		QUERY_ARG_CHARP,		/* variable */
		QUERY_ARG_END } },

	{ NULL, "binding-command", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "binding-default", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "binding-set", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "event-added", 0, {
		QUERY_ARG_CHARP,		/* event name */
		QUERY_ARG_END } },

	{ NULL, "event-removed", 0, {
		/* XXX, never used */
		QUERY_ARG_END } },

	{ NULL, "message-encrypt", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "message-decrypt", 0, {
		/* XXX */
		QUERY_ARG_END } },
	
	{ NULL, "metacontact-added", 0, {
		QUERY_ARG_CHARP,		/* metacontact name */
		QUERY_ARG_END } },

	{ NULL, "metacontact-item-added", 0, {
		/* XXX */
		QUERY_ARG_END } },
	
	{ NULL, "metacontact-item-removed", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "metacontact-removed", 0, {
		QUERY_ARG_CHARP,		/* metacontact name */
		QUERY_ARG_END } },

	{ NULL, "protocol-message-sent", 0, {
		QUERY_ARG_CHARP,	/* session */
		QUERY_ARG_CHARP,	/* uid */
		QUERY_ARG_CHARP,	/* text */
		QUERY_ARG_END } },

	{ NULL, "protocol-message-received", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_CHARP,		/* text */
		QUERY_ARG_UINT,	/* uint32_t */	/* format */
		QUERY_ARG_UINT, /* time_t */	/* sent */
		QUERY_ARG_INT,			/* mclass */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } },

	{ NULL, "protocol-message-post", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_UINT,	/* uint32_t */	/* format */
		QUERY_ARG_UINT, /* time_t */	/* sent */
		QUERY_ARG_INT,			/* mclass */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } }, 

	{ NULL, "event-away", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ NULL, "event-avail", 0, {
		/* XXX, emited, but noone connect to this. */
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ NULL, "event-descr", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* descr */
		QUERY_ARG_END } },

	{ NULL, "event-online", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ NULL, "event-na", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ NULL, "userlist-added", 0, {
		/* XXX, we need here a session->uid too (?) */

		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* nickname */
		QUERY_ARG_INT,			/* quiet */
		QUERY_ARG_END } },

	{ NULL, "userlist-changed", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ NULL, "userlist-removed", 0, {
		/* XXX, we need here a session->uid too (?) */

		QUERY_ARG_CHARP,		/* nickname or uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ NULL, "userlist-renamed", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "userlist-info", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "userlist-privhandle", 0, {
		QUERY_ARG_USERLIST,		/* userlist_t */
		QUERY_ARG_INT,			/* function */
		/* optional things? */
		QUERY_ARG_END } },

	{ NULL, "session-added", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ NULL, "session-changed", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "session-removed", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ NULL, "session-renamed", 0, {
		QUERY_ARG_CHARP,		/* new session alias */
		QUERY_ARG_END } },

	{ NULL, "session-status", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "ekg-sigusr1", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "ekg-sigusr2", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "config-postinit", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "quitting", 0, {
		/* XXX, emited, but never used */
		QUERY_ARG_CHARP,		/* reason */
		QUERY_ARG_END } },

	{ NULL, "irc-topic", 0, {
		QUERY_ARG_CHARP,		/* if CHANNEL -> topic;		if USER -> ident@host */
		QUERY_ARG_CHARP,		/* if CHANNEL -> topicby;	if USER -> realname */
		QUERY_ARG_CHARP,		/* if CHANNEL -> chanmodes;	if USER -> undefined */
		QUERY_ARG_END } },

	{ NULL, "irc-protocol-message", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* text */
		QUERY_ARG_INT,			/* isour */
		QUERY_ARG_INT,			/* foryou */
		QUERY_ARG_INT,			/* private */
		QUERY_ARG_CHARP,		/* channame */
		QUERY_ARG_END } },

	{ NULL, "irc-kick", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* nick */
		QUERY_ARG_CHARP,		/* channel */
		QUERY_ARG_CHARP,		/* kickedby */
		QUERY_ARG_END } },

	{ NULL, "rss-message", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* proto headers */
		QUERY_ARG_CHARP,		/* headers */
		QUERY_ARG_CHARP,		/* title */
		QUERY_ARG_CHARP,		/* url */
		QUERY_ARG_CHARP,		/* descr */
		QUERY_ARG_INT,			/* new */
		QUERY_ARG_INT,			/* modify */
		QUERY_ARG_END } },

	{ NULL, "protocol-connected", 0, {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_END } }, 

	{ NULL, "protocol-disconnected", 0, {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* reason */
		QUERY_ARG_INT,			/* type */
		QUERY_ARG_END } }, 

	{ NULL, "protocol-message", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_CHARP,		/* text */
		QUERY_ARG_UINT,	/* uint32 */	/* format */
		QUERY_ARG_UINT,	/* time_t */	/* sent */
		QUERY_ARG_INT,			/* mclass */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* dobeep */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } },

	{ NULL, "protocol-message-ack", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* status */
		QUERY_ARG_END } },

	{ NULL, "protocol-status", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* status */
		QUERY_ARG_CHARP,		/* descr */
		QUERY_ARG_UINT, /* time_t */	/* when */
		QUERY_ARG_END } }, 

	{ NULL, "protocol-validate-uid", 0, {
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* valid */
		QUERY_ARG_END } },

	{ NULL, "protocol-xstate", 0, {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* state	- bits on */
		QUERY_ARG_INT,			/* offstate	- bits off */
		QUERY_ARG_END } },

	{ NULL, "add-notify", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "remove-notify", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "protocol-ignore", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* oldlevel */
		QUERY_ARG_INT,			/* newlevel */
		QUERY_ARG_END } },

	{ NULL, "protocol-unignore", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "conference-renamed", 0, {
		/* XXX */
		QUERY_ARG_END } },

	{ NULL, "ui-beep", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "ui-is-initialized", 0, {
		QUERY_ARG_INT,			/* is_ui */
		QUERY_ARG_END } }, 

	{ NULL, "ui-keypress", 0, {
		QUERY_ARG_INT,	 /* XXX uint? *//* key */
		QUERY_ARG_END } },

	{ NULL, "ui-loop", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "ui-window-act-changed", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ NULL, "ui-window-clear", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ NULL, "ui-window-kill", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ NULL, "ui-window-new", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } }, 

	{ NULL, "ui-window-print", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_FSTRING,		/* fstring_t */
		QUERY_ARG_END } }, 

	{ NULL, "ui-window-refresh", 0, {
		QUERY_ARG_END } },		/* no params */

	{ NULL, "ui-window-switch", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ NULL, "ui-window-target-changed", 0, {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

/* GPG: PARAMS XXX */
	{ NULL, "gpg-message-encrypt", 0, {
		QUERY_ARG_END } },

	{ NULL, "gpg-message-decrypt", 0, {
		QUERY_ARG_END } },

	{ NULL, "gpg-sign", 0, {
		QUERY_ARG_END } },

	{ NULL, "gpg-verify", 0, {
		QUERY_ARG_END } },

	{ NULL, "ui-window-update-lastlog", 0, {
		QUERY_ARG_END } },

	{ NULL, "session-event", 0, {
		QUERY_ARG_SESSION,		/* session */
		QUERY_ARG_INT,			/* event type, [not used] */
		QUERY_ARG_END } },

	{ NULL, "ui-refresh", 0, {
		QUERY_ARG_END } },

	{ NULL, "protocol-typing-out", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* typed-in data length */
		QUERY_ARG_INT,			/* whether this is first typing notification in order
						   or only length change */
		QUERY_ARG_END } },

	{ NULL, "ui-password-input", 0, {
		QUERY_ARG_CHARP,		/* password pointer storage */
		QUERY_ARG_CHARP,		/* alternate input prompt (&NULL = default) */
		QUERY_ARG_CHARP,		/* alternate repeat prompt (&NULL = default, NULL = no) */
		QUERY_ARG_END } },

	{ NULL, "protocol-disconnecting", 0, { /* meant to be send before user-initiated disconnect,
								 when we can still send some data, e.g. <gone/> chatstate */
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ NULL, "userlist-refresh", 0, {
		QUERY_ARG_END } },

	{ NULL, "event-offline", 0, {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },
};


int queries_init() {
	query_def_t *p = core_query_list;
	size_t i;

	for (i = 0; i < sizeof(core_query_list) / sizeof(*core_query_list); ++i, ++p) {
		query_register_const(p);
	}
	return 0;
}

