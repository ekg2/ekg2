#ifndef __EKG_QUERIES
#define __EKG_QUERIES

#define QUERY_ARGS_MAX 15

enum query_arg_type {
	QUERY_ARG_END = 0,	/* MUSTBE LAST ELEMENT OF `query_arg_type` */

	QUERY_ARG_CHARP,	/* char * 	*/
	QUERY_ARG_CHARPP,	/* char ** 	*/
	QUERY_ARG_INT, 		/* int */
	QUERY_ARG_UINT,		/* unsgined int */		/* -> time_t, uint32_t */
			/* sizeof(unsigned int) == sizeof(int) nie wiadomo po co, moze wyleci... */
	
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
	PROTOCOL_CONNECTED = 0,
	PROTOCOL_DISCONNECTED,
	PROTOCOL_MESSAGE,
	PROTOCOL_STATUS,

	QUERY_INTERNAL,
};

/* list of known queries. keep it sorted too. */

struct query queries[] = {
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

	{ QUERY_UNKNOWN, (void *) 0, { }  }
};

/* other, not listed here queries, for example plugin which use internally his own query, and if devel of that plugin doesn't want share with us info about that plugin..
 * 	can use query_connect() query_emit() and it will work... however, binding that query from scripts won't work.. untill devel fill query_arg_type...
 */

extern list_t queries_other;
extern int next_internal_query = QUERY_INTERNAL;


#endif

