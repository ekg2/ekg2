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

SNAC_SUBHANDLER(icq_snac_sigon_error) {
	/* SNAC(17,01) SRV_REGISTRATION_ERROR	Server error (registration refused)
	 *
	 * Server replies with this SNAC to SNAC(17,04) - client registration
	 * request. This snac mean that registration failed for some reason.
	 */
	// XXX we don't support registration yet
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		pkt.error = 0;

	icq_snac_error_handler(s, "sigon", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_sigon_reply) {
	/* SNAC(17,03) SRV_LOGIN_REPLY Server login reply / error reply
	 *
	 * This is the server reply for for SNAC(17,02) and SNAC(17,06) client requests.
	 * It may contain error information or BOS address/cookie.
	 *
	 * XXX There are two modifications of this snac. First (error notification)
	 * contain TLV(0x01), TLV(0x08) and TLV(0x04). The second contain
	 * TLV(0x01), TLV(0x05), TLV(0x06), TLV(0x11), latest client info
	 * TLV(0x40)-TLV(0x48) and TLV(0x54).
	 */
	debug_function("icq_snac_sigon_reply()\n");

	// XXX ?wo? Is icq_flap_close_helper() adequate to handle it?
	return icq_flap_close_helper(s, buf, len);
}

SNAC_SUBHANDLER(icq_snac_new_uin) {
	/* SNAC(17,05) SRV_NEW_UIN	New uin response
	 *
	 * This is the server reply for SNAC(17,04). It contain new uin. This snac
	 * mean that registration finished succesfully. Server also can reply with
	 * SNAC(17,01) if it can't create new user account.
	 */
	// XXX we don't support registration yet
	return 0;
}

extern char *icq_md5_digest(const char *password, const unsigned char *key, int key_len);	/* digest.c */

SNAC_SUBHANDLER(icq_snac_sigon_authkey) {
	/* SNAC(17,07) SRV_AUTH_KEY_RESPONSE	Server md5 authkey response
	 *
	 * This is the second snac in md5 crypted login sequence. Server send this for SNAC(17,06) request.
	 * This snac contain server generated auth key. Client should use it to crypt password.
	 */

	struct {
		uint16_t key_len;
	} pkt;
	string_t str;

	char *digest;

	if (!ICQ_UNPACK(&buf, "W", &pkt.key_len)) {
		icq_handle_disconnect(s, "Secure login failed. Invalid server response.", 0);		/* XXX */
		return -1;
	}

	if (!pkt.key_len || len < pkt.key_len) {
		icq_handle_disconnect(s, "Secure login failed. Invalid key length.", 0);		/* XXX */
		return -1;
	}

	/* SNAC(17,02) CLI_MD5_LOGIN	Client login request (md5 login sequence)
	 *
	 * This is the second snac in md5 crypted login sequence.
	 * You'll need password, authkey from SNAC(17,07) and RFC 1321 md5 routines.
	 * Server should reply via SNAC(17,03)
	 */

	/* XXX, miranda limit key to 64B */

	digest = icq_md5_digest(session_password_get(s), buf, pkt.key_len);

	str = string_init(NULL);

	icq_pack_append(str, "T", icq_pack_tlv_str(1, s->uid + 4));	// TLV(0x01) - uin
	icq_pack_append(str, "T", icq_pack_tlv(0x25, digest, 16));	// TLV(0x25) - password md5 hash
	icq_pack_append(str, "T", icq_pack_tlv(0x4C, NULL, 0));		// empty TLV(0x4C): unknown	XXX ?wo? ssi flag?

	icq_pack_append_client_identification(str);			// Pack client identification details.

	icq_makesnac(s, str, 0x17, 2, 0, 0);
	icq_send_pkt(s, str);						// Send CLI_MD5_LOGIN

	return 0;
}

SNAC_SUBHANDLER(icq_snac_secure_id_request) {
	/* SNAC(17,0A) SRV_SECURID_REQUEST	Server SecureID request
	 *
	 * This snac used by server to request SecurID number from client. This
	 * happens only if you are trying to login as AIM administrator. AIM
	 * administrators have a little pager-like thing that display a SecurID
	 * number wich changes every 60 seconds. Client must respond with
	 * SNAC(17,0B) containing correct SecureID number (or server will send you
	 * an auth error).
	 */
	/* ?wo? I'm sure, we don't need it */
	return 0;
}

SNAC_HANDLER(icq_snac_sigon_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_sigon_error; break;
		case 0x03: handler = icq_snac_sigon_reply; break;
		case 0x05: handler = icq_snac_new_uin; break;
		case 0x07: handler = icq_snac_sigon_authkey; break;
		case 0x0a: handler = icq_snac_secure_id_request; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_sigon_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
