/*
 *  (C) Copyright 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

/* some functions are copied from ekg2's gg plugin/other ekg2's plugins/libgadu 
 * 	there're copyrighted under GPL-2 */

#include "ekg2-config.h"

#include <stdio.h>
#include <string.h>

#include <pcap.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ekg/debug.h>

#include <ekg/plugins.h>
#include <ekg/commands.h>
#include <ekg/vars.h>
#include <ekg/userlist.h>

#include <ekg/commands.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>

#include <ekg/queries.h>
#include <ekg/xmalloc.h>
#include <ekg/protocol.h>

#include "sniff_ip.h"
#include "sniff_gg.h"

static int sniff_theme_init();
PLUGIN_DEFINE(sniff, PLUGIN_PROTOCOL, sniff_theme_init);

#define SNAPLEN 2000
#define PROMISC 0

#define GET_DEV(s) ((pcap_t *) ((session_t *) s)->priv)

typedef struct {
	struct in_addr srcip;
	uint16_t srcport;

	struct in_addr dstip;
	uint16_t dstport;
} connection_t;

static char *build_code(const unsigned char *code) {
	static char buf[100];

	sprintf(buf, "%.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x", 
		code[0], code[1], code[2], code[3],
		code[4], code[5], code[6], code[7]);

	return buf;
}

static char *build_sha1(const unsigned char *digest) {
	static char result[40];
	int i;

	for (i = 0; i < 20; i++)
		sprintf(result + (i * 2), "%.2x", digest[i]);

	return &result[0];
}

static char *build_hex(uint32_t hex) {
	static char buf[20];

	sprintf(buf, "0x%x", hex);
	return buf;
}

static char *build_gg_uid(uint32_t sender) {
	static char buf[80];

	sprintf(buf, "gg:%d", sender);
	return buf;
}

static char *inet_ntoa(struct in_addr ip) {
	static char bufs[10][16];
	static int index = 0;
	char *tmp = bufs[index++];

	if (index > 9)
		index = 0;
	
	snprintf(tmp, 16, "%d.%d.%d.%d", (ip.s_addr) & 0xff, (ip.s_addr >> 8) & 0xff, (ip.s_addr >> 16) & 0xff, (ip.s_addr >> 24) & 0xff);
	return tmp;
}

static char *build_windowip_name(struct in_addr ip) {
	static char buf[50];

	sprintf(buf, "sniff:%s", inet_ntoa(ip));
	return buf;
}

/* XXX, make it only session-visible */
static list_t tcp_connections;

/* XXX, sniff_tcp_close_connection(connection_t *) */

static connection_t *sniff_tcp_find_connection(const struct iphdr *ip, const struct tcphdr *tcp) {
#if 0
	connection_t *d;
	list_t l;

	for (l = tcp_connections; l; l = l->next) {
		connection_t *c = l->data;

		if (	c->srcip.s_addr == ip->ip_src.s_addr && c->srcport == ntohs(tcp->th_sport) &&
			c->dstip.s_addr == ip->ip_dst.s_addr && c->dstport == ntohs(tcp->th_dport)) 
				return c;

		if (	c->srcip.s_addr == ip->ip_dst.s_addr && c->srcport == ntohs(tcp->th_dport) &&
			c->dstip.s_addr == ip->ip_src.s_addr && c->dstport == ntohs(tcp->th_sport))
				return c;
	}

	d	= xmalloc(sizeof(connection_t));

	d->srcip	= ip->ip_src;
	d->srcport	= ntohs(tcp->th_sport);
	
	d->dstip	= ip->ip_dst;
	d->dstport	= ntohs(tcp->th_dport);

	list_add(&tcp_connections, d, 0);
#endif
	static connection_t d;
	
	d.srcip		= ip->ip_src;
	d.srcport	= ntohs(tcp->th_sport);

	d.dstip		= ip->ip_dst;
	d.dstport	= ntohs(tcp->th_dport);
	return &d;
}

