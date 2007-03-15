#ifndef __JABBER_DCC_H
#define __JABBER_DCC_H

#define JABBER_DEFAULT_DCC_PORT 6000	/* XXX */

#include <ekg/plugins.h>

#include <stdio.h>

#include <ekg/dynstuff.h>
#include <ekg/protocol.h>

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
	FILE *fd;
	int sfd;
	session_t *session;

	char *req;
	char *sid;
	enum jabber_dcc_protocol_type_t protocol;
	union { /* private data based on protocol */
		jabber_dcc_bytestream_t *bytestream;		/* for JABBER_DCC_PROTOCOL_BYTESTREAMS */
		void *other;			/* XXX */
	} private;
} jabber_dcc_t; 


dcc_t *jabber_dcc_find(const char *uin, const char *id, const char *sid);
void jabber_dcc_close_handler(struct dcc_s *d);

WATCHER(jabber_dcc_handle_recv);

QUERY(jabber_dcc_postinit);
extern int jabber_dcc;
extern int jabber_dcc_port;
extern char *jabber_dcc_ip;
extern int jabber_dcc;

#endif
