/* $Id$ */

#ifndef __EKG_JABBER_JABBER_H
#define __EKG_JABBER_JABBER_H

#include <ekg2-config.h>

#include <ekg/char.h>
#include <ekg/plugins.h>
#include <ekg/sessions.h>

#include <expat.h>

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#define DEFAULT_CLIENT_NAME "EKG2 -- http://www.ekg2.org"
#define JABBER_DEFAULT_RESOURCE TEXT("ekg2")

/* some tlen constants */
#define TLEN_HUB "idi.tlen.pl"			/* hub 			*/
#define TLEN_FALLBACK_SERVER "s1.tlen.pl"	/* fallback server 	*/
#define TLEN_FALLBACK_PORT 443			/* fallback port 	*/

struct xmlnode_s {
	char *name;
	char *data;
	char **atts;

	struct xmlnode_s *parent;
	struct xmlnode_s *children;
	
	struct xmlnode_s *next;
/*	struct xmlnode_s *prev; */
};

typedef struct xmlnode_s xmlnode_t;

enum jabber_bookmark_type_t {			/* see JEP-0048 for details */
	JABBER_BOOKMARK_UNKNOWN = 0,
	JABBER_BOOKMARK_URL,
	JABBER_BOOKMARK_CONFERENCE,
};

typedef struct {
	char *name;
	char *url;
} jabber_bookmark_url_t;

typedef struct {
	char *name;
	char *jid;
	int autojoin;
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

enum jabber_dcc_protocol_type_t {
	JABBER_DCC_PROTOCOL_UNKNOWN	= 0,
	JABBER_DCC_PROTOCOL_BYTESTREAMS,	/* http://www.jabber.org/jeps/jep-0065.html */
	JABBER_DCC_PROTOCOL_IBB, 		/* http://www.jabber.org/jeps/jep-0047.html */
	JABBER_DCC_PROTOCOL_WEBDAV,		/* http://www.jabber.org/jeps/jep-0129.html */ /* DON'T IMPLEMENT IT UNTILL IT WILL BE STARNDARD DRAFT */
};

enum jabber_socks5_step_t {
	SOCKS5_UNKNOWN = 0,
	SOCKS5_CONNECT, 
	SOCKS5_AUTH,
	SOCKS5_DATA,
};

/* <JABBER_DCC_PROTOCOL_BYTESTREAMS> */
struct jabber_streamhost_item {
	char *jid;
	char *ip;
	int port;
};

typedef struct {
	int validate;		/* should be: JABBER_DCC_PROTOCOL_BYTESTREAMS */
	enum jabber_socks5_step_t step;

	struct jabber_streamhost_item *streamhost;
	list_t streamlist;
} jabber_dcc_bytestream_t;

/* </JABBER_DCC_PROTOCOL_BYTESTREAMS> */

typedef struct {
	session_t *session;
	char *req;
	char *sid;
	enum jabber_dcc_protocol_type_t protocol;
	union { /* private data based on protocol */
		jabber_dcc_bytestream_t *bytestream;		/* for JABBER_DCC_PROTOCOL_BYTESTREAMS */
		void *other;			/* XXX */
	} private;
} jabber_dcc_t; 

typedef struct {
	int fd;				/* deskryptor po³±czenia */
	int istlen;			/* czy to tlen? */
#ifdef HAVE_GNUTLS
	gnutls_session ssl_session;	/* sesja ssla */
	gnutls_certificate_credentials xcred;
	char using_ssl;			/* czy polaczono uzywajac ssl */
#endif
	int id;				/* id zapytañ */
	XML_Parser parser;		/* instancja parsera expata */
	char *server;			/* nazwa serwera */
	int port;			/* numer portu */
	int connecting;			/* czy siê w³a¶nie ³±czymy? */
	CHAR_T *resource;		/* resource jakie uzylismy przy laczeniu sie do jabberd */

	list_t bookmarks;		/* for jabber:iq:private <storage xmlns='storage:bookmarks'> */

	watch_t *send_watch;

	xmlnode_t *node;		/* aktualna ga³±¼ xmla */
} jabber_private_t;

typedef struct {
	session_t *session;
	char roster_retrieved;
} jabber_handler_data_t;

#define jabber_private(s) ((jabber_private_t*) session_private_get(s))

plugin_t jabber_plugin;
void jabber_register_commands(void);

char *jabber_attr(char **atts, const char *att);
char *jabber_digest(const char *sid, const CHAR_T *password);
char *jabber_dcc_digest(char *sid, char *initiator, char *target);

void jabber_handle(void *data, xmlnode_t *n);
void jabber_handle_message(xmlnode_t *n, session_t *s, jabber_private_t *j);
void jabber_handle_presence(xmlnode_t *n, session_t *s);
void jabber_handle_iq(xmlnode_t *n, jabber_handler_data_t *jdh);

void jabber_initialize_conversions(char *varname);
CHAR_T *jabber_escape(const char *text);
CHAR_T *jabber_uescape(const CHAR_T *text);
char *jabber_unescape(const char *text);
int jabber_write_status(session_t *s);

void jabber_reconnect_handler(int type, void *data);
WATCHER(jabber_handle_resolver);
WATCHER(jabber_handle_connect_tls);

int jabber_bookmarks_free(jabber_private_t *j);
time_t jabber_try_xdelay(const char *stamp);

#define jabber_write(s, args...) watch_write((s && s->priv) ? jabber_private(s)->send_watch : NULL, args);
#ifdef HAVE_GNUTLS
WATCHER(jabber_handle_write);
#endif

void xmlnode_handle_start(void *data, const char *name, const char **atts);
void xmlnode_handle_end(void *data, const char *name);
void xmlnode_handle_cdata(void *data, const char *text, int len);
xmlnode_t *xmlnode_find_child(xmlnode_t *n, const char *name);

void jabber_handle_disconnect(session_t *s, const char *reason, int type);

#endif /* __EKG_JABBER_JABBER_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
