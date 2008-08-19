/*
 *  (C) Copyright 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001-2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002-2004 Martin Ã–berg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004-2008 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *                     2008 Wies³aw Ochmiñski <wiechu@wiechu.com>
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

SNAC_SUBHANDLER(icq_snac_buddy_error) {
	struct {
		uint16_t error;
	} pkt;
	uint16_t error;
	// XXX ?wo? TLV(8) - error subcode

	if (ICQ_UNPACK(&buf, "W", &pkt.error))
		error = pkt.error;
	else
		error = 0;

	icq_snac_error_handler(s, "buddy", error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_reply) {
	struct icq_tlv_list *tlvs;

	if ((tlvs = icq_unpack_tlvs(buf, len, 0))) {
		icq_tlv_t *t_max_uins = icq_tlv_get(tlvs, 1);		// Max number of contact list entries
		icq_tlv_t *t_max_watchers = icq_tlv_get(tlvs, 2);	// Max number of watcher list entries
		uint16_t max_uins = 0, max_watchers = 0;

		icq_unpack_tlv_word(t_max_uins, max_uins);
		icq_unpack_tlv_word(t_max_watchers, max_watchers);

		debug_white("icq_snac_buddy_reply() maxUins = %u maxWatchers = %u\n", max_uins, max_watchers);

		icq_tlvs_destroy(&tlvs);
	} else
		debug_error("icq_snac_buddy_reply() tlvs == NULL\n");
	return 0;
}

static void icq_get_description(session_t *s, char *uin) {
	string_t pkt, tlv5, rdv;
	uint32_t cookie1=rand(), cookie2=rand();

	debug_function("icq_get_description() for: %s\n", uin);

	pkt = string_init(NULL);
	icq_pack_append(pkt, "II", cookie1, cookie2);		// cookie
	icq_pack_append(pkt, "W", (uint32_t) 2);		// message type
	icq_pack_append(pkt, "u", atoi(uin));

	tlv5 = string_init(NULL);
	icq_pack_append(tlv5, "W", (uint32_t) 0);
	icq_pack_append(tlv5, "II", cookie1, cookie2);		// cookie
	icq_pack_append(tlv5, "P", (uint32_t) 0x1349);		// AIM_CAPS_ICQSERVERRELAY - Client supports channel 2 extended, TLV(0x2711) based messages.
	icq_pack_append(tlv5, "tW", icq_pack_tlv_word(0xA, 1));	// TLV 0x0A: acktype (1 = normal message)
	icq_pack_append(tlv5, "T", icq_pack_tlv(0x0F, NULL, 0));	// TLV 0x0F: unknown

	// RendezvousMessageData
	rdv = string_init(NULL);
	icq_pack_append(rdv, "wwiiiiwicwwwiiiccwwwcii",
				(uint32_t) 27,		// length of this data segment, always 27
				(uint32_t) 8,		// protocol version
				(uint32_t) 0, (uint32_t) 0, (uint32_t) 0, (uint32_t) 0, // pluginID
				(uint32_t) 0,		// unknown
				(uint32_t) 3,		// unknown
				(uint32_t) 0, 		// unknown
				(uint32_t) 0x7fff,	// channel 2 counter XXX
				(uint32_t) 14,		// length of this data segment, always 14
				(uint32_t) 0x7fff,	// channel 2 counter XXX
				(uint32_t) 0, (uint32_t) 0, (uint32_t) 0, // unknown, usually all zeros
				(uint32_t) 0xe8,	// msg type   XXX ???
				(uint32_t) 3,		// msg flags  XXX ???
				(uint32_t) 1,		// status code ??? XXX
				(uint32_t) 1,		// priority ??? XXX
				(uint32_t) 1,		// 
				(uint32_t) 0,		//
				(uint32_t) 0,		// foreground
				(uint32_t) 0x00ffffff); // foreground

	icq_pack_append(tlv5, "T", icq_pack_tlv(0x2711, rdv->str, rdv->len));
	string_free(rdv, 1);

	icq_pack_append(pkt, "T", icq_pack_tlv(0x05, tlv5->str, tlv5->len));
	string_free(tlv5, 1);

	icq_pack_append(pkt, "T", icq_pack_tlv(0x03, NULL, 0));		// empty TLV 3 to get an ack from the server

	icq_makesnac(s, pkt, 0x04, 0x06, 0, 0);
	icq_send_pkt(s, pkt);
}

SNAC_SUBHANDLER(icq_snac_buddy_online) {
	struct {
		char *uid;
		uint16_t warning;
		uint16_t count;
	} pkt;

	/*
	 * Handle SNAC(0x3,0xb) -- User online notification
	 *
	 * Server sends this snac when user from your contact list goes online.
	 * Also you'll receive this snac on user status change.
	 */

	struct icq_tlv_list *tlvs;
	icq_tlv_t *t;
	int status;
	userlist_t *u;
	char *uid, *descr = NULL;

	if (!ICQ_UNPACK(&buf, "uWW", &pkt.uid, &pkt.warning, &pkt.count))
		return -1;

	tlvs = icq_unpack_tlvs(buf, len, pkt.count);

	status = EKG_STATUS_AVAIL;	/* assume avail */
	uid = icq_uid(pkt.uid);
	u =userlist_find(s, uid);

	debug_white("icq_snac_buddy_online() %s\n", uid);

	for (t = tlvs; t; t = t->next) {

		/* darkjames said: "I don't trust anything. Wiechu, check t->len" */
		switch (t->type) {
			case 0x01:
				if (tlv_length_check("icq_snac_buddy_online()", t, 2))
					continue;
				break;
			case 0x03:
			case 0x05:
			case 0x0a:
			case 0x0f:
				if (tlv_length_check("icq_snac_buddy_online()", t, 4))
					continue;
				break;
		}
		/* now we've got trusted length */

		switch (t->type) {
			case 0x06:
			{
				/* User status
				 *
				 * ICQ service presence notifications use user status field which consist
				 * of two parts. First is a various flags (birthday flag, webaware flag,
				 * etc). Second is a user status (online, away, busy, etc) flags.
				 */
				uint16_t icq_status, icq_status_flags;

				if (!icq_unpack_nc(t->buf, t->len, "WW", &icq_status_flags, &icq_status)) {
					debug_error("icq_snac_buddy_online() TLV(6) corrupted?\n");
					continue;
				}

				debug_white("icq_snac_buddy_online() status2=0x%04x status=0x%04x\n", icq_status_flags, icq_status);
				status = icq2ekg_status(icq_status);
				break;
			}

			case 0x0a: /* IP address */
			{
				uint32_t ip;
				if (icq_unpack_nc(t->buf, t->len, "i", &ip))
					user_private_item_set_int(u, "ip", ip);
				break;
			}

			case 0x01: /* User class */
				debug_white("icq_snac_buddy_online() class 0x%02x\n", t->nr);
				break;
			case 0x03: /* Time when client gone online (unix time_t) */
				debug_white("icq_snac_buddy_online() online since %d\n", t->nr);
				break;
			case 0x05: /* Time when this account was registered (unix time_t) */
				debug_white("icq_snac_buddy_online() ICQ Member since %d\n", t->nr);
				break;
			case 0x0f: /* Online time in seconds */
				debug_white("icq_snac_buddy_online() %d seconds online\n", t->nr);
				break;

			case 0x0c: /* DC info */
			{
				struct {
					uint32_t ip;
					uint32_t port;
					uint8_t tcp_flag;
					uint16_t version;
					uint32_t conn_cookie;

					uint32_t web_port;
					uint32_t client_features;
					/* faked time signatures, used to identify clients */
					uint32_t ts1;
					uint32_t ts2;
					uint32_t ts3;
				} tlv_c;

				if (!icq_unpack_nc(t->buf, t->len, "IICWI",
						&tlv_c.ip, &tlv_c.port,
						&tlv_c.tcp_flag, &tlv_c.version,
						&tlv_c.conn_cookie))
				{
					debug_error("icq_snac_buddy_online() TLV(C) corrupted?\n");
					continue;
				}

				/* XXX, this info should be saved for /dcc ! */

				break;
			}

			case 0x1d: /* user icon id & hash */
			{
				unsigned char *t_data = t->buf;
				int t_len = t->len;

				while (t_len > 0) {
					static char empty_item[0x10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

					uint16_t item_type;
					uint8_t item_flags;
					uint8_t item_len;

					if (!icq_unpack(t_data, &t_data, &t_len, "WCC", &item_type, &item_flags, &item_len)) {
						debug_error("icq_snac_buddy_online() TLV(1D) corrupted?\n");
						break;
					}
					
					/* just some validity check */
					if (item_len > t_len)
						item_len = t_len;

					if (memcmp(t_data, empty_item, (item_len < 0x10) ? item_len : 0x10)) {
						/* Item types
						 * 	0000: AIM mini avatar
						 * 	0001: AIM/ICQ avatar ID/hash (len 5 or 16 bytes)
						 * 	0002: iChat online message
						 *	0008: ICQ Flash avatar hash (16 bytes)
						 * 	0009: iTunes music store link
						 *	000C: ICQ contact photo (16 bytes)
						 *	000D: ?
						 *	000E: Custom Status (ICQ6)
						 */

						debug_white("icq_snac_buddy_online() user has got avatar: type: %d flags: %d\n", item_type, item_flags);
						icq_hexdump(DEBUG_WHITE, t_data, item_len);
						/* XXX, display message, get? do something? */
					}

					t_data += item_len;
					t_len -= item_len;
				}
				break;
			}

			case 0x0d: /* Client capabilities list */
			{
				debug_white("icq_snac_buddy_online() Capability list.\n");
				if (tlv_length_check("icq_snac_buddy_online()", t, t->len & ~0xF))
					break;
#if ICQ_DEBUG_UNUSED_INFORMATIONS
				unsigned char *data = t->buf;
				int d_len = t->len;
				while (d_len > 0) {
					debug_white("icq_snac_buddy_online() %s\n", icq_capability_name(data));
					data  += 0x10;		// capability length
					d_len -= 0x10;
				}
#endif
				break;
			}

			default:
				debug_error("icq_snac_buddy_online() Unknown TLV(0x%x)\n", t->type);
		}
	}

	/* We change only status, not description */
	if (u)
		descr = xstrdup(u->descr);

	protocol_status_emit(s, uid, status, descr, time(NULL));
	icq_get_description(s, pkt.uid);

	icq_tlvs_destroy(&tlvs);
	xfree(uid);
	xfree(descr);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_offline) {
	debug_function("icq_snac_buddy_offline()\n");

	do {
		char *cont = NULL;
		char *uid;
		uint16_t discard, t_count;

		if (!ICQ_UNPACK(&buf, "uWW", &cont, &discard, &t_count))
			return -1;

		while (t_count) {
			uint16_t t_type, t_len;

			if (!ICQ_UNPACK(&buf, "WW", &t_type, &t_len))
				return -1;

			if (len < t_len)
				return -1;

			buf += t_len;
			len -= t_len;
			t_count--;
		}

		uid = icq_uid(cont);
		debug_white("icq_snac_buddy_offline() uid: %s\n", uid);
		protocol_status_emit(s, uid, EKG_STATUS_NA, NULL, time(NULL));
		xfree(uid);
	} while (len >= 1);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_notify_rejected) {
	char *uid;

	if (!ICQ_UNPACK(&buf, "u", &uid))	// XXX ?wo? may be repeated more then once?
		return -1;

	debug_function("icq_snac_buddy_notify_rejected() for: %s\n", uid);
	return 0;
}

SNAC_HANDLER(icq_snac_buddy_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_buddy_error;	break;	/* Miranda: OK */
		case 0x03: handler = icq_snac_buddy_reply;	break;	/* Miranda: OK */
		case 0x0a: handler = icq_snac_buddy_notify_rejected;	/* Miranda: OK */
								break;	/*        .... */
		case 0x0b: handler = icq_snac_buddy_online;	break;	/* Miranda: handleUserOnline() */
		case 0x0c: handler = icq_snac_buddy_offline;	break;	/* Miranda: OK */
		default:   handler = NULL;		break;
	}

	if (!handler) {
		debug_error("icq_snac_buddy_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

