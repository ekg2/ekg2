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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ekg/debug.h>
#include <ekg/recode.h>

#include "icq.h"
#include "misc.h"
#include "icq_caps.h"
#include "icq_const.h"
#include "icq_flap_handlers.h"
#include "icq_snac_handlers.h"


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

	if ((tlvs = icq_unpack_tlvs(&buf, &len, 0))) {
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


void icq_pack_append_nullterm_msg(string_t pkt, const char *msg) {
	    icq_pack_append(pkt, "w", (uint32_t) xstrlen(msg)+1);	// null-terminated msg length
	    if (xstrlen(msg))
		    string_append(pkt, msg);
	    icq_pack_append(pkt, "c", (uint32_t) 0);			// msg terminate
}

void icq_pack_append_rendezvous(string_t pkt, int version, int cookie, int mtype, int mflags, int accept, int priority) {
	icq_pack_append(pkt, "wwiiiiwicw wwiiiccww",
				(uint32_t) 27,		// length of this data segment, always 27
				(uint32_t) version,	// protocol version
				(uint32_t) 0, (uint32_t) 0, (uint32_t) 0, (uint32_t) 0, // PSIG_MESSAGE
				(uint32_t) 0,		// unknown
				(uint32_t) 3,		// client capabilities flag
				(uint32_t) 0, 		// unknown byte
				cookie,			// cookie

				(uint32_t) 14,		// length of this data segment, always 14
				cookie,			// cookie
				(uint32_t) 0, (uint32_t) 0, (uint32_t) 0, // unknown, usually all zeros
				mtype,			// msg type
				mflags,			// msg flags
				(uint32_t) accept,	// status code - accepted
				(uint32_t) priority	// priority ??? XXX
				);
}

static void icq_get_description(session_t *s, char *uin, int status) {
	icq_private_t *j = s->priv;
	string_t pkt, tlv5, rdv;
	uint32_t cookie1=rand(), cookie2=rand();
	uint32_t mtype, cookie = (j->cookie_seq-- && 0x7fff);

	debug_function("icq_get_description() for: %s\n", uin);

	switch (status) {
	    case EKG_STATUS_AWAY:	mtype = MTYPE_AUTOAWAY;	break;
	    case EKG_STATUS_GONE:	mtype = MTYPE_AUTONA;	break;
	    case EKG_STATUS_XA:		mtype = MTYPE_AUTOBUSY;	break;
	    case EKG_STATUS_DND:	mtype = MTYPE_AUTODND;	break;
	    case EKG_STATUS_FFC:	mtype = MTYPE_AUTOFFC;	break;
	    default: return;
	}

	pkt = string_init(NULL);
	icq_pack_append(pkt, "II", cookie1, cookie2);		// cookie
	icq_pack_append(pkt, "W", (uint32_t) 2);		// message type
	icq_pack_append(pkt, "s", uin);

	tlv5 = string_init(NULL);
	icq_pack_append(tlv5, "W", (uint32_t) 0);
	icq_pack_append(tlv5, "II", cookie1, cookie2);		// cookie
	icq_pack_append_cap(tlv5, CAP_SRV_RELAY);		// AIM_CAPS_ICQSERVERRELAY - Client supports channel 2 extended, TLV(0x2711) based messages.
	icq_pack_append(tlv5, "tW", icq_pack_tlv_word(0xA, 1));	// TLV 0x0A: acktype (1 = normal message)
	icq_pack_append(tlv5, "T", icq_pack_tlv(0x0F, NULL, 0));	// TLV 0x0F: unknown

	// RendezvousMessageData
	rdv = string_init(NULL);
	icq_pack_append_rendezvous(rdv, 9, cookie, mtype, MFLAG_AUTO, 1, 1);
	icq_pack_append_nullterm_msg(rdv, "");
	icq_pack_append(tlv5, "T", icq_pack_tlv(0x2711, rdv->str, rdv->len));
	string_free(rdv, 1);

	icq_pack_append(pkt, "T", icq_pack_tlv(0x05, tlv5->str, tlv5->len));
	string_free(tlv5, 1);

	icq_pack_append(pkt, "T", icq_pack_tlv(0x03, NULL, 0));		// empty TLV 3 to get an ack from the server

	icq_makesnac(s, pkt, 0x04, 0x06, 0, 0);
	icq_send_pkt(s, pkt);
}

static void icq_get_user_info(session_t *s, userlist_t *u, struct icq_tlv_list *tlvs, int newstatus) {
	int caps = 0, desc_chg = 0;
	char *descr = NULL;
	icq_tlv_t *t;

	if (!u)
		return;

	debug_function("icq_get_user_info() %s\n", u->uid);

	descr = u->descr ? xstrdup(u->descr) : NULL;

	user_private_item_set_int(u, "idle", 0);
	user_private_item_set_int(u, "status_f", 0);
	user_private_item_set_int(u, "xstatus", 0);

	for (t = tlvs; t; t = t->next) {

		/* Check t->len */
		switch (t->type) {
			case 0x01:
				if (tlv_length_check("icq_get_user_info()", t, 2))
					continue;
				break;
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x0a:
			case 0x0f:
				if (tlv_length_check("icq_get_user_info()", t, 4))
					continue;
				break;
		}
		/* now we've got trusted length */
		switch (t->type) {
			case 0x01: /* User class */
				user_private_item_set_int(u, "class", t->nr);
				break;

			case 0x03: /* Time when client gone online (unix time_t) */
				user_private_item_set_int(u, "online", t->nr);
				break;

			case 0x04: /* idle timer */
			{
				uint16_t idle;
				if ( (idle = t->nr) )
					user_private_item_set_int(u, "idle", time(NULL) - 60*idle);
				break;
			}

			case 0x05: /* Time when this account was registered (unix time_t) */
				user_private_item_set_int(u, "member", t->nr);
				break;

			case 0x06:
			{
				/* User status
				 *
				 * ICQ service presence notifications use user status field which consist
				 * of two parts. First is a various flags (birthday flag, webaware flag,
				 * etc). Second is a user status (online, away, busy, etc) flags.
				 */
				uint16_t status	= t->nr & 0xffff;
				uint16_t flags	= t->nr >> 16;

				user_private_item_set_int(u, "status_f", flags);
				debug_white(" %s status flags=0x%04x status=0x%04x\n", u->uid, flags, status);
				newstatus = icq2ekg_status(status);
				break;
			}

			case 0x0a: /* IP address */
			{
				uint32_t ip;
				if (icq_unpack_nc(t->buf, t->len, "i", &ip)) {
					if (ip)
						user_private_item_set_int(u, "ip", ip);
				}
				break;
			}

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
					uint16_t junk;
				} tlv_c;

				if (!icq_unpack_nc(t->buf, t->len, "IICWIII",
						&tlv_c.ip, &tlv_c.port,
						&tlv_c.tcp_flag, &tlv_c.version,
						&tlv_c.conn_cookie, &tlv_c.web_port,
						&tlv_c.client_features))
				{
					debug_error(" %s TLV(C) corrupted?\n", u->uid);
					continue;
				} else {
					user_private_item_set_int(u, "dcc.ip", tlv_c.ip);
					user_private_item_set_int(u, "dcc.port", tlv_c.port);
					user_private_item_set_int(u, "dcc.flag", tlv_c.tcp_flag);
					user_private_item_set_int(u, "version", tlv_c.version);
					user_private_item_set_int(u, "dcc.cookie", tlv_c.conn_cookie);
					user_private_item_set_int(u, "dcc.web_port", tlv_c.web_port);	// ?wo? do we need it?
					if (t->len >= 12 && icq_unpack_nc(t->buf, t->len, "23 III", &tlv_c.ts1, &tlv_c.ts3, &tlv_c.ts3))
						;
				}
				break;
			}

			case 0x0d: /* Client capabilities list */
			{
				unsigned char *data = t->buf;
				int d_len = t->len;

				if (tlv_length_check("icq_get_user_info()", t, t->len & ~0xF))
					break;
				while (d_len > 0) {
					int cid = icq_cap_id(data);
					if (cid != CAP_UNKNOWN) {
						caps |= 1 << cid;
					} else if ((cid = icq_xstatus_id(data))) {
						user_private_item_set_int(u, "xstatus", cid);
					} else {
						/* ?wo? client id???*/
						debug_error("Unknown cap\n");
						icq_hexdump(DEBUG_ERROR, data, 0x10);
					}
					data  += 0x10;		// capability length
					d_len -= 0x10;
				}
				break;
			}

			case 0x0e: /* AOL users. No values */
				break;

			case 0x0f:
			case 0x10: /* Online time in seconds */
				break;

			case 0x19: /* short caps */
			{
				unsigned char *data = t->buf;
				int d_len = t->len;

				while (d_len > 0) {
					int cid = icq_short_cap_id(data);
					if (cid != CAP_UNKNOWN) {
						caps |= 1 << cid;
					} else {
						/* WTF? */
					}
					data  += 2;		// short capability length
					d_len -= 2;
				}
				break;
			}

			case 0x1d: /* user icon id & hash */
			{
				static char empty_item[0x10] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
				unsigned char *t_data = t->buf;
				int t_len = t->len;

				while (t_len > 0) {

					uint16_t item_type;
					uint8_t item_flags;
					uint8_t item_len;

					if (!icq_unpack(t_data, &t_data, &t_len, "WCC", &item_type, &item_flags, &item_len)) {
						debug_error(" %s TLV(1D) corrupted?\n", u->uid);
						break;
					}

					/* just some validity check */
					if (item_len > t_len)
						item_len = t_len;

					if ((item_type == 0x02) && (item_flags == 4)) {
						/* iChat online message */
						if (item_len>4) {
							char *tmp;
							uint16_t enc;
							icq_unpack_nc(t_data, item_len, "Uw", &tmp, &enc);
							descr = !enc ? ekg_utf8_to_locale_dup(tmp) : xstrdup(tmp);
						}
						desc_chg = 1;
					} else if ((item_type == 0x0e) && (item_len>7)) {
						/* 000E: Custom Status (ICQ6) */
						char *tmp = xstrndup((char *)t_data, item_len);
						if ( !xstrncmp(tmp, "icqmood", 7) && xisdigit(*(tmp+7)) ) {
							int xstatus = atoi(tmp+7) + 1;
							if (xstatus<=XSTATUS_COUNT)
								user_private_item_set_int(u, "xstatus", xstatus);
						}
						xfree(tmp);
					} else if (memcmp(t_data, empty_item, (item_len < 0x10) ? item_len : 0x10)) {
						/* Item types
						 * 	0000: AIM mini avatar
						 * 	0001: AIM/ICQ avatar ID/hash (len 5 or 16 bytes)
						 * 	0002: iChat online message
						 *	0008: ICQ Flash avatar hash (16 bytes)
						 * 	0009: iTunes music store link
						 *	000C: ICQ contact photo (16 bytes)
						 *	000D: ?
						 */

						debug_white(" %s has got avatar: type: %d flags: %d\n", u->uid, item_type, item_flags);
						icq_hexdump(DEBUG_WHITE, t_data, item_len);
						/* XXX, display message, get? do something? */
					}

					t_data += item_len;
					t_len -= item_len;
				}
				break;
			}
			default:
				if (t->len==4)
					debug_warn(" %s Unknown TLV(0x%x) len=4 v=%d (0x%x) (%s)\n", u->uid, t->type, t->nr, t->nr, t->nr?timestamp_time("%Y-%m-%d %H:%M:%S", t->nr):"");
				else 
					debug_error(" %s Unknown TLV(0x%x) len=%d\n", u->uid, t->type, t->len);
		}
	}

	if (desc_chg || (newstatus != u->status)) {
		if (!descr && !desc_chg && u->descr)
			descr = xstrdup(u->descr);

		protocol_status_emit(s, u->uid, newstatus, descr, time(NULL));
	}

	if (!desc_chg) {
		if (u->status == EKG_STATUS_NA) {
			icq_send_snac(s, 0x02, 0x05, NULL, NULL,	/* Request user info */
					"Ws",
					(uint32_t) 1,			/* request type (1 - general info, 2 - short user info, 3 - away message, 4 - client capabilities) */
					u->uid+4);
			user_private_item_set_int(u, "online", 0);
			user_private_item_set_int(u, "last_ip", user_private_item_get_int(u, "ip"));
			user_private_item_set_int(u, "ip", 0);
		} else {
			icq_get_description(s, u->uid+4, u->status);
#if 0
			// XXX ???
			if (u->status == EKG_STATUS_NA) {
				if (user_private_item_get_int(u, "version") < 8) {
					caps &= ~(1<<CAP_SRV_RELAY);
					debug_warn("icq_snac_buddy_online() Forcing simple messages due to compability issues (%s).\n", uid);
				}
				user_private_item_set_int(u, "caps", caps);
				user_private_item_set_int(u, "utf", (caps && (1<<CAP_UTF)) ? 1:0);
			}
#endif
		}
	}

	xfree(descr);
}

