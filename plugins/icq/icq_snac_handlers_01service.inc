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
	string_t pkt;

	debug_function("icq_snac_service_families()\n");
	// Handle incoming packet
#if ICQ_DEBUG_UNUSED_INFORMATIONS
	debug_white("icq_snac_service_families() Server knows families:");
	while (len>=2) {
		uint16_t family;
		ICQ_UNPACK(&buf, "W", &family);
		debug_white(" 0x%x", family);
	}
	debug_white("\n");
#endif

	/* This packet is a response to SRV_FAMILIES SNAC(1,3).
	 * This tells the server which SNAC families and their corresponding
	 * versions which the client understands. This also seems to identify
	 * the client as an ICQ vice AIM client to the server.
	 * Miranda mimics the behaviour of ICQ 6 */

	pkt = string_init(NULL);
	icq_pack_append(pkt, "WW", (uint32_t) 0x22, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x01, (uint32_t) 0x04);	// Generic service controls
	icq_pack_append(pkt, "WW", (uint32_t) 0x13, (uint32_t) 0x04);	// Server Side Information (SSI) service
	icq_pack_append(pkt, "WW", (uint32_t) 0x02, (uint32_t) 0x01);	// Location services
	icq_pack_append(pkt, "WW", (uint32_t) 0x03, (uint32_t) 0x01);	// Buddy List management service
	icq_pack_append(pkt, "WW", (uint32_t) 0x15, (uint32_t) 0x01);
	icq_pack_append(pkt, "WW", (uint32_t) 0x04, (uint32_t) 0x01);	// ICBM (messages) service
	icq_pack_append(pkt, "WW", (uint32_t) 0x06, (uint32_t) 0x01);	// Invitation service
	icq_pack_append(pkt, "WW", (uint32_t) 0x08, (uint32_t) 0x01);	// Popup notices service
	icq_pack_append(pkt, "WW", (uint32_t) 0x09, (uint32_t) 0x01);	// Privacy management service
	icq_pack_append(pkt, "WW", (uint32_t) 0x0a, (uint32_t) 0x01);	// User lookup service
	icq_pack_append(pkt, "WW", (uint32_t) 0x0b, (uint32_t) 0x01);	// Usage stats service
	icq_pack_append(pkt, "WW", (uint32_t) 0x0c, (uint32_t) 0x01);	// Translation service
	icq_pack_append(pkt, "WW", (uint32_t) 0x25, (uint32_t) 0x01);
#if 0
	icq_pack_append(pkt, "WW", (uint32_t) 0x05, (uint32_t) 0x01);	//  Advertisements service
	icq_pack_append(pkt, "WW", (uint32_t) 0x07, (uint32_t) 0x01);	//  Administrative service
	icq_pack_append(pkt, "WW", (uint32_t) 0x0d, (uint32_t) 0x01);	//  Chat navigation service
	icq_pack_append(pkt, "WW", (uint32_t) 0x0e, (uint32_t) 0x01);	//  Chat service
	icq_pack_append(pkt, "WW", (uint32_t) 0x0f, (uint32_t) 0x01);	//  Directory user search
	icq_pack_append(pkt, "WW", (uint32_t) 0x10, (uint32_t) 0x01);	//  Server-stored buddy icons (SSBI) service
	icq_pack_append(pkt, "WW", (uint32_t) 0x15, (uint32_t) 0x01);	//  ICQ specific extensions service
	icq_pack_append(pkt, "WW", (uint32_t) 0x17, (uint32_t) 0x01);	//  Authorization/registration service
	icq_pack_append(pkt, "WW", (uint32_t) 0x85, (uint32_t) 0x01);	//  Broadcast service - IServerd extension
