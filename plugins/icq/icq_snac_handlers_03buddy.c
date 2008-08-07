/*
 *  (C) Copyright 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001-2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002-2004 Martin Ã–berg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004-2008 Joe Kucera
 *
 * ekg2 port:
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

SNAC_SUBHANDLER(icq_snac_buddy_error) {
	struct {
		uint16_t error;
	} pkt;
	uint16_t error;

	if (ICQ_UNPACK(&buf, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	icq_snac_error_handler(s, "buddy", error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_reply) {
	struct icq_tlv_list *tlvs;

	if ((tlvs = icq_unpack_tlvs(buf, len, 0))) {
		icq_tlv_t *t_max_uins = icq_tlv_get(tlvs, 1);
		icq_tlv_t *t_max_watchers = icq_tlv_get(tlvs, 2);
		uint16_t max_uins = 0, max_watchers = 0;

		icq_unpack_tlv_word(t_max_uins, max_uins);
		icq_unpack_tlv_word(t_max_watchers, max_watchers);

		debug_white("icq_snac_buddy_03() maxUins = %u maxWatchers = %u\n", max_uins, max_watchers);

		icq_tlvs_destroy(&tlvs);
	} else
		debug_error("icq_snac_buddy_03() tlvs == NULL\n");
	return 0;
}

static void icq_get_description(session_t *s, char *uin) {
	string_t pkt, tlv5, rdv;
	uint32_t cookie1=rand(), cookie2=rand();

	debug_function("icq_get_description() for: %s\n", uin);

	pkt = string_init(NULL);
	icq_pack_append(pkt, "II", cookie1, cookie2);		// cookie
	icq_pack_append(pkt, "W", (uint32_t) 2);		// message type
	icq_pack_append(pkt, "u", atoi(uin));

	tlv5 = string_init(NULL);
	icq_pack_append(tlv5, "W", (uint32_t) 0);
	icq_pack_append(tlv5, "II", cookie1, cookie2);		// cookie
	icq_pack_append(tlv5, "P", (uint32_t) 0x1349);		// AIM_CAPS_ICQSERVERRELAY
	icq_pack_append(tlv5, "tW", icq_pack_tlv_word(0xA, 1));	// TLV 0x0A: acktype (1 = normal message)
	icq_pack_append(tlv5, "T", icq_pack_tlv(0x0F, NULL, 0));	// TLV 0x0F: unknown

	// RendezvousMessageData
	rdv = string_init(NULL);
	icq_pack_append(rdv, "wwiiiiwicwwwiiiccwwwcii",
				(uint32_t) 27,		// length of this data segment, always 27
				(uint32_t) 8,		// protocol version
				(uint32_t) 0, (uint32_t) 0, (uint32_t) 0, (uint32_t) 0, // pluginID
				(uint32_t) 0,		// unknown
				(uint32_t) 3,		// unknown
				(uint32_t) 0, 		// unknown
				(uint32_t) 0x7fff,	// channel 2 counter XXX
				(uint32_t) 14,		// length of this data segment, always 14
				(uint32_t) 0x7fff,	// channel 2 counter XXX
				(uint32_t) 0, (uint32_t) 0, (uint32_t) 0, // unknown, usually all zeros
				(uint32_t) 0xe8,	// msg type   XXX ???
				(uint32_t) 3,		// msg flags  XXX ???
				(uint32_t) 1,		// status code ??? XXX
				(uint32_t) 1,		// priority ??? XXX
				(uint32_t) 1,		// 
				(uint32_t) 0,		//
				(uint32_t) 0,		// foreground
				(uint32_t) 0x00ffffff); // foreground

	icq_pack_append(tlv5, "T", icq_pack_tlv(0x2711, rdv->str, rdv->len));
	string_free(rdv, 1);

	icq_pack_append(pkt, "T", icq_pack_tlv(0x05, tlv5->str, tlv5->len));
	string_free(tlv5, 1);

	icq_pack_append(pkt, "T", icq_pack_tlv(0x03, NULL, 0));		// empty TLV 3 to get an ack from the server

	icq_makesnac(s, pkt, 0x04, 0x06, 0, 0);
	icq_send_pkt(s, pkt);
}

SNAC_SUBHANDLER(icq_snac_buddy_online) {
	/*
	 * Handle SNAC(0x3,0xb) -- User online notification
	 *
	 * Server sends this snac when user from your contact list goes online.
	 * Also you'll receive this snac on user status change.
	 */

	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;
	char *uid, *tmp;
	uint16_t warning, count, status, status2;

	if (!icq_unpack(buf, &buf, &len, "uWW", &tmp, &warning, &count))
		return -1;

	uid = saprintf("icq:%s", tmp);

	tlvs = icq_unpack_tlvs(buf, len, count);

	for (t = tlvs; t; t = t->next) {

		/* darkjames said: "I don't trust anything. Wiechu, check t->len" */
		switch (t->type) {
			case 0x01:
				if (tlv_length_check("icq_snac_buddy_online()", t, 2))
					continue;
				break;
			case 0x03:
			case 0x05:
			case 0x06:
			case 0x0a:
			case 0x0f:
				if (tlv_length_check("icq_snac_buddy_online()", t, 4))
					continue;
				break;
		}
		/* now we've got trusted length */

		switch (t->type) {
			case 0x06:
				/* User status
				 *
				 * ICQ service presence notifications use user status field which consist
				 * of two parts. First is a various flags (birthday flag, webaware flag,
				 * etc). Second is a user status (online, away, busy, etc) flags.
				 */
				status  = t->nr & 0xffff;
				status2 = t->nr >> 16;
				debug_white("icq_snac_buddy_online()  %s status2=0x%04x status=0x%04x\n", uid, status2, status);
				protocol_status_emit(s, uid, icq2ekg_status(status), NULL, time(NULL));
				icq_get_description(s, uid+4);
				break;

			case 0x0a: /* IP address */
				/* XXX (?wo?) add to private */
				debug_white("icq_snac_buddy_online()  %s IP=%d.%d.%d.%d\n", uid, t->buf[0], t->buf[1], t->buf[2], t->buf[3]);
				break;

			case 0x01: /* User class */
				debug_white("icq_snac_buddy_online()  %s class 0x%02x\n", uid, t->nr);
				break;
			case 0x03: /* Time when client gone online (unix time_t) */
				debug_white("icq_snac_buddy_online()  %s online since %d\n", uid, t->nr);
				break;
			case 0x05: /* Time when this account was registered (unix time_t) */
				debug_white("icq_snac_buddy_online()  %s is ICQ Member since %d\n", uid, t->nr);
				break;
			case 0x0f: /* Online time in seconds */
				debug_white("icq_snac_buddy_online()  %s is %d seconds online\n", uid, t->nr);
				break;

			case 0x0c: /* DC info */
			case 0x0d: /* Client capabilities list */
			case 0x1d: /* user icon id & hash */
				debug_white("icq_snac_buddy_online() Not supported type=0x%02x\n", t->type);
				break;

			default:
				debug_error("icq_snac_buddy_online() Unknown type=0x%02x\n", t->type);
		}

	}

	icq_tlvs_destroy(&tlvs);
	xfree(uid);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_offline) {
	debug_white("icq_snac_buddy_offline() untested XXX\n");
	icq_hexdump(DEBUG_WHITE, buf, len);

	do {
		char *cont = NULL;
		char *uid;
		uint16_t discard, t_count;

		if (!ICQ_UNPACK(&buf, "uWW", &cont, &discard, &t_count))
			return -1;
		
		while (t_count) {
			uint16_t t_type, t_len;

			if (!ICQ_UNPACK(&buf, "WW", &t_type, &t_len))
				return -1;

			if (len < t_len)
				return -1;

			buf += t_len;
			len -= t_len;
			t_count--;
		}

		uid = icq_uid(cont);
		protocol_status_emit(s, uid, EKG_STATUS_NA, NULL, time(NULL));
		xfree(uid);
	} while (len >= 1);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_notify_rejected) {
	char *uid;

	if (!ICQ_UNPACK(&buf, "u", &uid))
		return -1;

	debug_function("icq_snac_buddy_notify_rejected() for: %s\n", uid);
	return 0;
}

SNAC_HANDLER(icq_snac_buddy_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_buddy_error;	break;	/* Miranda: OK */
		case 0x03: handler = icq_snac_buddy_reply;	break;	/* Miranda: OK */
		case 0x0a: handler = icq_snac_buddy_notify_rejected;	/* Miranda: OK */
								break;	/*        .... */
		case 0x0b: handler = icq_snac_buddy_online;	break;	/* Miranda: handleUserOnline() */
		case 0x0c: handler = icq_snac_buddy_offline;	break;	/* Miranda: OK, untested */
		default:   handler = NULL;		break;
	}

	if (!handler) {
		debug_error("icq_snac_buddy_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

