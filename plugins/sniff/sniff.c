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
#include <errno.h>

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
#include "sniff_dns.h"
#include "sniff_rivchat.h"

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

static char *_inet_ntoa(struct in_addr ip) {
	static char bufs[10][16];
	static int index = 0;
	char *tmp = bufs[index++];

	if (index > 9)
		index = 0;
	
	snprintf(tmp, 16, "%d.%d.%d.%d", (ip.s_addr) & 0xff, (ip.s_addr >> 8) & 0xff, (ip.s_addr >> 16) & 0xff, (ip.s_addr >> 24) & 0xff);
	return tmp;
}

static char *_inet_ntoa6(struct in6_addr ip) {
	static char bufs[10][INET6_ADDRSTRLEN+1];
	static int index = 0;
	char *tmp = bufs[index++];
	char *tmp2;

	if (index > 9)
		index = 0;

	if (!(tmp2 = inet_ntop(AF_INET6, &ip, tmp, INET6_ADDRSTRLEN))) {
		debug_error("inet_ntoa6() failed %s\n", strerror(errno));
		return "";
	}

	return tmp;
}

static char *build_windowip_name(struct in_addr ip) {
	static char buf[50];

	sprintf(buf, "sniff:%s", inet_ntoa(ip));
	return buf;
}


static char *build_rivchatport_name(const connection_t *hdr) {
	static char buf[50];

	sprintf(buf, "sniff:rivchat:%d", hdr->srcport);
	return buf;
}

