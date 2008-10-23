static char *build_gg_uid(uint32_t sender) {
	static char buf[80];

	sprintf(buf, "gg:%d", sender);
	return buf;
}

/* stolen from libgadu+gg plugin */
static int gg_status_to_text(uint32_t status, int *descr) {
	if (status & GG_STATUS_FRIENDS_MASK) status -= GG_STATUS_FRIENDS_MASK;
	if (status & GG_STATUS_VOICE_MASK) {
		status -= GG_STATUS_VOICE_MASK;
		debug_error("GG_STATUS_VOICE_MASK!!! set\n");
	}

	*descr = 0;
	switch (status) {
		case GG_STATUS_AVAIL_DESCR:
			*descr = 1;
		case GG_STATUS_AVAIL:
			return EKG_STATUS_AVAIL;

		case GG_STATUS_NOT_AVAIL_DESCR:
			*descr = 1;
		case GG_STATUS_NOT_AVAIL:
			return EKG_STATUS_NA;

		case GG_STATUS_BUSY_DESCR:
			*descr = 1;
		case GG_STATUS_BUSY:
			return EKG_STATUS_AWAY;
				
		case GG_STATUS_INVISIBLE_DESCR:
			*descr = 1;
		case GG_STATUS_INVISIBLE:
			return EKG_STATUS_INVISIBLE;

		case GG_STATUS_BLOCKED:
			return EKG_STATUS_BLOCKED;
	}
	debug_error("gg_status_to_text() last chance: %x\n", status);

	return EKG_STATUS_ERROR;
}

static void sniff_gg_print_message(session_t *s, const connection_t *hdr, uint32_t recpt, msgclass_t type, const char *msg, time_t sent) {
	struct tm *tm_msg;
	char timestamp[100] = { '\0' };
	const char *timestamp_f;

	const char *sender = build_gg_uid(recpt);

	tm_msg = localtime(&sent);
	timestamp_f = format_find((type == EKG_MSGCLASS_CHAT) ? "sent_timestamp" : "chat_timestamp");

	if (timestamp_f[0] && !strftime(timestamp, sizeof(timestamp), timestamp_f, tm_msg))
			xstrcpy(timestamp, "TOOLONG");

	print_window(build_windowip_name(type == EKG_MSGCLASS_CHAT ? hdr->dstip : hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		type == EKG_MSGCLASS_CHAT ? "message" : "sent", 	/* formatka */

		format_user(s, sender),			/* do kogo */
		timestamp, 				/* timestamp */
		msg,					/* wiadomosc */
		get_nickname(s, sender),		/* jego nickname */
		sender,					/* jego uid */
		"");					/* secure */
}

static void sniff_gg_print_status(session_t *s, const connection_t *hdr, uint32_t uin, int status, const char *descr) {
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		ekg_status_label(status, descr, "status_"), /* formatka */

		format_user(s, build_gg_uid(uin)),		/* od */
		NULL, 						/* nickname, realname */
		session_name(s), 				/* XXX! do */
		descr);						/* status */
}

static void sniff_gg_print_new_status(session_t *s, const connection_t *hdr, uint32_t uin, int status, const char *descr) {
	const char *fname = NULL;
	const char *whom;

	if (descr) {
		if (status == EKG_STATUS_AVAIL)			fname = "back_descr";
		if (status == EKG_STATUS_AWAY)			fname = "away_descr";
		if (status == EKG_STATUS_INVISIBLE)		fname = "invisible_descr";
		if (status == EKG_STATUS_NA)			fname = "disconnected_descr";
	} else {
		if (status == EKG_STATUS_AVAIL)			fname = "back";
		if (status == EKG_STATUS_AWAY)			fname = "away";
		if (status == EKG_STATUS_INVISIBLE)		fname = "invisible";
		if (status == EKG_STATUS_NA)			fname = "disconnected";

	}

	if (!fname) {
		debug_error("sniff_gg_print_new_status() XXX bad status: 0x%x\n", status);
		return;
	}

	whom = uin ? format_user(s, build_gg_uid(uin)) : session_name(s);	/* session_name() bad */

	if (descr) {
		if (status == EKG_STATUS_NA) {
			print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
				fname, 					/* formatka */
				descr, whom);

		} else {
			print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
				fname,					/* formatka */
				descr, "", whom);
		}
	} else 
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
				fname,					 /* formatka */
				whom);
}

SNIFF_HANDLER(sniff_print_payload, unsigned) {
	tcp_print_payload((u_char *) pkt, len);
	return 0;
}

SNIFF_HANDLER(sniff_gg_recv_msg, gg_recv_msg) {
	char *msg;

	CHECK_LEN(sizeof(gg_recv_msg))	len -= sizeof(gg_recv_msg);
	msg = ekg_cp_to_locale(xstrndup(pkt->msg_data, len));
		sniff_gg_print_message(s, hdr, pkt->sender, EKG_MSGCLASS_CHAT, msg, pkt->time);
	xfree(msg);
	return 0;
}

SNIFF_HANDLER(sniff_gg_send_msg, gg_send_msg) {
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg))  len -= sizeof(gg_send_msg);
	msg = ekg_cp_to_locale(xstrndup(pkt->msg_data, len));
		sniff_gg_print_message(s, hdr, pkt->recipient, EKG_MSGCLASS_SENT_CHAT, msg, time(NULL));
	xfree(msg);

	return 0;
}

SNIFF_HANDLER(sniff_gg_send_msg_ack, gg_send_msg_ack) {
	const char *format;

	CHECK_LEN(sizeof(gg_send_msg_ack))	len -= sizeof(gg_send_msg_ack);
	
	debug_function("sniff_gg_send_msg_ack() uid:%d %d %d\n", pkt->recipient, pkt->status, pkt->seq);

	switch (pkt->status) {
		/* XXX, implement GG_ACK_BLOCKED, GG_ACK_MBOXFULL */
		case GG_ACK_DELIVERED:		format = "ack_delivered";	break;
		case GG_ACK_QUEUED:		format = "ack_queued";		break;
		case GG_ACK_NOT_DELIVERED:	format = "ack_filtered";	break;
		default:			format = "ack_unknown";
						debug("[sniff,gg] unknown message ack status. consider upgrade\n");
						break;
	}

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			format, 
			build_gg_uid(pkt->recipient));
	return 0;
}

