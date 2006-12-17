/* $Id$ */

#ifndef __EKG_JABBER_JABBER_H
#define __EKG_JABBER_JABBER_H

#include <ekg2-config.h>

#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/sessions.h>

#ifdef HAVE_EXPAT_H
 #include <expat.h>
#endif

#include "jabber-ssl.h"

#define DEFAULT_CLIENT_NAME "EKG2 -- http://www.ekg2.org"
#define JABBER_DEFAULT_RESOURCE "ekg2"

/* some tlen constants */
#define TLEN_HUB "idi.tlen.pl"			/* hub 			*/
#define TLEN_FALLBACK_SERVER "s1.tlen.pl"	/* fallback server 	*/
#define TLEN_FALLBACK_PORT 443			/* fallback port 	*/

#define tlenjabber_escape(str)	(j->istlen ? tlen_encode(str) : jabber_escape(str))
#define tlenjabber_unescape(str) (j->istlen ? tlen_decode(str) : jabber_unescape(str))

#define WITH_JABBER_JINGLE 0
#define GMAIL_MAIL_NOTIFY 1

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

enum jabber_compression_method {
	JABBER_COMPRESSION_NONE = 0,
	JABBER_COMPRESSION_ZLIB_INIT,
	JABBER_COMPRESSION_LZW_INIT,
	
	JABBER_COMPRESSION_ZLIB,
	JABBER_COMPRESSION_LZW,
};

	/* name				bit			allow/block:	*/
#define PRIVACY_LIST_MESSAGE		1		/* 	incoming messages */
#define PRIVACY_LIST_IQ			2		/*      incoming iq packets */
#define PRIVACY_LIST_PRESENCE_IN	4		/*      incoming presence packets */
#define PRIVACY_LIST_PRESENCE_OUT	8		/*      outgoint presence packets */
#define PRIVACY_LIST_ALL		(PRIVACY_LIST_MESSAGE | PRIVACY_LIST_IQ | PRIVACY_LIST_PRESENCE_IN | PRIVACY_LIST_PRESENCE_OUT)

typedef struct {
	char *type;						/* jid/group/subscription/ */
	char *value;						/* jid:.../@group/subscription ---- value */
	int allow;						/* 1 - allow 0 - deny */
	int items;						/* lista bitmaski j/w */
	unsigned int order;					/* order */
} jabber_iq_privacy_t;

typedef struct {
	int fd;				/* deskryptor po³±czenia */
	int istlen;			/* czy to tlen? */

	enum jabber_compression_method using_compress;				/* czy kompresujemy polaczenie */
#ifdef JABBER_HAVE_SSL
	char using_ssl;			/* czy polaczono uzywajac ssl */	/* 1 - tak, uzywamy SSL, 2 - tak, uzywamy TLS */
	SSL_SESSION ssl_session;	/* sesja ssla */
#ifdef JABBER_HAVE_GNUTLS
	gnutls_certificate_credentials xcred;
#endif
#endif
	int id;				/* id zapytañ */
	XML_Parser parser;		/* instancja parsera expata */
	char *server;			/* nazwa serwera */
	int port;			/* numer portu */
	int connecting;			/* czy siê w³a¶nie ³±czymy? */		/* 1 - normalne laczenie, 2 - laczenie po SASLu */
	char *resource;		/* resource jakie uzylismy przy laczeniu sie do jabberd */
#ifdef GMAIL_MAIL_NOTIFY
	char *last_gmail_result_time; 	/* last time we've checked mail, this seems not to work correctly ;/ */
	char *last_gmail_tid;
#endif
	list_t privacy;			/* for jabber:iq:privacy */
	list_t bookmarks;		/* for jabber:iq:private <storage xmlns='storage:bookmarks'> */

	watch_t *send_watch;

	xmlnode_t *node;		/* aktualna ga³±¼ xmla */
} jabber_private_t;

typedef struct {
	session_t *session;
	char roster_retrieved;
} jabber_handler_data_t;
#define jabber_private(s) ((jabber_private_t*) session_private_get(s))

extern plugin_t jabber_plugin;
extern char *jabber_default_search_server;
extern int config_jabber_beep_mail;

void jabber_register_commands(void);
XML_Parser jabber_parser_recreate(XML_Parser parser, void *data);

int JABBER_COMMIT_DATA(watch_t *w);
void jabber_handle(void *data, xmlnode_t *n);

/* digest.c hashowanie.. */
char *jabber_digest(const char *sid, const char *password);
char *jabber_dcc_digest(char *sid, char *initiator, char *target);
char *jabber_challange_digest(const char *sid, const char *password, const char *nonce, const char *cnonce, const char *xmpp_temp, const char *realm);

char *jabber_attr(char **atts, const char *att);
char *jabber_escape(const char *text);
char *jabber_unescape(const char *text);
char *tlen_encode(const char *what);
char *tlen_decode(const char *what);
int jabber_write_status(session_t *s);

void jabber_reconnect_handler(int type, void *data);
WATCHER(jabber_handle_resolver);

int jabber_privacy_add_compare(void *data1, void *data2);
int jabber_privacy_free(jabber_private_t *j);
int jabber_bookmarks_free(jabber_private_t *j);

#define jabber_write(s, args...) watch_write((s && s->priv) ? jabber_private(s)->send_watch : NULL, args);
WATCHER_LINE(jabber_handle_write);

void xmlnode_handle_start(void *data, const char *name, const char **atts);
void xmlnode_handle_end(void *data, const char *name);
void xmlnode_handle_cdata(void *data, const char *text, int len);
xmlnode_t *xmlnode_find_child(xmlnode_t *n, const char *name);

void jabber_handle_disconnect(session_t *s, const char *reason, int type);

#ifdef HAVE_ZLIB
char *jabber_zlib_decompress(const char *buf, int *len);
char *jabber_zlib_compress(const char *buf, int *len);
#endif

#endif /* __EKG_JABBER_JABBER_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
