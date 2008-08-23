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

static void icq_snac_message_set_msg_channel(session_t *s, uint16_t chan, uint32_t flags) {
	string_t pkt;

	pkt = icq_pack("WIWWWWW",
		(uint32_t) chan, (uint32_t) flags,		/* channel, flags */
		(uint16_t) 8000, (uint32_t) 999,		/* max-message-snac-size, max-sender-warning-level */
		(uint32_t) 999, (uint32_t) 0,			/* max-rcv-warning-level, minimum message-interval-in-secons */
		(uint32_t) 0);					/* unknown */
	icq_makesnac(s, pkt, 0x04, 0x02, 0, 0);
	icq_send_pkt(s, pkt);
}

SNAC_SUBHANDLER(icq_snac_message_replyicbm) {
#if 1
	icq_snac_message_set_msg_channel(s, 0x01, 0x0b);
	icq_snac_message_set_msg_channel(s, 0x02, 0x03);
	icq_snac_message_set_msg_channel(s, 0x04, 0x03);

#else	/* Miranda-like */
	uint32_t flags;

	/* Set message parameters for all channels (imitate ICQ 6) */
	flags = 0x00000303;
#ifdef DBG_CAPHTML
	flags |= 0x00000400;
#endif
#ifdef DBG_CAPMTN
	flags |= 0x00000008;
#endif

	icq_snac_message_set_msg_channel(s, 0x00, flags);
#endif

	return 0;
}

