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

#include "sniff_ip.h"
#include "sniff_gg.h"

static int sniff_theme_init();
PLUGIN_DEFINE(sniff, PLUGIN_PROTOCOL, sniff_theme_init);

#define DEVICE "eth0"
#define SNAPLEN 2000
#define PROMISC 0

#define GET_DEV(s) ((pcap_t *) ((session_t *) s)->priv)

typedef struct {
	struct in_addr srcip;
	uint16_t srcport;

	struct in_addr dstip;
	uint16_t dstport;
} connection_t;

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

static connection_t *build_header(connection_t *d, const struct iphdr *ip, const struct tcphdr *tcp) {
	d->srcip	= ip->ip_src;
	d->srcport	= ntohs(tcp->th_sport);
	
	d->dstip	= ip->ip_dst;
	d->dstport	= ntohs(tcp->th_dport);

	return d;
}

/* stolen from libgadu+gg plugin */
static const char *gg_status_to_text(int status, int *descr) {
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

	buf[xstrlen(buf)-1] = 0;
	
	return buf;
}

/*  ****************************************************** */

#define SNIFF_HANDLER(x, type) static int x(session_t *s, const connection_t *hdr, const type *pkt, int len)
typedef int (*sniff_handler_t)(session_t *, const connection_t *, const unsigned char *, int);

#define CHECK_LEN(x) \
	if (len < x) {\
		debug_error("%s()  * READ less than: %d (len: %d) (%s)\n", __FUNCTION__, x, len, #x);\
		return -1;\
	}

SNIFF_HANDLER(sniff_gg_recv_msg, gg_recv_msg) {
	const char *sender;
	char *msg;

	CHECK_LEN(sizeof(gg_recv_msg))	len -= sizeof(gg_recv_msg);

	sender = build_gg_uid(pkt->sender);
	msg = gg_cp_to_iso(xstrndup(pkt->msg_data, len));

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1, 
		"message", 			/* formatka */

		format_user(s, sender),			/* sender */
		"timestamp", 				/* timestamp */
		msg,					/* wiadomosc */
		get_nickname(s, sender),		/* jego nickname */
		sender,					/* jego uid */
		"");					/* secure */
	xfree(msg);

	return 0;
}

SNIFF_HANDLER(sniff_gg_send_msg, gg_send_msg) {
	const char *sender;
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg))  len -= sizeof(gg_send_msg);

	sender = build_gg_uid(pkt->recipient);
	msg = gg_cp_to_iso(xstrndup(pkt->msg_data, len));

	print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
			"sent",                      /* formatka */

			format_user(s, sender),                 /* sender */
			"timestamp",                            /* timestamp */
			msg,                                    /* wiadomosc */
			get_nickname(s, sender),                /* jego nickname */
			sender,                                 /* jego uid */
			"");                                    /* secure */
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
		case GG_ACK_DELIVERED:
			format = "ack_delivered";
			break;
		case GG_ACK_QUEUED:
			format = "ack_queued";
			break;
		case GG_ACK_NOT_DELIVERED:
			format = "ack_filtered";
			break;
		default:
			format = "ack_unknown";
			debug("[sniff,gg] unknown message ack status. consider upgrade\n");
			break;
	}
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
			format, 
			format_user(s, build_gg_uid(pkt->recipient)));	/* XXX */
	return 0;
}

SNIFF_HANDLER(sniff_gg_welcome, gg_welcome) {
	char *key_hex;
	CHECK_LEN(sizeof(gg_welcome))		len -= sizeof(gg_welcome);

	key_hex = saprintf("0x%x\n", pkt->key);
	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1,
			"sniff_gg_welcome",

			key_hex);
	xfree(key_hex);
	return 0;
}

SNIFF_HANDLER(sniff_gg_status, gg_status) {
	const char *status;
	char *descr;
	int has_descr;

	CHECK_LEN(sizeof(gg_status))		len -= sizeof(gg_status);

/* XXX, update w->userlist->{user}->status/descr if have */
	status	= gg_status_to_text(pkt->status, &has_descr);
	descr	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;

	if (!has_descr && len > 0)
		debug_error("sniff_gg_status() !has_descr but len > 0?! (%d)\n", len);

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1, 
		ekg_status_label(status, descr, "status_"), /* formatka */

		format_user(s, build_gg_uid(pkt->uin)),		/* od */
		NULL, 						/* nickname, realname */
		session_name(s), 				/* XXX! do */
		descr);						/* status */

	xfree(descr);

	return 0;
}

