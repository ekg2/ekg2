#ifndef __EKG_QUERIES
#define __EKG_QUERIES

#define QUERY_ARGS_MAX 15

enum query_arg_type {
	QUERY_ARG_END = 0,	/* MUSTBE LAST ELEMENT OF `query_arg_type` */

	QUERY_ARG_CHARP,	/* char * 	*/
	QUERY_ARG_CHARPP,	/* char ** 	*/
	QUERY_ARG_INT, 		/* int */
	QUERY_ARG_UINT,		/* unsgined int */		/* -> time_t, uint32_t */
	QUERY_ARG_WINDOW = 100, /* window_t	*/
	QUERY_ARG_FSTRING, 	/* fstring_t	*/
};

struct query {
	int id;
	char *name;
	enum query_arg_type params[QUERY_ARGS_MAX];	/* scripts will use it */
};

/* uniq id of known queries... keep it sorted.. please */
enum queries_id {
	IRC_TOPIC = 0,
	MAIL_COUNT,

	CONFIG_POSTINIT,

	PROTOCOL_CONNECTED,
	PROTOCOL_DISCONNECTED,
	PROTOCOL_MESSAGE,
	PROTOCOL_STATUS,

	UI_BEEP,
	UI_IS_INITIALIZED,
	UI_KEYPRESS,
	UI_LOOP,
	UI_WINDOW_ACT_CHANGED,
	UI_WINDOW_KILL,
	UI_WINDOW_NEW,
	UI_WINDOW_PRINT,

	QUERY_EXTERNAL,
};

/* list of known queries. keep it sorted too. */
#ifdef __DECLARE_QUERIES_STUFF
#undef __DECLARE_QUERIES_STUFF

struct query query_list[] = {
	{ IRC_TOPIC, "irc-topic", {
		QUERY_ARG_CHARP,		/* if CHANNEL -> topic; 	if USER -> ident@host */
		QUERY_ARG_CHARP,		/* if CHANNEL -> topicby;	if USER -> realname */
		QUERY_ARG_CHARP,		/* if CHANNEL -> chanmodes;	if USER -> undefined */
		QUERY_ARG_END } },

	{ MAIL_COUNT, "mail-count", {
		QUERY_ARG_INT,			/* mail count */
		QUERY_ARG_END } },

	{ CONFIG_POSTINIT, "config-postinit", {
		QUERY_ARG_END } },		/* no params */

	{ PROTOCOL_CONNECTED, "protocol-connected", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_END } }, 

	{ PROTOCOL_DISCONNECTED, "protocol-disconnected", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* reason */
		QUERY_ARG_INT,			/* type */
		QUERY_ARG_END } }, 

	{ PROTOCOL_MESSAGE, "protocol-message", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARPP,		/* rcpts */
		QUERY_ARG_CHARP,		/* text */
		QUERY_ARG_UINT,	/* uint32 */	/* format */
		QUERY_ARG_UINT,	/* time_t */	/* sent */
		QUERY_ARG_INT,			/* class */
		QUERY_ARG_CHARP,		/* seq */
		QUERY_ARG_INT,			/* dobeep */
		QUERY_ARG_INT,			/* secure */
		QUERY_ARG_END } },

	{ PROTOCOL_STATUS, "protocol-status", {
		QUERY_ARG_CHARP,		/* session */
		QUERY_ARG_CHARP,		/* uid */
		QUERY_ARG_CHARP,		/* status */
		QUERY_ARG_CHARP,		/* descr */
		QUERY_ARG_CHARP,		/* host */
		QUERY_ARG_INT,			/* port */
		QUERY_ARG_UINT, /* time_t */	/* when */
		QUERY_ARG_END } }, 

	{ UI_BEEP, "ui-beep", {
		QUERY_ARG_END } }, 		/* no params */

	{ UI_IS_INITIALIZED, "ui-is-initialized", {
		QUERY_ARG_INT,			/* is_ui */
		QUERY_ARG_END } }, 

	{ UI_KEYPRESS, "ui-keypress", {
		QUERY_ARG_UINT,	 		/* key */
		QUERY_ARG_END } },

	{ UI_LOOP, "ui-loop", {
		QUERY_ARG_END } }, 		/* no params */

	{ UI_WINDOW_ACT_CHANGED, "ui-window-act-changed", {
		QUERY_ARG_END } },		/* no params */

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
};

/* other, not listed above here queries, for example plugin which use internally his own query, 
 * and if devel of that plugin doesn't want share with us info about that plugin..
 * can use query_connect() query_emit() and it will work... however, binding that query from scripts won't work.. untill devel fill query_arg_type...
 */

static list_t queries_external;
static int queries_count = QUERY_EXTERNAL;	/* list_count(queries_other)+QUERY_EXTERNAL */
#else

extern struct query query_list[];	/* for scripts.h */

#endif

#endif