SNIFF_HANDLER(sniff_gg_welcome, gg_welcome) {
	CHECK_LEN(sizeof(gg_welcome))		len -= sizeof(gg_welcome);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_welcome",

			build_hex(pkt->key));
	return 0;
}

SNIFF_HANDLER(sniff_gg_status, gg_status) {
	int status;
	char *descr;
	int has_descr;

	CHECK_LEN(sizeof(gg_status))		len -= sizeof(gg_status);

	status	= gg_status_to_text(pkt->status, &has_descr);
	descr	= has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_status(s, hdr, pkt->uin, status, descr);
	xfree(descr);

	return 0;
}

SNIFF_HANDLER(sniff_gg_new_status, gg_new_status) {
	int status;
	char *descr;
	int has_descr;
	int has_time = 0;

	CHECK_LEN(sizeof(gg_new_status))	len -= sizeof(gg_new_status);

	status	= gg_status_to_text(pkt->status, &has_descr);
	descr	= has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_new_status(s, hdr, 0, status, descr);
	xfree(descr);

	return 0;
}

SNIFF_HANDLER(sniff_gg_status60, gg_status60) {
	uint32_t uin;
	int status;
	char *descr;

	int has_descr = 0;
	int has_time = 0;

	CHECK_LEN(sizeof(gg_status60))		len -= sizeof(gg_status60);

	uin	= pkt->uin & 0x00ffffff;

	status	= gg_status_to_text(pkt->status, &has_descr);
	descr 	= has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_status60",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

	return 0;
}

SNIFF_HANDLER(sniff_gg_login60, gg_login60) {
	int status;
	char *descr;
	int has_descr = 0;
	int has_time = 0;

	CHECK_LEN(sizeof(gg_login60))	len -= sizeof(gg_login60);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login60",

			build_gg_uid(pkt->uin),
			build_hex(pkt->hash));
	
	status = gg_status_to_text(pkt->status, &has_descr);
	descr = has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_new_status(s, hdr, pkt->uin, status, descr);
	xfree(descr);
	return 0;
}

SNIFF_HANDLER(sniff_gg_add_notify, gg_add_remove) {
	CHECK_LEN(sizeof(gg_add_remove));	len -= sizeof(gg_add_remove);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_addnotify",

			build_gg_uid(pkt->uin),
			build_hex(pkt->dunno1));
	return 0;
}

SNIFF_HANDLER(sniff_gg_del_notify, gg_add_remove) {
	CHECK_LEN(sizeof(gg_add_remove));	len -= sizeof(gg_add_remove);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_delnotify",

			build_gg_uid(pkt->uin),
			build_hex(pkt->dunno1));
	return 0;
}

SNIFF_HANDLER(sniff_gg_notify_reply60, gg_notify_reply60) {
	unsigned char *next;

	uint32_t uin;
	int desc_len;
	int has_descr;
	int status;
	char *descr;

	CHECK_LEN(sizeof(gg_notify_reply60));	len -= sizeof(gg_notify_reply60);

	next = pkt->next;

	uin = pkt->uin & 0x00ffffff;

	status = gg_status_to_text(pkt->status, &has_descr);

	if (has_descr) {
		CHECK_LEN(1)
		desc_len = pkt->next[0];
		len--;	next++;

		if (!desc_len)
			debug_error("gg_notify_reply60() has_descr BUT NOT desc_len?\n");

		CHECK_LEN(desc_len)
		len  -= desc_len;
		next += desc_len;
	}

	descr = has_descr ? ekg_cp_to_locale(xstrndup((char *) &pkt->next[1], desc_len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_notify60",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

	if (pkt->uin & 0x40000000)
		debug_error("gg_notify_reply60: GG_HAS_AUDIO_MASK set\n");

	if (pkt->uin & 0x08000000)
		debug_error("gg_notify_reply60: GG_ERA_OMNIX_MASK set\n");

	if (len > 0) {
		debug_error("gg_notify_reply60: again? leftlen: %d\n", len);
		sniff_gg_notify_reply60(s, hdr, (gg_notify_reply60 *) next, len);
	}
	return 0;
}

static const char *sniff_gg_userlist_reply_str(uint8_t type) {
	if (type == GG_USERLIST_PUT_REPLY) 	return "GG_USERLIST_PUT_REPLY";
	if (type == GG_USERLIST_PUT_MORE_REPLY)	return "GG_USERLIST_PUT_MORE_REPLY";
	if (type == GG_USERLIST_GET_REPLY)	return "GG_USERLIST_GET_REPLY";
	if (type == GG_USERLIST_GET_MORE_REPLY)	return "GG_USERLIST_GET_MORE_REPLY";

	debug_error("sniff_gg_userlist_reply_str() unk type: 0x%x\n", type);
	return "unknown";
}

SNIFF_HANDLER(sniff_gg_userlist_reply, gg_userlist_reply) {
	char *data, *datatmp;
	char *dataline;
	CHECK_LEN(sizeof(gg_userlist_reply));	len -= sizeof(gg_userlist_reply);
	
	if (len) {
		debug_error("sniff_gg_userlist_reply() stublen: %d\n", len);
		tcp_print_payload((u_char *) pkt->data, len);
	}

	datatmp = data = len ? ekg_cp_to_locale(xstrndup(pkt->data, len)) : NULL;
	
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_userlist_reply",
		sniff_gg_userlist_reply_str(pkt->type),
		build_hex(pkt->type));

	while ((dataline = split_line(&datatmp))) {
		print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_userlist_data", "REPLY",
			dataline);
	}

	xfree(data);
	return 0;
}

static const char *sniff_gg_userlist_req_str(uint8_t type) {
	if (type == GG_USERLIST_PUT) return "GG_USERLIST_PUT";
	if (type == GG_USERLIST_PUT_MORE) return "GG_USERLIST_PUT_MORE";
	if (type == GG_USERLIST_GET) return "GG_USERLIST_GET";

	debug_error("sniff_gg_userlist_req_str() unk type: 0x%x\n", type);
	return "unknown";
}

