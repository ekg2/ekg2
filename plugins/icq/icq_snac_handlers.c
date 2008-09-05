/*
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *                     2008 Wies³aw Ochmiñski <wiechu@wiechu.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ekg/debug.h>
#include <ekg/dynstuff_inline.h>
#include <ekg/protocol.h>
#include <ekg/queries.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

#include "icq.h"
#include "misc.h"

#include "icq_caps.h"
#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"


static LIST_FREE_ITEM(icq_snac_references_list_free, icq_snac_reference_list_t *) {
	if (data && data->list)
		private_items_destroy(&data->list);
}

static         __DYNSTUFF_ADD(icq_snac_references_list, icq_snac_reference_list_t, __not_used)
static __DYNSTUFF_REMOVE_SAFE(icq_snac_references_list, icq_snac_reference_list_t, icq_snac_references_list_free)
static __DYNSTUFF_REMOVE_ITER(icq_snac_references_list, icq_snac_reference_list_t, icq_snac_references_list_free)
           __DYNSTUFF_DESTROY(icq_snac_references_list, icq_snac_reference_list_t, icq_snac_references_list_free)

static void icq_snac_ref_add(session_t *s, icq_snac_reference_list_t *elem) {
	icq_private_t *j;
	if (!s || !s->priv)
		return;
	j = (icq_private_t *) s->priv;
	icq_snac_references_list_add(&(j->snac_ref_list), elem);
}

static icq_snac_reference_list_t *icq_snac_ref_find(session_t *s, uint32_t ref) {
	icq_private_t *j;
	icq_snac_reference_list_t *l;

	if (!s || !(j = s->priv) || (!j->snac_ref_list) || (ref > 0xffff))
		return NULL;

	for (l = j->snac_ref_list; l; l = l->next) {
		if (l->ref == ref)
			return l;
	}
	return NULL;
}

static void icq_snac_ref_remove(session_t *s, icq_snac_reference_list_t *elem) {
	icq_private_t *j;
	if (!s || !(j = (icq_private_t *) s->priv))
		return;

	icq_snac_references_list_remove(&(j->snac_ref_list), elem);
}

TIMER_SESSION(icq_snac_ref_list_cleanup) {
	icq_private_t *j;
	icq_snac_reference_list_t *l;
	time_t t = time(NULL) - 100; /* XXX add to session configuration? */

	if (!s || !(j = (icq_private_t *) s->priv))
		return 0;

	/* XXX ?wo? inform about removed refs??? */
	for (l = j->snac_ref_list; l ; l = l->next) {
		if (t > l->timestamp)
			l = icq_snac_references_list_removei(&j->snac_ref_list, l);
	}
	return 0;
}


static inline char *_icq_makesnac(uint8_t family, uint16_t cmd, uint16_t flags, uint32_t ref) {
	static char buf[SNAC_PACKET_LEN];
	string_t tempstr;

	tempstr = icq_pack("WWWI", (uint32_t) family, (uint32_t) cmd, (uint32_t) flags, (uint32_t) ref);
	if (tempstr->len != SNAC_PACKET_LEN) {
		debug_error("_icq_makesnac() critical error\n");
		return NULL;
	}
	memcpy(buf, tempstr->str, SNAC_PACKET_LEN);
	string_free(tempstr, 1);
	return buf;
}

void icq_makesnac(session_t *s, string_t pkt, uint16_t fam, uint16_t cmd, private_data_t *data, snac_subhandler_t subhandler) {
	icq_private_t *j;
	icq_snac_reference_list_t *snac_data = NULL;

	if (!s || !(j = s->priv) || !pkt)
		return;

	if (data || subhandler) {
		snac_data = xmalloc(sizeof(icq_snac_reference_list_t));
		snac_data->ref = j->snac_seq;
		snac_data->timestamp = time(NULL);
		snac_data->subhandler = subhandler;
		snac_data->list = data;

		icq_snac_ref_add(s, snac_data);
	}

	string_insert_n(pkt, 0, _icq_makesnac(fam, cmd, 0x0000, j->snac_seq), SNAC_PACKET_LEN);

	debug_function("icq_makesnac() 0x%x 0x0%x 0x%x\n", fam, cmd, j->snac_seq);
#if ICQ_SNAC_NAMES_DEBUG
	{
	const char *tmp = icq_snac_name(fam, cmd);
	if (tmp)
		debug_white("icq_makesnac() //	SNAC(0x%x, 0x%x) -- %s\n", fam, cmd, tmp);
	}
#endif
	icq_makeflap(s, pkt, 0x02);

	j->snac_seq++;
}

