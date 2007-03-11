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
	if (status & GG_STATUS_FRIENDS_MASK) status -= GG_STATUS_FRIENDS_MASK;

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
static void sniff_gg_print_message(session_t *s, const connection_t *hdr, uint32_t recpt, enum msgclass_t type, const char *msg) {
	const char *sender = build_gg_uid(recpt);

	print_window(build_windowip_name(type == EKG_MSGCLASS_CHAT ? hdr->dstip : hdr->srcip) /* ip and/or gg# */, s, 1, 
		type == EKG_MSGCLASS_CHAT ? "message" : "sent", 	/* formatka */

		format_user(s, sender),			/* do kogo */
		"timestamp", 				/* XXX timestamp */
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
		sniff_gg_print_message(s, hdr, pkt->sender, EKG_MSGCLASS_CHAT, msg);
	xfree(msg);
	return 0;
}

SNIFF_HANDLER(sniff_gg_send_msg, gg_send_msg) {
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg))  len -= sizeof(gg_send_msg);
	msg = gg_cp_to_iso(xstrndup(pkt->msg_data, len));
		sniff_gg_print_message(s, hdr, pkt->recipient, EKG_MSGCLASS_SENT_CHAT, msg);
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
	CHECK_LEN(sizeof(gg_notify_reply60));	len -= sizeof(gg_notify_reply60);

	debug_error("sniff_notify_reply60() XXX\n");
	return -5;
}

