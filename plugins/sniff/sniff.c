#include "ekg2-config.h"

#include <stdio.h>
#include <signal.h>
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
#include <ekg/themes.h>

#include <ekg/queries.h>
#include <ekg/xmalloc.h>

#include "sniff_ip.h"
#include "sniff_gg.h"

PLUGIN_DEFINE(sniff, PLUGIN_PROTOCOL, NULL);

#define DEVICE "eth0"
#define SNAPLEN 2000
#define PROMISC 0

#define GET_DEV(s) ((pcap_t *) ((session_t *) s)->priv)

typedef struct {
	struct in_addr srcip;
	uint16_t srcport;

	struct in_addr dstip;
	uint16_t dstport;
} sniff_data_t;

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

static sniff_data_t *build_header(sniff_data_t *d, const struct iphdr *ip, const struct tcphdr *tcp) {
	d->srcip	= ip->ip_src;
	d->srcport	= ntohs(tcp->th_sport);
	
	d->dstip	= ip->ip_dst;
	d->dstport	= ntohs(tcp->th_dport);

	return d;
}

/* stolen from gg plugin */
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

/*  ****************************************************** */

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

#define CHECK_LEN(x) \
	if (len < x) {\
		debug_error("%s()  * READ less than: %d (len: %d) (%s)\n", __FUNCTION__, x, len, #x);\
		return -1;\
	}


static int sniff_gg_recv_msg(session_t *s, const sniff_data_t *hdr, const gg_recv_msg *pkt_msg, int len) {
	const char *sender;
	char *msg;

	CHECK_LEN(sizeof(gg_recv_msg))	len -= sizeof(gg_recv_msg);

	sender = build_gg_uid(pkt_msg->sender);
	msg = gg_cp_to_iso(xstrndup(pkt_msg->msg_data, len));

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

static int sniff_gg_send_msg(session_t *s, const sniff_data_t *hdr, const gg_send_msg *pkt_msg, int len) {
	const char *sender;
	char *msg;

	CHECK_LEN(sizeof(gg_send_msg))  len -= sizeof(gg_send_msg);

	sender = build_gg_uid(pkt_msg->recipient);
	msg = gg_cp_to_iso(xstrndup(pkt_msg->msg_data, len));

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

static int sniff_gg_send_msg_ack(session_t *s, const sniff_data_t *hdr, const gg_send_msg_ack *pkt_ack, int len) {
	CHECK_LEN(sizeof(gg_send_msg_ack))	len -= sizeof(gg_send_msg_ack);
	
	debug_function("sniff_gg_send_msg_ack() uid:%d %d %d\n", pkt_ack->recipient, pkt_ack->status, pkt_ack->seq);

	return 0;
}

/* return 0 on success */
int sniff_gg(session_t *s, const sniff_data_t *hdr, const gg_header *pkt, int len) {
	CHECK_LEN(sizeof(gg_header)) 	len -= sizeof(gg_header);
/* XXX, tcp fragmentation!!!!!!1111 */
	CHECK_LEN(pkt->len)

	debug_function("sniff_gg() rcv pkt type: %d len: %d next: %d\n", pkt->type, pkt->len, !(pkt->len == len));
	if (!(pkt->len == len)) 
		debug_error("sniff_gg() XXX NEXT PACKET?!\n");

/* XXX, check direction!!!!!111 */
	switch (pkt->type) {
#if 0
		case GG_WELCOME:
			return sniff_gg_welcome(s, hdr, (gg_welcome *) pkt->data, pkt->len);	/* OUTGOING */
#endif
		case GG_RECV_MSG:
			return sniff_gg_recv_msg(s, hdr, (gg_recv_msg *) pkt->data, pkt->len);	/* INCOMING */
		case GG_SEND_MSG:
			return sniff_gg_send_msg(s, hdr, (gg_send_msg *) pkt->data, pkt->len);	/* OUTGOING */
		case GG_SEND_MSG_ACK:
			return sniff_gg_send_msg_ack(s, hdr, (gg_send_msg_ack *) pkt->data, pkt->len);	/* INCOMING */
		case GG_PING:
			debug_function("sniff_gg() rcv GG_PING ip: %s\n", inet_ntoa(hdr->srcip));	/* OUTGOING */
			return 0;
		case GG_PONG:
			debug_function("sniff_gg() rcv GG_PONG ip: %s\n", inet_ntoa(hdr->dstip));	/* INCOMING */
			return 0;

		default:
			debug_error("sniff_gg() UNHANDLED pkt type: %x\n", pkt->type);
/*			print_payload(gg_hdr->pakiet, gg_hdr->len); */
	}

	return 0;
}

#undef CHECK_LEN
void sniff_loop(void *data, const struct pcap_pkthdr *header, const u_char *packet) {
	const struct ethhdr *ethernet;
	const struct iphdr *ip;
	const struct tcphdr *tcp;

	sniff_data_t hdr;
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

static QUERY(sniff_status_show) {
	return 0;
}

static QUERY(sniff_print_version) {
	print("generic", pcap_lib_version());
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