/* stolen from libgadu+gg plugin */
static int gg_status_to_text(uint32_t status, int *descr) {
#define GG_STATUS_NOT_AVAIL 0x0001		/* niedostępny */
#define GG_STATUS_NOT_AVAIL_DESCR 0x0015	/* niedostępny z opisem (4.8) */
#define GG_STATUS_AVAIL 0x0002			/* dostępny */
#define GG_STATUS_AVAIL_DESCR 0x0004		/* dostępny z opisem (4.9) */
#define GG_STATUS_BUSY 0x0003			/* zajęty */
#define GG_STATUS_BUSY_DESCR 0x0005		/* zajęty z opisem (4.8) */
#define GG_STATUS_INVISIBLE 0x0014		/* niewidoczny (4.6) */
#define GG_STATUS_INVISIBLE_DESCR 0x0016	/* niewidoczny z opisem (4.9) */
#define GG_STATUS_BLOCKED 0x0006		/* zablokowany */

#define GG_STATUS_FRIENDS_MASK 0x8000		/* tylko dla znajomych (4.6) */
#define GG_STATUS_VOICE_MASK 0x20000		/* czy ma wlaczone audio (7.7) */
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

static const unsigned char cp_to_iso_table[] = {
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',
	 '?',  '?', 0xa9,  '?', 0xa6, 0xab, 0xae, 0xac,
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',
	 '?',  '?', 0xb9,  '?', 0xb6, 0xbb, 0xbe, 0xbc,
	0xa0, 0xb7, 0xa2, 0xa3, 0xa4, 0xa1,  '?', 0xa7,
	0xa8,  '?', 0xaa,  '?',  '?', 0xad,  '?', 0xaf,
	0xb0,  '?', 0xb2, 0xb3, 0xb4,  '?',  '?',  '?',
	0xb8, 0xb1, 0xba,  '?', 0xa5, 0xbd, 0xb5, 0xbf,
};

static unsigned char *gg_cp_to_iso(unsigned char *buf) {
	unsigned char *tmp = buf;

	if (!buf)
		return NULL;

	while (*buf) {
		if (*buf >= 0x80 && *buf < 0xC0)
			*buf = cp_to_iso_table[*buf - 0x80];

		buf++;
	}
	return tmp;
}

static void tcp_print_payload(u_char *payload, size_t len) {
	#define MAX_BYTES_PER_LINE 16
        int offset = 0;

	while (1) {
		int display_len;
		int i;

		if (len <= 0) 
			break;
		
		if (len > MAX_BYTES_PER_LINE)
			display_len = MAX_BYTES_PER_LINE;
		else	display_len = len;
	
	/* offset */
        	debug_iorecv("\t0x%.4x  ", offset);
	/* hexdump */
		for(i = 0; i < MAX_BYTES_PER_LINE; i++) {
			if (i < display_len)
				debug_iorecv("%.2x ", payload[i]);
			else	debug_iorecv("   ");
		}
	/* seperate */
		debug_iorecv("   ");

	/* asciidump if printable, else '.' */
		for(i = 0; i < display_len; i++)
			debug_iorecv("%c", isprint(payload[i]) ? payload[i] : '.');
		debug_iorecv("\n");

		payload	+= display_len;
		offset	+= display_len;
		len 	-= display_len;
	}
}

static char *tcp_print_flags(u_char tcpflag) {
	static char buf[60];

	buf[0] = 0;
	if (tcpflag & TH_FIN) xstrcat(buf, "FIN+");
	if (tcpflag & TH_SYN) xstrcat(buf, "SYN+");
	if (tcpflag & TH_RST) xstrcat(buf, "RST+");
	if (tcpflag & TH_PUSH) xstrcat(buf, "PUSH+");
	if (tcpflag & TH_ACK) xstrcat(buf, "ACK+");
	if (tcpflag & TH_URG) xstrcat(buf, "UGE+");
	if (tcpflag & TH_ECE) xstrcat(buf, "ECE+");
	if (tcpflag & TH_CWR) xstrcat(buf, "CWR+");

	if (buf[0])
		buf[xstrlen(buf)-1] = 0;
	
	return buf;
}

/*  ****************************************************** */
static void sniff_gg_print_message(session_t *s, const connection_t *hdr, uint32_t recpt, enum msgclass_t type, const char *msg, time_t sent) {
	struct tm *tm_msg;
	char timestamp[100] = { '\0' };
	const char *timestamp_f;

	const char *sender = build_gg_uid(recpt);

	tm_msg = localtime(&sent);
	timestamp_f = format_find((type == EKG_MSGCLASS_CHAT) ? "sent_timestamp" : "chat_timestamp");

	if (timestamp_f[0] && !strftime(timestamp, sizeof(timestamp), timestamp_f, tm_msg))
			xstrcpy(timestamp, "TOOLONG");

	print_window(build_windowip_name(type == EKG_MSGCLASS_CHAT ? hdr->dstip : hdr->srcip) /* ip and/or gg# */, s, 1, 
		type == EKG_MSGCLASS_CHAT ? "message" : "sent", 	/* formatka */

		format_user(s, sender),			/* do kogo */
		timestamp, 				/* timestamp */
		msg,					/* wiadomosc */
		get_nickname(s, sender),		/* jego nickname */
		sender,					/* jego uid */
		"");					/* secure */
}

static void sniff_gg_print_status(session_t *s, const connection_t *hdr, uint32_t uin, int status, const char *descr) {
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1, 
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
			print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
				fname, 					/* formatka */
				descr, whom);

		} else {
			print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
				fname,					/* formatka */
				descr, "", whom);
		}
	} else 
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
				fname,					 /* formatka */
				whom);
}

/*  ****************************************************** */

#define SNIFF_HANDLER(x, type) static int x(session_t *s, const connection_t *hdr, const type *pkt, int len)
typedef int (*sniff_handler_t)(session_t *, const connection_t *, const unsigned char *, int);

#define CHECK_LEN(x) \
	if (len < x) {\
		debug_error("%s()  * READ less than: %d (len: %d) (%s)\n", __FUNCTION__, x, len, #x);\
		return -1;\
	}

SNIFF_HANDLER(sniff_print_payload, unsigned) {
	tcp_print_payload((u_char *) pkt, len);
	return 0;
}

SNIFF_HANDLER(sniff_gg_recv_msg, gg_recv_msg) {
	char *msg;

	CHECK_LEN(sizeof(gg_recv_msg))	len -= sizeof(gg_recv_msg);
	msg = gg_cp_to_iso(xstrndup(pkt->msg_data, len));
		sniff_gg_print_message(s, hdr, pkt->sender, EKG_MSGCLASS_CHAT, msg, pkt->time);
	xfree(msg);
	return 0;
}

SNIFF_HANDLER(sniff_gg_send_msg, gg_send_msg) {
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg))  len -= sizeof(gg_send_msg);
	msg = gg_cp_to_iso(xstrndup(pkt->msg_data, len));
		sniff_gg_print_message(s, hdr, pkt->recipient, EKG_MSGCLASS_SENT_CHAT, msg, time(NULL));
	xfree(msg);

	return 0;
}

SNIFF_HANDLER(sniff_gg_send_msg_ack, gg_send_msg_ack) {
#define GG_ACK_BLOCKED 0x0001
#define GG_ACK_DELIVERED 0x0002
#define GG_ACK_QUEUED 0x0003
#define GG_ACK_MBOXFULL 0x0004
#define GG_ACK_NOT_DELIVERED 0x0006
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

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
			format, 
			format_user(s, build_gg_uid(pkt->recipient)));	/* XXX */
	return 0;
}

