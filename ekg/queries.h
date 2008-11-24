#ifndef __EKG_QUERIES
#define __EKG_QUERIES

#ifdef __cplusplus
extern "C" {
#endif

#define QUERY_ARGS_MAX 12

enum query_arg_type {
	QUERY_ARG_END = 0,	/* MUSTBE LAST ELEMENT OF `query_arg_type` */

	QUERY_ARG_CHARP,	/* char *	*/
	QUERY_ARG_CHARPP,	/* char **	*/
	QUERY_ARG_INT,		/* int */
	QUERY_ARG_UINT,		/* unsgined int */		/* -> time_t, uint32_t */

	QUERY_ARG_WINDOW = 100, /* window_t	*/
	QUERY_ARG_FSTRING,	/* fstring_t	*/
	QUERY_ARG_USERLIST,	/* userlist_t	*/
	QUERY_ARG_SESSION	/* session_t	*/
};

struct query_def {
	int id;
	char *name;
	enum query_arg_type params[QUERY_ARGS_MAX];	/* scripts will use it */
};

/* uniq id of known queries..., add new just before QUERY_EXTERNAL */
enum queries_id {
	MAIL_COUNT = 0, DAY_CHANGED, STATUS_SHOW, PLUGIN_PRINT_VERSION,
	SET_VARS_DEFAULT, VARIABLE_CHANGED,

	BINDING_COMMAND, BINDING_DEFAULT, BINDING_SET,						/* bindings */
	EVENT_ADDED, EVENT_REMOVED,								/* event events */
	MESSAGE_ENCRYPT, MESSAGE_DECRYPT,							/* encryption */
	METACONTACT_ADDED, METACONTACT_ITEM_ADDED, METACONTACT_ITEM_REMOVED, METACONTACT_REMOVED,/* metacontact */
	PROTOCOL_MESSAGE_SENT, PROTOCOL_MESSAGE_RECEIVED, PROTOCOL_MESSAGE_POST,		/* proto-message-events */
	EVENT_AWAY, EVENT_AVAIL, EVENT_DESCR, EVENT_ONLINE, EVENT_NA,				/* status-events */
	USERLIST_ADDED, USERLIST_CHANGED, USERLIST_REMOVED, USERLIST_RENAMED, USERLIST_INFO,	/* userlist */
	USERLIST_PRIVHANDLE,
	SESSION_ADDED, SESSION_CHANGED, SESSION_REMOVED, SESSION_RENAMED, SESSION_STATUS,	/* session */
	EKG_SIGUSR1, EKG_SIGUSR2,								/* signals */
	CONFIG_POSTINIT, QUITTING,								/* ekg-events */

	IRC_TOPIC, IRC_PROTOCOL_MESSAGE, IRC_KICK,						/* irc-events */
	RSS_MESSAGE,										/* rss-events */

	PROTOCOL_CONNECTED, PROTOCOL_DISCONNECTED, PROTOCOL_MESSAGE, PROTOCOL_MESSAGE_ACK, PROTOCOL_STATUS,
	PROTOCOL_VALIDATE_UID, PROTOCOL_XSTATE,

	ADD_NOTIFY, REMOVE_NOTIFY,
	PROTOCOL_IGNORE, PROTOCOL_UNIGNORE,

	CONFERENCE_RENAMED,

	UI_BEEP, UI_IS_INITIALIZED, UI_KEYPRESS, UI_LOOP, UI_WINDOW_ACT_CHANGED,
	UI_WINDOW_CLEAR, UI_WINDOW_KILL, UI_WINDOW_NEW, UI_WINDOW_PRINT, UI_WINDOW_REFRESH,
	UI_WINDOW_SWITCH, UI_WINDOW_TARGET_CHANGED,

	GPG_MESSAGE_ENCRYPT, GPG_MESSAGE_DECRYPT, GPG_SIGN, GPG_VERIFY,

	UI_WINDOW_UPDATE_LASTLOG,
	SESSION_EVENT,
	UI_REFRESH,
	PROTOCOL_TYPING_OUT,
	UI_PASSWORD_INPUT,
	PROTOCOL_DISCONNECTING,

	USERLIST_REFRESH,

	QUERY_EXTERNAL,
};

#ifdef __DECLARE_QUERIES_STUFF
#undef __DECLARE_QUERIES_STUFF

/* list of known queries. keep it sorted with enum. */

const struct query_def query_list[] = {
	{ MAIL_COUNT, "mail-count", {
		QUERY_ARG_INT,			/* mail count */
		QUERY_ARG_END } },

	{ DAY_CHANGED, "day-changed", {
		/* XXX: struct tm *, struct tm * */
		QUERY_ARG_END } },

	{ STATUS_SHOW, "status-show", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ PLUGIN_PRINT_VERSION, "plugin-print-version", {
		QUERY_ARG_END } },		/* no params */

	{ SET_VARS_DEFAULT, "set-vars-default", {
		QUERY_ARG_END } },		/* no params */

	{ VARIABLE_CHANGED, "variable-changed", {
		QUERY_ARG_CHARP,		/* variable */
		QUERY_ARG_END } },

	{ BINDING_COMMAND, "binding-command", {
		/* XXX */
		QUERY_ARG_END } },

	{ BINDING_DEFAULT, "binding-default", {
		/* XXX */
		QUERY_ARG_END } },

	{ BINDING_SET, "binding-set", {
		/* XXX */
		QUERY_ARG_END } },

	{ EVENT_ADDED, "event-added", {
		QUERY_ARG_CHARP,		/* event name */
		QUERY_ARG_END } },

	{ EVENT_REMOVED, "event-removed", {
		/* XXX, never used */
		QUERY_ARG_END } },

	{ MESSAGE_ENCRYPT, "message-encrypt", {
		/* XXX */
		QUERY_ARG_END } },

	{ MESSAGE_DECRYPT, "message-decrypt", {
		/* XXX */
		QUERY_ARG_END } },
	
	{ METACONTACT_ADDED, "metacontact-added", {
		QUERY_ARG_CHARP,		/* metacontact name */
		QUERY_ARG_END } },

	{ METACONTACT_ITEM_ADDED, "metacontact-item-added", {
		/* XXX */
		QUERY_ARG_END } },
	
	{ METACONTACT_ITEM_REMOVED, "metacontact-item-removed", {
		/* XXX */
		QUERY_ARG_END } },

	{ METACONTACT_REMOVED, "metacontact-removed", {
		QUERY_ARG_CHARP,		/* metacontact name */
		QUERY_ARG_END } },

	{ PROTOCOL_MESSAGE_SENT, "protocol-message-sent", {
		QUERY_ARG_CHARP,	/* session */
		QUERY_ARG_CHARP,	/* uid */
		QUERY_ARG_CHARP,	/* text */
		QUERY_ARG_END } },

	{ PROTOCOL_MESSAGE_RECEIVED, "protocol-message-received", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_UINT,	/* uint32_t */	/* format */
		QUERY_ARG_UINT, /* time_t */	/* sent */
		QUERY_ARG_INT,			/* klass */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } },
	
	{ PROTOCOL_MESSAGE_POST, "protocol-message-post", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_UINT,	/* uint32_t */	/* format */
		QUERY_ARG_UINT, /* time_t */	/* sent */
		QUERY_ARG_INT,			/* klass */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } }, 

	{ EVENT_AWAY, "event_away", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ EVENT_AVAIL, "event_avail", {
		/* XXX, emited, but noone connect to this. */
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ EVENT_DESCR, "event_descr", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* descr */
		QUERY_ARG_END } },

	{ EVENT_ONLINE, "event_online", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ EVENT_NA, "event_na", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ USERLIST_ADDED, "userlist-added", {
		/* XXX, we need here a session->uid too (?) */

		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* nickname */
		QUERY_ARG_INT,			/* quiet */
		QUERY_ARG_END } },

	{ USERLIST_CHANGED, "userlist-changed", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ USERLIST_REMOVED, "userlist-removed", {
		/* XXX, we need here a session->uid too (?) */

		QUERY_ARG_CHARP,		/* nickname or uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_END } },

	{ USERLIST_RENAMED, "userlist-renamed", {
		/* XXX */
		QUERY_ARG_END } },

	{ USERLIST_INFO, "userlist-info", {
		/* XXX */
		QUERY_ARG_END } },

	{ USERLIST_PRIVHANDLE, "userlist-privhandle", {
		QUERY_ARG_USERLIST,		/* userlist_t */
		QUERY_ARG_INT,			/* function */
		/* optional things? */
		QUERY_ARG_END } },

	{ SESSION_ADDED, "session-added", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ SESSION_CHANGED, "session-changed", {
		QUERY_ARG_END } },		/* no params */

	{ SESSION_REMOVED, "session-removed", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ SESSION_RENAMED, "session-renamed", {
		QUERY_ARG_CHARP,		/* new session alias */
		QUERY_ARG_END } },

	{ SESSION_STATUS, "session-status", {
		/* XXX */
		QUERY_ARG_END } },

	{ EKG_SIGUSR1, "sigusr1", {
		QUERY_ARG_END } },		/* no params */

	{ EKG_SIGUSR2, "sigusr2", {
		QUERY_ARG_END } },		/* no params */

	{ CONFIG_POSTINIT, "config-postinit", {
		QUERY_ARG_END } },		/* no params */

	{ QUITTING, "quitting", {
		/* XXX, emited, but never used */
		QUERY_ARG_CHARP,		/* reason */
		QUERY_ARG_END } },

	{ IRC_TOPIC, "irc-topic", {
		QUERY_ARG_CHARP,		/* if CHANNEL -> topic;		if USER -> ident@host */
		QUERY_ARG_CHARP,		/* if CHANNEL -> topicby;	if USER -> realname */
		QUERY_ARG_CHARP,		/* if CHANNEL -> chanmodes;	if USER -> undefined */
		QUERY_ARG_END } },

	{ IRC_PROTOCOL_MESSAGE, "irc-protocol-message", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* text */
		QUERY_ARG_INT,			/* isour */
		QUERY_ARG_INT,			/* foryou */
		QUERY_ARG_INT,			/* private */
		QUERY_ARG_CHARP,		/* channame */
		QUERY_ARG_END } },

	{ IRC_KICK, "irc-kick", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* nick */
		QUERY_ARG_CHARP,		/* channel */
		QUERY_ARG_CHARP,		/* kickedby */
		QUERY_ARG_END } },

	{ RSS_MESSAGE, "rss-message", {
		/* XXX */
		QUERY_ARG_END } },

	{ PROTOCOL_CONNECTED, "protocol-connected", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_END } }, 

	{ PROTOCOL_DISCONNECTED, "protocol-disconnected", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* reason */
		QUERY_ARG_INT,			/* type */
		QUERY_ARG_END } }, 

	{ PROTOCOL_MESSAGE, "protocol-message", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_CHARP,		/* text */
		QUERY_ARG_UINT,	/* uint32 */	/* format */
		QUERY_ARG_UINT,	/* time_t */	/* sent */
		QUERY_ARG_INT,			/* klass */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* dobeep */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } },

	{ PROTOCOL_MESSAGE_ACK, "protocol-message-ack", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* status */
		QUERY_ARG_END } },

	{ PROTOCOL_STATUS, "protocol-status", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* status */
		QUERY_ARG_CHARP,		/* descr */
		QUERY_ARG_UINT, /* time_t */	/* when */
		QUERY_ARG_END } }, 

	{ PROTOCOL_VALIDATE_UID, "protocol-validate-uid", {
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* valid */
		QUERY_ARG_END } },

	{ PROTOCOL_XSTATE, "protocol-xstate", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* state	- bits on */
		QUERY_ARG_INT,			/* offstate	- bits off */
		QUERY_ARG_END } },

	{ ADD_NOTIFY, "add-notify", {
		/* XXX */
		QUERY_ARG_END } },

	{ REMOVE_NOTIFY, "remove-notify", {
		/* XXX */
		QUERY_ARG_END } },

	{ PROTOCOL_IGNORE, "protocol-ignore", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* oldlevel */
		QUERY_ARG_INT,			/* newlevel */
		QUERY_ARG_END } },

	{ PROTOCOL_UNIGNORE, "protocol-unignore", {
		/* XXX */
		QUERY_ARG_END } },

	{ CONFERENCE_RENAMED, "conference-renamed", {
		/* XXX */
		QUERY_ARG_END } },

	{ UI_BEEP, "ui-beep", {
		QUERY_ARG_END } },		/* no params */

	{ UI_IS_INITIALIZED, "ui-is-initialized", {
		QUERY_ARG_INT,			/* is_ui */
		QUERY_ARG_END } }, 

	{ UI_KEYPRESS, "ui-keypress", {
		QUERY_ARG_INT,	 /* XXX uint? *//* key */
		QUERY_ARG_END } },

	{ UI_LOOP, "ui-loop", {
		QUERY_ARG_END } },		/* no params */

	{ UI_WINDOW_ACT_CHANGED, "ui-window-act-changed", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ UI_WINDOW_CLEAR, "ui-window-clear", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ UI_WINDOW_KILL, "ui-window-kill", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ UI_WINDOW_NEW, "ui-window-new", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } }, 

	{ UI_WINDOW_PRINT, "ui-window-print", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_FSTRING,		/* fstring_t */
		QUERY_ARG_END } }, 

	{ UI_WINDOW_REFRESH, "ui-window-refresh", {
		QUERY_ARG_END } },		/* no params */

	{ UI_WINDOW_SWITCH, "ui-window-switch", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

	{ UI_WINDOW_TARGET_CHANGED , "ui-window-target-changed", {
		QUERY_ARG_WINDOW,		/* window */
		QUERY_ARG_END } },

/* GPG: PARAMS XXX */
	{ GPG_MESSAGE_ENCRYPT, "gpg-message-encrypt", {
		QUERY_ARG_END } },

	{ GPG_MESSAGE_DECRYPT, "gpg-message-decrypt", {
		QUERY_ARG_END } },

	{ GPG_SIGN, "gpg-sign", {
		QUERY_ARG_END } },

	{ GPG_VERIFY, "gpg-verify", {
		QUERY_ARG_END } },

	{ UI_WINDOW_UPDATE_LASTLOG, "ui-window-update-lastlog", {
		QUERY_ARG_END } },

	{ SESSION_EVENT, "session-event", {
		QUERY_ARG_SESSION,		/* session */
		QUERY_ARG_INT,			/* event type, [not used] */
		QUERY_ARG_END } },

	{ UI_REFRESH, "ui-refresh", {
		QUERY_ARG_END } },

	{ PROTOCOL_TYPING_OUT, "protocol-typing-out", {
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_INT,			/* typed-in data length */
		QUERY_ARG_INT,			/* whether this is first typing notification in order
						   or only length change */
		QUERY_ARG_END } },

	{ UI_PASSWORD_INPUT, "ui-password-input", {
		QUERY_ARG_CHARP,		/* password pointer storage */
		QUERY_ARG_CHARP,		/* alternate input prompt (&NULL = default) */
		QUERY_ARG_CHARP,		/* alternate repeat prompt (&NULL = default, NULL = no) */
		QUERY_ARG_END } },

	{ PROTOCOL_DISCONNECTING, "protocol-disconnecting", { /* meant to be send before user-initiated disconnect,
								 when we can still send some data, e.g. <gone/> chatstate */
		QUERY_ARG_CHARP,		/* session uid */
		QUERY_ARG_END } },

	{ USERLIST_REFRESH, "userlist-refresh", {
		QUERY_ARG_END } },
};

/* other, not listed above here queries, for example plugin which use internally his own query, 
 * and if devel of that plugin doesn't want share with us info about that plugin..
 * can use query_connect() query_emit() and it will work... however, binding that query from scripts/events (/on) won't work.. untill devel fill query_arg_type...
 */

static list_t queries_external;
static int queries_count = QUERY_EXTERNAL;	/* list_count(queries_other)+QUERY_EXTERNAL */
#else

extern struct query_def query_list[];		/* for: events.h scripts.h */

#endif

#ifdef __cplusplus
}
#endif

#endif