SNIFF_HANDLER(sniff_gg_userlist_req, gg_userlist_request) {
	char *data, *datatmp;
	char *dataline;
	CHECK_LEN(sizeof(gg_userlist_request));	len -= sizeof(gg_userlist_request);
	
	if (len) {
		debug_error("sniff_gg_userlist_req() stublen: %d\n", len);
		tcp_print_payload((u_char *) pkt->data, len);
	}

	datatmp = data = len ? ekg_cp_to_locale(xstrndup(pkt->data, len)) : NULL;

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_userlist_req",
		sniff_gg_userlist_req_str(pkt->type),
		build_hex(pkt->type));

	while ((dataline = split_line(&datatmp))) {
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_userlist_data", "REQUEST",
			dataline);
	}

	xfree(data);
	return 0;
}

static const char *sniff_gg_pubdir50_data_str(const char *name) {
	if (!xstrcmp(name, GG_PUBDIR50_UIN)) 		return "GG_PUBDIR50_UIN";
	if (!xstrcmp(name, GG_PUBDIR50_STATUS))		return "GG_PUBDIR50_STATUS";
	if (!xstrcmp(name, GG_PUBDIR50_FIRSTNAME))	return "GG_PUBDIR50_FIRSTNAME";
	if (!xstrcmp(name, GG_PUBDIR50_LASTNAME))	return "GG_PUBDIR50_LASTNAME";
	if (!xstrcmp(name, GG_PUBDIR50_NICKNAME))	return "GG_PUBDIR50_NICKNAME";
	if (!xstrcmp(name, GG_PUBDIR50_BIRTHYEAR))	return "GG_PUBDIR50_BIRTHYEAR";
	if (!xstrcmp(name, GG_PUBDIR50_CITY))		return "GG_PUBDIR50_CITY";
	if (!xstrcmp(name, GG_PUBDIR50_GENDER))		return "GG_PUBDIR50_GENDER";
	if (!xstrcmp(name, GG_PUBDIR50_ACTIVE))		return "GG_PUBDIR50_ACTIVE";
	if (!xstrcmp(name, GG_PUBDIR50_START))		return "GG_PUBDIR50_START";
	if (!xstrcmp(name, GG_PUBDIR50_FAMILYNAME))	return "GG_PUBDIR50_FAMILYNAME";
	if (!xstrcmp(name, GG_PUBDIR50_FAMILYCITY))	return "GG_PUBDIR50_FAMILYCITY";

	debug_error("sniff_gg_pubdir50_data_str() unk name: %s\n", name);

	return name;
}

static const char *sniff_gg_pubdir50_str(uint8_t type) {
	if (type == GG_PUBDIR50_WRITE) return "GG_PUBDIR50_WRITE";
	if (type == GG_PUBDIR50_READ) return "GG_PUBDIR50_READ";
	if (type == GG_PUBDIR50_SEARCH_REQUEST) return "GG_PUBDIR50_SEARCH_REQUEST";
	if (type == GG_PUBDIR50_SEARCH_REPLY) return "GG_PUBDIR50_SEARCH_REPLY";

	debug_error("sniff_gg_pubdir50_req_str() unk type: 0x%x\n", type);
	return "unknown";
}

SNIFF_HANDLER(sniff_gg_pubdir50_reply, gg_pubdir50_reply) {
	CHECK_LEN(sizeof(gg_pubdir50_reply));	len -= sizeof(gg_pubdir50_reply);
	char *data;

	char *attr = NULL;
	char *val  = NULL;

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_pubdir50_reply",
		sniff_gg_pubdir50_str(pkt->type),
		build_hex(pkt->type),
		itoa(pkt->seq));

	data = pkt->data;

	while (len > 0) {
	#if 0
		if (!strcasecmp(field, "nextstart")) {
			res->next = atoi(value);
			num--;
		}
	#endif
		char *thisdata;
		int thislen;

		thisdata = xstrndup(data, len);
		thislen = xstrlen(thisdata);

		if (!attr)
			attr = thisdata;
		else if (!val)
			val = thisdata;

		if (val && attr) {
			print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
				"sniff_gg_pubdir50_reply_data",
				sniff_gg_pubdir50_data_str(attr), val);

			xfree(attr);
			xfree(val);
			val = attr = NULL;
		}

		data += thislen;
		len -= thislen;

		if (len > 0) {		/* skip NUL */
			data++;
			len--;
		}

		if (len > 0 && *data == '\0') {
			print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
				"sniff_gg_pubdir50_reply_data_sep");
			/* XXX, komunikat: nastepna osoba */
			data++;
			len--;
		}
	}

	if (attr)
		debug_error("sniff_gg_pubdir50_reply() attr alone: %s\n", attr);

	xfree(attr);
	xfree(val);

	return 0;
}

SNIFF_HANDLER(sniff_gg_pubdir50_req, gg_pubdir50_request) {
	char *attr = NULL;
	char *val  = NULL;
	char *data;

	CHECK_LEN(sizeof(gg_pubdir50_request));	len -= sizeof(gg_pubdir50_request);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_pubdir50_req",
		sniff_gg_pubdir50_str(pkt->type),
		build_hex(pkt->type),
		itoa(pkt->seq));

	data = pkt->data;

	while (len > 0) {
		char *thisdata;
		int thislen;

		thisdata = xstrndup(data, len);
		thislen = xstrlen(thisdata);

		if (!attr)
			attr = thisdata;
		else if (!val)
			val = thisdata;

		if (val && attr) {
			print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
				"sniff_gg_pubdir50_req_data",
				sniff_gg_pubdir50_data_str(attr), val);

			xfree(attr);
			xfree(val);
			val = attr = NULL;
		}

		data += thislen;
		len -= thislen;

		if (len > 0) {		/* skip NUL */
			data++;
			len--;
		}
	}

	if (attr)
		debug_error("sniff_gg_pubdir50_req() attr alone: %s\n", attr);

	xfree(attr);
	xfree(val);

	return 0;
}

SNIFF_HANDLER(sniff_gg_list_first, gg_notify) {
	CHECK_LEN(sizeof(gg_notify));

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_list", "GG_LIST_FIRST", itoa(len));

	while (len >= sizeof(gg_notify)) {
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1, "sniff_gg_list_data", 
			"GG_LIST_FIRST",
			build_gg_uid(pkt->uin),
			build_hex(pkt->dunno1));

		pkt = (gg_notify *) pkt->data;
		len -= sizeof(gg_notify);
	}

	if (len) {
		debug_error("sniff_gg_list_first() leftlen: %d\n", len);
		tcp_print_payload((u_char *) pkt, len);
	}
	return 0;
}


