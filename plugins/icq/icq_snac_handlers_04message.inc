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

typedef struct {
	uint32_t id1;
	uint32_t id2;
	uint16_t channel;	/* Channel 1 message format (plain-text messages)
				   Channel 2 message format (rtf messages, rendezvous)
				   Channel 4 message format (typed old-style messages)
				 */
	char *sender;
	int ack_type;
	int msg_type;		/* message type */
	int msg_flags;		/* message flags */
	int version;
	int cookie;
	char uid[50];
	session_t *s;
	userlist_t *u;
} msg_params_t;

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

	icq_send_snac(s, 0x04, 0x02, 0, 0,
			"WIWWWWW",
			(uint32_t) chan, (uint32_t) flags,		/* channel, flags */
			(uint16_t) 8000, (uint32_t) 999,		/* max-message-snac-size, max-sender-warning-level */
			(uint32_t) 999, (uint32_t) 0,			/* max-rcv-warning-level, minimum message-interval-in-secons */
			(uint32_t) 0);					/* unknown */
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

static void icq_pack_append_msg_header(string_t pkt, msg_params_t *msg_param) {
	icq_pack_append(pkt, "IIWsW",
			msg_param->id1,		// message id part 1
			msg_param->id2,		// message id part 2
			(uint32_t) 2,		// channel
			msg_param->sender,
			(uint32_t) 3		// message formating
			);
}

static void icq_send_adv_msg_ack(session_t *s, msg_params_t *msg_param) {
	string_t pkt = string_init(NULL);

	icq_pack_append_msg_header(pkt, msg_param);
	icq_pack_append_rendezvous(pkt, ICQ_VERSION, msg_param->cookie, msg_param->msg_type, msg_param->msg_flags, 0, 0);
	icq_pack_append_nullterm_msg(pkt, "");

	icq_makesnac(s, pkt, 0x04, 0x0b, NULL, NULL);	/*  */
	icq_send_pkt(s, pkt);
}