static connection_t *sniff_udp_get(const struct iphdr *ip, const struct udphdr *udp) {
	static connection_t d;
	
	d.srcip		= ip->ip_src;
	d.srcport	= ntohs(udp->th_sport);

	d.dstip		= ip->ip_dst;
	d.dstport	= ntohs(udp->th_dport);
	return &d;
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

static char *gg_cp_to_iso(char *b) {
	char *tmp = b;
	unsigned char *buf = (unsigned char *) b;

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

	while (len) {
		int display_len;
		int i;

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
	descr = has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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

	descr = has_descr ? gg_cp_to_iso(xstrndup((char *) &pkt->next[1], desc_len)) : NULL;
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
	
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
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

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, EKG_WINACT_MSG, 1,
		"sniff_gg_pubdir50_req",
		sniff_gg_pubdir50_str(pkt->type),
		build_hex(pkt->type),
		itoa(pkt->seq));

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

	descr = has_descr ? gg_cp_to_iso(xstrndup((char *) &pkt->next[1], desc_len)) : NULL;
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
	descr 	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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
	descr = has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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

	descr = has_descr ? gg_cp_to_iso(xstrndup((char *) &pkt->next[1], desc_len)) : NULL;
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
	descr 	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
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

#define GG_RECV_MSG80 0x2e

typedef struct {
	uint32_t sender;
	uint32_t seq;
	uint32_t time;
	uint32_t msgclass;
	uint32_t offset_plaintext;
	uint32_t offset_attr;
	char msg_data[];
	/* '\0' */
	/* plaintext msg */
	/* '\0' */
	/* uint32_t dunno3; */						/* { 02 06 00 00 } */
	/* uint8_t dunno4; */						/* { 00 } */
	/* uint32_t dunno5; */		/* like msgclass? */		/* { 08 00 00 00 } */
} GG_PACKED gg_recv_msg80;

SNIFF_HANDLER(sniff_gg_recv_msg80, gg_recv_msg80) {
	/* XXX, like sniff_gg_send_msg80() */
}

#define GG_SEND_MSG80 0x2d

typedef struct {
	uint32_t recipient;
	uint32_t seq;			/* time(0) */
	uint32_t msgclass;						/* GG_CLASS_CHAT  { 08 00 00 00 } */
	uint32_t offset_plaintext;
	uint32_t offset_attr;
	char html_data[];
	/* '\0' */
	/* plaintext msg */
	/* '\0' */
	/* uint32_t dunno3; */						/* { 02 06 00 00 } */
	/* uint8_t dunno4; */						/* { 00 } */
	/* uint32_t dunno5; */		/* like msgclass? */		/* { 08 00 00 00 } */
} GG_PACKED gg_send_msg80;

SNIFF_HANDLER(sniff_gg_send_msg80, gg_send_msg80) {
	int orglen = len;
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg80))  len -= sizeof(gg_send_msg80);

	tcp_print_payload(pkt->html_data, len);

	if (pkt->offset_plaintext < orglen) 
		tcp_print_payload(((char *) pkt) + pkt->offset_plaintext, orglen - pkt->offset_plaintext);
	if (pkt->offset_attr < orglen) 
		tcp_print_payload(((char *) pkt) + pkt->offset_attr, orglen - pkt->offset_attr);

	if (pkt->offset_plaintext < orglen) {
		msg = gg_cp_to_iso(xstrndup(((char *) pkt) + pkt->offset_plaintext, orglen - pkt->offset_plaintext));
			sniff_gg_print_message(s, hdr, pkt->recipient, EKG_MSGCLASS_SENT_CHAT, msg, time(NULL));
		xfree(msg);
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
				sniff_gg_callbacks[i].handler(s, hdr, (char *) pkt->data, pkt->len);

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

SNIFF_HANDLER(sniff_dns, DNS_HEADER) {
	CHECK_LEN(DNS_HFIXEDSZ)

	if (hdr->srcport == 53) {
		debug("INCOMING DNS REPLY: [ID: %x]\n", pkt->id);
		/* 1) Check send id request with recv id request. */	/* pkt->id */

		/* 2a, 2b) RES_INSECURE1, RES_INSECURE2 implementation -> ignore */

		if (pkt->rcode == SERVFAIL || pkt->rcode == NOTIMP || pkt->rcode == REFUSED) {
			debug_error("[sniff_dns()] Server rejected query\n");
			goto end;
			return 0;
		}

		if (pkt->rcode == NOERROR && pkt->ancount == 0 && pkt->aa == 0 && pkt->ra == 0 && pkt->arcount == 0) {
			debug_error("[sniff_dns()] Referred query\n");
			return 0;
		}
		
		/* if set RES_IGNTC */
		if (pkt->tc) {
			debug_error("[sniff_dns()] XXX ; truncated answer\n");
			return -1;
		}
	
		if (pkt->rcode != NOERROR || pkt->ancount == 0) {

			switch (pkt->rcode) {
				case NXDOMAIN:	debug_error("[sniff_dns()] NXDOMAIN [%d]\n", ntohs(pkt->ancount));	break;
				case SERVFAIL:	debug_error("[sniff_dns()] SERVFAIL [%d]\n", ntohs(pkt->ancount));	break;
				case NOERROR:	debug_error("[sniff_dns()] NODATA\n");					break;

				case FORMERR:
				case NOTIMP:
				case REFUSED:
				default:
						debug_error("[sniff_dns()] NO_RECORVERY [%d, %d]\n", pkt->rcode, ntohs(pkt->ancount));	break;
			}
			goto end;
			return 0;
		}

		{
			int qdcount = ntohs(pkt->qdcount);			/* question count */
			int ancount = ntohs(pkt->ancount);			/* answer count */
			int i;
			int displayed = 0;

			char *eom = ((char *) pkt) + len;
			char *cur = ((char *) pkt) + sizeof(DNS_HEADER);
			char *beg = ((char *) pkt);

			debug_function("sniff_dns() NOERROR qdcount: %d, ancount: %d\n", qdcount, ancount);

			if (qdcount == 0 || !(cur < eom))
				print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_reply", inet_ntoa(hdr->dstip), "????");

			i = 0;
			while (qdcount > 0 && cur < eom) {
				char host[257];
				int len;

				if ((len = dn_expand(beg, eom, cur, host, sizeof(host))) < 0) {
					debug_error("sniff_dns() dn_expand() fail, on qdcount[%d] item\n", i);
					return 0;
				}

				cur += (len + DNS_QFIXEDSZ);	/* naglowek + payload */

				print_window_w(window_status,  EKG_WINACT_MSG, "sniff_dns_reply", inet_ntoa(hdr->dstip), host);

				qdcount--;
				i++;
			}

			i = 0;
			while (ancount > 0 && cur < eom) {
				char host[257];
				int len;

				if ((len = dn_expand(beg, eom, cur, host, sizeof(host))) < 0) {
					debug_error("sniff_dns() dn_expand() fail on ancount[%d] item\n", i);
					return 0;
				}
				
				cur += len;

				if (cur + 2 + 2 + 4 + 2 >= eom) {
					debug_error("sniff_dns() ancount[%d] no space for header?\n", i);
					return 0;
				}
				/* type [2b], class [2b], ttl [4b], size [2b] */

				int type = (cur[0] << 8 | cur[1]);
				int payload = (cur[8] << 8 | cur[9]);

				cur += 10;		/* skip header */

				if (cur + payload >= eom) {
					debug_error("sniff_dns() ancount[%d] no space for data?\n", i);
					return 0;
				}

				switch (type) {
					char tmp_addr[257];

					case T_A:
						if (payload != 4) {
							debug_error("T_A record but size != 4 [%d]\n", payload);
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_a", 
							host, inet_ntoa(*((struct in_addr *) &cur[0])));

						displayed = 1;
						break;

					case T_AAAA:
						if (payload != 16) {
							debug_error("T_AAAA record but size != 16 [%d]\n", payload);
							break;
						}
						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_aaaa",
							host, _inet_ntoa6(*((struct in6_addr *) &cur[0])));

						displayed = 1;
						break;

					case T_CNAME:
						if ((len = dn_expand(beg, eom, cur, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_CNAME failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_cname",
							host, tmp_addr);
						displayed = 1;
						break;

					case T_PTR:
						if ((len = dn_expand(beg, eom, cur, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_PTR failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_ptr",
							host, tmp_addr);
						displayed = 1;
						break;

					case T_MX: {
						int prio;
						if (payload < 2) {
							debug_error("T_MX record but size < 2 [%d]\n", payload);
							break;
						}
					/* LE STUFF */
						prio = cur[0] << 8 | cur[1];
						
						if ((len = dn_expand(beg, eom, cur + 2, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_MX failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_mx",
							host, tmp_addr, itoa(prio));
						displayed = 1;
						break;
					}

					case T_SRV: {
						int prio, weight, port;
						if (payload < 6) {
							debug_error("T_SRV record but size < 6 [%d]\n", payload);
							break;
						}
					/* LE stuff */
						prio	= cur[0] << 8 | cur[1];
						weight	= cur[2] << 8 | cur[3];
						port	= cur[4] << 8 | cur[5];

						if ((len = dn_expand(beg, eom, cur + 6, tmp_addr, sizeof(tmp_addr))) < 0) {
							debug_error("dn_expand() on T_SRV failed\n");
							break;
						}

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_srv",
							host, tmp_addr, itoa(port), itoa(prio), itoa(weight));
						displayed = 1;
						break;
					}

					default:
						/* print payload */
						debug_error("ancount[%d] %d size: %d\n", i, type, payload);
						tcp_print_payload((u_char *) cur, payload);

						print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_?", 
							host, itoa(type), itoa(payload));
						displayed = 1;
				}
				cur += payload;		/* skip payload */

				ancount--;
				i++;
			}

			if (!displayed)
				print_window_w(window_status, EKG_WINACT_MSG, "sniff_dns_entry_ndisplay");
		}

		return 0;


	} else if (hdr->dstport == 53) {
		debug("INCOMING DNS REQUEST: [ID: %x]\n", pkt->id);

	} else {
		debug_error("sniff_dns() SRCPORT/DSTPORT NOT 53!!!\n");
		return -2;
	}
	return 0;

end:
	tcp_print_payload((u_char *) pkt, len);
	return 0;
}

SNIFF_HANDLER(sniff_rivchat_info, rivchat_packet_rcinfo) {
	char *hostname	= gg_cp_to_iso(xstrndup(pkt->host, sizeof(pkt->host)));
	char *os	= gg_cp_to_iso(xstrndup(pkt->os, sizeof(pkt->os)));
	char *program	= gg_cp_to_iso(xstrndup(pkt->prog, sizeof(pkt->prog)));
	char *username	= gg_cp_to_iso(xstrndup(pkt->user, sizeof(pkt->user)));

	char program_ver[8];	/* ddd '.' ddd */

	sprintf(program_ver, "%d.%d", pkt->version[0], pkt->version[1]);

	print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
			"sniff_rivchat_rcinfo", inet_ntoa(hdr->srcip),

			username, hostname, os, program, program_ver);

	/* not used: away, master, slowa, kod, plec, online, filetransfer, pisze */

	xfree(hostname);
	xfree(os);
	xfree(program);
	xfree(username);
	return 0;
}

SNIFF_HANDLER(sniff_rivchat, rivchat_packet) {
	int type = pkt->type;		/* pkt->type is LE, if you're using BE you know what to do */

	char *nick;

	CHECK_LEN(sizeof(rivchat_packet))

	nick = gg_cp_to_iso(xstrndup(pkt->nick, sizeof(pkt->nick)));

	debug("UDP RIVCHAT PACKET [SIZE: %d FROMID: %d TOID: %d TYPE: %x NICK: %s\n", pkt->size, pkt->fromid, pkt->toid, pkt->type, nick);

	tcp_print_payload((u_char *) pkt->format, sizeof(pkt->format));

	switch (type) {
		char *data;

		case RIVCHAT_ME:
			debug_function("sniff_rivchat() RIVCHAT_ME\n");

			data = gg_cp_to_iso(xstrndup(pkt->data, sizeof(pkt->data)));
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_me", inet_ntoa(hdr->srcip),
				nick,
				data);

			xfree(data);
			break;

		case RIVCHAT_MESSAGE:
			debug_error("sniff_rivchat() RIVCHAT_MESSAGE\n");
			tcp_print_payload((u_char *) pkt->data, sizeof(pkt->data));

			/* XXX secure, not secure */

			data = gg_cp_to_iso(xstrndup(pkt->data, sizeof(pkt->data)));
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_message", inet_ntoa(hdr->srcip),
				nick,
				data);

			xfree(data);
			break;

		case RIVCHAT_INIT:
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_init", inet_ntoa(hdr->srcip));
			/* no break */
		case RIVCHAT_PING:
			debug_function("sniff_rivchat() RIVCHAT_PING\n");
			sniff_rivchat_info(s, hdr, (rivchat_packet_rcinfo *) pkt->data, sizeof(pkt->data));
			break;

		case RIVCHAT_AWAY:
		case RIVCHAT_PINGAWAY:
			debug_function("sniff_rivchat() RIVCHAT_AWAY/RIVCHAT_PINGAWAY\n");

			data = gg_cp_to_iso(xstrndup(pkt->data, sizeof(pkt->data)));
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				type == RIVCHAT_AWAY ? "sniff_rivchat_away" : "sniff_rivchat_pingaway", inet_ntoa(hdr->srcip),

				data);
			xfree(data);
			break;

		case RIVCHAT_QUIT:
			debug_function("sniff_rivchat() RIVCHAT_QUIT\n");
			
			print_window(build_rivchatport_name(hdr) /* sniff:rc:port or smth */, s, EKG_WINACT_MSG, 1,
				"sniff_rivchat_quit", inet_ntoa(hdr->srcip));
				/* data discarded, however in some time maybe there'd be reason */

			break;

		default:
			debug_error("sniff_rivchat() unknown type: %x\n", pkt->type);
			tcp_print_payload((u_char *) pkt->data, sizeof(pkt->data));
	}
	xfree(nick);
	return 0;
}

#undef CHECK_LEN
#define CHECK_LEN(x) \
	if (len < x) {\
		debug_error("%s()  * READ less than: %d (len: %d) (%s)\n", __FUNCTION__, x, len, #x);	\
		return;											\
	}


/* XXX, some notes about tcp fragment*
 * 		@ sniff_loop_tcp() we'll do: sniff_find_tcp_connection(connection_t *hdr);
 * 		it'll find (or create) struct with inited string_t buf...
 * 		than we append to that string_t recv data from packet, and than pass this to sniff_gg() [or anyother sniff handler]
 * 		than in sniff_loop() we'll remove already data.. [of length len, len returned from sniff_gg()]
 */

static inline void sniff_loop_tcp(session_t *s, int len, const u_char *packet, const struct iphdr *ip, int size_ip) {
	/* XXX here, make some struct with known TCP services, and demangler-function */
	const struct tcphdr *tcp;
	int size_tcp;
	connection_t *hdr;

	const char *payload;
	int size_payload;

	CHECK_LEN(sizeof(struct tcphdr))	tcp = (struct tcphdr*) (packet);
	size_tcp = TH_OFF(tcp)*4;

	if (size_tcp < 20) {
		debug_error("sniff_loop_tcp()   * Invalid TCP header length: %u bytes\n", size_tcp);
		return;
	}

	size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

	CHECK_LEN(size_tcp + size_payload);

	payload = (char *) (packet + size_tcp);

	hdr = sniff_tcp_find_connection(ip, tcp);

	debug_function("sniff_loop_tcp() IP/TCP %15s:%5d <==> %15s:%5d %s (SEQ: %lx ACK: %lx len: %d)\n", 
			_inet_ntoa(hdr->srcip),		/* src ip */
			hdr->srcport,			/* src port */
			_inet_ntoa(hdr->dstip),		/* dest ip */
			hdr->dstport, 			/* dest port */
			tcp_print_flags(tcp->th_flags), /* tcp flags */
			htonl(tcp->th_seq), 		/* seq */
			htonl(tcp->th_ack), 		/* ack */
			size_payload);			/* payload len */

	/* XXX check tcp flags */
	if (!size_payload) return;

	/* XXX what proto ? check based on ip + port? */

	if (hdr->dstport == 80 || hdr->srcport == 80) {		/* HTTP		[basic check on magic values, ~80% hit] */
		static const char http_magic11[] = { 'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ' };	/* HTTP/1.1 */
		static const char http_magic10[] = { 'H', 'T', 'T', 'P', '/', '1', '.', '0', ' ' };	/* HTTP/1.0 */

		static const char http_get_magic[] = { 'G', 'E', 'T', ' ' };				/* GET */
		static const char http_post_magic[] = { 'P', 'O', 'S', 'T', ' ' };			/* POST */

		/* SERVER REPLIES: */

		if (	(size_payload > sizeof(http_magic10) && !memcmp(payload, http_magic10, sizeof(http_magic10))) ||
				(size_payload > sizeof(http_magic11) && !memcmp(payload, http_magic11, sizeof(http_magic11)))
		   ) {
//				debug_error("HTTP DATA FOLLOW\n");
//				tcp_print_payload((u_char *) payload, size_payload);

			return;		/* done */
		}


		/* CLIENT REQUESTs: */

		if (	(size_payload > sizeof(http_get_magic) && !memcmp(payload, http_get_magic, sizeof(http_get_magic))) ||
				(size_payload > sizeof(http_post_magic) && !memcmp(payload, http_post_magic, sizeof(http_post_magic)))

		   ) {
//				debug_error("HTTP DATA FOLLOW?\n");
//				tcp_print_payload((u_char *) payload, size_payload);

			return;		/* done */
		}
	}

	sniff_gg(s, hdr, (gg_header *) payload, size_payload);		/* GG		[no check, ~3% hit] */
}

static inline void sniff_loop_udp(session_t *s, int len, const u_char *packet, const struct iphdr *ip) {
#define	RIVCHAT_PACKET_LEN 328
	static const char rivchat_magic[11] = { 'R', 'i', 'v', 'C', 'h', 'a', 't' /* here NULs */};	/* RivChat\0\0\0\0 */

	/* XXX here, make some struct with known UDP services, and demangler-function */
	const struct udphdr *udp;
	connection_t *hdr;

	const char *payload;
	int size_payload;

	/* XXX, it's enough? */
	/* code copied from: http://gpsbots.com/tutorials/sniff_packets.php */
	CHECK_LEN(sizeof(struct udphdr));	udp = (struct udphdr *) (packet);

	hdr = sniff_udp_get(ip, udp);

	payload = (char *) (packet + sizeof(struct udphdr));
	size_payload = ntohs(udp->th_len)-sizeof(struct udphdr);

	CHECK_LEN(sizeof(struct udphdr) + size_payload);

	debug_error("sniff_loop_udp() IP/UDP %15s:%5d <==> %15s:%5d\n",
			_inet_ntoa(hdr->srcip),		/* src ip */
			hdr->srcport,			/* src port */
			_inet_ntoa(hdr->dstip),		/* dest ip */
			hdr->dstport); 			/* dest port */

	if (size_payload == RIVCHAT_PACKET_LEN && !memcmp(payload, rivchat_magic, sizeof(rivchat_magic))) {		/* RIVCHAT	[check based on header (11b), ~100% hit] */
		sniff_rivchat(s, hdr, (rivchat_packet *) payload, size_payload);
	} else if (hdr->srcport == 53 || hdr->dstport == 53) {								/* DNS		[check based on port, ~80% hit] */
		sniff_dns(s, hdr, (DNS_HEADER *) payload, size_payload);
	} else {													/* OTHER PROTOs, feel free */
		debug_error("NOT RIVCHAT/ NOT DNS:\n");
		tcp_print_payload((u_char *) payload, size_payload);
	}
}

static inline void sniff_loop_ip(session_t *s, int len, const u_char *packet) {
	const struct iphdr *ip;
	int size_ip;

	CHECK_LEN(sizeof(struct iphdr))			ip = (struct iphdr *) (packet);
	size_ip = ip->ip_hl*4;
	
	if (size_ip < 20) {
		debug_error("sniff_loop_ip()   * Invalid IP header length: %u bytes\n", size_ip);
		return;
	}
	
	if (ip->ip_p == IPPROTO_TCP)
		sniff_loop_tcp(s, len - size_ip, packet + size_ip, ip, size_ip);
	else if (ip->ip_p == IPPROTO_UDP) 
		sniff_loop_udp(s, len - size_ip, packet + size_ip, ip);
	else if (ip->ip_p == IPPROTO_ICMP) {	/* ICMP, stub only */
		const struct icmphdr *icmp;

		CHECK_LEN(size_ip + sizeof(struct icmphdr));	icmp = (struct icmphdr *) (packet + size_ip);

		debug_function("sniff_loop_ip() IP/ICMP %15s <==> %15s TYPE: %d CODE: %d CHKSUM: %d\n",
				_inet_ntoa(ip->ip_src),		/* src ip */
				_inet_ntoa(ip->ip_dst),		/* dest ip */
				icmp->icmp_type,
				icmp->icmp_code,
				icmp->icmp_cksum);
		/* XXX */
	} else {
		/* other, implement if u want to || die. */
		debug_error("sniff_loop_ip() IP/0x%x %15s <==> %15s\n",
				ip->ip_p,			/* protocol */
				_inet_ntoa(ip->ip_src),		/* src ip */
				_inet_ntoa(ip->ip_dst));	/* dest ip */

	}
}

#undef CHECK_LEN

static inline void sniff_loop_ether(u_char *data, const struct pcap_pkthdr *header, const u_char *packet) {
	const struct ethhdr *ethernet;
	uint16_t ethtype;		/* ntohs(ethernet->ether_type) */

	if (header->caplen < sizeof(struct ethhdr)) {
		debug_error("sniff_loop_ether() %x %x\n", header->caplen, sizeof(struct ethhdr));
		return;
	}

	ethernet = (const struct ethhdr *) packet;
	ethtype = ntohs(ethernet->ether_type);

	if (ethtype == ETHERTYPE_ARP)
		debug_function("sniff_loop_ether() ARP\n");
	else if (ethtype == ETHERTYPE_IP) 
		sniff_loop_ip((session_t *) data, header->caplen - sizeof(struct ethhdr), packet + SIZE_ETHERNET);
	else
		debug_error("sniff_loop_ether() ethtype [0x%x] != ETHERTYPE_IP, CUL\n", ethtype);
}

void sniff_loop_sll(u_char *data, const struct pcap_pkthdr *header, const u_char *packet) {
	const struct sll_header *sll;
	uint16_t ethtype;

	if (header->caplen < sizeof(struct sll_header)) {
		debug_error("sniff_loop_ssl() %x %x\n", header->caplen, sizeof(struct sll_header));
		return;
	}

	sll = (const struct sll_header *) packet;
	ethtype = ntohs(sll->sll_protocol);
	
	if (ethtype == ETHERTYPE_IP) 
		sniff_loop_ip((session_t *) data, header->caplen - sizeof(struct sll_header), packet + SIZE_SLL);
	else
		debug_error("sniff_loop_sll() ethtype [0x%x] != ETHERTYPE_IP, CUL\n", ethtype);
}

#define WATCHER_PCAP(x, y)						\
	static WATCHER_SESSION(x) {					\
		if (type) return 0;					\
		if (!s) {						\
			debug_error("sniff_pcap_read() no session!\n");	\
			return -1;					\
		}							\
		pcap_dispatch(GET_DEV(s), 1, y, (void *) s);		\
		return 0;						\
	}
	

WATCHER_PCAP(sniff_pcap_read_EN10MB, sniff_loop_ether);
WATCHER_PCAP(sniff_pcap_read_SLL, sniff_loop_sll);
WATCHER_PCAP(sniff_pcap_read, sniff_loop_ether);

#define DEFAULT_FILTER \
	"(tcp and (net 217.17.41.80/28 or net 217.17.45.128/27 or net 85.232.236.0/24 or net 91.197.12.0/22)) or (udp and (port 16127 or port 53))"

static COMMAND(sniff_command_connect) {
	struct bpf_program fp;
	char errbuf[PCAP_ERRBUF_SIZE] = { 0 };
	pcap_t *dev;
	const char *filter;
	char *device;
	char *tmp;

	filter = session_get(session, "filter");

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (session->uid[6] != '/') {
		if ((tmp = xstrchr(session->uid+6, ':')))
			device = xstrndup(session->uid+6, tmp-(session->uid+6));
		else	device = xstrdup(session->uid+6);

		dev = pcap_open_live(device, SNAPLEN, PROMISC, 1000, errbuf);
	} else {
		device = xstrdup(session->uid+6);
		dev = pcap_open_offline(device, errbuf);
	}

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
	if (filter && *filter) {
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
		/* pcap_freecode(&fp); */
	}

	session->priv = dev;
	
	switch (pcap_datalink(dev)) {
		case DLT_LINUX_SLL:
			watch_add_session(session, pcap_fileno(dev), WATCH_READ, sniff_pcap_read_SLL);
			break;

		case DLT_EN10MB:
			watch_add_session(session, pcap_fileno(dev), WATCH_READ, sniff_pcap_read_EN10MB);
			break;

		default:
			debug_error("_connect() unk: %s\n", pcap_datalink_val_to_name(pcap_datalink(dev)));
			watch_add_session(session, pcap_fileno(dev), WATCH_READ, sniff_pcap_read);
	}
	

	session->status = EKG_STATUS_AVAIL;
	query_emit_id(NULL, PROTOCOL_CONNECTED, &(session->uid));
	return 0;
}

static COMMAND(sniff_command_disconnect) {
	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	{
		char *uid = session->uid;
		char *reason = NULL;
		int type = EKG_DISCONNECT_USER;

		query_emit_id(NULL, PROTOCOL_DISCONNECTED, &uid, &reason, &type);
	}

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

		print_window("__status", session, EKG_WINACT_MSG, 1,
			"sniff_tcp_connection", 
				inet_ntop(AF_INET, &c->srcip, src_ip, sizeof(src_ip)),
				itoa(c->srcport),
				inet_ntop(AF_INET, &c->dstip, dst_ip, sizeof(dst_ip)),
				itoa(c->dstport));
	}
	return 0;
}

static QUERY(sniff_session_deinit) {
	char *session = *(va_arg(ap, char**));
	session_t *s = session_find(session);

	if (!s || !s->priv || s->plugin != &sniff_plugin)
		return 1;

	debug("sniff closing pcap dev: 0x%x\n", s->priv);
	pcap_close(GET_DEV(s));	

	s->priv = NULL;
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
	format_add("sniff_gg_welcome",	 ("%) %b[GG_WELCOME] %gSEED: %W%1"), 1);
	format_add("sniff_gg_login60",	 ("%) %b[GG_LOGIN60] %gUIN: %W%1 %gHASH: %W%2"), 1);

	format_add("sniff_gg_login70_sha1",	 ("%) %b[GG_LOGIN70] %gUIN: %W%1 %gSHA1: %W%2"), 1);
	format_add("sniff_gg_login70_hash",	 ("%) %b[GG_LOGIN70] %gUIN: %W%1 %gHASH: %W%2"), 1);
	format_add("sniff_gg_login70_unknown",	 ("%) %b[GG_LOGIN70] %gUIN: %W%1 %gTYPE: %W%2"), 1);

	format_add("sniff_gg_userlist_req",	 ("%) %b[GG_USERLIST_REQUEST] %gTYPE: %W%1 (%2)"), 1);
	format_add("sniff_gg_userlist_reply",	 ("%) %b[GG_USERLIST_REPLY] %gTYPE: %W%1 (%2)"), 1);

	format_add("sniff_gg_userlist_data",	 ("%)   %b[%1] %gENTRY: %W%2"), 1);

	format_add("sniff_gg_list",		 ("%) %b[%1] %gLEN: %W%2"), 1);
	format_add("sniff_gg_list_data",	 ("%)   %b[%1] %gENTRY: %W%2 %gTYPE: %W%3"), 1);

	format_add("sniff_gg_pubdir50_req",	 ("%) %b[GG_PUBDIR50_REQUEST] %gTYPE: %W%1 (%2) %gSEQ: %W%3"), 1);
	format_add("sniff_gg_pubdir50_reply",	 ("%) %b[GG_PUBDIR50_REPLY] %gTYPE: %W%1 (%2) %gSEQ: %W%3"), 1);
	format_add("sniff_gg_disconnecting",	 ("%) %b[GG_DISCONNECTING]"), 1);

	format_add("sniff_gg_status60",  ("%) %b[GG_STATUS60] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_notify60",  ("%) %b[GG_NOTIFY60] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);

	format_add("sniff_gg_status77",  ("%) %b[GG_STATUS77] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_notify77",  ("%) %b[GG_NOTIFY77] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);

	format_add("sniff_gg_login80_sha1",	 ("%) %b[GG_LOGIN80] %gUIN: %W%1 %gSHA1: %W%2"), 1);
	format_add("sniff_gg_login80_hash",	 ("%) %b[GG_LOGIN80] %gUIN: %W%1 %gHASH: %W%2"), 1);	/* untested */
	format_add("sniff_gg_login80_unknown",	 ("%) %b[GG_LOGIN80] %gUIN: %W%1 %gTYPE: %W%2"), 1);

	format_add("sniff_gg_status80",  ("%) %b[GG_STATUS80] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);
	format_add("sniff_gg_notify80",  ("%) %b[GG_NOTIFY80] %gDCC: %W%1:%2 %gVERSION: %W#%3 (%4) %gIMGSIZE: %W%5KiB"), 1);

	format_add("sniff_gg_addnotify", ("%) %b[GG_ADD_NOTIFY] %gUIN: %W%1 %gDATA: %W%2"), 1);
	format_add("sniff_gg_delnotify", ("%) %b[GG_REMOVE_NOTIFY] %gUIN: %W%1 %gDATA: %W%2"), 1);
	
/* sniff dns */
	format_add("sniff_dns_reply",		("%) %b[SNIFF_DNS, %r%1%b] %gDOMAIN: %W%2"), 1);
	format_add("sniff_dns_entry_a",		("%)         %b[IN_A] %gDOMAIN: %W%1 %gIP: %W%2"), 1);
	format_add("sniff_dns_entry_aaaa",	("%)      %b[IN_AAAA] %gDOMAIN: %W%1 %gIP6: %W%2"), 1);
	format_add("sniff_dns_entry_cname",	("%)     %b[IN_CNAME] %gDOMAIN: %W%1 %gCNAME: %W%2"), 1);
	format_add("sniff_dns_entry_ptr",	("%)       %b[IN_PTR] %gIP_PTR: %W%1 %gDOMAIN: %W%2"), 1);
	format_add("sniff_dns_entry_mx",	("%)        %b[IN_MX] %gDOMAIN: %W%1 %gENTRY: %W%2 %gPREF: %W%3"), 1);
	format_add("sniff_dns_entry_srv",	("%)       %b[IN_SRV] %gDOMAIN: %W%1 %gENTRY: %W%2 %gPORT: %W%3 %gPRIO: %W%4 %gWEIGHT: %W%5"), 1);
	format_add("sniff_dns_entry_?",		("%)         %b[IN_?] %gDOMAIN: %W%1 %gTYPE: %W%2 %gLEN: %W%3"), 1);
	format_add("sniff_dns_entry_ndisplay",	("%)   %rZADEN REKORD NIE WYSWIETLONY DLA ZAPYTANIE POWYZEJ ;), OBEJRZYJ DEBUG"), 1);

/* sniff rivchat */
	format_add("sniff_rivchat_init",	("%) %b[RIVCHAT_INIT, %r%1%b]"), 1);
	format_add("sniff_rivchat_me",		("%) %b[RIVCHAT_ME, %r%1%b] %W* %2 %3"), 1);
	format_add("sniff_rivchat_away",	("%) %b[RIVCHAT_AWAY, %r%1%b] %gREASON: %W%2"), 1);
	format_add("sniff_rivchat_quit",	("%) %b[RIVCHAT_QUIT, %r%1%b]"), 1);
	format_add("sniff_rivchat_pingaway",	("%) %b[RIVCHAT_PINGAWAY, %r%1%b] %gREASON: %W%2"), 1);
	format_add("sniff_rivchat_message",	("%) %b[RIVCHAT_MESSAGE, %r%1%b] <%2> %W%3"), 1);
	format_add("sniff_rivchat_rcinfo",	("%) %b[RIVCHAT_INFO, %r%1%b] %gFINGER: %W%2@%3 %gOS: %W%4 %gPROGRAM: %W%5 %6"), 1);

/* stats */
	format_add("sniff_pkt_rcv", 		("%) %2 packets captured"), 1);
	format_add("sniff_pkt_drop",		("%) %2 packets dropped"), 1);

	format_add("sniff_conn_db", 		("%) %2 connections founded"), 1);
	format_add("sniff_tcp_connection",	"TCP %1:%2 <==> %3:%4", 1);

	return 0;
}

static plugins_params_t sniff_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("auto_connect", 		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("filter", 		VAR_STR, DEFAULT_FILTER, 0, NULL),

	PLUGIN_VAR_END()
};

EXPORT int sniff_plugin_init(int prio) {
	PLUGIN_CHECK_VER("sniff");

	sniff_plugin.params = sniff_plugin_vars;
	plugin_register(&sniff_plugin, prio);

	query_connect_id(&sniff_plugin, PROTOCOL_VALIDATE_UID,	sniff_validate_uid, NULL);
	query_connect_id(&sniff_plugin, STATUS_SHOW, 		sniff_status_show, NULL);
	query_connect_id(&sniff_plugin, PLUGIN_PRINT_VERSION,	sniff_print_version, NULL);
	query_connect_id(&sniff_plugin, SESSION_REMOVED,	sniff_session_deinit, NULL);

	command_add(&sniff_plugin, "sniff:connect", NULL, sniff_command_connect,    SESSION_MUSTBELONG, NULL);
	command_add(&sniff_plugin, "sniff:connections", NULL, sniff_command_connections, SESSION_MUSTBELONG | SESSION_MUSTBECONNECTED, NULL);
	command_add(&sniff_plugin, "sniff:disconnect", NULL,sniff_command_disconnect, SESSION_MUSTBELONG, NULL);

	return 0;
}

static int sniff_plugin_destroy() {
	plugin_unregister(&sniff_plugin);
	return 0;
}

