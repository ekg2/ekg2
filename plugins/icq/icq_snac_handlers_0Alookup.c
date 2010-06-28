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

#include <ekg/debug.h>

#include "icq.h"
#include "misc.h"
#include "icq_caps.h"
#include "icq_const.h"
#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"


SNAC_SUBHANDLER(icq_snac_lookup_error) {
	/* SNAC(0A,01) SRV_EMAILxSEARCH_ERROR	Client/server error
	 *
	 * This is an error notification snac. You'll receive it when search fails.
	 */
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		pkt.error = 0;
	/* XXX TLV.Type(0x08) - error subcode */

	icq_snac_error_handler(s, "lookup", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_lookup_replyreq) {
	/* SNAC(0A,03) SRV_SEARCHxEMAIL_REPLY	Search response
	 *
	 * This is the server reply to client search by email request. It contain
	 * found screennames associated with requested email. Server replies with
	 * this SNAC to SNAC(0A,02).
	 */
	/* XXX list of tlv(1) */
	return -3;	/* not handled yet */
}

SNAC_HANDLER(icq_snac_lookup_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_lookup_error; break;
		case 0x03: handler = icq_snac_lookup_replyreq; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_lookup_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
