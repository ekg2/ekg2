/*
 *  (C) Copyright 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001-2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004-2008 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *                     2008 Wiesław Ochmiński <wiechu@wiechu.com>
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

SNAC_SUBHANDLER(icq_snac_location_error) {
	struct {
		uint16_t error;
	} pkt;
	uint16_t error;

	if (ICQ_UNPACK(&buf, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	icq_snac_error_handler(s, "location", error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_location_replyreq) {
	/*
	 * Handle SNAC(0x2,0x3) -- Limitations/params response
	 *
	 */

	debug_function("icq_snac_location_replyreq()\n");

#ifdef ICQ_DEBUG_UNUSED_INFORMATIONS
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;

	tlvs = icq_unpack_tlvs(buf, len, 0);
	for (t = tlvs; t; t = t->next) {
		if (tlv_length_check("icq_snac_location_replyreq()", t, 2))
			continue;
		switch (t->type) {
			case 0x01:
				debug_white("Maximum signature length for this user: %d\n", t->nr);
				break;
			case 0x02:
				debug_white("Number of full UUID capabilities allowed: %d\n", t->nr);
				break;
			case 0x03:
				debug_white("Maximum number of email addresses to look up at once: %d\n", t->nr);
				break;
			case 0x04:
				debug_white("Largest CERT length for end to end encryption: %d\n", t->nr);
				break;
			case 0x05:
				debug_white("Number of short UUID capabilities allowed: %d\n", t->nr);
				break;
			default:
				debug_error("icq_snac_location_replyreq() Unknown type=0x%x\n", t->type);
		}
	}
	icq_tlvs_destroy(&tlvs);
#endif
	return 0;
}

SNAC_SUBHANDLER(icq_user_online_info) {
	/*
	 * Handle SNAC(0x2,0x6) -- User information response
	 *
	 */
	debug_error("icq_user_online_info() XXX\n");

	return -3;
}

SNAC_SUBHANDLER(icq_watcher_notification) {
	/*
	 * Handle SNAC(0x2,0x8) -- Watcher notification
	 *
	 */
	debug_error("icq_watcher_notification() XXX\n");

	return -3;
}

SNAC_SUBHANDLER(icq__update_dir_info_result) {
	/*
	 * Handle SNAC(0x2,0xa) -- Update directory info reply
	 *
	 */
	debug_error("icq__update_dir_info_result() XXX\n");

	return -3;
}

SNAC_HANDLER(icq_snac_location_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_location_error;		break;
		case 0x03: handler = icq_snac_location_replyreq;	break;	/* Miranda: OK */
		case 0x06: handler = icq_user_online_info;		break;
		case 0x08: handler = icq_watcher_notification;		break;
		case 0x0A: handler = icq__update_dir_info_result;	break;
		default:   handler = NULL;			break;
	}

	if (!handler) {
		debug_error("icq_snac_location_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