void icq_makemetasnac(session_t *s, string_t pkt, uint16_t sub, uint16_t type, private_data_t *data, snac_subhandler_t subhandler) {
	icq_private_t *j;
	string_t newbuf;

	if (!s || !(j = s->priv) || !pkt) 
		return;

/* XXX */
	if (j->snacmeta_seq)
		j->snacmeta_seq = (j->snacmeta_seq + 1) % 0x7fff;
	else
		j->snacmeta_seq = 2;


	newbuf = icq_pack("t", (uint32_t) 0x01, (uint32_t) pkt->len + (type ? 0x0C : 0x0A));
	icq_pack_append(newbuf, "wiww",
				(uint32_t) pkt->len + (type ? 0x0A : 0x08),
				(uint32_t) atoi(s->uid+4),
				(uint32_t) sub,
				(uint32_t) j->snacmeta_seq);
	if (type)
		icq_pack_append(newbuf, "w", (uint32_t) type);
	
	string_insert_n(pkt, 0, newbuf->str, newbuf->len);
	string_free(newbuf, 1);

	debug_function("icq_makemetasnac() 0x%x 0x0%x\n", sub, type);
	icq_makesnac(s, pkt, 0x15, 2, data, subhandler);
}

/* stolen from Miranda ICQ plugin CIcqProto::LogFamilyError() chan_02data.cpp under GPL-2 or later */
void icq_snac_error_handler(session_t *s, const char *from, uint16_t error) {
	const char *msg;

	switch (error) {
		case 0x01: msg = "Invalid SNAC header"; break;
		case 0x02: msg = "Server rate limit exceeded"; break;
		case 0x03: msg = "Client rate limit exceeded"; break;
		case 0x04: msg = "Recipient is not logged in"; break;
		case 0x05: msg = "Requested service unavailable"; break;
		case 0x06: msg = "Requested service not defined"; break;
		case 0x07: msg = "You sent obsolete SNAC"; break;
		case 0x08: msg = "Not supported by server"; break;
		case 0x09: msg = "Not supported by client"; break;
		case 0x0A: msg = "Refused by client"; break;
		case 0x0B: msg = "Reply too big"; break;
		case 0x0C: msg = "Responses lost"; break;
		case 0x0D: msg = "Request denied"; break;
		case 0x0E: msg = "Incorrect SNAC format"; break;
		case 0x0F: msg = "Insufficient rights"; break;
		case 0x10: msg = "In local permit/deny (recipient blocked)"; break;
		case 0x11: msg = "Sender is too evil"; break;
		case 0x12: msg = "Receiver is too evil"; break;
		case 0x13: msg = "User temporarily unavailable"; break;
		case 0x14: msg = "No match"; break;
		case 0x15: msg = "List overflow"; break;
		case 0x16: msg = "Request ambiguous"; break;
		case 0x17: msg = "Server queue full"; break;
		case 0x18: msg = "Not while on AOL"; break;
		case 0x19: msg = "Query failed"; break;
		case 0x1A: msg = "Timeout"; break;
		case 0x1C: msg = "General failure"; break;
		case 0x1D: msg = "Progress"; break;
		case 0x1E: msg = "In free area"; break;
		case 0x1F: msg = "Restricted by parental controls"; break;
		case 0x20: msg = "Remote restricted by parental controls"; break;
		default:   msg = ""; break;
	}

	debug_error("icq_snac_error_handler(%s) %s: %s (%.4x)\n", s->uid, from, msg, error);
}


#include "icq_snac_handlers_01service.inc"
#include "icq_snac_handlers_02location.inc"
#include "icq_snac_handlers_03buddy.inc"
#include "icq_snac_handlers_04message.inc"
#include "icq_snac_handlers_09bos.inc"
#include "icq_snac_handlers_0Alookup.inc"
#include "icq_snac_handlers_0Bstatus.inc"
#include "icq_snac_handlers_13userlist.inc"
#include "icq_snac_handlers_15extension.inc"
#include "icq_snac_handlers_17sigon.inc"

int icq_snac_handler(session_t *s, uint16_t family, uint16_t cmd, unsigned char *buf, int len, uint16_t flags, uint32_t ref_no) {
	snac_handler_t handler;
	icq_snac_reference_list_t *ref_data = icq_snac_ref_find(s, ref_no);;
	private_data_t *h_data = ref_data ? ref_data->list : NULL;

	debug_white("icq_snac_handler() family=%.4x cmd=%.4x (len=%d)\n", family, cmd, len);

	/* XXX, queue */
//	debug_error("icq_flap_data() XXX\n");

	if (ref_data && ref_data->subhandler) {
		ref_data->subhandler(s, buf, len, h_data);
		if (!(flags & 0x0001))
			icq_snac_ref_remove(s, ref_data);

		return 0;
	}

	switch (family) {
		case 0x01: handler = icq_snac_service_handler;	break;
		case 0x02: handler = icq_snac_location_handler;	break;
		case 0x03: handler = icq_snac_buddy_handler;	break;
		case 0x04: handler = icq_snac_message_handler;	break;
		case 0x09: handler = icq_snac_bos_handler;	break;
		case 0x0a: handler = icq_snac_lookup_handler;	break;
		case 0x0b: handler = icq_snac_status_handler;	break;
		case 0x13: handler = icq_snac_userlist_handler;	break;
		case 0x15: handler = icq_snac_extension_handler;break;
		case 0x17: handler = icq_snac_sigon_handler;	break;
		default:   handler = NULL;			break;
	}

	if (!handler) {
		debug_error("snac_handler() SNAC with unknown family: %.4x cmd: %.4x received.\n", family, cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
		return 0;
	}

	handler(s, cmd, buf, len, h_data);

	return 0;
}
