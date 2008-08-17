/* $Id$ */

#ifndef __EKG_JABBER_JABBER_H
#define __EKG_JABBER_JABBER_H

#include <ekg2-config.h>

#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>		/* XXX, protocol_uid() */
#include <ekg/sessions.h>
#include <ekg/userlist.h>

#ifdef HAVE_EXPAT_H
 #include <expat.h>
#endif

#include "jabber-ssl.h"

#define DEFAULT_CLIENT_NAME "EKG2 -- http://www.ekg2.org"
#define JABBER_DEFAULT_RESOURCE "ekg2"

/* some tlen constants */
#define TLEN_HUB "idi.tlen.pl"			/* hub			*/
#define TLEN_FALLBACK_SERVER "s1.tlen.pl"	/* fallback server	*/
#define TLEN_FALLBACK_PORT 443			/* fallback port	*/

#define tlenjabber_escape(str)	 (j->istlen ? tlen_encode(str) : jabber_escape(str))
#define tlenjabber_unescape(str) (j->istlen ? tlen_decode(str) : jabber_unescape(str))
#define tlenjabber_uid(target)	 protocol_uid(j->istlen ? "tlen" : "xmpp", target)

#define tlen_uid(target) protocol_uid("tlen", target)
#define xmpp_uid(target) protocol_uid("xmpp", target)

struct xmlnode_s {
	char *name;
	char *data; 
	char **atts;
	char *xmlns;

	struct xmlnode_s *parent;
	struct xmlnode_s *children;
	
	struct xmlnode_s *next;
/*	struct xmlnode_s *prev; */
};

typedef struct xmlnode_s xmlnode_t;

enum jabber_opengpg_type_t {
	JABBER_OPENGPG_ENCRYPT = 0,
	JABBER_OPENGPG_DECRYPT,
	JABBER_OPENGPG_SIGN,
	JABBER_OPENGPG_VERIFY
};

enum jabber_bookmark_type_t {			/* see JEP-0048 for details */
	JABBER_BOOKMARK_UNKNOWN = 0,
	JABBER_BOOKMARK_URL,
	JABBER_BOOKMARK_CONFERENCE,
};

typedef enum {
	JABBER_IQ_TYPE_NONE,
	JABBER_IQ_TYPE_GET,
	JABBER_IQ_TYPE_SET,
	JABBER_IQ_TYPE_RESULT,
	JABBER_IQ_TYPE_ERROR,
} jabber_iq_type_t;

typedef struct {
	char *name;
	char *url;
} jabber_bookmark_url_t;

typedef struct {
	char *name;
	char *jid;
	unsigned int autojoin : 1;
	char *nick;
	char *pass;
} jabber_bookmark_conference_t;

typedef struct {
	enum jabber_bookmark_type_t type;
	union {	/* private data based on bookmark type */
		jabber_bookmark_url_t *url;		/* for JABBER_BOOKMARK_URL */
		jabber_bookmark_conference_t *conf;	/* for JABBER_BOOKMARK_CONFERENCE */
		void *other; /* ? ;p */
	} private;
} jabber_bookmark_t;

enum jabber_compression_method {
	JABBER_COMPRESSION_NONE = 0,
	JABBER_COMPRESSION_ZLIB_INIT,
	JABBER_COMPRESSION_LZW_INIT,
	
	JABBER_COMPRESSION_ZLIB,
	JABBER_COMPRESSION_LZW,
};

	/* name				bit			allow/block:	*/
typedef enum {
	PRIVACY_LIST_MESSAGE		= 1,		/*	incoming messages */
	PRIVACY_LIST_IQ			= 2,		/*	incoming iq packets */
	PRIVACY_LIST_PRESENCE_IN	= 4,		/*	incoming presence packets */
	PRIVACY_LIST_PRESENCE_OUT	= 8,		/*	outgoint presence packets */
	PRIVACY_LIST_ALL		= (PRIVACY_LIST_MESSAGE | PRIVACY_LIST_IQ | PRIVACY_LIST_PRESENCE_IN | PRIVACY_LIST_PRESENCE_OUT)
} jabber_iq_privacy_flags_t;

typedef struct {
	char *type;						/* jid/group/subscription/ */
	char *value;						/* jid:.../@group/subscription ---- value */
	unsigned int allow : 1;					/* 1 - allow 0 - deny */
	jabber_iq_privacy_flags_t items;			/* lista bitmaski j/w */
	unsigned int order;					/* order */
} jabber_iq_privacy_t;

typedef struct {
	char	*thread;
	char	*uid;
	char	*subject;
	void	*next;
} jabber_conversation_t;

typedef struct {
	char *id;
	char *to;
	char *type;
	char *xmlns;
	void (*handler)(session_t *s, xmlnode_t *n, const char *from, const char *id);
	void (*error)(session_t *s, xmlnode_t *n, const char *from, const char *id);
} jabber_stanza_t;

/**
 * jabber_private_t contains private data of jabber/tlen session.
 */