SNIFF_HANDLER(sniff_gg_new_status, gg_new_status) {
	const char *status;
	char *descr;
	int has_descr;

	CHECK_LEN(sizeof(gg_new_status))	len -= sizeof(gg_new_status);

/* XXX, update s->status/descr */
	status	= gg_status_to_text(pkt->status, &has_descr);

	if (!xstrcmp(status, EKG_STATUS_AVAIL)) 		status = "back";
	else if (!xstrcmp(status, EKG_STATUS_AWAY))		status = "away";
	else if (!xstrcmp(status, EKG_STATUS_INVISIBLE))	status = "invsible";
	else {
/* XXX, rozlaczony */
		debug_error("sniff_gg_new_status() bad status: %s\n", status);
		return -5;
	}

	descr	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;

	if (!has_descr && len > 0)
		debug_error("sniff_gg_new_status() !has_descr but len > 0?! (%d)\n", len);
/* XXX tajm */

/* XXX, session_name(s) is wrong here. */
	if (descr) {
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
				ekg_status_label(status, descr, NULL), /* formatka */

				descr, "", session_name(s));
	} else 
		print_window(build_windowip_name(hdr->srcip) /* ip and/or gg# */, s, 1,
				ekg_status_label(status, descr, NULL), /* formatka */

				session_name(s));


	return -5;
}

SNIFF_HANDLER(sniff_gg_status60, gg_status60) {
	uint32_t uin;
	const char *status;
	char *descr;

	int has_time = 0;
	int has_descr = 0;

	CHECK_LEN(sizeof(gg_status60))		len -= sizeof(gg_status60);

/* XXX, tajm */
#if 0
	if (len > 4 && pkt->status_data[len - 5] == 0) {
		has_time = 1;
		len -= 5;
	}
#endif
	uin	= pkt->uin & 0x00ffffff;

	status	= gg_status_to_text(pkt->status, &has_descr);
	descr 	= has_descr ? gg_cp_to_iso(xstrndup(pkt->status_data, len)) : NULL;

/* XXX, update w->userlist */
	if (!has_descr && len > 0)
		debug_error("sniff_gg_status60() !has_descr but len > 0?!\n");

	if (has_time)
		debug_error("sniff_gg_status60() HAS_TIME?!\n");

	print_window(build_windowip_name(hdr->dstip) /* ip and/or gg# */, s, 1, 
			ekg_status_label(status, descr, "status_"), /* formatka */

			format_user(s, build_gg_uid(uin)), 		/* od */
			NULL, 						/* nickname, realname */
			session_name(s), 				/* XXX! do */
			descr);						/* status */

	xfree(descr);
	return 0;
}

typedef enum {
	SNIFF_OUTGOING = 0,
	SNIFF_INCOMING
} pkt_way_t;

static const struct {
	uint32_t 	type;
	char 		*name;
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
	{ -1,		NULL,		-1,		(void *) NULL, 0},
};

/* return 0 on success */
SNIFF_HANDLER(sniff_gg, gg_header) {
	int i;
	pkt_way_t way = SNIFF_OUTGOING;

	CHECK_LEN(sizeof(gg_header)) 	len -= sizeof(gg_header);
	/* XXX, tcp fragmentation!!!!!!1111 */
	CHECK_LEN(pkt->len)

	/* XXX, check direction!!!!!111, in better way: */
	if (!xstrncmp(inet_ntoa(hdr->srcip), "217.17.", 7))
		way = SNIFF_INCOMING;

	if (!(pkt->len == len)) 
		debug_error("sniff_gg() XXX NEXT PACKET?!\n");

	for (i=0; sniff_gg_callbacks[i].name; i++) {
		if (sniff_gg_callbacks[i].type == pkt->type && sniff_gg_callbacks[i].way == way) {
			debug("sniff_gg() %s [%d,%d,%db] %s\n", sniff_gg_callbacks[i].name, pkt->type, way, pkt->len, inet_ntoa(way ? hdr->dstip : hdr->srcip));
			if (sniff_gg_callbacks[i].handler) 
				return sniff_gg_callbacks[i].handler(s, hdr, pkt->data, pkt->len);
			return 0;
		}
	}
	debug_error("sniff_gg() UNHANDLED pkt type: %x way: %d len: %db\n", pkt->type, way, pkt->len);
/*	print_payload(gg_hdr->pakiet, gg_hdr->len); */
	return -2;
}

