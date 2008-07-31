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

SNAC_SUBHANDLER(icq_snac_userlist_error) {
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		return -1;

	if (s->connected == 0)
		icq_session_connected(s);

	icq_snac_error_handler(s, "userlist", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_userlist_reply) {
	debug_error("icq_snac_userlist_reply() XXX\n");

#if 0
	oscar_tlv_chain* chain;

	memset(m_wServerListLimits, -1, sizeof(m_wServerListLimits));
	m_wServerListGroupMaxContacts = 0;
	m_wServerListRecordNameMaxLength = 0xFFFF;

	if (!(chain = readIntoTLVChain(&pBuffer, wBufferLength, 0)))
		return;

	oscar_tlv* pTLV;

	// determine max number of contacts in a group
	m_wServerListGroupMaxContacts = getWordFromChain(chain, 0x0C, 1);
	// determine length limit for server-list item's name
	m_wServerListRecordNameMaxLength = getWordFromChain(chain, 0x06, 1);

	if (pTLV = getTLV(chain, 0x04, 1))
	{ // limits for item types
		int i;
		WORD *pLimits = (WORD*)pTLV->pData;

		for (i = 0; i < pTLV->wLen / 2; i++)
		{
			m_wServerListLimits[i] = (pLimits[i] & 0xFF) << 8 | (pLimits[i] >> 8);

			if (i + 1 >= SIZEOF(m_wServerListLimits)) break;
		}

		NetLog_Server("SSI: Max %d contacts (%d per group), %d groups, %d permit, %d deny, %d ignore items.", m_wServerListLimits[SSI_ITEM_BUDDY], m_wServerListGroupMaxContacts, m_wServerListLimits[SSI_ITEM_GROUP], m_wServerListLimits[SSI_ITEM_PERMIT], m_wServerListLimits[SSI_ITEM_DENY], m_wServerListLimits[SSI_ITEM_IGNORE]);
	}

	disposeChain(&chain);
#endif

	return -3;
}

static int icq_userlist_parse_entry(session_t *s, struct icq_tlv_list *tlvs, const char *name, uint16_t type, uint16_t item_id, uint16_t group) {
	switch (type) {
		case 0x0000:	/* normal; SSI_ITEM_BUDDY */
		{
			icq_tlv_t *t_nick = icq_tlv_get(tlvs, 0x131);
			icq_tlv_t *t_auth = icq_tlv_get(tlvs, 0x66);
			icq_tlv_t *t_comment = icq_tlv_get(tlvs, 0x013C);

			char *comment = (t_comment && t_comment->len) ? xstrndup(t_comment->buf, t_comment->len) : NULL;	/* XXX, recode */
			char *uid = icq_uid(name);
			char *nick = (t_nick && t_nick->len) ? xstrndup(t_nick->buf, t_nick->len) : xstrdup(uid);		/* XXX, recode */

			userlist_t *u;

			if (!(u = userlist_find(s, uid))) 
				u = userlist_add(s, uid, nick);

			debug("[group] %u\n", group);
			
			if (comment)
				debug("[Notes] %s\n", comment);

			if (t_auth)
				debug("[auth] %d\n", 1);


			/* XXX, t_auth */
/*
			if (t_auth) 
				print("icq_auth_subscribe", uid, session_name(s));
 */
			/* XXX, grupy */

			xfree(uid);
			xfree(nick);
			xfree(comment);
			break;
		}

		case 0x0001:	/* ROSTER_TYPE_GROUP; SSI_ITEM_GROUP; */
		{
			debug_error("ROSTER_TYPE_GROUP: %s %u\n", name, group);

			if ((group == 0) && (item_id == 0)) {
				/* list of groups. wTlvType=1, data is TLV(C8) containing list of WORDs which */
				/* is the group ids */
				/* we don't need to use this. Our processing is on-the-fly */
				/* this record is always sent first in the first packet only, */
				break;
			}

			if (group != 0)
			{
				/* wGroupId != 0: a group record */
				if (item_id == 0)
				{ /* no item ID: this is a group */
					/* XXX */
				} else {
					debug_error("icq_userlist_parse_entry() Unhandled ROSTER_TYPE_GROUP wItemID != 0\n");
				}
				break;
			}

			debug_error("icq_userlist_parse_entry() Unhandled ROSTER_TYPE_GROUP\n");
			break;
		}

		case 0x0002: /* SSI_ITEM_PERMIT */
		{
			char *uid = icq_uid(name);
			userlist_t *u;

			if (!(u = userlist_find(s, uid)))
				u = userlist_add(s, uid, NULL);	/* XXX */

			/* XXX */
			xfree(uid);
			break;
		}

		case 0x0003:	/* SSI_ITEM_DENY */
		{
			char *uid = icq_uid(name);
			userlist_t *u;

			if (!(u = userlist_find(s, uid)))
				u = userlist_add(s, uid, NULL); /* XXX */

			/* XXX */
			break;
		}

		case 0x0004: /* SSI_ITEM_VISIBILITY: */ /* My visibility settings */
		{
			/* XXX */
			break;
		}

		case 0x000e: 	/* _IGNORE */
		{
			char *uid = icq_uid(name);
			userlist_t *u;

			if (!(u = userlist_find(s, uid)))
				u = userlist_add(s, uid, NULL);

			/* XXX, dodac do grup? */
			/* XXX */

			xfree(uid);
			break;
		}

		case 0x0009: /* SSI_ITEM_CLIENTDATA */
		{
			if (group == 0) {
				/* ICQ2k ShortcutBar Items */
				/* data is TLV(CD) text */
			}
			break;
		}

		case 0x0013: /* SSI_ITEM_IMPORTTIME */
		{
			if (group == 0)
			{
				/* time our list was first imported */
				/* pszRecordName is "Import Time" */
				/* data is TLV(13) {TLV(D4) {time_t importTime}} */

				/* XXX */
			}
			break;
		}

		case 0x0014: /* SSI_ITEM_BUDDYICON */
		{
			if (group == 0)
			{
				/* our avatar MD5-hash */
				/* pszRecordName is "1" */
				/* data is TLV(D5) hash */
				/* we ignore this, just save the id */
				/* cause we get the hash again after login */

				/* XXX */
			}
			break;
		}

		default:
			 debug_error("icq_userlist_parse_entry() unkown type: %.4x\n", type);
	}
	return 0;
}

SNAC_SUBHANDLER(icq_snac_userlist_roster) {
	struct {
		uint8_t unk1;		/* empty */	/* possible version */
		uint16_t count;		/* COUNT */
		unsigned char *data;
	} pkt;
	int i;

	/* XXX, here we have should check if we send roster request with that ref.... if not, "Unrequested roster packet received.\n" return; */

	if (!ICQ_UNPACK(&pkt.data, "CW", &pkt.unk1, &pkt.count))
		return -1;

	debug_function("icq_snac_userlist_roster() contacts count: %d\n", pkt.count);
	buf = pkt.data;

	for (i = 0; i < pkt.count; i++) {
		char *orgname;
		uint16_t item_id, item_type;
		uint16_t group_id;
		uint16_t tlv_len;
		struct icq_tlv_list *tlvs;
		char *name;

		if (!ICQ_UNPACK(&buf, "UWWWW", &orgname, &group_id, &item_id, &item_type, &tlv_len))
			return -1;

		debug("%s groupID: %u entryID: %u entryTYPE: %u tlvLEN: %u\n", orgname, group_id, item_id, item_type, tlv_len);

		if (len < tlv_len) {
			debug_error("smth bad!\n");
			return -1;
		}

		tlvs = icq_unpack_tlvs(buf, tlv_len, 0);

		name = xstrdup(orgname);		/* XXX, recode (?) */
		icq_userlist_parse_entry(s, tlvs, name, item_type, item_id, group_id);
		xfree(name);

		buf += tlv_len; len -= tlv_len;

		icq_tlvs_destroy(&tlvs);
	}
#warning "icq_snac_userlist_roster XXX, koncowka"

	debug("icq_snac_userlist_roster() left: %u bytes\n", len);

	if (1) {
		/* session_int_set(s, "__roster_retrieved", 1); */

		if (len >= 4) {
			uint32_t last_update;
			string_t str;

			if (!ICQ_UNPACK(&buf, "I", &last_update))
				return -1;

			/* XXX, strftime() format */
			debug("icq_snac_userlist_roster() Last update of server list was (%u) %s\n",
					last_update, timestamp_time("%d/%m/%y %H:%M:%S", last_update));

			/* sendRosterAck() */
			str = string_init(NULL);
			icq_makesnac(s, str, 0x13, 0x07, 0, 0);
			icq_send_pkt(s, str);

			icq_session_connected(s);

		} else
			debug_error("icq_snac_userlist_roster() Last packet missed update time...\n");
		/* XXX */
	} else {
		debug("icq_snac_userlist_roster() Waiting for more packets...");
	}
	return 0;
}

SNAC_SUBHANDLER(SNAC_USR_0E) {
	debug_error("SNAC_USR_0E XXX()\n");
	return -3;
}

SNAC_SUBHANDLER(SNAC_USR_0F) {
	debug_error("SNAC_USR_0F XXX()\n");
	return -3;
}

SNAC_SUBHANDLER(SNAC_USR_09) {
	debug_error("SNAC_USR_09 XXX()\n");
	return -3;
}

SNAC_SUBHANDLER(SNAC_USR_11) {
	debug_white("SNAC_USR_11() Server is modifying contact list\n");
	return 0;
}

SNAC_SUBHANDLER(SNAC_USR_12) {
	debug_white("SNAC_USR_12() End of server modification\n");
	return 0;
}

SNAC_HANDLER(icq_snac_userlist_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_userlist_error; break;
		case 0x03: handler = icq_snac_userlist_reply; break;	/* Miranda: OK */
		case 0x06: handler = icq_snac_userlist_roster; break;	/* Miranda: 1/3 OK */	/* XXX, handleServerCList() */
		case 0x09: handler = SNAC_USR_09; break;
		case 0x0E: handler = SNAC_USR_0E; break;
		case 0x0F: handler = SNAC_USR_0F; break;
		case 0x11: handler = SNAC_USR_11; break;
		case 0x12: handler = SNAC_USR_12; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_userlist_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}

