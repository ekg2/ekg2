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
 * Protocol description with author's permission from: http://iserverd.khstu.ru/oscar/
 *  (C) Copyright 2000-2005 Alexander V. Shutko <AVShutko@mail.khstu.ru>
 *
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
	/* SNAC(09,01) SRV_PRIVACY_ERROR	Client/server error
	 *
	 * This is an error notification snac.
	 */
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		pkt.error = 0;
	/* XXX TLV.Type(0x08) - error subcode */

	icq_snac_error_handler(s, "bos", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_bos_replyreq) {
	/* SNAC(09,03) SRV_PRIVACY_RIGHTS_REPLY	Requested service parameters
	 *
	 * Server replies with this SNAC to SNAC(09,02) - client service parameters request.
	 */

	if (len < 12) {
		// Failure
		debug_error("icq_snac_bos_replyreq() Malformed\n");
		return 0;
	}

#if ICQ_DEBUG_UNUSED_INFORMATIONS
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t_max_visible_contacts, *t_max_invisible_contacts, *t_max_temp_visible_contacts;
	uint16_t max_visible_contacts, max_invisible_contacts, max_temp_visible_contacts;

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
#endif
	return 0;
}

SNAC_SUBHANDLER(icq_snac_bos_service_error) {
	/* SNAC(09,09) SRV_BOS_ERROR	Service error
	 *
	 * This is an error notification snac. Known error codes: 0x01 - wrong mode
	 */
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		pkt.error = 0;
	/* XXX TLV.Type(0x08) - error subcode */
	/* XXX TLV.Type(0x04) - error description url */

	icq_snac_error_handler(s, "bos", pkt.error);
	return 0;
}

SNAC_HANDLER(icq_snac_bos_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_bos_error; break;
		case 0x03: handler = icq_snac_bos_replyreq; break;
		case 0x09: handler = icq_snac_bos_service_error; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_bos_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