SNAC_SUBHANDLER(icq_snac_buddy_online) {
	/*
	 * Handle SNAC(0x3,0xb) -- User online notification
	 *
	 * Server sends this snac when user from your contact list goes online.
	 * Also you'll receive this snac on user status change.
	 */

	struct {
		char *uid;
		uint16_t warning;
		uint16_t count;
	} pkt;

	struct icq_tlv_list *tlvs;
	userlist_t *u;
	char *uid;

	do {
		// Following user info may be repeated more then once

		if (!ICQ_UNPACK(&buf, "uWW", &pkt.uid, &pkt.warning, &pkt.count))
			return -1;

		uid = icq_uid(pkt.uid);
		if (!(u = userlist_find(s, uid)) && config_auto_user_add)
			u = userlist_add(s, uid, uid);

		tlvs = icq_unpack_tlvs(&buf, &len, pkt.count);

		if (!u || !tlvs) {
			if (!u)
				debug_warn("icq_snac_buddy_online() Ignoring online notification from %s\n", uid);
			if (tlvs)
				icq_tlvs_destroy(&tlvs);
			else
				debug_warn("icq_snac_buddy_online() Empty online notification from %s\n", uid);
			xfree(uid);
			continue;
		}

		debug_function("icq_snac_buddy_online() %s\n", uid);

		icq_get_user_info(s, u, tlvs, EKG_STATUS_AVAIL);

		icq_tlvs_destroy(&tlvs);

		xfree(uid);

	} while (len > 0);

	return 0;
}

SNAC_SUBHANDLER(icq_snac_buddy_offline) {
	/* SNAC(03,0C) SRV_USER_OFFLINE User offline notification
	 *
	 * Server send this when user from your contact list goes offline.
	 */
	struct {
		char *uid;
		uint16_t warning;
		uint16_t count;
	} pkt;

	char *uid;
	userlist_t *u;
	struct icq_tlv_list *tlvs;

	debug_function("icq_snac_buddy_offline()\n");

	do {
		if (!ICQ_UNPACK(&buf, "uWW", &pkt.uid, &pkt.warning, &pkt.count))
			return -1;

		uid = icq_uid(pkt.uid);
		u = userlist_find(s, uid);

		tlvs = icq_unpack_tlvs(&buf, &len, pkt.count);
		icq_get_user_info(s, u, tlvs, EKG_STATUS_NA);
		icq_tlvs_destroy(&tlvs);

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
		handler(s, buf, len, data);

	return 0;
}

// vim:syn=c
