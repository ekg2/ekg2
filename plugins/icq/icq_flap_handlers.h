#ifndef __ICQ_FLAP_H
#define __ICQ_FLAP_H

#include <glib.h>

#include <ekg/sessions.h>
#include <ekg/dynstuff.h>

void icq_makeflap(session_t *s, string_t pkt, guint8 cmd);
int icq_flap_handler(session_t *s, string_t buffer);
int icq_flap_close_helper(session_t *s, unsigned char *buf, int len);

typedef struct {
	guint8 unique;		/* 0x2A */
	guint8 cmd;
	guint16 id;
	guint16 len;
	unsigned char *data;
} flap_packet_t;
#define FLAP_PACKET_LEN 6

#endif