SNIFF_HANDLER(sniff_gg_welcome, gg_welcome) {
	CHECK_LEN(sizeof(gg_welcome))		len -= sizeof(gg_welcome);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
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
	descr	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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
	descr	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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
	descr 	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
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

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_login60",

			build_gg_uid(pkt->uin),
			build_hex(pkt->hash));
	
	status = gg_status_to_text(pkt->status, &has_descr);
	descr = has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_new_status(s, hdr, pkt->uin, status, descr);
	xfree(descr);
	return 0;
}

SNIFF_HANDLER(sniff_gg_add_notify, gg_add_remove) {
	CHECK_LEN(sizeof(gg_add_remove));	len -= sizeof(gg_add_remove);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_addnotify",

			build_gg_uid(pkt->uin),
			build_hex(pkt->dunno1));
	return 0;
}

SNIFF_HANDLER(sniff_gg_del_notify, gg_add_remove) {
	CHECK_LEN(sizeof(gg_add_remove));	len -= sizeof(gg_add_remove);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_delnotify",

			build_gg_uid(pkt->uin),
			build_hex(pkt->dunno1));
	return 0;
}

SNIFF_HANDLER(sniff_notify_reply60, gg_notify_reply60) {
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

	descr = has_descr ? gg_cp_to_iso(xstrndup(&pkt->next[1], desc_len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	debug_error("gg_notify_reply60: ip: %d port: %d ver: %x isize: %d\n", pkt->remote_ip, pkt->remote_port, pkt->version, pkt->image_size);

	if (pkt->uin & 0x40000000)
		debug_error("gg_notify_reply60: GG_HAS_AUDIO_MASK set\n");

	if (pkt->uin & 0x08000000)
		debug_error("gg_notify_reply60: GG_ERA_OMNIX_MASK set\n");

	if (len > 0) {
		debug_error("gg_notify_reply60: again? leftlen: %d\n", len);
		sniff_notify_reply60(s, hdr, (gg_notify_reply60 *) next, len);
	}
	return 0;
}

static const char *sniff_gg_userlist_reply_str(uint8_t type) {
#define GG_USERLIST_PUT_REPLY 0x00
#define GG_USERLIST_PUT_MORE_REPLY 0x02
#define GG_USERLIST_GET_REPLY 0x06
#define GG_USERLIST_GET_MORE_REPLY 0x04
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

	datatmp = data = len ? gg_cp_to_iso(xstrndup(pkt->data, len)) : NULL;
	
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
		"sniff_gg_userlist_reply",
		sniff_gg_userlist_reply_str(pkt->type),
		build_hex(pkt->type));

	while ((dataline = split_line(&datatmp))) {
		print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
			"sniff_gg_userlist_data", "REPLY",
			dataline);
	}

	xfree(data);
	return 0;
}

static const char *sniff_gg_userlist_req_str(uint8_t type) {
#define GG_USERLIST_PUT 0x00
#define GG_USERLIST_PUT_MORE 0x01
#define GG_USERLIST_GET 0x02
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

	datatmp = data = len ? gg_cp_to_iso(xstrndup(pkt->data, len)) : NULL;

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
		"sniff_gg_userlist_req",
		sniff_gg_userlist_req_str(pkt->type),
		build_hex(pkt->type));

	while ((dataline = split_line(&datatmp))) {
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_userlist_data", "REQUEST",
			dataline);
	}

	xfree(data);
	return 0;
}

static const char *sniff_gg_pubdir50_str(uint8_t type) {
#define GG_PUBDIR50_WRITE 0x01
#define GG_PUBDIR50_READ 0x02
#define GG_PUBDIR50_SEARCH_REQUEST 0x03
#define GG_PUBDIR50_SEARCH_REPLY 0x05
	if (type == GG_PUBDIR50_WRITE) return "GG_PUBDIR50_WRITE";
	if (type == GG_PUBDIR50_READ) return "GG_PUBDIR50_READ";
	if (type == GG_PUBDIR50_SEARCH_REQUEST) return "GG_PUBDIR50_SEARCH_REQUEST";
	if (type == GG_PUBDIR50_SEARCH_REPLY) return "GG_PUBDIR50_SEARCH_REPLY";

	debug_error("sniff_gg_pubdir50_req_str() unk type: 0x%x\n", type);
	return "unknown";
}

SNIFF_HANDLER(sniff_gg_pubdir50_reply, gg_pubdir50_reply) {
	CHECK_LEN(sizeof(gg_pubdir50_reply));	len -= sizeof(gg_pubdir50_reply);

	if (len) {
		debug_error("sniff_gg_pubdir50_reply() stublen: %d\n", len);
		tcp_print_payload((u_char *) pkt->data, len);
	}
	
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
		"sniff_gg_pubdir50_reply",
		sniff_gg_pubdir50_str(pkt->type),
		build_hex(pkt->type),
		itoa(pkt->seq));

	return 0;
}

SNIFF_HANDLER(sniff_gg_pubdir50_req, gg_pubdir50_request) {
	CHECK_LEN(sizeof(gg_pubdir50_request));	len -= sizeof(gg_pubdir50_request);

	if (len) {
		debug_error("sniff_gg_pubdir50_req() stublen: %d\n", len);
		tcp_print_payload((u_char *) pkt->data, len);
	}

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
		"sniff_gg_pubdir50_req",
		sniff_gg_pubdir50_str(pkt->type),
		build_hex(pkt->type),
		itoa(pkt->seq));

	return 0;
}

