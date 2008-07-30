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

SNAC_SUBHANDLER(icq_snac_message_error) {
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		return -1;

	debug_error("icq_snac_message_error() XXX\n");

	icq_snac_error_handler(s, "message", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_replyicbm) {
#warning "Miranda: icq_snac_message_replyicbm() -DDBG_CAPHTML -DDBG_CAPMTN"

	string_t pkt;
	uint32_t flags;

	/* Set message parameters for all channels (imitate ICQ 6) */
	flags = 0x00000303;
#ifdef DBG_CAPHTML
	flags |= 0x00000400;
#endif
#ifdef DBG_CAPMTN
	flags |= 0x00000008;
#endif
	/* SnacCliSeticbm() */
	pkt = icq_pack("WIWWWWW",
		(uint32_t) 0x0000, (uint32_t) flags,		/* channel, flags */
		(uint16_t) 8000, (uint32_t) 999,		/* max-message-snac-size, max-sender-warning-level */
		(uint32_t) 999, (uint32_t) 0,			/* max-rcv-warning-level, minimum message-interval-in-secons */
		(uint32_t) 0);					/* unknown */
	icq_makesnac(s, pkt, 0x04, 0x02, 0, 0);
	icq_send_pkt(s, pkt);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_recv) {
	if (len < 11) {
		debug_error("SNAC_04_07() Malformed message thru server\n");
		return -1;
	}

	debug_error("icq_snac_message_recv() XXX\n");
	return -3;
}

SNAC_HANDLER(icq_snac_message_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_message_error; break;
		case 0x05: handler = icq_snac_message_replyicbm; break;		/* Miranda: OK */
		case 0x07: handler = icq_snac_message_recv; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_message_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