SNIFF_HANDLER(sniff_gg_list_last, gg_notify) {
	CHECK_LEN(sizeof(gg_notify));

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_list", "GG_LIST_LAST", itoa(len));

	while (len >= sizeof(gg_notify)) {
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s,  EKG_WINACT_MSG, 1, "sniff_gg_list_data", 
			"GG_LIST_LAST",
			build_gg_uid(pkt->uin),
			build_hex(pkt->dunno1));

		pkt = (gg_notify *) pkt->data;
		len -= sizeof(gg_notify);
	}

	if (len) {
		debug_error("sniff_gg_list_last() leftlen: %d\n", len);
		tcp_print_payload((u_char *) pkt, len);
	}

	return 0;
}

SNIFF_HANDLER(sniff_gg_list_empty, gg_notify) {
	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s,  EKG_WINACT_MSG, 1,
		"sniff_gg_list", "GG_LIST_EMPTY", itoa(len));

	if (len) {
		debug_error("sniff_gg_list_empty() LIST EMPTY BUT len: %d (?)\n", len);
		tcp_print_payload((u_char *) pkt, len);
	}
	return 0;
}

SNIFF_HANDLER(sniff_gg_disconnecting, char) {
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_disconnecting");

	if (len) {
		debug_error("sniff_gg_disconnecting() with len: %d\n", len);
		tcp_print_payload((u_char *) pkt, len);
	}
	return 0;
}

