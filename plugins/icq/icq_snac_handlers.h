#ifndef __ICQ_SNAC_H
#define __ICQ_SNAC_H

#include <stdint.h>

#include <ekg/sessions.h>
#include <ekg/stuff.h>

#include "icq.h"

typedef struct {	/* flap_packet_t->data ** if flap_packet_t->cmd == 0x02 */
	uint16_t family;
	uint16_t cmd;
	uint16_t flags;
	uint32_t ref;
	unsigned char *data;
} snac_packet_t;
#define SNAC_PACKET_LEN 10

void icq_makesnac(session_t *s, string_t pkt, uint16_t fam, uint16_t cmd, icq_snac_reference_list_t *data, snac_subhandler_t subhandler);
void icq_makemetasnac(session_t *s, string_t pkt, uint16_t sub, uint16_t type, icq_snac_reference_list_t *data, snac_subhandler_t subhandler);

int icq_snac_handler(session_t *s, uint16_t family, uint16_t cmd, unsigned char *buf, int len, uint16_t flags, uint32_t ref_no);
void icq_snac_error_handler(session_t *s, const char *from, uint16_t error);

void icq_snac_references_list_destroy(icq_snac_reference_list_t **lista);
TIMER_SESSION(icq_snac_ref_list_cleanup);

SNAC_SUBHANDLER(icq_my_meta_information_response);

#endif
