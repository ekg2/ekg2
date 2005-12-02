/* $Id$ */

#ifndef __EKG_JABBER_JABBER_H
#define __EKG_JABBER_JABBER_H

#include <ekg2-config.h>

#include <ekg/plugins.h>
#include <ekg/sessions.h>

#include <expat.h>

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#define DEFAULT_CLIENT_NAME "EKG2 -- http://www.ekg2.org"
#define JABBER_DEFAULT_RESOURCE "ekg2"

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

typedef struct {
	int fd;				/* deskryptor po³±czenia */
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
	char *stream_id;		/* id strumienia */

	char *obuf;			/* bufor wyj¶ciowy */
	int obuf_len;			/* rozmiar bufora wyj¶ciowego */

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
char *jabber_digest(const char *sid, const char *password);

void jabber_handle(void *data, xmlnode_t *n);
void jabber_handle_message(xmlnode_t *n, session_t *s, jabber_private_t *j);
void jabber_handle_presence(xmlnode_t *n, session_t *s);
void jabber_handle_iq(xmlnode_t *n, jabber_handler_data_t *jdh);

extern char *config_jabber_console_charset;
void jabber_initialize_conversions(char *varname);
char *jabber_escape(const char *text);
char *jabber_unescape(const char *text);
int jabber_write_status(session_t *s);

void jabber_reconnect_handler(int type, void *data);
WATCHER(jabber_handle_resolver);
WATCHER(jabber_handle_connect_tls);

time_t jabber_try_xdelay(xmlnode_t *xmlnode, const char *ns);

#ifdef __GNU__
int jabber_write(jabber_private_t *j, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
#else
int jabber_write(jabber_private_t *j, const char *format, ...);
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