/* nie w libgadu */
#define CHECK_PRINT(is, shouldbe) if (is != shouldbe) {\
		if (sizeof(is) == 2)		debug_error("%s() values not match: %s [%.4x != %.4x]\n", __FUNCTION__, #is, is, shouldbe); \
		else if (sizeof(is) == 4)	debug_error("%s() values not match: %s [%.8x != %.8x]\n", __FUNCTION__, #is, is, shouldbe); \
		else 				debug_error("%s() values not match: %s [%x != %x]\n", __FUNCTION__, #is, is, shouldbe);\
	}
	
#define GG_DCC_NEW_REQUEST_ID 0x23

#define GG_DCC_REQUEST_VOICE 0x01
#define GG_DCC_REQUEST_FILE 0x04

typedef struct {
	uint32_t type;		/* GG_DCC_REQUEST_VOICE / GG_DCC_REQUEST_FILE */
} GG_PACKED gg_dcc_new_request_id_out;

SNIFF_HANDLER(sniff_gg_dcc_new_request_id_out, gg_dcc_new_request_id_out) {
	if (len != sizeof(gg_dcc_new_request_id_out)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	if (pkt->type != GG_DCC_REQUEST_VOICE && pkt->type != GG_DCC_REQUEST_FILE)
		debug_error("sniff_gg_dcc_new_request_id_out() type nor VOICE nor FILE -- %.8x\n", pkt->type);
	else	debug("sniff_gg_dcc_new_request_id_out() %s CONNECTION\n", pkt->type == GG_DCC_REQUEST_VOICE ? "AUDIO" : "FILE");

	return 0;
}

typedef struct {
	uint32_t type;		/* GG_DCC_REQUEST_VOICE / GG_DCC_REQUEST_FILE */
	unsigned char code1[8];
} GG_PACKED gg_dcc_new_request_id_in;

SNIFF_HANDLER(sniff_gg_dcc_new_request_id_in, gg_dcc_new_request_id_in) {
	if (len != sizeof(gg_dcc_new_request_id_in)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	if (pkt->type != GG_DCC_REQUEST_VOICE && pkt->type != GG_DCC_REQUEST_FILE)
		debug_error("sniff_gg_dcc_new_request_id_in() type nor VOICE nor FILE -- %.8x\n", pkt->type);
	else	debug("sniff_gg_dcc_new_request_id_in() %s CONNECTION\n", pkt->type == GG_DCC_REQUEST_VOICE ? "AUDIO" : "FILE");

	debug("sniff_gg_dcc_new_request_id_in() code: %s\n", build_code(pkt->code1));

	return 0;
}

#define GG_DCC_NEW 0x20
typedef struct {
	unsigned char code1[8];
	uint32_t uin1;		/* LE, from */
	uint32_t uin2;		/* LE, to */
	uint32_t dcctype;	/* GG_DCC_REQUEST_AUDIO / GG_DCC_REQUEST_FILE */
	unsigned char filename[226];
	uint16_t emp1;		/* 00 00 */
	uint16_t dunno2;	/* 10 00 */
	uint16_t emp2;		/* 00 00 */

	uint16_t dunno31;
	uint8_t	 dunno32;
	uint8_t  dunno33;	/* 02 */

	uint32_t emp3;		/* 00 00 00 00 */
	uint32_t dunno4;	/* b4 e5 32 00 */
	uint32_t dunno5;	/* 8e d0 4c 00 */
	uint32_t dunno6;	/* 10 00 00 00 */
	uint16_t dunno7;	/* unknown */
	uint8_t dunno8;		/* unknown */
	uint32_t size;		/* rozmiar, LE */
	uint32_t emp4;		/* 00 00 00 00 */
	unsigned char hash[20];	/* hash w sha1 */
} GG_PACKED gg_dcc_new;

SNIFF_HANDLER(sniff_gg_dcc_new, gg_dcc_new) {
	char *fname;
	CHECK_LEN(sizeof(gg_dcc_new));	len -= sizeof(gg_dcc_new);
	
	if (len != 0)
		debug_error("sniff_gg_dcc_new() extra data?\n");

/* print known data: */
	if (pkt->dcctype != GG_DCC_REQUEST_VOICE && pkt->dcctype != GG_DCC_REQUEST_FILE)
		debug_error("sniff_gg_dcc_new() unknown dcc request %x\n", pkt->dcctype);
	else	debug("sniff_gg_dcc_new() REQUEST FOR: %s CONNECTION\n", pkt->dcctype == GG_DCC_REQUEST_VOICE ? "AUDIO" : "FILE");

	if (pkt->dcctype != GG_DCC_REQUEST_FILE) {
		int print_hash = 0;
		int i;

		for (i = 0; i < sizeof(pkt->hash); i++)
			if (pkt->hash[i] != '\0') print_hash = 1;

		if (print_hash) {
			debug_error("sniff_gg_dcc_new() NOT GG_DCC_REQUEST_FILE, pkt->hash NOT NULL, printing...\n");
			tcp_print_payload((u_char *) pkt->hash, sizeof(pkt->hash));
		}
	}

	tcp_print_payload((u_char *) pkt->filename, sizeof(pkt->filename));	/* tutaj smieci */

	fname = xstrndup((u_char *) pkt->filename, sizeof(pkt->filename));
	debug("sniff_gg_dcc_new() code: %s uin1: %d uin2: %d fname: %s [%db]\n", 
		build_code(pkt->code1), pkt->uin1, pkt->uin2, fname, pkt->size);
	xfree(fname);

/* CHECK unknown vals.. */
	/* these are known as 0 */
	CHECK_PRINT(pkt->emp1, 0);
	CHECK_PRINT(pkt->emp2, 0);
	CHECK_PRINT(pkt->emp3, 0);
	CHECK_PRINT(pkt->emp4, 0);

	CHECK_PRINT(pkt->dunno2, htons(0x1000));

	CHECK_PRINT(pkt->dunno31, !pkt->dunno31);
	CHECK_PRINT(pkt->dunno32, !pkt->dunno32);
	CHECK_PRINT(pkt->dunno33, 0x02);

	CHECK_PRINT(pkt->dunno4, htonl(0xb4e53200));
	CHECK_PRINT(pkt->dunno5, htonl(0x8ed04c00));
	CHECK_PRINT(pkt->dunno6, htonl(0x10000000));
	CHECK_PRINT(pkt->dunno7, !pkt->dunno7);
	CHECK_PRINT(pkt->dunno8, !pkt->dunno8);
	return 0;
}

#define GG_DCC_REJECT_XXX 0x22
typedef struct {
	uint32_t uid;
	unsigned char code1[8];
	uint32_t dunno1;		/* known values: 0x02 -> rejected, 0x06 -> invalid version (6.x) 
							 0x01 -> niemozliwe teraz? [jak ktos przesyla inny plik do Ciebie?] */
} GG_PACKED gg_dcc_reject;

SNIFF_HANDLER(sniff_gg_dcc_reject_in, gg_dcc_reject) {
	if (len != sizeof(gg_dcc_reject)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	debug_error("XXX sniff_gg_dcc_reject_in() uid: %d code: %s\n", pkt->uid, build_code(pkt->code1));

	CHECK_PRINT(pkt->dunno1, !pkt->dunno1);
	return 0;
}

SNIFF_HANDLER(sniff_gg_dcc_reject_out, gg_dcc_reject) {
	if (len != sizeof(gg_dcc_reject)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	debug_error("XXX sniff_gg_dcc_reject_out() uid: %d code: %s\n", pkt->uid, build_code(pkt->code1));

	CHECK_PRINT(pkt->dunno1, !pkt->dunno1);
	return 0;
}

#define GG_DCC_1XXX 0x21

typedef struct {
	uint32_t uin;			/* uin */
	unsigned char code1[8];		/* kod polaczenia */
	uint32_t seek;			/* od ktorego miejsca chcemy/mamy wysylac. */
	uint32_t empty;
} GG_PACKED gg_dcc_1xx;

SNIFF_HANDLER(sniff_gg_dcc1xx_in, gg_dcc_1xx) {
	if (len != sizeof(gg_dcc_1xx)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}
	debug_error("XXX sniff_gg_dcc1xx_in() uid: %d code: %s from: %d\n", pkt->uin, build_code(pkt->code1), pkt->seek);

	CHECK_PRINT(pkt->empty, 0);
	return 0;
}

SNIFF_HANDLER(sniff_gg_dcc1xx_out, gg_dcc_1xx) {
	if (len != sizeof(gg_dcc_1xx)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}
	debug_error("XXX sniff_gg_dcc1xx_out() uid: %d code: %s from: %d\n", pkt->uin, build_code(pkt->code1), pkt->seek);

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

	ipport = xstrndup(pkt->ipport, 21);
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

#define GG_NOTIFY_REPLY77 0x0018
typedef struct {
	uint32_t uin;			/* [gg_notify_reply60] numerek plus flagi w MSB */
/* 14 bajtow */
	uint8_t status;			/* [gg_notify_reply60] status danej osoby */
	uint32_t remote_ip;		/* [XXX] adres ip delikwenta */
	uint16_t remote_port;		/* [XXX] port, na którym słucha klient */
	uint8_t version;		/* [gg_notify_reply60] wersja klienta */
	uint8_t image_size;		/* [gg_notify_reply60] maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	uint32_t dunno2;		/* 0x00000000 */
	unsigned char next[];		/* [like gg_notify_reply60] nastepny (gg_notify_reply77), lub DLUGOSC_OPISU+OPIS + nastepny (gg_notify_reply77) */
} GG_PACKED gg_notify_reply77;

SNIFF_HANDLER(sniff_notify_reply77, gg_notify_reply77) {
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

	descr = has_descr ? gg_cp_to_iso(xstrndup(&pkt->next[1], desc_len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
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
		sniff_notify_reply77(s, hdr, (gg_notify_reply77 *) next, len);
	}
	return 0;
}


#define GG_STATUS77 0x17
typedef struct {
	uint32_t uin;			/* [gg_status60] numerek plus flagi w MSB */
	uint8_t status;			/* [gg_status60] status danej osoby */
	uint32_t remote_ip;		/* [XXX] adres ip delikwenta */
	uint16_t remote_port;		/* [XXX] port, na którym słucha klient */
	uint8_t version;		/* [gg_status60] wersja klienta */
	uint8_t image_size;		/* [gg_status60] maksymalny rozmiar grafiki w KiB */
	uint8_t dunno1;			/* 0x00 */
	uint32_t dunno2;		/* 0x00 */
	char status_data[];
} GG_PACKED gg_status77;

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
	descr 	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_status(s, hdr, uin, status, descr);
	xfree(descr);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
		"sniff_gg_status77",

		inet_ntoa(*(struct in_addr *) &(pkt->remote_ip)),
		itoa(pkt->remote_port),
		itoa(pkt->version), build_hex(pkt->version),
		itoa(pkt->image_size));

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

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_login70_hash",

			build_gg_uid(pkt->uin),
			build_hex(pkt->hash_type));

	} else if (pkt->hash_type == GG_LOGIN_HASH_SHA1) {
		sughash_len = 20;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_login70_sha1",

			build_gg_uid(pkt->uin),
			build_sha1(pkt->hash));
	} else {
		sughash_len = 0;

		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_login70_unknown",

			build_gg_uid(pkt->uin), build_hex(pkt->hash_type));

	}


	status = gg_status_to_text(pkt->status, &has_descr);
	descr = has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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
		print_window(build_windowip_name(hdr->srcip), s, 1,
			"generic_error", "gg_login70() print_payload flag set, see debug");
	}
	return 0;
}

#undef CHECK_PRINT

typedef enum {
	SNIFF_OUTGOING = 0,
	SNIFF_INCOMING
} pkt_way_t;

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
	{ GG_NOTIFY_REPLY60,	"GG_NOTIFY_REPLY60",	SNIFF_INCOMING, (void *) sniff_notify_reply60, 0}, 

	{ GG_LIST_EMPTY,	"GG_LIST_EMPTY",	SNIFF_OUTGOING, (void *) NULL, 0}, /* XXX */
	{ GG_NOTIFY_FIRST,	"GG_NOTIFY_FIRST",	SNIFF_OUTGOING, (void *) NULL, 0}, /* XXX */
	{ GG_NOTIFY_LAST,	"GG_NOTIFY_LAST",	SNIFF_OUTGOING, (void *) NULL, 0}, /* XXX */
	{ GG_LOGIN70,		"GG_LOGIN70",		SNIFF_OUTGOING, (void *) sniff_gg_login70, 0},

	{ GG_USERLIST_REQUEST,	"GG_USERLIST_REQUEST",	SNIFF_OUTGOING, (void *) sniff_gg_userlist_req, 0},
	{ GG_USERLIST_REPLY,	"GG_USERLIST_REPLY",	SNIFF_INCOMING, (void *) sniff_gg_userlist_reply, 0},

	{ GG_PUBDIR50_REPLY,	"GG_PUBDIR50_REPLY",	SNIFF_INCOMING, (void *) sniff_gg_pubdir50_reply, 0},
	{ GG_PUBDIR50_REQUEST,	"GG_PUBDIR50_REQUEST",	SNIFF_OUTGOING, (void *) sniff_gg_pubdir50_req, 0},

/* pakiety nie w libgadu: */
	{ GG_NOTIFY_REPLY77,	"GG_NOTIFY_REPLY77",	SNIFF_INCOMING, (void *) sniff_notify_reply77, 0},
	{ GG_STATUS77,		"GG_STATUS77",		SNIFF_INCOMING, (void *) sniff_gg_status77, 0},

	{ GG_DCC_NEW,		"GG_DCC_NEW",		SNIFF_INCOMING, (void *) sniff_gg_dcc_new, 0}, 
	{ GG_DCC_NEW,		"GG_DCC_NEW",		SNIFF_OUTGOING, (void *) sniff_gg_dcc_new, 0}, 
	{ GG_DCC_NEW_REQUEST_ID, "GG_DCC_NEW_REQUEST_ID", SNIFF_INCOMING, (void *) sniff_gg_dcc_new_request_id_in, 0},
	{ GG_DCC_NEW_REQUEST_ID, "GG_DCC_NEW_REQUEST_ID", SNIFF_OUTGOING, (void *) sniff_gg_dcc_new_request_id_out, 0},
	{ GG_DCC_REJECT_XXX,	"GG_DCC_REJECT ?",	SNIFF_INCOMING, (void *) sniff_gg_dcc_reject_in, 0},
	{ GG_DCC_REJECT_XXX,	"GG_DCC_REJECT ?",	SNIFF_OUTGOING, (void *) sniff_gg_dcc_reject_out, 0},
/* unknown, 0x21 */
	{ GG_DCC_1XXX,		"GG_DCC_1XXX",		SNIFF_INCOMING, (void *) sniff_gg_dcc1xx_in, 0}, 
	{ GG_DCC_1XXX,		"GG_DCC_1XXX",		SNIFF_OUTGOING, (void *) sniff_gg_dcc1xx_out, 0}, 
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

	CHECK_LEN(sizeof(gg_header)) 	len -= sizeof(gg_header);
	CHECK_LEN(pkt->len)

	/* XXX, check direction!!!!!111, in better way: */
	if (!xstrncmp(inet_ntoa(hdr->srcip), "217.17.", 7))
		way = SNIFF_INCOMING;

	/* XXX, jesli mamy podejrzenia ze to nie jest pakiet gg, to wtedy powinnismy zwrocic -2 i pozwolic zeby inni za nas to przetworzyli */
	for (i=0; sniff_gg_callbacks[i].sname; i++) {
		if (sniff_gg_callbacks[i].type == pkt->type && sniff_gg_callbacks[i].way == way) {
			debug("sniff_gg() %s [%d,%d,%db] %s\n", sniff_gg_callbacks[i].sname, pkt->type, way, pkt->len, inet_ntoa(way ? hdr->dstip : hdr->srcip));
			if (sniff_gg_callbacks[i].handler) 
				sniff_gg_callbacks[i].handler(s, hdr, pkt->data, pkt->len);

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

#undef CHECK_LEN
void sniff_loop(u_char *data, const struct pcap_pkthdr *header, const u_char *packet) {
	const struct ethhdr *ethernet;
	const struct iphdr *ip;
	const struct tcphdr *tcp;

	connection_t *hdr;
	const char *payload;
	
	int size_ip;
	int size_tcp;
	int size_payload;

#define CHECK_LEN(x) \
	if (header->caplen < x) {\
		debug_error("sniff_loop()  * READ less than: %d (%d %d) (%s)\n", x, header->caplen, header->len, #x);\
		return;\
	}

	CHECK_LEN(sizeof(struct ethhdr))					ethernet = (struct ethhdr *) (packet);
	CHECK_LEN(sizeof(struct ethhdr) + sizeof(struct iphdr))			ip = (struct iphdr *) (packet + SIZE_ETHERNET);
	size_ip = ip->ip_hl*4;
	
	if (size_ip < 20) {
		debug_error("sniff_loop()   * Invalid IP header length: %u bytes\n", size_ip);
		return;
	}

	if (ip->ip_p != IPPROTO_TCP) return; /* XXX, tylko tcp */

	CHECK_LEN(sizeof(struct ethhdr) + size_ip + sizeof(struct tcphdr))	tcp = (struct tcphdr*) (packet + SIZE_ETHERNET + size_ip);
	size_tcp = TH_OFF(tcp)*4;
	
	if (size_tcp < 20) {
		debug_error("sniff_loop()   * Invalid TCP header length: %u bytes\n", size_tcp);
		return;
	}

	size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

	CHECK_LEN(SIZE_ETHERNET + size_ip + size_tcp + size_payload);
	
	payload = (u_char *) (packet + SIZE_ETHERNET + size_ip + size_tcp);

	hdr = sniff_tcp_find_connection(ip, tcp);

	debug_function("sniff_loop() %15s:%5d <==> ", 
			inet_ntoa(hdr->srcip), 		/* src ip */
			hdr->srcport);			/* src port */

	debug_function("%15s:%5d %s (SEQ: %lx ACK: %lx len: %d)\n", 
			inet_ntoa(hdr->dstip), 		/* dest ip */
			hdr->dstport, 			/* dest port */
			tcp_print_flags(tcp->th_flags), /* tcp flags */
			htonl(tcp->th_seq), 		/* seq */
			htonl(tcp->th_ack), 		/* ack */
			size_payload);			/* payload len */

/* XXX check tcp flags */
	if (!size_payload) return;
/* XXX what proto ? check based on ip + port? */
	sniff_gg((session_t *) data, hdr, (gg_header *) payload, size_payload);
#undef CHECK_LEN
}

/* XXX, some notes about tcp fragment*
 * 		@ sniff_loop() we'll do: sniff_find_tcp_connection(connection_t *hdr);
 * 		it'll find (or create) struct with inited string_t buf...
 * 		than we append to that string_t recv data from packet, and than pass this to sniff_gg() [or anyother sniff handler]
 * 		than in sniff_loop() we'll remove already data.. [of length len, len returned from sniff_gg()]
 */

static WATCHER_SESSION(sniff_pcap_read) {
	if (type)
		return 0;

	if (!s) {
		debug_error("sniff_pcap_read() no session!\n");
		return -1;
	}
	pcap_dispatch(GET_DEV(s), 1, sniff_loop, (void *) s);
	return 0;
}

static COMMAND(sniff_command_connect) {
#define DEFAULT_FILTER "tcp and net 217.17.41.80/28 or net 217.17.45.128/27 and tcp"
	struct bpf_program fp;
	char errbuf[PCAP_ERRBUF_SIZE] = { 0 };
	pcap_t *dev;
	const char *filter;
	char *device;
	char *tmp;

	if (!(filter = session_get(session, "filter")))
		filter = DEFAULT_FILTER;

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if ((tmp = xstrchr(session->uid+6, ':')))
		device = xstrndup(session->uid+6, tmp-(session->uid+6));
	else	device = xstrdup(session->uid+6);

	dev = pcap_open_live(device, SNAPLEN, PROMISC, 1000, errbuf);

	if (!dev) {
		debug_error("Couldn't open dev: %s (%s)\n", device, errbuf);
		printq("conn_failed", errbuf, session_name(session));
		xfree(device);
		return -1;
	}

	if (pcap_setnonblock(dev, 1, errbuf) == -1) {
		debug_error("Could not set device \"%s\" to non-blocking: %s\n", device, errbuf);
		pcap_close(dev);
		xfree(device);
		return -1;
	}

	xfree(device);

	if (pcap_compile(dev, &fp, (char *) filter, 0, 0 /*net*/) == -1) {
		debug_error("Couldn't parse filter %s: %s\n", filter, pcap_geterr(dev));
		pcap_close(dev);
		return -1;
	}

	if (pcap_setfilter(dev, &fp) == -1) {
		debug_error("Couldn't install filter %s: %s\n", filter, pcap_geterr(dev));
		pcap_close(dev);
		return -1;
	}
/*
	pcap_freecode(&fp);
 */
	session->priv = dev;

	watch_add_session(session, pcap_fileno(dev), WATCH_READ, sniff_pcap_read);

	session->status = EKG_STATUS_AVAIL;
	query_emit_id(NULL, PROTOCOL_CONNECTED, &(session->uid));
	return 0;
}

static COMMAND(sniff_command_disconnect) {
	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	session->connected = 0;
	/* XXX, notify */

	if (!GET_DEV(session)) {
		debug_error("sniff_command_disconnect() not dev?!\n");
		return -1;
	}

	pcap_close(GET_DEV(session));
	session->priv = NULL;

	return 0;
}

static COMMAND(sniff_command_connections) {
	list_t l;

	for (l = tcp_connections; l; l = l->next) {
		connection_t *c = l->data;
		char src_ip[INET_ADDRSTRLEN];
		char dst_ip[INET_ADDRSTRLEN];

		print_window("__status", session, 0,
			"sniff_tcp_connection", 
				inet_ntop(AF_INET, &c->srcip, src_ip, sizeof(src_ip)),
				itoa(c->srcport),
				inet_ntop(AF_INET, &c->dstip, dst_ip, sizeof(dst_ip)),
				itoa(c->dstport));
	}
	return 0;
}

static QUERY(sniff_validate_uid) {
	char    *uid    = *(va_arg(ap, char **));
	int     *valid  = va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncasecmp(uid, "sniff:", 6) && uid[6]) {
		(*valid)++;
		return -1;
	}

	return 0;
}

static QUERY(sniff_status_show) {
	char		*uid = *(va_arg(ap, char **));
	session_t	*s = session_find(uid);
	struct pcap_stat stats;

	if (!s)
		return -1;

	if (!s->connected)
		return 0;

	if (!s->priv) {
		debug_error("sniff_status_show() s->priv NULL\n");
		return -1;
	}

/* Device: DEVICE (PROMISC?) */

/* some stats */
	memset(&stats, 0, sizeof(struct pcap_stat));
	if (pcap_stats(GET_DEV(s), &stats) == -1) {
		debug_error("sniff_status_show() pcap_stats() failed\n");
		return -1;
	}

	debug("pcap_stats() recv: %d drop: %d ifdrop: %d\n", stats.ps_recv, stats.ps_drop, stats.ps_ifdrop);
	print("sniff_pkt_rcv",	session_name(s), itoa(stats.ps_recv));
	print("sniff_pkt_drop",	session_name(s), itoa(stats.ps_drop));
	print("sniff_conn_db",	session_name(s), itoa(list_count(tcp_connections)));

	return 0;
}

static QUERY(sniff_print_version) {
	print("generic", pcap_lib_version());
	return 0;
}

static int sniff_theme_init() {
/* sniff gg */
	format_add("sniff_gg_welcome",	_("%) %b[GG_WELCOME] %gSEED: %W%1"), 1);
	format_add("sniff_gg_login60",	_("%) %b[GG_LOGIN60] %gUIN: %W%1 %gHASH: %W%2"), 1);

	format_add("sniff_gg_login70_sha1",	_("%) %b[GG_LOGIN70] %gUIN: %W%1 %gSHA1: %W%2"), 1);
	format_add("sniff_gg_login70_hash",	_("%) %b[GG_LOGIN70] %gUIN: %W%1 %gHASH: %W%2"), 1);
	format_add("sniff_gg_login70_unknown",	_("%) %b[GG_LOGIN70] %gUIN: %W%1 %gTYPE: %W%2"), 1);

	format_add("sniff_gg_userlist_req",	_("%) %b[GG_USERLIST_REQUEST] %gTYPE: %W%1 (%2)"), 1);
	format_add("sniff_gg_userlist_reply",	_("%) %b[GG_USERLIST_REPLY] %gTYPE: %W%1 (%2)"), 1);

	format_add("sniff_gg_userlist_data",	_("%)   %b[%1] %gENTRY: %W%2"), 1);

	format_add("sniff_gg_pubdir50_req",	_("%) %b[GG_PUBDIR50_REQUEST] %gTYPE: %W%1 (%2) %gSEQ: %W%3"), 1);
	format_add("sniff_gg_pubdir50_reply",	_("%) %b[GG_PUBDIR50_REPLY] %gTYPE: %W%1 (%2) %gSEQ: %W%3"), 1);

	format_add("sniff_gg_status60", _("%) %b[GG_STATUS60] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_status77", _("%) %b[GG_STATUS77] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_notify77", _("%) %b[GG_NOTIFY77] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);

	format_add("sniff_gg_addnotify",_("%) %b[GG_ADD_NOTIFY] %gUIN: %W%1 %gDATA: %W%2"), 1);
	format_add("sniff_gg_delnotify",_("%) %b[GG_REMOVE_NOTIFY] %gUIN: %W%1 %gDATA: %W%2"), 1);
/* stats */
	format_add("sniff_pkt_rcv", _("%) %2 packets captured"), 1);
	format_add("sniff_pkt_drop",_("%) %2 packets dropped"), 1);

	format_add("sniff_conn_db", 		_("%) %2 connections founded"), 1);
	format_add("sniff_tcp_connection",	"TCP %1:%2 <==> %3:%4", 1);

	return 0;
}

static plugins_params_t sniff_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		SESSION_VAR_ALIAS, VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("auto_connect", 		SESSION_VAR_AUTO_CONNECT, VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("filter", 		0, VAR_STR, 0, 0, NULL),

	PLUGIN_VAR_END()
};

EXPORT int sniff_plugin_init(int prio) {
	sniff_plugin.params = sniff_plugin_vars;
	plugin_register(&sniff_plugin, prio);

	query_connect_id(&sniff_plugin, PROTOCOL_VALIDATE_UID,	sniff_validate_uid, NULL);
	query_connect_id(&sniff_plugin, STATUS_SHOW, 		sniff_status_show, NULL);
	query_connect_id(&sniff_plugin, PLUGIN_PRINT_VERSION,	sniff_print_version, NULL);

	command_add(&sniff_plugin, "sniff:connect", NULL, sniff_command_connect,    SESSION_MUSTBELONG, NULL);
	command_add(&sniff_plugin, "sniff:connections", NULL, sniff_command_connections, SESSION_MUSTBELONG | SESSION_MUSTBECONNECTED, NULL);
	command_add(&sniff_plugin, "sniff:disconnect", NULL,sniff_command_disconnect, SESSION_MUSTBELONG, NULL);

	return 0;
}

static int sniff_plugin_destroy() {
	list_t  l;

	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;
		if (GET_DEV(s) && !xstrncmp(s->uid, "sniff", 5)) {
			debug("sniff closing pcap dev: 0x%x\n", s->priv);
			pcap_close(GET_DEV(s));		
		}
	}

	plugin_unregister(&sniff_plugin);

	return 0;
}

