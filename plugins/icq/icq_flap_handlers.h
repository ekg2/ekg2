#ifndef __ICQ_FLAP_H
#define __ICQ_FLAP_H

#include <stdint.h>

#include <ekg/sessions.h>
#include <ekg/dynstuff.h>

void icq_makeflap(session_t *s, string_t pkt, uint8_t cmd);
int icq_flap_handler(session_t *s, int fd, char *b, int len);

typedef struct {
	uint8_t unique;		/* 0x2A */
	uint8_t cmd;
	uint16_t id;
	uint16_t len;
	unsigned char *data;
} flap_packet_t;
#define FLAP_PACKET_LEN 6

#endif