static int icq_snac_message_recv_icbm_ch1(session_t *s, unsigned char *buf, int len, msg_params_t *msg_param) {
	struct icq_tlv_list *tlvs;
	struct icq_tlv_list *tlvs_msg;
	icq_tlv_t *t;

	debug_function("icq_snac_message_recv_icbm_ch1() from: %s leftlen: %d\n", msg_param->sender, len);

	if (!(tlvs = icq_unpack_tlvs(&buf, &len, 0))) {
		debug("icq_snac_message_recv_icbm_ch1() ignoring empty message.\n");
		return 0;
	}

	if (!(t = icq_tlv_get(tlvs, 0x02)) || t->type != 0x02) {
		debug_error("icq_snac_message_recv_icbm_ch1() TLV(0x02) not found?\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	// TLV(2) contains yet another TLV chain with the following TLVs:
	//	TLV(0x501): Capability
	//	TLV(0x101): this TLV contains the actual message (can be fragmented)
	if (!(tlvs_msg = icq_unpack_tlvs_nc(t->buf, t->len, 0))) {
		debug_error("icq_snac_message_recv_icbm_ch1() failed to read tlv chain in message\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	if ((t = icq_tlv_get(tlvs_msg, 0x501)))
		debug("icq_snac_message_recv_icbm_ch1() message has: %d caps\n", t->len);
	else
		debug("icq_snac_message_recv_icbm_ch1() no message cap\n");

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

			debug_function("icq_snac_message_recv_icbm_ch1() enc: %.4x cp: %.4x\n", t_msg.encoding, t_msg.codepage);

			switch (t_msg.encoding) {
				case 0x02:	/* UCS-2BE */
				{
					string_t str = string_init(NULL);
					string_append_raw(str, (char *) t_msg.message, t_len);
					recode = icq_convert_from_ucs2be(str);
					string_free(str, 1);
					break;
				}
				case 0x00:	/* US-ASCII */
				case 0x03:	/* ANSI */
					recode = xstrndup((char *) t_msg.message, t_len);
					break;
				default:
					debug_error("icq_snac_message_recv_icbm_ch1() Unsupported encoding 0x%x\n", t_msg.encoding);
			}
			string_append_n(msg, recode, xstrlen(recode));
			xfree(recode);
		}

		/* XXX, check if message was recv when we was offline */

		sent = time(NULL);

		if (msg->len) {
			int msgclass = xisdigit(*(msg_param->sender)) ? EKG_MSGCLASS_CHAT : EKG_MSGCLASS_SYSTEM;
			protocol_message_emit(s, msg_param->uid, NULL, msg->str, NULL, sent, msgclass, NULL, EKG_TRY_BEEP, 0);
		}

		string_free(msg, 1);
	}

	icq_tlvs_destroy(&tlvs);
	icq_tlvs_destroy(&tlvs_msg);
	return 0;
}

static void icq_send_status_descr(session_t *s, int msg_type, msg_params_t *msg_param) {
	string_t pkt;
	char *desc;

	if (!msg_param->u) {
		debug_warn("icq_send_status_descr(). Ignore request from %s\n", msg_param->uid);
		return;
	}
	debug_function("icq_send_status_descr() to %s\n", msg_param->uid);

	pkt = string_init(NULL);

	desc = xstrdup(s->descr);
	icq_pack_append_msg_header(pkt, msg_param);
	icq_pack_append_rendezvous(pkt, msg_param->version, msg_param->cookie, msg_type, MFLAG_AUTO, 0, 0);
	if (msg_param->version == 9) /* unicode */
		desc = ekg_locale_to_utf8(desc);
	else {
		/* XXX recode? */
	}
	icq_pack_append_nullterm_msg(pkt, desc);
	xfree(desc);

	icq_makesnac(s, pkt, 0x04, 0x0b, NULL, NULL);
	icq_send_pkt(s, pkt);
}

static int icq_snac_message_recv_rtf2711(session_t *s, unsigned char *buf, int len, msg_params_t *msg_param) {
	struct {
		uint16_t len;		/* length of following data (LE) */
		uint16_t ver;		/* protocol version (LE) */
		unsigned char *plug;	/* plugin or zero bytes (len=0x16) */
		uint16_t _unkn1;	/* unknown */
		uint32_t _capflg;	/* client capabilities flags */
		uint8_t _unkn2;		/* unknown */
		uint16_t _cookie;	/* cookie (LE) */
	} pkt1;
	struct {
		uint16_t len;		/* length of following data (LE) */
		uint16_t cookie;	/* cookie as in first chunk above (LE) */
		char *_unkn;		/* unknown, usually zeros */
	} pkt2;
	/* if plugin field in first chunk above is zero, here is message, overwise here is plugin-specific data. */
	struct {
		uint8_t type;		/* message type */
		uint8_t flags;		/* message flags */
		uint16_t status;	/* status code (LE) */
		uint16_t prio;		/* priority code (LE) */
		uint16_t len;		/* message string length (LE) */
		char *str;		/* message string (null-terminated) */
	} msg;

	debug_function("icq_snac_message_recv_rtf2711()\n");

	if ( len<27 ) {
		debug_error("icq_snac_message_recv_rtf2711() packet too short\n");
		return 1;
	}
	icq_unpack_nc(buf, len, "ww", &pkt1.len, &pkt1.ver);
	pkt1.plug = buf + 4;

	msg_param->version = pkt1.ver;

	if (msg_param->u)
		user_private_item_set_int(msg_param->u, "version", pkt1.ver);

	/* next chunk */
	buf += pkt1.len + 2;
	len -= pkt1.len + 2;

	icq_unpack_nc(buf, len, "ww", &pkt2.len, &pkt2.cookie);
	msg_param->cookie = pkt2.cookie;

	/* next chunk */
	buf += pkt2.len + 2;
	len -= pkt2.len + 2;

	switch (icq_plugin_id(pkt1.plug)) {
		case PSIG_MESSAGE:
		{
			/* message */
			ICQ_UNPACK(&buf, "ccwww", &msg.type, &msg.flags, &msg.status, &msg.prio, &msg.len);
			msg.str = (char *)buf;

			debug_white("icq_snac_message_recv_rtf2711() PSIG_MESSAGE, msg type:0x%x, flags:0x%x, ack:0x%x\n", msg.type, msg.flags, msg_param->ack_type);

			msg_param->msg_type = msg.type;
			msg_param->msg_flags = msg.flags;

			switch (msg.type) {
				/* strange types - XXX ?wo? handle? */
				case MTYPE_FILEREQ:
				case MTYPE_CHAT:
				case MTYPE_PLUGIN:
					debug_warn("icq_snac_message_recv_rtf2711() message type 0x%x not handled yet\n", msg.type);
					break;
				case MTYPE_AUTOAWAY:
				case MTYPE_AUTOBUSY:
				case MTYPE_AUTONA:
				case MTYPE_AUTODND:
				case MTYPE_AUTOFFC:
					/* Somebody ask for our status */
					icq_send_status_descr(s, msg.type, msg_param);
					break;
				case MTYPE_PLAIN:    /* plain message */
				{
					buf += msg.len;
					len -= msg.len;
					if (msg.len > 0) {
						time_t sent = time(NULL);
						char *tmp = ekg_utf8_to_locale_dup(msg.str);

						protocol_message_emit(s, msg_param->uid, NULL, tmp, NULL, sent, EKG_MSGCLASS_CHAT, NULL, EKG_TRY_BEEP, 0);

						xfree(tmp);
					}
					icq_send_adv_msg_ack(s, msg_param);
					break;
				}

				default:
					debug_error("icq_snac_message_recv_rtf2711() PSIG_MESSAGE, Not supported message type:0x%x\n", msg.type);
					break;
			}
			break;
		}
		default:
			debug_error("icq_snac_message_recv_rtf2711() we've got data for unknown plugin\n");
			icq_hexdump(DEBUG_ERROR, buf, len);
	}


	return 0;
}

static int icq_snac_message_recv_icbm_ch2(session_t *s, unsigned char *buf, int len, msg_params_t *msg_param) {
	struct {
		uint16_t type;		// message type (0 - normal, 1 - cancel, 2 - ack)
		uint32_t _id1, _id2;	// msg-id cookie (unused here)
		unsigned char *cap;	// capability (determines format of message data)

	} pkt;
	struct icq_tlv_list *tlvs, *tlvs5 = NULL;
	icq_tlv_t *t, *t5, *t2711;

	debug_function("icq_snac_message_recv_icbm_ch2() from: %s leftlen: %d\n", msg_param->sender, len);

	if (!(tlvs = icq_unpack_tlvs(&buf, &len, 0))) {
		debug("icq_snac_message_recv_icbm_ch2() ignoring empty message.\n");
		return 0;
	}

	if (!(t5 = icq_tlv_get(tlvs, 0x05))) {
		debug_error("icq_snac_message_recv_icbm_ch2() TLV(0x05) not found?\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	if (t5->len < 2 + 8 + 0x10) {
		debug_error("icq_snac_message_recv_icbm_ch2() TLV(0x05) too short\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	icq_unpack_nc(t5->buf, t5->len, "WII", &pkt.type, &pkt._id1, &pkt._id2);

	pkt.cap = t5->buf + (2 + 4 + 4);	/* msg capability */
	t5->buf += (2 + 4 + 4 + 0x10);
	t5->len -= (2 + 4 + 4 + 0x10);

	/* contents of TLV(0x05) is capability-specific */
	switch (icq_cap_id(pkt.cap)) {
		case CAP_SRV_RELAY:
		{
			if (pkt.type == 1) {
				debug_warn("icq_snac_message_recv_icbm_ch2() Can't handle abort message yet\n");
				icq_tlvs_destroy(&tlvs);
				return 0;
			}

			if ( !(tlvs5 = icq_unpack_tlvs_nc(t5->buf, t5->len, 0)) ) {
				debug("icq_snac_message_recv_icbm_ch2() ignoring empty TLV(0x05).\n");
				icq_tlvs_destroy(&tlvs);
				return 0;
			}

			/* tlvs5 may contain the following TVLs:
			 *
			 * TLV.Type(0x0A) - Acktype:
			 *	0x0000 - normal message
			 *	0x0001 - file request / abort request
			 *	0x0002 - file ack
			 *
			 * TLV.Type(0x03) - external ip
			 * TLV.Type(0x04) - internal ip
			 * TLV.Type(0x05) - listening port
			 * TLV.Type(0x0F) - unknown (empty)
			 * TLV.Type(0x2711) - extention data
			 */

			if ( (t = icq_tlv_get(tlvs5, 0x0a) ) )
				msg_param->ack_type = t->nr;
			else
				msg_param->ack_type = 1;

			if ( msg_param->u &&  (t = icq_tlv_get(tlvs5, 0x03)) )	// External IP
				user_private_item_set_int(msg_param->u, "IP", t->nr);

			if (!(t2711 = icq_tlv_get(tlvs5, 0x2711))) {
				debug_error("icq_snac_message_recv_icbm_ch2() TLV(0x2711) not found?\n");
				icq_tlvs_destroy(&tlvs5);
				icq_tlvs_destroy(&tlvs);
				return 1;
			}

			icq_snac_message_recv_rtf2711(s, t2711->buf, t2711->len, msg_param);
			break;
		}
		/* TO DO! ?wo? XXX */
		case CAP_ICQDIRECT:	// handle reverse DC request
		case CAP_SENDFILE:	// handle OFT packet
		case CAP_CONTACTS:	// handle contacts transfer
			debug_error("XXX icq_snac_message_recv_icbm_ch2() don't handle this yet\n");
			icq_hexdump(DEBUG_ERROR, t5->buf, t5->len);
			break;
		default:
			debug_error("icq_snac_message_recv_icbm_ch2() Unknow 0x2711 message for capability id=%d\n", icq_cap_id(pkt.cap));
			icq_hexdump(DEBUG_ERROR, t5->buf, t5->len);
			break;
	}

	icq_tlvs_destroy(&tlvs5);
	icq_tlvs_destroy(&tlvs);
	return 0;
}

static int icq_snac_message_recv_icbm_ch4(session_t *s, unsigned char *buf, int len, msg_params_t *msg_param) {
	debug_error("XXX icq_snac_message_recv_icbm_ch4()\n");
	icq_hexdump(DEBUG_ERROR, buf, len);
	return 0;
}

static int icq_snac_unpack_message_params(session_t *s, unsigned char **buf, int *len, msg_params_t *msg_param) {
	/* init msg_params */
	memset(msg_param, 0, sizeof(msg_params_t));

	if (!icq_unpack(*buf, buf, len, "IIWu", &msg_param->id1, &msg_param->id2, &msg_param->channel, &msg_param->sender))
		return 0;

	char *uid = icq_uid(msg_param->sender);
	msg_param->s = s;
	memcpy(msg_param->uid, uid, xstrlen(uid)+1);
	msg_param->u = userlist_find(s, uid);
	xfree(uid);
	return 1; /* OK */
}

SNAC_SUBHANDLER(icq_snac_message_recv) {
	msg_params_t msg_param;
	struct {
		uint16_t warning_level;	/* not used */
		uint16_t tlv_count;
	} pkt;
	struct icq_tlv_list *tlvs;

	if (!icq_snac_unpack_message_params(s, &buf, &len, &msg_param) || !ICQ_UNPACK(&buf, "WW", &pkt.warning_level, &pkt.tlv_count) ) {
		debug_error("icq_snac_message_recv() Malformed message thru server\n");
		return -1;
	}

	debug_function("icq_snac_message_recv() from: %s id1: %.8x id2: %.8x channel: %.4x warning: %.4x tlvs: %.4x\n",
				msg_param.sender, msg_param.id1, msg_param.id2, msg_param.channel, pkt.warning_level, pkt.tlv_count);

	/* XXX, spamer? */

	tlvs = icq_unpack_tlvs(&buf, &len, pkt.tlv_count);
	/*
	 * TLV.Type(0x01) - user class
	 * TLV.Type(0x03) - account creation time
	 * TLV.Type(0x06) - user status
	 * TLV.Type(0x0F) - online time
	 */
	icq_tlvs_destroy(&tlvs);

	switch (msg_param.channel) {
		case 0x01:	/* plain-text messages */
			icq_snac_message_recv_icbm_ch1(s, buf, len, &msg_param);
			break;

		case 0x02:	/* rtf messages, rendezvous */
			icq_snac_message_recv_icbm_ch2(s, buf, len, &msg_param);
			break;

		case 0x04:	/* yped old-style messages */
			icq_snac_message_recv_icbm_ch4(s, buf, len, &msg_param);
			break;

		default:
			debug_error("icq_snac_message_recv() unknown format message from server. Channel:%d Sender: %s\n", msg_param.channel, msg_param.sender);
			/* dump message */
			icq_hexdump(DEBUG_ERROR, buf, len);
			break;
	}
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_server_ack) {
	msg_params_t msg_param;

	if (!icq_snac_unpack_message_params(s, &buf, &len, &msg_param) ) {
		debug_error("icq_snac_message_server_ack() packet to short!\n");
		return -1;
	}

	debug_error("XXX icq_snac_message_server_ack() chan=%.4x uid=%s\n", msg_param.channel, msg_param.sender);

	/* XXX, cookie, etc.. */

	return 0;
}

static void icq_snac_message_status_reply(msg_params_t *msg_param, char *msg) {
	char *descr;

	if (!msg_param->u) {
		debug_warn("icq_snac_message_status_reply() Ignoring status description from unknown %s msg: %s\n", msg_param->uid, msg);
		return;
	}

	debug_function("icq_snac_message_status_reply() status from %s msg: %s\n", msg_param->uid, msg);

	if (msg_param->version == 9) /* utf-8 message */
		descr = ekg_utf8_to_locale_dup(msg);
	else
		descr = xstrdup(msg);

	/* We change only description, not status */
	protocol_status_emit(msg_param->s, msg_param->uid, msg_param->u->status, descr, time(NULL));

	xfree(descr);
}

SNAC_SUBHANDLER(icq_snac_message_response) {
	msg_params_t msg_param;
	struct {
		uint16_t len;
		uint16_t version;				/* this can be v8 greeting message reply */
		/* 27b unknowns from the msg we sent */
		uint16_t cookie;				/* Message sequence (cookie) */
		/* 12b Unknown */
		uint8_t msg_type;				/* Message type: MTYPE_AUTOAWAY, MTYPE_AUTOBUSY, etc. */
		uint8_t flags;					/* Message flags: MFLAG_NORMAL, MFLAG_AUTO, etc. */
		uint16_t status;				/* Status */
		/* 2b Priority? */
	} pkt;

	if (!icq_snac_unpack_message_params(s, &buf, &len, &msg_param))
		return -1;

	debug_function("icq_snac_message_response() uid: %s\n", msg_param.sender);

	if (msg_param.channel != 0x02) {
		debug_error("icq_snac_message_response() unknown type: %.4x\n", msg_param.channel);
		return 0;
	}

	/* XXX, cookie, check cookie uid */

	// skip 2 unknown bytes & get length
	if (!ICQ_UNPACK(&buf, "2 w", &pkt.len))
		pkt.len = 0;

	if (pkt.len == 0x1b && 1 /* XXX */) {

		if (!ICQ_UNPACK(&buf, "w27w12ccw2", &pkt.version, &pkt.cookie, &pkt.msg_type, &pkt.flags, &pkt.status))
			return -1;

		msg_param.version = pkt.version;
		/* XXX, more cookies... */

	} else {
		/* XXX */
		pkt.flags = 0;
	}

	if (pkt.flags == MFLAG_AUTO) {     /* A status message reply */
		char *reason;

		if (len < 2)
			return -1;

		reason = xstrndup((char *) buf + 2, len);
		icq_snac_message_status_reply(&msg_param, reason);
		xfree(reason);
	} else {
		/* XXX */
		debug_error("icq_snac_message_response() Sorry, we dont't handle it yet\n");
		icq_hexdump(DEBUG_ERROR, buf, len);
	}

	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_mini_typing_notification) {
#ifndef DBG_CAPMTN
	debug_error("icq_snac_message_mini_typing_notification() Ignoring unexpected typing notification");
#else
	msg_params_t msg_param;

	struct {
		uint16_t typing;
	} pkt;

	if (!icq_snac_unpack_message_params(s, &buf, &len, &msg_param) || !ICQ_UNPACK(&buf, "W", &pkt.typing))
		return -1;

	/* SetContactCapabilities(hContact, CAPF_TYPING); */

	switch (pkt.typing) {
		case 0x0000:	/* MTN_FINISHED */
			protocol_xstate_emit(s, msg_param.uid, 0, EKG_XSTATE_TYPING);
			break;

		case 0x0001:	/* MTN_TYPED */		/* XXX, accroding to Miranda code:  user stopped typing... (like MTN_FINISHED) */
		case 0x0002:	/* MTN_BEGUN */
			protocol_xstate_emit(s, msg_param.uid, EKG_XSTATE_TYPING, 0);
			break;

		case 0x000F:	/* MTN_WINDOW_CLOSED */
		{	/* XXX ?wo? print_info(...) ??? */
		#if 0
			char szFormat[MAX_PATH];
			char szMsg[MAX_PATH];
			char *nick = NickFromHandleUtf(hContact);

			null_snprintf(szMsg, MAX_PATH, ICQTranslateUtfStatic(LPGEN("Contact \"%s\" has closed the message window."), szFormat, MAX_PATH), nick);
			ShowPopUpMsg(hContact, ICQTranslateUtfStatic(LPGEN("ICQ Note"), szFormat, MAX_PATH), szMsg, LOG_NOTE);
			SAFE_FREE((void**)&nick);

			NetLog_Server("%s has closed the message window.", strUID(dwUin, szUID));
			break;
		#endif
		}

		default:
			debug_function("icq_snac_message_mini_typing_notification() uid: %s, UNKNOWN typing!!! (0x%x)\n", msg_param.sender, pkt.typing);
	}
#endif
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
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