typedef struct {
	int fd;				/**< connection's fd */
	unsigned int istlen	: 2;	/**< whether this is a tlen session, 2 if connecting to tlen hub (XXX: ugly hack) */

	enum jabber_compression_method using_compress;	/**< whether we're using compressed connection, and what method */
#ifdef JABBER_HAVE_SSL
	unsigned char using_ssl	: 2;	/**< 1 if we're using SSL, 2 if we're using TLS, else 0 */
	SSL_SESSION ssl_session;	/**< SSL session */
#ifdef JABBER_HAVE_GNUTLS
	gnutls_certificate_credentials xcred;	/**< gnutls credentials (?) */
#endif
#endif
	int id;				/**< queries ID */
	XML_Parser parser;		/**< expat instance */
	char *server;			/**< server name */
	uint16_t port;			/**< server's port number */
	unsigned int sasl_connecting :1;/**< whether we're connecting over SASL */
	char *resource;			/**< resource used when connecting to daemon */
	char *last_gmail_result_time;	/**< last time we're checking mail (this seems not to work correctly ;/) */
	char *last_gmail_tid;		/**< lastseen mail thread-id */
	list_t privacy;			/**< for jabber:iq:privacy */
	list_t bookmarks;		/**< for jabber:iq:private <storage xmlns='storage:bookmarks'> */
	list_t iq_stanzas;

	watch_t *send_watch;
	watch_t *connect_watch;

	xmlnode_t *node;		/**< current XML branch */
	jabber_conversation_t *conversations;	/**< known conversations */
} jabber_private_t;

typedef struct {
	int authtype;

	/* from muc_userlist_t */
	char *role;		/* role: */
	char *aff;		/* affiliation: */
} jabber_userlist_private_t;

enum jabber_auth_t {
	EKG_JABBER_AUTH_NONE	= 0,
	EKG_JABBER_AUTH_FROM	= 1,
	EKG_JABBER_AUTH_TO	= 2,
	EKG_JABBER_AUTH_BOTH	= 3,
	EKG_JABBER_AUTH_REQ	= 4,
	EKG_JABBER_AUTH_UNREQ	= 8
};

extern plugin_t jabber_plugin;
extern char *jabber_default_pubsub_server;
extern char *jabber_default_search_server;
extern int config_jabber_beep_mail;
extern const char *jabber_authtypes[];

#define jabber_private(s)		((jabber_private_t*) session_private_get(s))
#define jabber_userlist_priv_get(u)	((jabber_userlist_private_t *) userlist_private_get(&jabber_plugin, u))

void jabber_register_commands(void);
XML_Parser jabber_parser_recreate(XML_Parser parser, void *data);

int JABBER_COMMIT_DATA(watch_t *w);
void jabber_handle(void *data, xmlnode_t *n);

int jabber_privacy_freeone(jabber_private_t *j, jabber_iq_privacy_t *item);
int jabber_stanza_freeone(jabber_private_t *j, jabber_stanza_t *stanza);

const char *jabber_iq_reg(session_t *s, const char *prefix, const char *to, const char *type, const char *xmlns);
const char *jabber_iq_send(session_t *s, const char *prefix, jabber_iq_type_t iqtype, const char *to, const char *type, const char *xmlns);

/* digest.c hashowanie.. */
char *jabber_digest(const char *sid, const char *password, void *charset);
char *jabber_sha1_generic(char *buf, int len);
char *jabber_dcc_digest(char *sid, char *initiator, char *target);
char *jabber_challange_digest(const char *sid, const char *password, const char *nonce, const char *cnonce, const char *xmpp_temp, const char *realm);
void jabber_iq_auth_send(session_t *s, const char *username, const char *passwd, const char *stream_id);

char *jabber_attr(char **atts, const char *att);
char *jabber_escape(const char *text);
char *jabber_unescape(const char *text);
char *tlen_encode(const char *what);
char *tlen_decode(const char *what);
int jabber_write_status(session_t *s);

void jabber_convert_string_init(int is_tlen);
void jabber_convert_string_destroy();
QUERY(jabber_convert_string_reinit);

void jabber_reconnect_handler(int type, void *data);

LIST_ADD_COMPARE(jabber_privacy_add_compare, jabber_iq_privacy_t *);
int jabber_privacy_free(jabber_private_t *j);
int jabber_bookmarks_free(jabber_private_t *j);
int jabber_iq_stanza_free(jabber_private_t *j);

#define jabber_write(s, args...) watch_write((s && s->priv) ? jabber_private(s)->send_watch : NULL, args);
WATCHER_LINE(jabber_handle_write);

void xmlnode_handle_end(void *data, const char *name);
void xmlnode_handle_cdata(void *data, const char *text, int len);

void jabber_handle_disconnect(session_t *s, const char *reason, int type);

char *jabber_openpgp(session_t *s, const char *fromto, enum jabber_opengpg_type_t way, char *message, char *key, char **error);
#ifdef HAVE_ZLIB
char *jabber_zlib_decompress(const char *buf, int *len);
char *jabber_zlib_compress(const char *buf, int *len);
#endif

int jabber_conversation_find(jabber_private_t *j, const char *uid, const char *subject, const char *thread, jabber_conversation_t **result, const int can_add);
jabber_conversation_t *jabber_conversation_get(jabber_private_t *j, const int n);
char *jabber_thread_gen(jabber_private_t *j, const char *uid);

uint32_t *jabber_msg_format(const char *plaintext, const xmlnode_t *html);
#endif /* __EKG_JABBER_JABBER_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