/* nie w libgadu */
#define CHECK_PRINT(is, shouldbe) if (is != shouldbe) {\
		if (sizeof(is) == 2)		debug_error("%s() values not match: %s [%.4x != %.4x]\n", __FUNCTION__, #is, is, shouldbe); \
		else if (sizeof(is) == 4)	debug_error("%s() values not match: %s [%.8x != %.8x]\n", __FUNCTION__, #is, is, shouldbe); \
		else 				debug_error("%s() values not match: %s [%x != %x]\n", __FUNCTION__, #is, is, shouldbe);\
	}
	
#define GG_DCC_NEW_REQUEST_ID 0x23
typedef struct {
	uint32_t type;		/* 04 00 00 00 */
} GG_PACKED gg_dcc_new_request_id_out;

SNIFF_HANDLER(sniff_gg_dcc_new_request_id_out, gg_dcc_new_request_id_out) {
	if (len != sizeof(gg_dcc_new_request_id_out)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	CHECK_PRINT(pkt->type, htonl(0x04000000));
	return 0;
}

typedef struct {
	uint32_t type;		/* 04 00 00 00 */
	unsigned char code1[8];
} GG_PACKED gg_dcc_new_request_id_in;

SNIFF_HANDLER(sniff_gg_dcc_new_request_id_in, gg_dcc_new_request_id_in) {
	if (len != sizeof(gg_dcc_new_request_id_in)) {
		tcp_print_payload((u_char *) pkt, len);
		return -1;
	}

	CHECK_PRINT(pkt->type, htonl(0x04000000));
	debug("sniff_gg_dcc_new_request_id_in() code: %s\n", build_code(pkt->code1));

	return 0;
}

#define GG_DCC_NEW 0x20
typedef struct {
	unsigned char code1[8];
	uint32_t uin1;		/* LE, from */
	uint32_t uin2;		/* LE, to */
	uint32_t dunno1;	/* 04 00 00 00 */
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
	fname = xstrndup(pkt->filename, 226);
	debug("sniff_gg_dcc_new() code: %s uin1: %d uin2: %d fname: %s [%db]\n", 
		build_code(pkt->code1), pkt->uin1, pkt->uin2, fname, pkt->size);
	xfree(fname);

/* CHECK unknown vals.. */
	/* these are known as 0 */
	CHECK_PRINT(pkt->emp1, 0);
	CHECK_PRINT(pkt->emp2, 0);
	CHECK_PRINT(pkt->emp3, 0);
	CHECK_PRINT(pkt->emp4, 0);

	CHECK_PRINT(pkt->dunno1, htonl(0x04000000));
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

#define GG_LOGIN70 0x19
typedef struct {
	uint32_t uin;			/* mój numerek [gg_login60] */
	uint8_t dunno0;			/* 02 */
	unsigned char hash[20];		/* sha1 [haslo i seed] */
	unsigned char unknown[44];	/* ??? duzo 00 ??? */
	uint32_t status;		/* status na dzień dobry [gg_login60] */
	uint32_t version;		/* moja wersja klienta [gg_login60] */
	uint8_t dunno1;			/* 0x00 [gg_login60] */

	uint32_t local_ip;		/* mój adres ip [gg_login60] */
	uint16_t local_port;		/* port, na którym słucham [gg_login60] */

	uint32_t external_ip;		/* XXX */
	uint16_t external_port;		/* XXX */

	uint8_t image_size;		/* maksymalny rozmiar grafiki w KiB [gg_login60] */
	uint8_t dunno2;			/* 0xbe [gg_login60] */
	char status_data[];
} GG_PACKED gg_login70;

SNIFF_HANDLER(sniff_gg_login70, gg_login70) {
	int status;
	char *descr;
	int has_time = 0;	/* XXX */
	int has_descr = 0;
	int print_payload = 0;
	int i; 

	CHECK_LEN(sizeof(gg_login70));	len -= sizeof(gg_login70);

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sniff_gg_login70",

			build_gg_uid(pkt->uin),
			build_sha1(pkt->hash));

	status = gg_status_to_text(pkt->status, &has_descr);
	descr = has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;
	sniff_gg_print_new_status(s, hdr, pkt->uin, status, descr);
	xfree(descr);
	
	debug_error("sniff_gg_login70() XXX ip %d:%d\n", pkt->external_ip, pkt->external_port);

	CHECK_PRINT(pkt->dunno0, 0x02);
	CHECK_PRINT(pkt->dunno1, 0x00);
	CHECK_PRINT(pkt->dunno2, 0xbe);

	for (i = 0; i < sizeof(pkt->unknown); i++)
		if (pkt->unknown[i] != 0) {
			print_payload = 1;
			break;
		}

	if (print_payload)
		tcp_print_payload((u_char *) pkt->unknown, sizeof(pkt->unknown));
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
	{ GG_LIST_EMPTY,"GG_LIST_EMPTY",SNIFF_INCOMING, (void *) NULL, 0},		/* XXX */
	{ GG_STATUS60,	"GG_STATUS60",	SNIFF_INCOMING, (void *) sniff_gg_status60, 0},
	{ GG_NEED_EMAIL,"GG_NEED_EMAIL",SNIFF_INCOMING, (void *) NULL, 0},		/* XXX */
	{ GG_LOGIN60,	"GG_LOGIN60",	SNIFF_OUTGOING, (void *) sniff_gg_login60, 0},

	{ GG_ADD_NOTIFY,	"GG_ADD_NOTIFY",	SNIFF_OUTGOING, (void *) sniff_gg_add_notify, 0},
	{ GG_REMOVE_NOTIFY,	"GG_REMOVE_NOTIFY", 	SNIFF_OUTGOING, (void *) sniff_gg_del_notify, 0},
	{ GG_NOTIFY_REPLY60,	"GG_NOTIFY_REPLY60",	SNIFF_INCOMING, (void *) sniff_notify_reply60, 0}, 
	{ GG_NOTIFY_FIRST,	"GG_NOTIFY_FIRST",	SNIFF_OUTGOING, (void *) NULL, 0}, /* XXX */
	{ GG_NOTIFY_LAST,	"GG_NOTIFY_LAST",	SNIFF_OUTGOING, (void *) NULL, 0}, /* XXX */

/* pakiety nie w libgadu: */
	{ GG_LOGIN70,		"GG_LOGIN70",		SNIFF_OUTGOING, (void *) sniff_gg_login70, 0},

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
void sniff_loop(void *data, const struct pcap_pkthdr *header, const u_char *packet) {
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

static WATCHER(sniff_pcap_read) {
	if (type) {
		return 0;
	}

	if (!data) {
		debug_error("sniff_pcap_read() no session!\n");
		return -1;
	}
	pcap_dispatch(GET_DEV(data), 1, sniff_loop, data);
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

	watch_add(&sniff_plugin, pcap_fileno(dev), WATCH_READ, sniff_pcap_read, session);

	session->status = EKG_STATUS_AVAIL;
	session->connected = 1;
	session->last_conn = time(NULL);
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
	format_add("sniff_gg_welcome",	_("%) [GG_WELCOME] SEED: %1"), 1);
	format_add("sniff_gg_login60",	_("%) [GG_LOGIN60] UIN: %1 HASH: %2"), 1);
	format_add("sniff_gg_login70",	_("%) [GG_LOGIN70] UIN: %1 SHA1: %2"), 1);
	format_add("sniff_gg_addnotify",_("%) [GG_ADD_NOTIFY] UIN: %1 DATA: %2"), 1);
	format_add("sniff_gg_delnotify",_("%) [GG_REMOVE_NOTIFY] UIN: %1 DATA: %2"), 1);
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

int sniff_plugin_init(int prio) {
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

