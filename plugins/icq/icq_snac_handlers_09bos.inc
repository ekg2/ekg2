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

SNAC_SUBHANDLER(icq_snac_bos_error) {
	struct {
		uint16_t error;
	} pkt;
	uint16_t error;

	if (ICQ_UNPACK(&buf, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	icq_snac_error_handler(s, "bos", error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_bos_replyreq) {	/* [reply to CLI_REQBOS] */
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t_max_visible_contacts, *t_max_invisible_contacts, *t_max_temp_visible_contacts;
	uint16_t max_visible_contacts, max_invisible_contacts, max_temp_visible_contacts;

	if (len < 12) {
		// Failure
		debug_error("icq_snac_bos_replyreq() Malformed\n");
		return 0;
	}

	if (!(tlvs = icq_unpack_tlvs(&buf, &len, 0)))
		return 0;

	t_max_visible_contacts = icq_tlv_get(tlvs, 1);
	t_max_invisible_contacts = icq_tlv_get(tlvs, 2);
	t_max_temp_visible_contacts = icq_tlv_get(tlvs, 3);

	icq_unpack_tlv_word(t_max_visible_contacts, max_visible_contacts);
	icq_unpack_tlv_word(t_max_invisible_contacts, max_invisible_contacts);
	icq_unpack_tlv_word(t_max_temp_visible_contacts, max_temp_visible_contacts);

	debug_white("icq_snac_bos_replyreq() Max visible %u, max invisible %u, max temporary visible %u items.\n", 
			max_visible_contacts, max_invisible_contacts, max_temp_visible_contacts);

	icq_tlvs_destroy(&tlvs);
	return 0;
}

SNAC_HANDLER(icq_snac_bos_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_bos_error; break;		/* Miranda: OK */
		case 0x03: handler = icq_snac_bos_replyreq; break;	/* Miranda: OK */
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_bos_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

// vim:syn=c
