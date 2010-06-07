/*
 *  (C) Copyright 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001,2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002,2003,2004 Martin Öberg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004,2005,2006 Joe Kucera
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

SNAC_SUBHANDLER(icq_snac_status_error) {
	/* SNAC(0B,01) SRV_STATS_ERROR	Client / server error
	 *
	 * This is an error notification snac.
	 */
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		pkt.error = 0;
	/* XXX  	TLV.Type(0x08) - error subcode? */

	icq_snac_error_handler(s, "status", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_status_minreport) {
	/* SNAC(0B,02) SRV_SET_MINxREPORTxINTERVAL	Set minimum report interval
	 *
	 * Server send this to client after login. This snac contain minimum stats report
	 * interval value. Client should send stats report every 1200 hours (default value).
	 */
	struct {
		uint16_t interval;	// min interval between stats reports (hours)
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.interval))
		return -1;

#if ICQ_DEBUG_UNUSED_INFORMATIONS
	debug_white("icq_snac_status_minreport() minimum interval between stats reports: %u [hours]\n", pkt.interval);
#endif
	return 0;
}

SNAC_SUBHANDLER(icq_snac_status_report_ack) {
	/* SNAC(0B,04) SRV_REPORT_ACK	Usage stats report ack
	 *
	 * Server send this as an ack for client stats report. Currently used only
	 * by AIM service. ICQ clients never send stat reports.
	 * Empty snac (no snac data)
	 */
	return 0;
}

SNAC_HANDLER(icq_snac_status_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_status_error; break;
		case 0x02: handler = icq_snac_status_minreport; break;
		case 0x04: handler = icq_snac_status_report_ack; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_status_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
