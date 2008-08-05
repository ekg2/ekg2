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

SNAC_SUBHANDLER(icq_snac_service_error) {
	struct {
		uint16_t error;
		unsigned char *data;
	} pkt;

	uint16_t error;

	debug_function("icq_snac_service_error()\n");

	if (ICQ_UNPACK(&pkt.data, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	/* Something went wrong, probably the request for avatar family failed */
	icq_snac_error_handler(s, "service", error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_families) {
	string_t pkt = string_init(NULL);

	/* This packet is a response to SRV_FAMILIES SNAC(1,3).
	 * This tells the server which SNAC families and their corresponding
	 * versions which the client understands. This also seems to identify
	 * the client as an ICQ vice AIM client to the server.
	 * Miranda mimics the behaviour of ICQ 6 */

	icq_pack_append(pkt, "WW", (uint32_t) 0x22, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x01, (uint32_t) 0x03);
	icq_pack_append(pkt, "WW", (uint32_t) 0x13, (uint32_t) 0x04);
	icq_pack_append(pkt, "WW", (uint32_t) 0x02, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x03, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x15, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x04, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x06, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x08, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x09, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x0a, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x0b, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x0c, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x25, (uint32_t) 0x01);

	icq_makesnac(s, pkt, 0x01, 0x17, 0, 0);
	icq_send_pkt(s, pkt);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_redirect) {
	debug_error("icq_snac_service_redirect() XXX\n");
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_rateinfo) {
	icq_private_t *j = s->priv;
	string_t str;

#if MIRANDA
	/* init rates management */
	m_rates = ratesCreate(pBuffer, wBufferLength);
#endif

	/* ack rate levels */
	str = icq_pack("WWWWW", (uint32_t) 0x01, (uint32_t) 0x02, (uint32_t) 0x03, (uint32_t) 0x04, (uint32_t) 0x05);
	icq_makesnac(s, str, 0x01, 0x08, 0, 0);
	icq_send_pkt(s, str);

	/* CLI_REQINFO - This command requests from the server certain information about the client that is stored on the server. */
	str = string_init(NULL);
	icq_makesnac(s, str, 0x01, 0x0e, 0, 0);
	icq_send_pkt(s, str);

	if (j->ssi) {
#if 0
		DWORD dwLastUpdate = getSettingDword(NULL, "SrvLastUpdate", 0);
		WORD wRecordCount = getSettingWord(NULL, "SrvRecordCount", 0);

		servlistcookie* ack;
		DWORD dwCookie;
#endif
		/* CLI_REQLISTS - we want to use SSI */
		str = string_init(NULL);
//		str = icq_pack("tW", icq_pack_tlv_word(0x0B, 0x000F));	/* mimic ICQ 6 */
		icq_makesnac(s, str, 0x13, 0x02, 0, 0);
		icq_send_pkt(s, str);
#if 0
		if (!wRecordCount) { /* CLI_REQROSTER */
			/* we do not have any data - request full list */

			serverPacketInit(&packet, 10);
			ack = (servlistcookie*)SAFE_MALLOC(sizeof(servlistcookie));
			if (ack)
			{ // we try to use standalone cookie if available
				ack->dwAction = SSA_CHECK_ROSTER; // loading list
				dwCookie = AllocateCookie(CKT_SERVERLIST, ICQ_LISTS_CLI_REQUEST, 0, ack);
			}
			else // if not use that old fake
				dwCookie = ICQ_LISTS_CLI_REQUEST<<0x10;

			packFNACHeaderFull(&packet, ICQ_LISTS_FAMILY, ICQ_LISTS_CLI_REQUEST, 0, dwCookie);
			sendServPacket(&packet);

		} else { /* CLI_CHECKROSTER */

			serverPacketInit(&packet, 16);
			ack = (servlistcookie*)SAFE_MALLOC(sizeof(servlistcookie));
			if (ack)  // TODO: rewrite - use get list service for empty list
			{ // we try to use standalone cookie if available
				ack->dwAction = SSA_CHECK_ROSTER; // loading list
				dwCookie = AllocateCookie(CKT_SERVERLIST, ICQ_LISTS_CLI_CHECK, 0, ack);
			}
			else // if not use that old fake
				dwCookie = ICQ_LISTS_CLI_CHECK<<0x10;

			packFNACHeaderFull(&packet, ICQ_LISTS_FAMILY, ICQ_LISTS_CLI_CHECK, 0, dwCookie);
			// check if it was not changed elsewhere (force reload, set that setting to zero)
			if (IsServerGroupsDefined())
			{
				packDWord(&packet, dwLastUpdate);  // last saved time
				packWord(&packet, wRecordCount);   // number of records saved
			}
			else
			{ // we need to get groups info into DB, force receive list
				packDWord(&packet, 0);  // last saved time
				packWord(&packet, 0);   // number of records saved
			}
			sendServPacket(&packet);
		}
#else
		str = string_init(NULL);
//		icq_makesnac(s, str, 0x13, 0x04, 0, 0);
		icq_pack_append(str, "I", (uint32_t) 0x0000);
		icq_pack_append(str, "I", (uint32_t) 0x0000);
		icq_makesnac(s, str, 0x13, 0x05, 0, 0);
		icq_send_pkt(s, str);
#endif
	}

	/* CLI_REQLOCATION */
	str = string_init(NULL);
	icq_makesnac(s, str, 0x02, 0x02, 0, 0);
	icq_send_pkt(s, str);

	/* CLI_REQBUDDY */
	/* Query flags: 1 = Enable Avatars
	                2 = Enable offline status message notification
	                4 = Enable Avatars for offline contacts
	 */
//	str = icq_pack("tW", icq_pack_tlv_word(0x05, 0x0003));	/* mimic ICQ 6 */
	str = string_init(NULL);
	icq_makesnac(s, str, 0x03, 0x02, 0, 0);
	icq_send_pkt(s, str);

	/* CLI_REQICBM */
	str = string_init(NULL);
	icq_makesnac(s, str, 0x04, 0x04, 0, 0);
	icq_send_pkt(s, str);

	/* CLI_REQBOS */
	str = string_init(NULL);
	icq_makesnac(s, str, 0x09, 0x02, 0, 0);
	icq_send_pkt(s, str);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_ratechange) {
	struct {
		uint16_t status;
		uint16_t class;
		/* 20B */
		uint32_t level;
	} pkt;

	if (!ICQ_UNPACK(&buf, "WW", &pkt.status, &pkt.class))
		return -1;

	if (len < 20)
		return -1;

	buf += 20; len -= 20;

	if (!ICQ_UNPACK(&buf, "I", &pkt.level))
		return -1;

	/* XXX, Miranda do smth here :> */

	debug_error("XXX icq_snac_service_ratechange() %u %u %u\n", pkt.status, pkt.class, pkt.level);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_pause) {
	debug("icq_snac_service_pause() Server is going down in a few seconds...\n");
#warning "icq_snac_service_pause XXX"

#if MIRANDA
	/* This is the list of groups that we want to have on the next server */
	serverPacketInit(&packet, 30);
	packFNACHeader(&packet, ICQ_SERVICE_FAMILY, ICQ_CLIENT_PAUSE_ACK);

	packWord(&packet,ICQ_SERVICE_FAMILY);
	packWord(&packet,ICQ_LISTS_FAMILY);
	packWord(&packet,ICQ_LOCATION_FAMILY);
	packWord(&packet,ICQ_BUDDY_FAMILY);
	packWord(&packet,ICQ_EXTENSIONS_FAMILY);
	packWord(&packet,ICQ_MSG_FAMILY);
	packWord(&packet,0x06);
	packWord(&packet,ICQ_BOS_FAMILY);
	packWord(&packet,ICQ_LOOKUP_FAMILY);
	packWord(&packet,ICQ_STATS_FAMILY);
	sendServPacket(&packet);
#endif
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_reqinfo) {
	icq_private_t *j = s->priv;
	unsigned char *databuf;

	char *uin;
	uint16_t warning, count;
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;

	if (!icq_unpack(buf, &databuf, &len, "uWW", &uin, &warning, &count))
		return -1;

	if (xstrcmp(s->uid+4, uin))
		debug_error("icq_snac_service_reqinfo() Warning: Server thinks our UIN is %s, when it is %s\n", uin, s->uid+4);

	/*
	 * TLV(x01) User type?
	 * TLV(x0C) Empty CLI2CLI Direct connection info
	 * TLV(x0A) External IP
	 * TLV(x0F) Number of seconds that user has been online
	 * TLV(x03) The online since time.
	 * TLV(x0A) External IP again
	 * TLV(x22) Unknown
	 * TLV(x1E) Unknown: empty.
	 * TLV(x05) Member of ICQ since.
	 * TLV(x14) Unknown
	 */
	tlvs = icq_unpack_tlvs(databuf, len, count);

	/* XXX, miranda check dwRef. */
	/* XXX, miranda saves */
	for (t = tlvs; t; t = t->next) {
		switch (t->type) {
			case 0x03:
				if (t->len != 4) { debug_error("icq_snac_service_reqinfo() [0x03] len != 4\n"); goto def; }
				debug_white("icq_snac_service_reqinfo() Logon TS: %u\n", t->nr);
				break;

			case 0x05:
				if (t->len != 4) { debug_error("icq_snac_service_reqinfo() [0x05] len != 4\n"); goto def; }
				debug_white("icq_snac_service_reqinfo() ICQ Member since: %u\n", t->nr);
				break;

			case 0x0A:
				if (t->len != 4) { debug_error("icq_snac_service_reqinfo() [0x0A] len != 4\n"); goto def; }
				debug_white("icq_snac_service_reqinfo() External IP: %u.%u.%u.%u\n", t->buf[0], t->buf[1], t->buf[2], t->buf[3]);
				break;

			/* XXX */

			default:
			    def:
				debug_error("icq_snac_service_reqinfo() TLV[%u] datalen: %u\n", t->type, t->len);
				icq_hexdump(DEBUG_WHITE, t->buf, t->len);
		}
	}

	/* If we are in SSI mode, this is sent after the list is acked instead
	   to make sure that we don't set status before seing the visibility code
	 */
	if (!j->ssi /* || XXX: info->isMigrating */)
		icq_session_connected(s);

	icq_tlvs_destroy(&tlvs);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_migrate) {
	debug_error("!!! icq_snac_service_migrate() XXX\n");
	return -3;
}

SNAC_SUBHANDLER(icq_snac_service_motd) { return 0; }

SNAC_SUBHANDLER(icq_snac_service_families2) {
	string_t pkt = string_init(NULL);

	/* This is a reply to CLI_FAMILIES and it tells the client which families and their versions that this server understands.
	 * We send a rate request packet */

	pkt = string_init(NULL);

	icq_makesnac(s, pkt, 0x01, 0x06, 0, 0);
	icq_send_pkt(s, pkt);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_extstatus) {
	debug_error("!!! icq_snac_service_extstatus() XXX\n");
	return -3;
}

SNAC_HANDLER(icq_snac_service_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_service_error; 	break;	/* Miranda: OK */
		case 0x03: handler = icq_snac_service_families;	break;	/* Miranda: OK */
		case 0x05: handler = icq_snac_service_redirect; break;
		case 0x07: handler = icq_snac_service_rateinfo; break;	/* Miranda: OK */
		case 0x0A: handler = icq_snac_service_ratechange;break;	/* Miranda: 2/3 OK */
		case 0x0B: handler = icq_snac_service_pause;	break;	/* Miranda: OK */
		case 0x0F: handler = icq_snac_service_reqinfo;	break;	/* Miranda: OK */
		case 0x12: handler = icq_snac_service_migrate;	break;
		case 0x13: handler = icq_snac_service_motd;	break;	/* Miranda: OK */
		case 0x18: handler = icq_snac_service_families2;break;	/* Miranda: OK */
		case 0x21: handler = icq_snac_service_extstatus;break;
		default:   handler = NULL;			break;
	}

	if (!handler) {
		debug_error("icq_snac_service_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