#undef CHECK_LEN
void sniff_loop(void *data, const struct pcap_pkthdr *header, const u_char *packet) {
	const struct ethhdr *ethernet;
	const struct iphdr *ip;
	const struct tcphdr *tcp;

	connection_t hdr;
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

	build_header(&hdr, ip, tcp);

	debug_function("sniff_loop() %15s:%5d <==> ", 
			inet_ntoa(hdr.srcip), 		/* src ip */
			hdr.srcport);			/* src port */

	debug_function("%15s:%5d %s (SEQ: %lx ACK: %lx len: %d)\n", 
			inet_ntoa(hdr.dstip), 		/* dest ip */
			hdr.dstport, 			/* dest port */
			tcp_print_flags(tcp->th_flags), /* tcp flags */
			htonl(tcp->th_seq), 		/* seq */
			htonl(tcp->th_ack), 		/* ack */
			size_payload);			/* payload len */

/* XXX check tcp flags */
	if (!size_payload) return;
/* XXX what proto ? check based on ip + port? */
	sniff_gg((session_t *) data, &hdr, (gg_header *) payload, size_payload);
#undef CHECK_LEN
}

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

	if (!(filter = session_get(session, "filter")))
		filter = DEFAULT_FILTER;

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	dev = pcap_open_live(DEVICE, SNAPLEN, PROMISC, 1000, errbuf);

	if (!dev) {
		debug_error("Couldn't open dev: %s\n", DEVICE);
		return -1;
	}

	if (pcap_setnonblock(dev, 1, errbuf) == -1) {
		debug_error("Could not set device \"%s\" to non-blocking: %s\n", DEVICE, errbuf);
		pcap_close(dev);
		return -1;
	}

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

	xfree(session->status); session->status = xstrdup(EKG_STATUS_AVAIL);
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

static QUERY(sniff_validate_uid) {
	char    *uid    = *(va_arg(ap, char **));
	int     *valid  = va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncasecmp(uid, "sniff", 5) && uid[5]) {
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

	return 0;
}

static QUERY(sniff_print_version) {
	print("generic", pcap_lib_version());
	return 0;
}

static int sniff_theme_init() {
/* sniff gg */
	format_add("sniff_gg_welcome",	_("%> [GG_WELCOME] SEED: %1"), 1);
/* stats */
	format_add("sniff_pkt_rcv", _("%) %2 packets captured"), 1);
	format_add("sniff_pkt_drop",_("%) %2 packets dropped"), 1);

	return 0;
}

int sniff_plugin_init(int prio) {
	plugin_register(&sniff_plugin, prio);

	query_connect_id(&sniff_plugin, PROTOCOL_VALIDATE_UID,	sniff_validate_uid, NULL);
	query_connect_id(&sniff_plugin, STATUS_SHOW, 		sniff_status_show, NULL);
	query_connect_id(&sniff_plugin, PLUGIN_PRINT_VERSION,	sniff_print_version, NULL);

        command_add(&sniff_plugin, "sniff:connect", NULL, sniff_command_connect,    SESSION_MUSTBELONG, NULL);
	command_add(&sniff_plugin, "sniff:disconnect", NULL,sniff_command_disconnect, SESSION_MUSTBELONG, NULL);

	plugin_var_add(&sniff_plugin, "alias", VAR_STR, 0, 0, NULL);
	plugin_var_add(&sniff_plugin, "auto_connect", VAR_BOOL, "0", 0, NULL);
	plugin_var_add(&sniff_plugin, "filter", VAR_STR, 0, 0, NULL);

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

