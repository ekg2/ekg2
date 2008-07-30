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

SNAC_SUBHANDLER(icq_snac_sigon_error) {		/* Miranda: OK */
	struct {
		uint16_t error;
	} pkt;
	uint16_t error;

	if (ICQ_UNPACK(&buf, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	icq_snac_error_handler(s, "sigon", error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_sigon_reply) {
#warning "icq_snac_sigon_reply() dokonczyc"
	debug_error("icq_snac_sigon_reply() XXX\n");

#if 0
	oscar_tlv_chain* chain = NULL;
	WORD wError;

	icq_sendCloseConnection(); // imitate icq5 behaviour

	if (!(chain = readIntoTLVChain(&buf, datalen, 0)))
	{
		NetLog_Server("Error: Missing chain on close channel");
		NetLib_CloseConnection(&hServerConn, TRUE);
		return; // Invalid data
	}

	// TLV 8 errors (signon errors?)
	wError = getWordFromChain(chain, 0x08, 1);
	if (wError)
	{
		handleSignonError(wError);

		// we return only if the server did not gave us cookie (possible to connect with soft error)
		if (!getLenFromChain(chain, 0x06, 1)) 
		{
			disposeChain(&chain);
			SetCurrentStatus(ID_STATUS_OFFLINE);
			NetLib_CloseConnection(&hServerConn, TRUE);
			return; // Failure
		}
	}

	// We are in the login phase and no errors were reported.
	// Extract communication server info.
	info->newServer = (char*)getStrFromChain(chain, 0x05, 1);
	info->cookieData = getStrFromChain(chain, 0x06, 1);
	info->cookieDataLen = getLenFromChain(chain, 0x06, 1);

	// We dont need this anymore
	disposeChain(&chain);

	if (!info->newServer || !info->cookieData)
	{
		icq_LogMessage(LOG_FATAL, LPGEN("You could not sign on because the server returned invalid data. Try again."));

		SAFE_FREE((void**)&info->newServer);
		SAFE_FREE((void**)&info->cookieData);
		info->cookieDataLen = 0;

		SetCurrentStatus(ID_STATUS_OFFLINE);
		NetLib_CloseConnection(&hServerConn, TRUE);
		return; // Failure
	}

	NetLog_Server("Authenticated.");
	info->newServerReady = 1;
#endif
	return -3;
}

SNAC_SUBHANDLER(icq_snac_sigon_authkey) {
	struct {
		uint16_t key_len;
	} pkt;
	string_t str;

	unsigned char digest[16];

#warning "icq_snac_sigon_authkey() dokonczyc"

	icq_hexdump(DEBUG_ERROR, buf, len);

	if (!ICQ_UNPACK(&buf, "W", &pkt.key_len)) {
		icq_handle_disconnect(s, "Secure login failed. Invalid server response.", 0);		/* XXX */
		return -1;
	}

	if (!pkt.key_len || len < pkt.key_len) {
		icq_handle_disconnect(s, "Secure login failed. Invalid key length.", 0);		/* XXX */
		return -1;
	}

	/* XXX, miranda limit key to 64B */

#if 0
	mir_md5_state_t state;

	unpackString(&buf, szKey, wKeyLen);

	mir_md5_init(&state);
	mir_md5_append(&state, info->szAuthKey, info->wAuthKeyLen);
	mir_md5_finish(&state, digest);

	mir_md5_init(&state);
	mir_md5_append(&state, (LPBYTE)szKey, wKeyLen);
	mir_md5_append(&state, digest, 16);
	mir_md5_append(&state, (LPBYTE)CLIENT_MD5_STRING, sizeof(CLIENT_MD5_STRING)-1);
	mir_md5_finish(&state, digest);
#endif
	
	str = string_init(NULL);

	icq_pack_append(str, "T", icq_pack_tlv_str(1, s->uid + 4));		/* uid */
	icq_pack_append(str, "T", icq_pack_tlv(0x25, digest, sizeof(digest)));	/* MD5-digest */

	icq_pack_append(str, "T", icq_pack_tlv_str(3, "ICQ Inc. - Product of ICQ (TM).2003b.5.37.1.3728.85"));
	icq_pack_append(str, "tW", icq_pack_tlv_word(0x16, 0x010A));	/* unk, 0x01 0x0A */
	icq_pack_append(str, "tW", icq_pack_tlv_word(0x17, 5));		/* FLAP_VER_MAJOR */
	icq_pack_append(str, "tW", icq_pack_tlv_word(0x18, 37));	/* FLAP_VER_MINOR */
	icq_pack_append(str, "tW", icq_pack_tlv_word(0x19, 1));		/* FLAP_VER_LESSER */
	icq_pack_append(str, "tW", icq_pack_tlv_word(0x1A, 3828));	/* FLAP_VER_BUILD */
	icq_pack_append(str, "tI", icq_pack_tlv_dword(0x14, 85));	/* FLAP_VER_SUBBUILD */
	icq_pack_append(str, "T", icq_pack_tlv_str(0x0F, "pl"));	/* language 2 chars */ /* XXX, en */
	icq_pack_append(str, "T", icq_pack_tlv_str(0x0E, "pl"));	/* country 2 chars */ /* XXX, en */

	icq_makesnac(s, str, 0x17, 2, 0, 0);
	icq_send_pkt(s, str);
	return 0;
}

SNAC_HANDLER(icq_snac_sigon_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_sigon_error; break;	/* Miranda: OK */
		case 0x03: handler = icq_snac_sigon_reply; break;	/* Miranda: START */
		case 0x07: handler = icq_snac_sigon_authkey; break;	/* Miranda: START */
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_sigon_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