int icq_snac_message_recv_simple(session_t *s, unsigned char *buf, int len, const char *sender) {
	struct icq_tlv_list *tlvs;
	struct icq_tlv_list *tlvs_msg;
	icq_tlv_t *t;
	
	debug_function("icq_snac_message_recv_simple() from: %s leftlen: %d\n", sender, len);

	if (!(tlvs = icq_unpack_tlvs(&buf, &len, 0))) {
		debug("icq_snac_message_recv_simple() ignoring empty message..\n");
		return 0;
	}
	
	if (!(t = icq_tlv_get(tlvs, 0x02)) || t->type != 0x02) {
		debug_error("icq_snac_message_recv_simple() TLV(0x02) not found?\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	if (!(tlvs_msg = icq_unpack_tlvs_nc(t->buf, t->len, 0))) {
		debug_error("icq_snac_message_recv_simple() failed to read tlv chain in message\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	if ((t = icq_tlv_get(tlvs_msg, 0x501))) {
		debug("icq_snac_message_recv_simple() message has: %d caps\n", t->len);
	} else
		debug("icq_snac_message_recv_simple() no message cap\n");

	{
		/* Parse the message parts, usually only one 0x0101 TLV containing the message,
		 * but in some cases there can be more 0x0101 TLVs containing message parts in
		 * different encoding (just like the new format of Offline Messages) */ 
		struct icq_tlv_list *t;
		string_t msg = string_init(NULL);
		time_t sent;

		for (t = tlvs_msg; t; t = t->next) {
			struct {
				uint16_t encoding;
				uint16_t codepage;
				unsigned char *message;
			} t_msg;

			int t_len = t->len;
			char *recode = NULL;

			if (t->type != 0x0101)
				continue;

			if (!icq_unpack(t->buf, &t_msg.message, &t_len, "WW", &t_msg.encoding, &t_msg.codepage))
				continue;

			debug_function("icq_snac_message_recv_simple() enc: %.4x cp: %.4x\n", t_msg.encoding, t_msg.codepage);

			recode = xstrndup((char *) t_msg.message, t_len);
			switch (t_msg.encoding) {
				case 0x02:	/* UCS-2BE */
				{
					string_t str = string_init(NULL);
					xfree(recode);
					string_append_raw(str, (char *) t_msg.message, t_len);
					recode = icq_convert_from_ucs2be(str);
					string_free(str, 1);
					break;
				}
				case 0x00:	/* US-ASCII */
				case 0x03:	/* ANSI */
					break;
				default:
					debug_error("icq_snac_message_recv_simple() Unsupported encoding 0x%x\n", t_msg.encoding);
			}
			string_append_n(msg, recode, xstrlen(recode));
			xfree(recode);
		}

		/* XXX, check if message was recv when we was offline */

		sent = time(NULL);

		if (msg->len) {
			char *uid = icq_uid(sender);
			/* XXX, class? */
			protocol_message_emit(s, uid, NULL, msg->str, NULL, sent, EKG_MSGCLASS_CHAT, NULL, EKG_TRY_BEEP, 0);
			xfree(uid);
		}

		string_free(msg, 1);
	}

	icq_tlvs_destroy(&tlvs);
	icq_tlvs_destroy(&tlvs_msg);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_recv) {
	struct {
		uint32_t id1;
		uint32_t id2;
		uint16_t format;	/* 0x01 - simple message format
					   0x02 - advanced message format
					   0x04 - 'new' message format
					 */
		char *sender;
		uint16_t warning_level;	/* not used */
		uint16_t tlv_count;
	} pkt;
	struct icq_tlv_list *tlvs;

	if (!ICQ_UNPACK(&buf, "iiWuWW", &pkt.id1, &pkt.id2, &pkt.format, &pkt.sender, &pkt.warning_level, &pkt.tlv_count)) {
		debug_error("icq_snac_message_recv() Malformed message thru server\n");
		return -1;
	}

	debug_function("icq_snac_message_recv() from: %s id1: %.8x id2: %.8x format: %.4x warning: %.4x tlvs: %.4x\n",
				pkt.sender, pkt.id1, pkt.id2, pkt.format, pkt.warning_level, pkt.tlv_count);

	/* XXX, spamer? */

	tlvs = icq_unpack_tlvs(&buf, &len, pkt.tlv_count);
	/* XXX ?wo? Handle tvls? */
	icq_tlvs_destroy(&tlvs);

	switch (pkt.format) {
		case 0x01:
			icq_snac_message_recv_simple(s, buf, len, pkt.sender);
			break;

		case 0x02:
		case 0x04:
		default:
			debug_error("icq_snac_message_recv() unknown format message from server sender: %s\n", pkt.sender);
			/* dump message */
			icq_hexdump(DEBUG_ERROR, buf, len);
			break;
	}
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_server_ack) {
	struct {
		uint32_t discard1;
		uint32_t discard2;
		uint16_t channel;
		char *uid;
	} pkt;

	if (!ICQ_UNPACK(&buf, "IIWu", &pkt.discard1, &pkt.discard2, &pkt.channel, &pkt.uid)) {
		debug_error("icq_snac_message_server_ack() packet to short!\n");
		return -1;
	}

	debug_error("XXX icq_snac_message_server_ack() chan=%.4x uid=%s\n", pkt.channel, pkt.uid);

	/* XXX, cookie, etc.. */

	return 0;
}

void icq_snac_message_status_reply(session_t *s, const char *from, char *uin, uint16_t version, char *msg) {
	status_t status;
	userlist_t *u;
	char *uid;
	
#if 0
	CCSDATA ccs;
	PROTORECVEVENT pre = {0};
	int status;
	char* pszMsg;
#endif

#if 0
	pszMsg = null_strdup((char*)msg);

	// it is probably UTF-8 status reply
	if (wVersion == 9)
		pszMsg = detect_decode_utf8(pszMsg);

	ccs.szProtoService = PSR_AWAYMSG;
	ccs.hContact = hContact;
	ccs.wParam = status;
	ccs.lParam = (LPARAM)&pre;
	pre.szMessage = pszMsg;
	pre.timestamp = time(NULL);

	CallService(MS_PROTO_CHAINRECV,0,(LPARAM)&ccs);

	SAFE_FREE((void**)&pszMsg);
#endif
	debug_function("icq_snac_message_status_reply() status from: %s msg: %s\n", uin, msg);

	uid = icq_uid(uin);
	/* We change only description, not status */
	status = (u = userlist_find(s, uid)) ? u->status : EKG_STATUS_UNKNOWN;
	protocol_status_emit(s, uid, status, msg, time(NULL));
	xfree(uid);
}

SNAC_SUBHANDLER(icq_snac_message_response) {
	struct {
		uint32_t msg_id1;
		uint32_t msg_id2;
		uint16_t format;
		char *uid;
		uint16_t dunno1;

		/* next part.. */
		uint16_t len;
	} pkt;

	if (!ICQ_UNPACK(&buf, "iiWuw", &pkt.msg_id1, &pkt.msg_id2, &pkt.format, &pkt.uid, &pkt.dunno1))
		return -1;

	debug_function("icq_snac_message_response() uid: %s\n", pkt.uid);

	if (pkt.format != 0x02) {
		debug_error("icq_snac_message_response() unknown type: %.4x\n", pkt.format);
		return 0;
	}

	/* XXX, cookie, check cookie uid */

	if (!ICQ_UNPACK(&buf, "w", &pkt.len)) {
		/* XXX */
		icq_hexdump(DEBUG_ERROR, buf, len);
		return 0;
	}

	if (pkt.len == 0x1b && 1 /* XXX */) {
		/* this can be v8 greeting message reply */
		uint16_t version;
		/* 27b unknowns from the msg we sent */
		uint16_t seq2;					/* Message sequence (SEQ2) */
		/* 12b Unknown */
		uint8_t msg_type, flags;			/* Message type: MTYPE_AUTOAWAY, MTYPE_AUTOBUSY, etc. */
		uint16_t status;				/* Status */
		/* 2b Priority? */

		if (!ICQ_UNPACK(&buf, "w27w12ccw2", &version, &seq2, &msg_type, &flags, &status))
			return -1;

		/* XXX, more cookies... */

		icq_hexdump(DEBUG_ERROR, buf, len);

		if (flags == 3) {     /* A status message reply */
			char *reason;

			if (len < 2)
				return -1;

			reason = xstrndup((char *) buf + 2, len);
			icq_snac_message_status_reply(s, "SNAC(4,B)", pkt.uid, version, reason);
			xfree(reason);
			return 0;
		}
	} else {
		/* XXX */
		icq_hexdump(DEBUG_ERROR, buf, len);
		return 0;
	}

	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_mini_typing_notification) {
	struct {
		uint32_t msg_id1;
		uint32_t msg_id2;
		uint16_t format;
		char *uid;
		uint16_t typing;
	} pkt;
	char *icquid;

	if (!ICQ_UNPACK(&buf, "iiWuW", &pkt.msg_id1, &pkt.msg_id2, &pkt.format, &pkt.uid, &pkt.typing))
		return -1;

	icquid = icq_uid(pkt.uid);
	switch (pkt.typing) {
		case 0x0000:
			protocol_xstate_emit(s, icquid, 0, EKG_XSTATE_TYPING);
			break;
		case 0x0001:
		case 0x0002:
			protocol_xstate_emit(s, icquid, EKG_XSTATE_TYPING, 0);
			break;
		default:
			debug_function("icq_snac_message_mini_typing_notification() uid: %s, UNKNOWN typing!!! (0x%x)\n", pkt.uid, pkt.typing);
	}
	xfree(icquid);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_queue) {	/* SNAC(4, 0x17) Offline Messages response */
	debug_error("icq_snac_message_queue() XXX\n");

#if MIRANDA
	offline_message_cookie *cookie;

	if (FindCookie(dwRef, NULL, (void**)&cookie))
	{
		NetLog_Server("End of offline msgs, %u received", cookie->nMessages);
		if (cookie->nMissed)
		{ // NASTY WORKAROUND!!
			// The ICQ server has a bug that causes offline messages to be received again and again when some 
			// missed message notification is present (most probably it is not processed correctly and causes
			// the server to fail the purging process); try to purge them using the old offline messages
			// protocol.  2008/05/21
			NetLog_Server("Warning: Received %u missed message notifications, trying to fix the server.", cookie->nMissed);

			icq_packet packet;
			// This will delete the messages stored on server
			serverPacketInit(&packet, 24);
			packFNACHeader(&packet, ICQ_EXTENSIONS_FAMILY, ICQ_META_CLI_REQ);
			packWord(&packet, 1);             // TLV Type
			packWord(&packet, 10);            // TLV Length
			packLEWord(&packet, 8);           // Data length
			packLEDWord(&packet, m_dwLocalUIN); // My UIN
			packLEWord(&packet, CLI_DELETE_OFFLINE_MSGS_REQ); // Ack offline msgs
			packLEWord(&packet, 0x0000);      // Request sequence number (we dont use this for now)

			sendServPacket(&packet);
		}

		ReleaseCookie(dwRef);
	}
	else
		NetLog_Server("Error: Received unexpected end of offline msgs.");
#endif
	return -3;
}

SNAC_HANDLER(icq_snac_message_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_message_error; break;
		case 0x05: handler = icq_snac_message_replyicbm; break;		/* Miranda: OK */
		case 0x07: handler = icq_snac_message_recv; break;
		case 0x0B: handler = icq_snac_message_response; break;
		case 0x0C: handler = icq_snac_message_server_ack; break;
		case 0x14: handler = icq_snac_message_mini_typing_notification; break;
		case 0x17: handler = icq_snac_message_queue; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_message_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

