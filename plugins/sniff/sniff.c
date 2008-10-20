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
 *	there're copyrighted under GPL-2 */

#include "ekg2-config.h"

#define _GNU_SOURCE
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
	const char *tmp2;

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
		len	-= display_len;
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

#define SNIFF_HANDLER(x, type) static int x(session_t *s, const connection_t *hdr, const type *pkt, int len)
typedef int (*sniff_handler_t)(session_t *, const connection_t *, const unsigned char *, int);

#define CHECK_LEN(x) \
	if (len < x) {\
		debug_error("%s()  * READ less than: %d (len: %d) (%s)\n", __FUNCTION__, x, len, #x);\
		return -1;\
	}


typedef enum {
	SNIFF_OUTGOING = 0,
	SNIFF_INCOMING
} pkt_way_t;


#include "sniff_dns.inc"
#include "sniff_gg.inc"
#include "sniff_rivchat.inc"

#undef CHECK_LEN
#define CHECK_LEN(x) \
	if (len < x) {\
		debug_error("%s()  * READ less than: %d (len: %d) (%s)\n", __FUNCTION__, x, len, #x);	\
		return;											\
	}


/* XXX, some notes about tcp fragment*
 *		@ sniff_loop_tcp() we'll do: sniff_find_tcp_connection(connection_t *hdr);
 *		it'll find (or create) struct with inited string_t buf...
 *		than we append to that string_t recv data from packet, and than pass this to sniff_gg() [or anyother sniff handler]
 *		than in sniff_loop() we'll remove already data.. [of length len, len returned from sniff_gg()]
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
		debug_error("sniff_loop_tcp()	* Invalid TCP header length: %u bytes\n", size_tcp);
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
			hdr->dstport,			/* dest port */
			tcp_print_flags(tcp->th_flags), /* tcp flags */
			htonl(tcp->th_seq),		/* seq */
			htonl(tcp->th_ack),		/* ack */
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
			hdr->dstport);			/* dest port */

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
	protocol_connected_emit(session);
	return 0;
}

static COMMAND(sniff_command_disconnect) {
	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	protocol_disconnected_emit(session, NULL, EKG_DISCONNECT_USER);

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
	char	*uid	= *(va_arg(ap, char **));
	int	*valid	= va_arg(ap, int *);

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

	format_add("sniff_gg_userlist_data",	 ("%)	%b[%1] %gENTRY: %W%2"), 1);

	format_add("sniff_gg_list",		 ("%) %b[%1] %gLEN: %W%2"), 1);
	format_add("sniff_gg_list_data",	 ("%)	%b[%1] %gENTRY: %W%2 %gTYPE: %W%3"), 1);

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
	format_add("sniff_dns_entry_a",		("%)	     %b[IN_A] %gDOMAIN: %W%1 %gIP: %W%2"), 1);
	format_add("sniff_dns_entry_aaaa",	("%)	  %b[IN_AAAA] %gDOMAIN: %W%1 %gIP6: %W%2"), 1);
	format_add("sniff_dns_entry_cname",	("%)	 %b[IN_CNAME] %gDOMAIN: %W%1 %gCNAME: %W%2"), 1);
	format_add("sniff_dns_entry_ptr",	("%)	   %b[IN_PTR] %gIP_PTR: %W%1 %gDOMAIN: %W%2"), 1);
	format_add("sniff_dns_entry_mx",	("%)	    %b[IN_MX] %gDOMAIN: %W%1 %gENTRY: %W%2 %gPREF: %W%3"), 1);
	format_add("sniff_dns_entry_srv",	("%)	   %b[IN_SRV] %gDOMAIN: %W%1 %gENTRY: %W%2 %gPORT: %W%3 %gPRIO: %W%4 %gWEIGHT: %W%5"), 1);
	format_add("sniff_dns_entry_?",		("%)	     %b[IN_?] %gDOMAIN: %W%1 %gTYPE: %W%2 %gLEN: %W%3"), 1);
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
	format_add("sniff_pkt_rcv",		("%) %2 packets captured"), 1);
	format_add("sniff_pkt_drop",		("%) %2 packets dropped"), 1);

	format_add("sniff_conn_db",		("%) %2 connections founded"), 1);
	format_add("sniff_tcp_connection",	"TCP %1:%2 <==> %3:%4", 1);

	return 0;
}

static plugins_params_t sniff_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias",			VAR_STR, 0, 0, NULL),
	PLUGIN_VAR_ADD("auto_connect",		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("filter",		VAR_STR, DEFAULT_FILTER, 0, NULL),

	PLUGIN_VAR_END()
};

EXPORT int sniff_plugin_init(int prio) {
	PLUGIN_CHECK_VER("sniff");

	sniff_plugin.params = sniff_plugin_vars;
	plugin_register(&sniff_plugin, prio);
	ekg_recode_cp_inc();

	query_connect_id(&sniff_plugin, PROTOCOL_VALIDATE_UID,	sniff_validate_uid, NULL);
	query_connect_id(&sniff_plugin, STATUS_SHOW,		sniff_status_show, NULL);
	query_connect_id(&sniff_plugin, PLUGIN_PRINT_VERSION,	sniff_print_version, NULL);
	query_connect_id(&sniff_plugin, SESSION_REMOVED,	sniff_session_deinit, NULL);

	command_add(&sniff_plugin, "sniff:connect", NULL, sniff_command_connect,    SESSION_MUSTBELONG, NULL);
	command_add(&sniff_plugin, "sniff:connections", NULL, sniff_command_connections, SESSION_MUSTBELONG | SESSION_MUSTBECONNECTED, NULL);
	command_add(&sniff_plugin, "sniff:disconnect", NULL,sniff_command_disconnect, SESSION_MUSTBELONG, NULL);

	return 0;
}

static int sniff_plugin_destroy() {
	plugin_unregister(&sniff_plugin);
	ekg_recode_cp_dec();
	return 0;
}