#endif

	icq_makesnac(s, pkt, 0x01, 0x17, 0, 0);
	icq_send_pkt(s, pkt);						// Send CLI_FAMILIES_VERSIONS

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_redirect) {
	/*
	 * SNAC(01,05) SRV_REDIRECTxSERVICE -- Redirect (for 0x0004 subtype)
	 *
	 * Server replies with this to SNAC(01,04) CLI_SERVICE_REQ -- Client service request.
	 * After receiving this snac client should connect to specified server to use requested service.
	 */
	debug_error("icq_snac_service_redirect() XXX\n");
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_rateinfo) {
	/* SNAC(01,07) SRV_RATE_LIMIT_INFO -- Rate limits information response
	 * This snac contain server information about its snac-rate limitations.
	 */
	icq_private_t *j = s->priv;

	struct {
		uint16_t no;	// Number of rate classes
	} pkt;
	struct {
		uint16_t cl;	// Class
		uint16_t no;	// Number of rate groups
	} pkt2;
	int i;

	if (!ICQ_UNPACK(&buf, "W", &pkt.no))
		goto wrong;
	if (pkt.no*(2+8*4+1) > len)
		goto wrong;

	/* init rates management */
	icq_rates_init(s, pkt.no);
	for (i=0; i<pkt.no; i++) {
		uint16_t id;
		icq_rate_t *r;
		(void) ICQ_UNPACK(&buf, "W", &id);	// Rate class ID
		if (id && (id <= j->n_rates)) {
			r = j->rates[id - 1];
			r->last_time = time(NULL);
			ICQ_UNPACK(&buf, "IIII III 5",
				&r->win_size,		// Window size
				&r->clear_lvl,		// Clear level
				&r->alert_lvl,		// Alert level
				&r->limit_lvl,		// Limit level
				&r->discn_lvl,		// Disconnect level
				&r->curr_lvl,		// Current level
				&r->max_lvl		// Max level
				);
		} else {
			buf += 7*4 + 5;
			len -= 7*4 + 5;
		}
	}
	// store rate groups
	while (len >= 4) {
		(void) ICQ_UNPACK(&buf, "WW", &pkt2.cl, &pkt2.no);
		if (pkt2.cl > j->n_rates) goto wrong;
		if (len < pkt2.no*4) goto wrong;

		pkt2.cl--;
		j->rates[pkt2.cl]->groups = xcalloc(pkt2.no, sizeof(uint32_t));
		j->rates[pkt2.cl]->n_groups = pkt2.no;
		for (i=0; i<pkt2.no; i++) {
			ICQ_UNPACK(&buf, "I", &j->rates[pkt2.cl]->groups[i]);
		}
	}

wrong:

	/* ack rate levels */
	icq_send_snac(s, 0x01, 0x08, 0, 0,
			"WWWWW",
			(uint32_t) 0x01, (uint32_t) 0x02, (uint32_t) 0x03, (uint32_t) 0x04, (uint32_t) 0x05);

	/* CLI_REQINFO - This command requests from the server certain information about the client that is stored on the server. */
	icq_send_snac(s, 0x01, 0x0e, 0, 0, NULL);

	if (j->ssi) {
#if 0
		DWORD dwLastUpdate = getSettingDword(NULL, "SrvLastUpdate", 0);
		WORD wRecordCount = getSettingWord(NULL, "SrvRecordCount", 0);

		servlistcookie* ack;
		DWORD dwCookie;
#endif
		/* CLI_REQLISTS - we want to use SSI */
		icq_send_snac(s, 0x13, 0x02, 0, 0, NULL);
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
		// XXX ?wo? number of items should be "W" ?
		icq_send_snac(s, 0x13, 0x05, 0, 0,
				"II",
				(uint32_t) 0x0000,		// modification date/time of client local SSI copy
				(uint32_t) 0x0000);		// number of items in client local SSI copy
#endif
	}

	/* SNAC(02,02) CLI_LOCATION_RIGHTS_REQ Request limitations/params
	 * Client use this SNAC to request location service parameters and limitations.
	 * Server should reply via SNAC(02,03).
	 */
	icq_send_snac(s, 0x02, 0x02, 0, 0, NULL);

	/* SNAC(03,02) CLI_BUDDYLIST_RIGHTS_REQ Request limitations/params
	 * Client use this SNAC to request buddylist service parameters and
	 * limitations. Server should reply via SNAC(03,03)
	 */
	icq_send_snac(s, 0x03, 0x02, 0, 0, ""); // example for empty snac: "" or NULL works

	/* SNAC(04,04) CLI_ICBM_PARAM_REQ Request parameters info
	 * Use this snac to request your icbm parameters from server.
	 * Server should reply via SNAC(04,05). You can change them using SNAC(04,02).
	 */
	icq_send_snac(s, 0x04, 0x04, 0, 0, "");

	/* SNAC(09,02) CLI_PRIVACY_RIGHTS_REQ Request service parameters
	 * Client use this SNAC to request buddylist service parameters and limitations.
	 * Server should reply via SNAC(09,03).
	 */
	icq_send_snac(s, 0x09, 0x02, 0, 0, "");

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_ratechange) {
	/*
	 * SNAC(01,0A) SRV_RATE_LIMIT_WARN Rate information changed / rate limit warning
	 *
	 * Server send this snac when you goes over rate limit or when rate parameters changing.
	 * Snac content is described by "message code". Here is the known code list:
	 * 0x0001	Rate limits parameters changed
	 * 0x0002	Rate limits warning (current level < alert level)
	 * 0x0003	Rate limit hit (current level < limit level)
	 * 0x0004	Rate limit clear (current level become > clear level)
	 */
	struct {
		uint16_t status;
	} pkt;
	icq_private_t *j = s->priv;

	if (!ICQ_UNPACK(&buf, "W", &pkt.status))	// Message code (see above)
		return -1;

	/* TODO ?wo? -- print warning here ? */

	while (len >= (2 + 8*4 +1)) {
		uint16_t id;
		uint32_t x0, x1, x2, x3, x4, x5, x6, x7;
		(void) ICQ_UNPACK(&buf, "W", &id);	// Rate class ID
		(void) ICQ_UNPACK(&buf, "IIII IIII x", &x0, &x1, &x2, &x3, &x4, &x5, &x6, &x7);
		if (id && (id <= j->n_rates)) {
			id--;
			j->rates[id]->win_size	= x0;		// Window size
			j->rates[id]->clear_lvl	= x1;		// Clear level
			j->rates[id]->alert_lvl	= x2;		// Alert level
			j->rates[id]->limit_lvl	= x3;		// Limit level
			j->rates[id]->discn_lvl	= x4;		// Disconnect level
			j->rates[id]->curr_lvl	= x5;		// Current level
			j->rates[id]->max_lvl	= x6;		// Max level
		}
	}

	debug_function("icq_snac_service_ratechange() status:%u\n", pkt.status);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_pause) {
	/*
	 * SNAC(01,0B) SRV_PAUSE -- Server pause command
	 *
	 * This is the first SNAC in client migration sequence.
	 *
	 * Client should ack pause command with SNAC(01,0C) and stop send packets until it receive server
	 * resume SNAC(01,0D), or migration notice SNAC(01,12).
	 *
	 * Migration sequence used to redirect client to new BOS server during current BOS shutdown.
	 */

	debug("icq_snac_service_pause() Server is going down in a few seconds...\n");

	icq_send_snac(s, 0x01, 0x0c, NULL, NULL,	/* CLI_PAUSE_ACK -- Client pause ack */
			"WWWW WWWW WW",
			/* This is the list of families that we want to have on the next server */
			(uint32_t) 0x01,	/* Generic service controls */
			(uint32_t) 0x02,	/* Location services */
			(uint32_t) 0x03,	/* Buddy List management service */
			(uint32_t) 0x04,	/* ICBM (messages) service */

			(uint32_t) 0x09,	/* Privacy management service */
			(uint32_t) 0x0a,	/* User lookup service (not used any more) (XXX ??? - This service used by old AIM clients to search users by email) */
			(uint32_t) 0x0b,	/* Usage stats service */
			(uint32_t) 0x13,	/* Server Side Information (SSI) service */

			(uint32_t) 0x15,	/* ICQ specific extensions service */
			(uint32_t) 0x17		/* Authorization/registration service */
			);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_resume) {
	/*
	 * SNAC(01,0D) SRV_RESUME -- Server resume command
	 *
	 * After this SNAC client, paused by SNAC(01,0B) may continue send packets to BOS.
	 * Migration sequence used to redirect client to new BOS server during current BOS shutdown.
	 */
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

	tlvs = icq_unpack_tlvs(&databuf, &len, count);

	/* XXX, miranda check dwRef. */
	/* XXX, miranda saves */
	for (t = tlvs; t; t = t->next) {
		switch (t->type) {
			case 0x01:	/* User type */
				if (tlv_length_check("icq_snac_service_reqinfo()", t, 2)) goto def;
				debug_white("icq_snac_service_reqinfo() User type: 0x%x\n", t->nr);
				break;

			case 0x03:	/* The online since time */
				if (tlv_length_check("icq_snac_service_reqinfo()", t, 4)) goto def;
				debug_white("icq_snac_service_reqinfo() Logon time: %s\n", timestamp_time("%Y-%m-%d %H:%M:%S", t->nr));
				break;

			case 0x05:	/* Member of ICQ since */
				if (tlv_length_check("icq_snac_service_reqinfo()", t, 4)) goto def;
				debug_white("icq_snac_service_reqinfo() ICQ Member since: %s\n", timestamp_time("%Y-%m-%d %H:%M:%S", t->nr));
				break;

			case 0x0a:	/* External IP */
				if (tlv_length_check("icq_snac_service_reqinfo()", t, 4)) goto def;
				debug_white("icq_snac_service_reqinfo() External IP: %u.%u.%u.%u\n", t->buf[0], t->buf[1], t->buf[2], t->buf[3]);
				break;

			case 0x0c: /* Empty CLI2CLI Direct connection info */
				break;

			case 0x0d: /* Our capabilities */
				debug_white("icq_snac_service_reqinfo() Server knows %d our caps\n", t->len >> 4);
				break;

			case 0x0f:	/* Number of seconds that user has been online */
				if (tlv_length_check("icq_snac_service_reqinfo()", t, 4)) goto def;
				debug_white("icq_snac_service_reqinfo() Online: %u seconds.\n", t->nr);
				break;

			case 0x14:
			case 0x1e:	/* We know that we don't know them */
			case 0x22:
				break;

			/* XXX */

			default:
			    def:
				debug_error("icq_snac_service_reqinfo() TLV[0x%x] datalen: %u\n", t->type, t->len);
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

SNAC_SUBHANDLER(icq_snac_service_evil) {
	/*
	 * SNAC(01,10) SRV_EVIL_NOTICE Evil notification
	 *
	 * You'll receive this snac when somebody complain about you to an OSCAR server.
	 * This snac contain your new warning_level and (optionaly) info about complaining user.
	 * Anonymous evil notification doesn't contain user info.
	 */
	char *uin;
	uint16_t warning, count;
	struct icq_tlv_list *tlvs;
	unsigned char *databuf;

	while (len>4) {
		if (!icq_unpack(buf, &databuf, &len, "uWW", &uin, &warning, &count))
			return -1;
		debug_function("icq_snac_service_evil() %s\n", uin);
		tlvs = icq_unpack_tlvs(&databuf, &len, count);

		/* XXX - parse tlvs... */

		icq_tlvs_destroy(&tlvs);
	}
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_migrate) {
	/*
	 * SNAC(01,12) SRV_MIGRATION -- Migration notice and info
	 *
	 * This is the last SNAC in client migration sequence.
	 * Client should open new connection to specified server and use this connection for specified families.
	 * If there is no family list (empty) or it contain all connection families - client should close current
	 * connection and use new one.
	 */
	debug_error("!!! icq_snac_service_migrate() XXX\n");
	return -3;
}

SNAC_SUBHANDLER(icq_snac_service_motd) {
	/* SNAC(01,0x13) SRV_MOTD -- Message of the day (MOTD)
	 *
	 * Server send this during protocol negotiation sequence. Various docs call this SNAC as
	 * "message of the day" but it looks like that ICQ2K+ ignores this SNAC completely.
	 */
	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;
	uint16_t type;

	ICQ_UNPACK(&buf, "W", &type);
	/* MOTD types:
	 *	0x0001	MTD_MDT_UPGRAGE		Mandatory upgrade needed notice
	 *	0x0002	MTD_ADV_UPGRAGE		Advisable upgrade notice
	 *	0x0003	MTD_SYS_BULLETIN	AIM/ICQ service system announcements
	 *	0x0004	MTD_NORMAL		Standart notice
	 *	0x0006	MTD_NEWS		Some news from AOL service
	 */

	tlvs = icq_unpack_tlvs(&buf, &len, 0);
	if ( (t = icq_tlv_get(tlvs, 0x0b)) )
		debug_white("icq_snac_service_motd() type:%d, MOTD: %s\n", type, t->buf);
	else
		debug_white("icq_snac_service_motd() type:%d\n", type);
	icq_tlvs_destroy(&tlvs);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_urls) {
	/* SNAC(01,0x15) Well known urls */
	debug_function("icq_snac_service_urls()\n");
#if ICQ_DEBUG_UNUSED_INFORMATIONS
	uint16_t x, ulen;
	char *url;
	while (len>0) {
		ICQ_UNPACK(&buf, "WW", &x, &ulen);
		url = xstrndup((const char*)buf, ulen);
		debug_white("ICQ - well known url %d: %s\n", x, url);
		buf += ulen;
		len -= ulen;
		xfree(url);
	}
#endif
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_nop) {
	/* SNAC(01,16) CLI_KEEPALIVE No operation (NOP) 
	 *
	 * WinAIM (AIM service) use this snac to keep connection alive. This is usefull
	 * when it use proxy server to connect to BOS. Proxy servers can disconnect client
	 * if there is no activity for some time.
         */
	debug_warn("icq_snac_service_nop() is deprecated!\n");
	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_families2) {
	/* SNAC(01,18) SRV_FAMILIES_VERSIONS -- Server services versions
	 *
	 * This is a reply to CLI_FAMILIES and it tells the client which families
	 * and their versions that this server understands.
	 */
	debug_function("icq_snac_service_families2()\n");
	// Handle incoming packet
	#if ICQ_DEBUG_UNUSED_INFORMATIONS
	while (len>=2) {
		uint16_t fam, ver;
		ICQ_UNPACK(&buf, "WW", &fam, &ver);
		debug_white("icq_snac_service_families2() fam=0x%x ver=0x%x // %s\n",  fam, ver, icq_lookuptable(snac_families, fam));
	}
	#endif

	/* SNAC(01,06) CLI_RATES_REQUEST  
	 * Client use this SNAC to request server rate-limits. This happens during
	 * protocol negotiation sequence. Server should reply via SNAC(01,07)
	 */
	icq_send_snac(s, 0x01, 0x06, NULL, NULL, "");	// Request rate limits information

	return 0;
}

SNAC_SUBHANDLER(icq_snac_service_extstatus) {
	debug_function("icq_snac_service_extstatus() Received our avatar hash & status. XXX\n");
	icq_hexdump(DEBUG_ERROR, buf, len);
	return 0;
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
		case 0x0D: handler = icq_snac_service_resume;	break;
		case 0x0F: handler = icq_snac_service_reqinfo;	break;	/* Miranda: OK */
		case 0x10: handler = icq_snac_service_evil;	break;	/* Evil notification */
		case 0x12: handler = icq_snac_service_migrate;	break;
		case 0x13: handler = icq_snac_service_motd;	break;	/* Miranda: OK */
		case 0x15: handler = icq_snac_service_urls;	break;
		case 0x16: handler = icq_snac_service_nop;	break;	/* No operation (NOP) */
		case 0x18: handler = icq_snac_service_families2;break;	/* Miranda: OK */
		case 0x1F: handler = NULL;			break;	/* XXX Client verification request */
		case 0x21: handler = icq_snac_service_extstatus;break;
		default:   handler = NULL;			break;
	}

	if (!handler) {
		debug_error("icq_snac_service_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