/* nie w libgadu */
#define CHECK_PRINT(is, shouldbe) if (is != shouldbe) {\
		if (sizeof(is) == 2)		debug_error("%s() values not match: %s [%.4x != %.4x]\n", __FUNCTION__, #is, is, shouldbe); \
		else if (sizeof(is) == 4)	debug_error("%s() values not match: %s [%.8x != %.8x]\n", __FUNCTION__, #is, is, shouldbe); \
		else 				debug_error("%s() values not match: %s [%x != %x]\n", __FUNCTION__, #is, is, shouldbe);\
	}
	
SNIFF_HANDLER(sniff_gg_dcc7_new_id_request, gg_dcc7_id_request) {
	if (len != sizeof(gg_dcc7_id_request)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	if (pkt->type != GG_DCC7_TYPE_VOICE && pkt->type != GG_DCC7_TYPE_FILE)
		debug_error("sniff_gg_dcc7_new_id_request() type nor VOICE nor FILE -- %.8x\n", pkt->type);
	else	debug("sniff_gg_dcc7_new_id_request() %s CONNECTION\n", pkt->type == GG_DCC7_TYPE_VOICE ? "AUDIO" : "FILE");

	return 0;
}

SNIFF_HANDLER(sniff_gg_dcc7_new_id_reply, gg_dcc7_id_reply) {
	if (len != sizeof(gg_dcc7_id_reply)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	if (pkt->type != GG_DCC7_TYPE_VOICE && pkt->type != GG_DCC7_TYPE_FILE)
		debug_error("sniff_gg_dcc7_new_id_reply() type nor VOICE nor FILE -- %.8x\n", pkt->type);
	else	debug("sniff_gg_dcc7_new_id_reply() %s CONNECTION\n", pkt->type == GG_DCC7_TYPE_VOICE ? "AUDIO" : "FILE");

	debug("sniff_gg_dcc7_new_id_reply() code: %s\n", build_code(pkt->code1));

	return 0;
}

SNIFF_HANDLER(sniff_gg_dcc7_new, gg_dcc7_new) {
	char *fname;
	CHECK_LEN(sizeof(gg_dcc7_new));	len -= sizeof(gg_dcc7_new);
	
	if (len != 0)
		debug_error("sniff_gg_dcc7_new() extra data?\n");

/* print known data: */
	if (pkt->type != GG_DCC7_TYPE_VOICE  && pkt->type != GG_DCC7_TYPE_FILE)
		debug_error("sniff_gg_dcc7_new() unknown dcc request %x\n", pkt->type);
	else	debug("sniff_gg_dcc_new7() REQUEST FOR: %s CONNECTION\n", pkt->type == GG_DCC7_TYPE_VOICE ? "AUDIO" : "FILE");

	if (pkt->type != GG_DCC7_TYPE_FILE) {
		int print_hash = 0;
		int i;

		for (i = 0; i < sizeof(pkt->hash); i++)
			if (pkt->hash[i] != '\0') print_hash = 1;

		if (print_hash) {
			debug_error("sniff_gg_dcc_new7() NOT GG_DCC7_TYPE_FILE, pkt->hash NOT NULL, printing...\n");
			tcp_print_payload((u_char *) pkt->hash, sizeof(pkt->hash));
		}
	}

	tcp_print_payload((u_char *) pkt->filename, sizeof(pkt->filename));	/* tutaj smieci */

	fname = xstrndup((char *) pkt->filename, sizeof(pkt->filename));
	debug("sniff_gg_dcc_new7() code: %s from: %d to: %d fname: %s [%db]\n", 
		build_code(pkt->code1), pkt->uin_from, pkt->uin_to, fname, pkt->size);
	xfree(fname);

	CHECK_PRINT(pkt->dunno1, 0);
	return 0;
}


SNIFF_HANDLER(sniff_gg_dcc7_reject, gg_dcc7_reject) {
	if (len != sizeof(gg_dcc7_reject)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	debug("sniff_gg_dcc7_reject() uid: %d code: %s\n", pkt->uid, build_code(pkt->code1));

	/* XXX, pkt->reason */

	CHECK_PRINT(pkt->reason, !pkt->reason);
	return 0;
}

SNIFF_HANDLER(sniff_gg_dcc7_accept, gg_dcc7_accept) {
	if (len != sizeof(gg_dcc7_accept)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}
	debug("sniff_gg_dcc7_accept() uid: %d code: %s from: %d\n", pkt->uin, build_code(pkt->code1), pkt->seek);

	CHECK_PRINT(pkt->empty, 0);
	return 0;
}

#define GG_DCC_2XXX 0x1f
typedef struct {
	uint32_t uin;			/* uin */
	uint32_t dunno1;		/* XXX */		/* 000003e8 -> transfer wstrzymano? */
	unsigned char code1[8];		/* kod */
	unsigned char ipport[15+1+5];	/* ip <SPACE> port */	/* XXX, what about NUL char? */	/* XXX, not always (ip+port) */
	unsigned char unk[43];		/* large amount of unknown data */
} GG_PACKED gg_dcc_2xx;

SNIFF_HANDLER(sniff_gg_dcc_2xx_in, gg_dcc_2xx) {
	char *ipport;
	if (len != sizeof(gg_dcc_2xx)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	ipport = xstrndup((char *) pkt->ipport, 21);
	debug_error("XXX sniff_gg_dcc_2xx_in() uin: %d ip: %s code: %s\n", pkt->uin, ipport, build_code(pkt->code1));
	xfree(ipport);
	tcp_print_payload((u_char *) pkt->ipport, sizeof(pkt->ipport));
	tcp_print_payload((u_char *) pkt->unk, sizeof(pkt->unk));
	CHECK_PRINT(pkt->dunno1, !pkt->dunno1);
	return 0;
}

SNIFF_HANDLER(sniff_gg_dcc_2xx_out, gg_dcc_2xx) {
	char *ipport;
	if (len != sizeof(gg_dcc_2xx)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	ipport = xstrndup(pkt->ipport, 21);
	debug_error("XXX sniff_gg_dcc_2xx_out() uin: %d ip: %s code: %s\n", pkt->uin, ipport, build_code(pkt->code1));
	xfree(ipport);
	tcp_print_payload((u_char *) pkt->ipport, sizeof(pkt->ipport));
	tcp_print_payload((u_char *) pkt->unk, sizeof(pkt->unk));
	CHECK_PRINT(pkt->dunno1, !pkt->dunno1);
	return 0;
}

#define GG_DCC_3XXX 0x24
typedef struct {
	unsigned char code1[8];	/* code 1 */
	uint32_t dunno1;	/* 04 00 00 00 */
	uint32_t dunno2;
	uint32_t dunno3;
} GG_PACKED gg_dcc_3xx;

SNIFF_HANDLER(sniff_gg_dcc_3xx_out, gg_dcc_3xx) {
	if (len != sizeof(gg_dcc_3xx)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	debug_error("XXX sniff_gg_dcc_3xx_out() code1: %s\n", build_code(pkt->code1));
	CHECK_PRINT(pkt->dunno1, htonl(0x04000000));
	CHECK_PRINT(pkt->dunno2, !pkt->dunno2);
	CHECK_PRINT(pkt->dunno3, !pkt->dunno3);
	return 0;
}

#define GG_DCC_4XXX 0x25
typedef struct {
	unsigned char code1[8];
} GG_PACKED gg_dcc_4xx_in;

SNIFF_HANDLER(sniff_gg_dcc_4xx_in, gg_dcc_4xx_in) {
	if (len != sizeof(gg_dcc_4xx_in)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}
	debug_error("XXX sniff_gg_dcc_4xx_in() code: %s\n", build_code(pkt->code1));
	return 0;
}

typedef struct {
	unsigned char code1[8];
	uint32_t uin1;
	uint32_t uin2;
} GG_PACKED gg_dcc_4xx_out;

SNIFF_HANDLER(sniff_gg_dcc_4xx_out, gg_dcc_4xx_out) {
	if (len != sizeof(gg_dcc_4xx_out)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}
	debug_error("XXX sniff_gg_dcc_4xx_out() uin1: %d uin2: %d code: %s\n", pkt->uin1, pkt->uin2, build_code(pkt->code1));
	return 0;
}

SNIFF_HANDLER(sniff_gg_login70, gg_login70) {
	int status;
	char *descr;
	int has_time = 0;	/* XXX */
	int has_descr = 0;
	int print_payload = 0;
	int i; 

	int sughash_len;

	CHECK_LEN(sizeof(gg_login70));	len -= sizeof(gg_login70);

	if (pkt->hash_type == GG_LOGIN_HASH_GG32) {
		sughash_len = 4;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login70_hash",

			build_gg_uid(pkt->uin),
			build_hex(pkt->hash_type));

	} else if (pkt->hash_type == GG_LOGIN_HASH_SHA1) {
		sughash_len = 20;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login70_sha1",

			build_gg_uid(pkt->uin),
			build_sha1(pkt->hash));
	} else {
		sughash_len = 0;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login70_unknown",

			build_gg_uid(pkt->uin), build_hex(pkt->hash_type));
	}


	status = gg_status_to_text(pkt->status, &has_descr);
	descr = has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_new_status(s, hdr, pkt->uin, status, descr);
	xfree(descr);
	
	debug_error("sniff_gg_login70() XXX ip %d:%d\n", pkt->external_ip, pkt->external_port);

	CHECK_PRINT(pkt->dunno1, 0x00);
	CHECK_PRINT(pkt->dunno2, 0xbe);

	for (i = sughash_len; i < sizeof(pkt->hash); i++)
		if (pkt->hash[i] != 0) {
			print_payload = 1;
			break;
		}

	if (print_payload) {
		tcp_print_payload((u_char *) pkt->hash, sizeof(pkt->hash));
		print_window(build_windowip_name(hdr->srcip), s, EKG_WINACT_MSG, 1,
			"generic_error", "gg_login70() print_payload flag set, see debug");
	}
	return 0;
}

SNIFF_HANDLER(sniff_gg_notify_reply77, gg_notify_reply77) {
	unsigned char *next;

	uint32_t uin;
	int desc_len;
	int has_descr;
	int status;
	char *descr;

	CHECK_LEN(sizeof(gg_notify_reply77));	len -= sizeof(gg_notify_reply77);

	CHECK_PRINT(pkt->dunno2, 0x00);
	CHECK_PRINT(pkt->dunno1, 0x00);

	next = pkt->next;

	uin = pkt->uin & 0x00ffffff;

	status = gg_status_to_text(pkt->status, &has_descr);

	if (has_descr) {
		CHECK_LEN(1)
		desc_len = pkt->next[0];
		len--;	next++;

		if (!desc_len)
			debug_error("gg_notify_reply77() has_descr BUT NOT desc_len?\n");

		CHECK_LEN(desc_len)
		len  -= desc_len;
		next += desc_len;
	}

	descr = has_descr ? ekg_cp_to_locale(xstrndup((char *) &pkt->next[1], desc_len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_notify77",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

#if 0
	if (pkt->uin & 0x40000000)
		debug_error("gg_notify_reply60: GG_HAS_AUDIO_MASK set\n");

	if (pkt->uin & 0x08000000)
		debug_error("gg_notify_reply60: GG_ERA_OMNIX_MASK set\n");
#endif
	if (len > 0) {
		debug_error("gg_notify_reply77: again? leftlen: %d\n", len);
		sniff_gg_notify_reply77(s, hdr, (gg_notify_reply77 *) next, len);
	}
	return 0;
}


SNIFF_HANDLER(sniff_gg_status77, gg_status77) {
	uint32_t uin;
	int status;
	int has_descr;
	char *descr;

	uint32_t dunno2;
	uint8_t uinflag;

	CHECK_LEN(sizeof(gg_status77)); len -= sizeof(gg_status77);

	uin	= pkt->uin & 0x00ffffff;

	uinflag = pkt->uin >> 24;
	dunno2	= pkt->dunno2;

	if (dunno2 & GG_STATUS_VOICE_MASK) dunno2 -= GG_STATUS_VOICE_MASK;

	CHECK_PRINT(uinflag, 0x00);
	CHECK_PRINT(pkt->dunno1, 0x00);

	CHECK_PRINT(dunno2, 0x00);

	status	= gg_status_to_text(pkt->status, &has_descr);
	descr 	= has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_status77",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

	return 0;
}

#define GG_LOGIN80 0x29

typedef struct {
	uint32_t uin;			/* mój numerek */
	uint8_t hash_type;		/* rodzaj hashowania hasła */
	uint8_t hash[64];		/* hash hasła dopełniony zerami */
	uint32_t status;		/* status na dzień dobry */
	uint32_t version;		/* moja wersja klienta */
	uint8_t dunno1;			/* 0x00 */
	uint32_t local_ip;		/* mój adres ip */
	uint16_t local_port;		/* port, na którym słucham */
	uint32_t external_ip;		/* zewnętrzny adres ip (???) */
	uint16_t external_port;		/* zewnętrzny port (???) */
	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB */
	uint8_t dunno2;			/* 0x64 */
	char status_data[];
} GG_PACKED gg_login80;	
	/* like gg_login70, pkt->dunno2 diff [0xbe vs 0x64] */

SNIFF_HANDLER(sniff_gg_login80, gg_login80) {
	int status;
	char *descr;
	int has_time = 0;	/* XXX */
	int has_descr = 0;
	int print_payload = 0;
	int i; 

	int sughash_len;

	CHECK_LEN(sizeof(gg_login80));	len -= sizeof(gg_login80);

	if (pkt->hash_type == GG_LOGIN_HASH_GG32) {	/* untested */
		sughash_len = 4;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login80_hash",

			build_gg_uid(pkt->uin),
			build_hex(pkt->hash_type));

	} else if (pkt->hash_type == GG_LOGIN_HASH_SHA1) {
		sughash_len = 20;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login80_sha1",

			build_gg_uid(pkt->uin),
			build_sha1(pkt->hash));
	} else {
		sughash_len = 0;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
			"sniff_gg_login80_unknown",

			build_gg_uid(pkt->uin), build_hex(pkt->hash_type));
	}


	status = gg_status_to_text(pkt->status, &has_descr);
	descr = has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_new_status(s, hdr, pkt->uin, status, descr);
	xfree(descr);
	
	debug_error("sniff_gg_login80() XXX ip %d:%d\n", pkt->external_ip, pkt->external_port);

	CHECK_PRINT(pkt->dunno1, 0x00);
	CHECK_PRINT(pkt->dunno2, 0x64);

	for (i = sughash_len; i < sizeof(pkt->hash); i++)
		if (pkt->hash[i] != 0) {
			print_payload = 1;
			break;
		}

	if (print_payload) {
		tcp_print_payload((u_char *) pkt->hash, sizeof(pkt->hash));
		print_window(build_windowip_name(hdr->srcip), s, EKG_WINACT_MSG, 1,
			"generic_error", "gg_login80() print_payload flag set, see debug");
	}
	return 0;
}

#define GG_NOTIFY_REPLY80 0x2b

typedef struct {
	uint32_t uin;			/* [gg_notify_reply60] numerek plus flagi w MSB */
	uint8_t status;			/* [gg_notify_reply60] status danej osoby */
	uint32_t remote_ip;		/* [XXX] adres ip delikwenta */
	uint16_t remote_port;		/* [XXX] port, na którym słucha klient */
	uint8_t version;		/* [gg_notify_reply60] wersja klienta */
	uint8_t image_size;		/* [gg_notify_reply60] maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	uint32_t dunno2;		/* 0x00000000 */
	unsigned char next[];		/* [like gg_notify_reply60] nastepny (gg_notify_reply77), lub DLUGOSC_OPISU+OPIS + nastepny (gg_notify_reply77) */
} GG_PACKED gg_notify_reply80;
	/* identiko z gg_notify_reply77 */

SNIFF_HANDLER(sniff_gg_notify_reply80, gg_notify_reply80) {
	unsigned char *next;

	uint32_t uin;
	int desc_len;
	int has_descr;
	int status;
	char *descr;

	CHECK_LEN(sizeof(gg_notify_reply80));	len -= sizeof(gg_notify_reply80);

	CHECK_PRINT(pkt->dunno2, 0x00);
	CHECK_PRINT(pkt->dunno1, 0x00);

	next = pkt->next;

	uin = pkt->uin & 0x00ffffff;

	status = gg_status_to_text(pkt->status, &has_descr);

	if (has_descr) {
		CHECK_LEN(1)
		desc_len = pkt->next[0];
		len--;	next++;

		if (!desc_len)
			debug_error("gg_notify_reply80() has_descr BUT NOT desc_len?\n");

		CHECK_LEN(desc_len)
		len  -= desc_len;
		next += desc_len;
	}

	descr = has_descr ? ekg_cp_to_locale(xstrndup((char *) &pkt->next[1], desc_len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_notify80",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

	if (len > 0) {
		debug_error("gg_notify_reply80: again? leftlen: %d\n", len);
		sniff_gg_notify_reply77(s, hdr, (gg_notify_reply77 *) next, len);
	}
	return 0;
}

#define GG_STATUS80 0x2a

typedef struct {
	uint32_t uin;			/* [gg_status60, gg_status77] numerek plus flagi w MSB [XXX?] */
	uint8_t status;			/* [gg_status60, gg_status77] status danej osoby */
	uint32_t remote_ip;		/* [XXX] adres ip delikwenta */
	uint16_t remote_port;		/* [XXX] port, na którym słucha klient */
	uint8_t version;		/* [gg_status60] wersja klienta */
	uint8_t image_size;		/* [gg_status60] maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x64 lub 0x00 */
	uint32_t dunno2;		/* 0x00 */
	char status_data[];
} GG_PACKED gg_status80;

SNIFF_HANDLER(sniff_gg_status80, gg_status80) {
	uint32_t uin;
	int status;
	int has_descr;
	char *descr;

	uint32_t dunno2;
	uint8_t uinflag;

	CHECK_LEN(sizeof(gg_status80)); len -= sizeof(gg_status80);

	uin	= pkt->uin & 0x00ffffff;

	uinflag = pkt->uin >> 24;
	dunno2	= pkt->dunno2;

	if (dunno2 & GG_STATUS_VOICE_MASK) dunno2 -= GG_STATUS_VOICE_MASK;

	CHECK_PRINT(uinflag, 0x00);
	CHECK_PRINT(pkt->dunno1, 0x64);
	/* 23:12:31 sniff_gg_status80() values not match: pkt->dunno1 [0 != 64] */

	CHECK_PRINT(dunno2, 0x00);

	status	= gg_status_to_text(pkt->status, &has_descr);
	descr 	= has_descr ? ekg_cp_to_locale(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_status80",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

	return 0;
}

SNIFF_HANDLER(sniff_gg_recv_msg80, gg_recv_msg80) {
	/* XXX, like sniff_gg_send_msg80() */
}

SNIFF_HANDLER(sniff_gg_send_msg80, gg_send_msg80) {
	int orglen = len;
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg80))  len -= sizeof(gg_send_msg80);

	tcp_print_payload((unsigned char *) pkt->html_data, len);

	if (pkt->offset_plain < orglen) 
		tcp_print_payload(((unsigned char *) pkt) + pkt->offset_plain, orglen - pkt->offset_plain);
	if (pkt->offset_attr < orglen) 
		tcp_print_payload(((unsigned char *) pkt) + pkt->offset_attr, orglen - pkt->offset_attr);

	if (pkt->offset_plain < orglen) {
		msg = ekg_cp_to_locale(xstrndup(((char *) pkt) + pkt->offset_plain, orglen - pkt->offset_plain));
			sniff_gg_print_message(s, hdr, pkt->recipient, EKG_MSGCLASS_SENT_CHAT, msg, time(NULL));
		xfree(msg);
	}

	return 0;
}

static const struct {
	uint32_t 	type;
	char 		*sname;
	pkt_way_t 	way;
	sniff_handler_t handler;
	int 		just_print;

} sniff_gg_callbacks[] = {
	{ GG_WELCOME,	"GG_WELCOME",	SNIFF_INCOMING, (void *) sniff_gg_welcome, 0},
	{ GG_LOGIN_OK,	"GG_LOGIN_OK",	SNIFF_INCOMING, (void *) NULL, 1}, 
	{ GG_SEND_MSG,	"GG_SEND_MSG",	SNIFF_OUTGOING, (void *) sniff_gg_send_msg, 0},
	{ GG_RECV_MSG,	"GG_RECV_MSG",	SNIFF_INCOMING, (void *) sniff_gg_recv_msg, 0},
	{ GG_SEND_MSG_ACK,"GG_MSG_ACK",	SNIFF_INCOMING, (void *) sniff_gg_send_msg_ack, 0}, 
	{ GG_STATUS, 	"GG_STATUS",	SNIFF_INCOMING, (void *) sniff_gg_status, 0},
	{ GG_NEW_STATUS,"GG_NEW_STATUS",SNIFF_OUTGOING, (void *) sniff_gg_new_status, 0},
	{ GG_PING,	"GG_PING",	SNIFF_OUTGOING,	(void *) NULL, 0},
	{ GG_PONG,	"GG_PONG",	SNIFF_INCOMING, (void *) NULL, 0},
	{ GG_STATUS60,	"GG_STATUS60",	SNIFF_INCOMING, (void *) sniff_gg_status60, 0},
	{ GG_NEED_EMAIL,"GG_NEED_EMAIL",SNIFF_INCOMING, (void *) NULL, 0},		/* XXX */
	{ GG_LOGIN60,	"GG_LOGIN60",	SNIFF_OUTGOING, (void *) sniff_gg_login60, 0},

	{ GG_ADD_NOTIFY,	"GG_ADD_NOTIFY",	SNIFF_OUTGOING, (void *) sniff_gg_add_notify, 0},
	{ GG_REMOVE_NOTIFY,	"GG_REMOVE_NOTIFY", 	SNIFF_OUTGOING, (void *) sniff_gg_del_notify, 0},
	{ GG_NOTIFY_REPLY60,	"GG_NOTIFY_REPLY60",	SNIFF_INCOMING, (void *) sniff_gg_notify_reply60, 0}, 

	{ GG_LIST_EMPTY,	"GG_LIST_EMPTY",	SNIFF_OUTGOING, (void *) sniff_gg_list_empty, 0},
	{ GG_NOTIFY_FIRST,	"GG_NOTIFY_FIRST",	SNIFF_OUTGOING, (void *) sniff_gg_list_first, 0},
	{ GG_NOTIFY_LAST,	"GG_NOTIFY_LAST",	SNIFF_OUTGOING, (void *) sniff_gg_list_last, 0},
	{ GG_LOGIN70,		"GG_LOGIN70",		SNIFF_OUTGOING, (void *) sniff_gg_login70, 0},

	{ GG_USERLIST_REQUEST,	"GG_USERLIST_REQUEST",	SNIFF_OUTGOING, (void *) sniff_gg_userlist_req, 0},
	{ GG_USERLIST_REPLY,	"GG_USERLIST_REPLY",	SNIFF_INCOMING, (void *) sniff_gg_userlist_reply, 0},

	{ GG_PUBDIR50_REPLY,	"GG_PUBDIR50_REPLY",	SNIFF_INCOMING, (void *) sniff_gg_pubdir50_reply, 0},
	{ GG_PUBDIR50_REQUEST,	"GG_PUBDIR50_REQUEST",	SNIFF_OUTGOING, (void *) sniff_gg_pubdir50_req, 0},
	{ GG_DISCONNECTING,	"GG_DISCONNECTING",	SNIFF_INCOMING, (void *) sniff_gg_disconnecting, 0},

	{ GG_NOTIFY_REPLY77,	"GG_NOTIFY_REPLY77",	SNIFF_INCOMING, (void *) sniff_gg_notify_reply77, 0},
	{ GG_STATUS77,		"GG_STATUS77",		SNIFF_INCOMING, (void *) sniff_gg_status77, 0},

#define GG_NEW_STATUS80 0x28
	{ GG_LOGIN80,		"GG_LOGIN80",		SNIFF_OUTGOING, (void *) sniff_gg_login80, 0},			/* XXX, UTF-8 */
	{ GG_NEW_STATUS80, 	"GG_NEW_STATUS80",	SNIFF_OUTGOING, (void *) sniff_gg_new_status, 0},		/* XXX, UTF-8 */
	{ GG_NOTIFY_REPLY80,	"GG_NOTIFY_REPLY80",	SNIFF_INCOMING, (void *) sniff_gg_notify_reply80, 0},		/* XXX, UTF-8 */
	{ GG_STATUS80,	   	"GG_STATUS80",		SNIFF_INCOMING, (void *) sniff_gg_status80, 0},			/* XXX, UTF-8 */
	{ GG_RECV_MSG80,	"GG_RECV_MSG80",	SNIFF_INCOMING, (void *) sniff_gg_recv_msg80, 0},		/* XXX, UTF-8 */
	{ GG_SEND_MSG80,	"GG_SEND_MSG80",	SNIFF_OUTGOING, (void *) sniff_gg_send_msg80, 0},		/* XXX, UTF-8 */

/* pakiety [nie] w libgadu: [czesc mozliwie ze nieaktualna] */
	{ GG_DCC7_NEW,		"GG_DCC7_NEW",		SNIFF_INCOMING, (void *) sniff_gg_dcc7_new, 0}, 
	{ GG_DCC7_NEW,		"GG_DCC7_NEW",		SNIFF_OUTGOING, (void *) sniff_gg_dcc7_new, 0}, 
	{ GG_DCC7_ID_REPLY,	"GG_DCC7_ID_REPLY",	SNIFF_INCOMING, (void *) sniff_gg_dcc7_new_id_reply, 0},
	{ GG_DCC7_ID_REQUEST,	"GG_DCC7_ID_REQUEST",	SNIFF_OUTGOING, (void *) sniff_gg_dcc7_new_id_request, 0},
	{ GG_DCC7_REJECT,	"GG_DCC7_REJECT",	SNIFF_INCOMING, (void *) sniff_gg_dcc7_reject, 0},
	{ GG_DCC7_REJECT,	"GG_DCC7_REJECT",	SNIFF_OUTGOING, (void *) sniff_gg_dcc7_reject, 0},
/* unknown, 0x21 */
	{ GG_DCC_ACCEPT,	"GG_DCC_ACCEPT",	SNIFF_INCOMING, (void *) sniff_gg_dcc7_accept, 0}, 
	{ GG_DCC_ACCEPT,	"GG_DCC_ACCEPT",	SNIFF_OUTGOING, (void *) sniff_gg_dcc7_accept, 0}, 
/* unknown, 0x1f */
	{ GG_DCC_2XXX,		"GG_DCC_2XXX",		SNIFF_INCOMING, (void *) sniff_gg_dcc_2xx_in, 0},
	{ GG_DCC_2XXX,		"GG_DCC_2XXX",		SNIFF_OUTGOING, (void *) sniff_gg_dcc_2xx_out, 0},
/* unknown, 0x24 */
	{ GG_DCC_3XXX,		"GG_DCC_3XXX",		SNIFF_OUTGOING, (void *) sniff_gg_dcc_3xx_out, 0},
/* unknown, 0x25 */
	{ GG_DCC_4XXX,		"GG_DCC_4XXX",		SNIFF_INCOMING, (void *) sniff_gg_dcc_4xx_in, 0},
	{ GG_DCC_4XXX,		"GG_DCC_4XXX",		SNIFF_OUTGOING, (void *) sniff_gg_dcc_4xx_out, 0},

	{ -1,		NULL,		-1,		(void *) NULL, 0},
};

SNIFF_HANDLER(sniff_gg, gg_header) {
	int i;
	int handled = 0;
	pkt_way_t way = SNIFF_OUTGOING;
	int ret = 0;

//	tcp_print_payload((u_char *) pkt, len);

	CHECK_LEN(sizeof(gg_header)) 	len -= sizeof(gg_header);
	CHECK_LEN(pkt->len)

	/* XXX, check direction!!!!!111, in better way: */
	if (!xstrncmp(inet_ntoa(hdr->srcip), "91.197.13.41", 7))
		way = SNIFF_INCOMING;

	/* XXX, jesli mamy podejrzenia ze to nie jest pakiet gg, to wtedy powinnismy zwrocic -2 i pozwolic zeby inni za nas to przetworzyli */
	for (i=0; sniff_gg_callbacks[i].sname; i++) {
		if (sniff_gg_callbacks[i].type == pkt->type && sniff_gg_callbacks[i].way == way) {
			debug("sniff_gg() %s [%d,%d,%db] %s\n", sniff_gg_callbacks[i].sname, pkt->type, way, pkt->len, inet_ntoa(way ? hdr->dstip : hdr->srcip));
			if (sniff_gg_callbacks[i].handler) 
				sniff_gg_callbacks[i].handler(s, hdr, (unsigned char *) pkt->data, pkt->len);

			handled = 1;
		}
	}
	if (!handled) {
		debug_error("sniff_gg() UNHANDLED pkt type: %x way: %d len: %db\n", pkt->type, way, pkt->len);
		tcp_print_payload((u_char *) pkt->data, pkt->len);
	}

	if (len > pkt->len) {
		debug_error("sniff_gg() next packet?\n");
		ret = sniff_gg(s, hdr, (gg_header *) (pkt->data + pkt->len), len - pkt->len);
		if (ret < 0) ret = 0;
	}
	return (sizeof(gg_header) + pkt->len) + ret;
}

#undef CHECK_PRINT

// vim:syn=c
