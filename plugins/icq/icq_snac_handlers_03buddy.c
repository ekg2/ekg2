/*
 *  (C) Copyright 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001-2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004-2008 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

SNAC_SUBHANDLER(icq_snac_buddy_online) {
	debug_error("icq_snac_buddy_online() XXX\n");
	icq_hexdump(DEBUG_ERROR, buf, len);
	return -3;
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

SNAC_HANDLER(icq_snac_buddy_notify_rejected) {
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

