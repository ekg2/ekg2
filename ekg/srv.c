/*
 *  (C) Copyright 2004-2005 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
 *      parts of this code are losely based on cifs/smb dns utility stuff
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

/* this is srv resolver as used by ekg2 */

#include "ekg2-config.h"

#define __USE_BSD
#define _GNU_SOURCE
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> /* getprotobynumber, getservbyport */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#if HAVE_RESOLV_H
#include <resolv.h> /* res_init, res_query */
#endif

#include "dynstuff.h"
#include "xmalloc.h"
#include "srv.h"

#ifndef T_SRV
#define T_SRV 33 /* rfc 2782 */
#endif

/* Stolen from ClamAV */
#if WORDS_BIGENDIAN == 0
union unaligned_32 { uint32_t una_u32; int32_t una_s32; } __attribute__((packed));
union unaligned_16 { uint16_t una_u16; int16_t una_s16; } __attribute__((packed));

#define cli_readint16(buff) (((const union unaligned_16 *)(buff))->una_u16)
#define cli_readint32(buff) (((const union unaligned_32 *)(buff))->una_u32)

#else
static inline uint16_t cli_readint16(const unsigned char *buff)
{
	uint16_t ret;
	ret = buff[0] & 0xff;
	ret |= (buff[1] & 0xff) << 8;
	return ret;
}

static inline uint32_t cli_readint32(const unsigned char *buff)
{
	uint32_t ret;
	ret = buff[0] & 0xff;
	ret |= (buff[1] & 0xff) << 8;
	ret |= (buff[2] & 0xff) << 16;
	ret |= (buff[3] & 0xff) << 24;
	return ret;
}
#endif

/* idea:
 *   get srv (+ A's and AAAA's)
 *   resolve missing
 */
/* cannot place this struct in .h
 * since it requires resolv.h
 * and this conlficts with compilation of other files
 */
struct _gim_host
{
	struct _gim_host *next;

	unsigned char name[NS_MAXDNAME];
	uint16_t prio;
	uint16_t weight;
	uint16_t port;

	int *ai_family;
	char **ip;
};

const int DNS_NS_MAXDNAME = NS_MAXDNAME;

LIST_ADD_COMPARE(gim_host_cmp, gim_host* ) { return data1->prio - data2->prio; }

/**
 * ekg_inet_ntostr - convert sockaddr_in
 * to string representing ip address
 *
 * since srv_resolver uses this function and we can't
 * place srv_resolver in net.c (due to conflict in
 * resolv.h) for a while this function must sit here.
 *
 */
char *ekg_inet_ntostr(int family,  void *buf)
{
#ifdef HAVE_INET_NTOP
#  define RESOLVER_MAXLEN INET6_ADDRSTRLEN
    char tmp[RESOLVER_MAXLEN];
    inet_ntop(family, buf, tmp, RESOLVER_MAXLEN);
    return xstrdup(tmp);
#else
    if (family == AF_INET6) {
	return xstrdup("::");
    } else
	/* try to dup the string as fast as possible,
	 * and hope for the best
	 */
	return xstrdup(inet_ntoa(*(struct in_addr *)buf));
#endif
}


/**
 * extract_rr()
 * 
 * parses RR header from dns reply. RR format according to rfc 1035.
 *
 * @param start - beginning of a buffer with dns response
 * @param end - end of buffer
 * @ptr - beginning of RR, ptr will be adjusted approprietly
 * @rr - structure where result will be placed
 *
 * @returns 0 - on success, non zero otherwise
 *
 * IF RETURNED STATUS IS NON-ZERO, content of 'rr' struct is undefined
 */
int extract_rr(unsigned char *start, unsigned char *end, unsigned char **ptr, ns_rr *rr)
{
#ifdef HAVE_LIBRESOLV
	unsigned char *rrs;
	char exp_dn[2048];
	int exp_len;

	if (!start || !end || !ptr || !*ptr || !rr)
		return -1;

	rrs = *ptr;

	if ((exp_len = dn_expand(start, end, rrs, exp_dn , sizeof(exp_dn))) == -1)
		return 1;

	/* no checking here, since if there wouldn't be at least exp_len
	 * bytes, dn_expand would return -1
	 */
	rrs += exp_len;

	strncpy(rr->name, exp_dn, DNS_NS_MAXDNAME);

	if (rrs + 10 > end)
		return 3;
	/* this works both on sparc and intel, so don't mess with it */
	rr->type	= ntohs(cli_readint16(rrs));
	rr->rr_class	= ntohs(cli_readint16(rrs+2));
	rr->ttl		= ntohs(cli_readint32(rrs+4));
	rr->rdlength	= ntohs(cli_readint32(rrs+8));

	rrs += 10;
	if (rrs + rr->rdlength > end)
		return 4;
	rr->rdata	= rrs;

	rrs += rr->rdlength;

	*ptr = rrs;
#endif
	return 0;
}


int extract_rr_srv(unsigned char *start, unsigned char *end, unsigned char **ptr, gim_host *srv)
{
#ifdef HAVE_LIBRESOLV
	char exp_dn[2048];
	int exp_len;
	/* arpa/nameser.h */
	ns_rr rr;

	if ((extract_rr(start, end, ptr, &rr)) != 0)
	{
		/* fprintf (stderr, "EPIC FAIL srv\n"); */
		return 1;
	}

	if (ns_rr_type(rr) != ns_t_srv)
	{
		/* fprintf (stderr, "Unexpected record of type(%d) found instead of srv(%d)\n", ns_rr_type(rr), ns_t_srv); */
		return 1;
	}

	if (rr.rdlength < 6)
		return 1;

	srv->prio	= ntohs(cli_readint16(rr.rdata));
	srv->weight	= ntohs(cli_readint16(rr.rdata+2));
	srv->port	= ntohs(cli_readint16(rr.rdata+4));

	if ((exp_len = dn_expand(start, end, rr.rdata+6, exp_dn , sizeof(exp_dn))) == -1)
		return 1;
	strncpy((char*)srv->name, (char*)exp_dn, DNS_NS_MAXDNAME);
#endif
	return 0;
}

int skip_rr_ns(unsigned char *start, unsigned char *end, unsigned char **ptr)
{
	/* arpa/nameser.h */
	ns_rr rr;

	if ((extract_rr(start, end, ptr, &rr)) != 0)
	{
		/* fprintf (stderr, "EPIC FAIL ns\n"); */
		return 1;
	}

	if (ns_rr_type(rr) != ns_t_ns)
	{
		/* fprintf (stderr, "Unexpected record of type(%d) found instead of ns(%d)\n", ns_rr_type(rr), ns_t_ns); */
		return 1;
	}

	return 0;
}

typedef enum _gim_sects { SQUERY=ns_s_qd, SANSWER=ns_s_an, SAUTH=ns_s_ns, SEXTRA=ns_s_ar, SMAX=ns_s_max } gim_sects;

int srv_resolver(gim_host **hostlist, const char *hostname, const int proto_port, const int port, const int proto) {
#ifndef NO_POSIX_SYSTEM
#ifdef HAVE_RESOLV_H
	struct protoent *pro;
	struct servent *srv;
	char expanded_dn[2048], *srvhost;
	unsigned char res_answer[2048], *rrs;
	int res_query_len, expanded_len, cnt[SMAX];
	/* this is query header, on my distro it lies under arpa/nameser_compat.h
	 */
	HEADER *query_resp;
	gim_host *iter;

	if (!(pro = getprotobynumber(proto ? proto : IPPROTO_TCP)))
		return 1;

	if (!(srv = getservbyport(htons(proto_port), pro->p_name)))
		return 2;

	if (res_init() == -1)
	{
	    /* fprintf (stderr, "resolver initialization FAILED\n"); */
	    return 3;
	}

	srvhost = saprintf("_%s._%s.%s", srv->s_name, pro->p_name, hostname);

#define RET(x) do { xfree(srvhost); return x; } while (0)

	/*fprintf (stderr, "trying: %s\n", srvhost);*/
	if ((res_query_len = res_query(srvhost, C_IN, T_SRV, res_answer, sizeof(res_answer))) == -1)
	    RET(4);

	if (res_query_len < sizeof(HEADER))
	{
	    /* fprintf(stderr, "underflow resolver reply, someone's truing to hack you up?"); */
	    RET(5);
	}

	/*
	if (res_query_len > 512)
	    fprintf (stderr, "long resolver reply, pleas report to developers\n");
	*/

	query_resp = (HEADER*)res_answer;

	/*
	fprintf (stderr, "> %d %d\n", res_query_len, sizeof(HEADER));
	fprintf (stderr, "> %s\n", srvhost);
	*/

	/* check if there was no error, and if there is answer section available
	 */
	if ( (ntohs(query_resp->rcode) == NOERROR) && (ntohs(query_resp->ancount) > 0) ) {
		int i;

		cnt[SQUERY]	= ntohs(query_resp->qdcount);
		cnt[SANSWER]	= ntohs(query_resp->ancount);
		cnt[SAUTH]	= ntohs(query_resp->nscount);
		cnt[SEXTRA]	= ntohs(query_resp->arcount);
		if (cnt[SQUERY] != 1)
		{
			/* fprintf (stderr, "wth, not our query?"); */
			RET(6);
		}
		/*
		fprintf (stderr, "> question_cnt: %d\n", cnt[0]);
		fprintf (stderr, ">   answer_cnt: %d\n", cnt[1]);
		fprintf (stderr, ">       ns_cnt: %d\n", cnt[2]);
		fprintf (stderr, ">       ar_cnt: %d\n", cnt[3]);
		*/


		/* PARSE QUERY */
		if ((expanded_len = dn_expand(res_answer,
				res_answer + res_query_len, 
				res_answer + sizeof(HEADER),
				expanded_dn,
				sizeof(expanded_dn))) == -1)
			RET(6);
		/* fprintf (stderr, "> %s %d\n", expanded_dn, expanded_len); */

		/* rfc 1035 $ 4.1.2 - question section format
		 * qname, qtype - 16b, qclass - 16b
		 */
		rrs = res_answer + sizeof(HEADER) + expanded_len + 2 + 2;

		/* PARSE ANSWER SECTION */
		for (i = 0; i < cnt[SANSWER]; i++)
		{
			gim_host *srv = xmalloc(sizeof(gim_host));
			if (extract_rr_srv (res_answer, res_answer + res_query_len, &rrs, srv) != 0)
				RET(7);

			/* alter port to user specified port
			 * this is temporary hack and final solution
			 * will probably be different
			 */
			srv->port = port;
			LIST_ADD_SORTED2(hostlist, srv, gim_host_cmp);
		}
		/* PARSE (skip) NS SECTION */
		for (i = 0; i < cnt[SAUTH]; i++)
			if (skip_rr_ns (res_answer, res_answer + res_query_len, &rrs) != 0)
				RET(8);
		
		/* PARSE ADDITIONAL SECTION */
		for (i=0; i<cnt[SEXTRA]; i++)
		{
			int family, ip_cnt;
			ns_rr rr;


			if (extract_rr (res_answer, res_answer + res_query_len, &rrs, &rr) != 0)
				RET(9);

			/* check if host is on the answer list */
			for (iter = *hostlist; iter; iter = iter->next)
				if (!xstrcmp((char *)iter->name, (char*)rr.name))
					break;

			/* probably A/AAAA for ns, skip to next */
			if (!iter)
				continue;

			if ((ns_rr_type(rr) != ns_t_a || rr.rdlength != 4) &&
					(ns_rr_type(rr) != ns_t_aaaa || rr.rdlength != 16))
			{
				/* fprintf (stderr, " unhandled type in additional section\n"); */
				continue;
			}

			family = (rr.rdlength == 4) ? AF_INET : AF_INET6;
			ip_cnt = array_add_check (&(iter->ip), ekg_inet_ntostr(family, (void *)rr.rdata), 0);
			if (ip_cnt)
			{
				iter->ai_family = xrealloc (iter->ai_family, ip_cnt*sizeof(iter->ai_family));
				iter->ai_family[ip_cnt-1] = family;
			}
		}
	}

#endif
#endif
	return 0;
}


/**
 * this is mostly copy of basic_resolver below
 * it's for internal use only, for resolving
 * missing items on the list
 */
static int basic_resolver_item (gim_host *srv)
{
#ifdef HAVE_GETADDRINFO
	struct addrinfo	*ai, *aitmp, hint;
#else
#  warning "resolver: You don't have getaddrinfo(), resolver may not work! (ipv6 for sure)"
	struct hostent	*he4;
#endif
	/* if it's on the 'missing list', it's the result of
	 * srv query, so on
	 */

#ifdef HAVE_GETADDRINFO
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_socktype = SOCK_STREAM;

	if (!getaddrinfo(srv->name, NULL, &hint, &ai)) {

		for (aitmp = ai; aitmp; aitmp = aitmp->ai_next) {
			int ip_cnt;

			if (aitmp->ai_family != AF_INET && aitmp->ai_family != AF_INET6)
				continue;

			ip_cnt = array_add_check (&(srv->ip), ekg_inet_ntostr(aitmp->ai_family, &((struct sockaddr_in *)aitmp->ai_addr)->sin_addr), 0);
			if (ip_cnt)
			{
			    srv->ai_family = xrealloc(srv->ai_family, ip_cnt*sizeof(srv->ai_family));
			    srv->ai_family[ip_cnt-1] = aitmp->ai_family;
			}
		}
		freeaddrinfo(ai);
	}
#else 
	if ((he4 = gethostbyname(hostname))) {
		int ip_cnt = array_add (&(srv->ip), xstrdup(inet_ntoa(*(struct in_addr *) he4->h_addr)));
		srv->ai_family = xrealloc (srv->ai_family, ip_cnt*sizeof(srv->ai_family));
		srv->ai_family[ip_cnt-1] = AF_INET;
	} 
#endif
	return 0;
}

int resolve_missing_entries(gim_host **hostlist)
{
    gim_host *iter;
    for (iter = *hostlist; iter; iter = iter->next)
    {
	if (iter->ip)
	{
	    /*debug ("  >%s already done\n", iter->name);*/
	    continue;
	}
	/*debug ("  >%s resolving\n", iter->name);*/
	basic_resolver_item (iter);
    }
    return 0;
}

/**
 * this is exactly irc_resolver2, but instead
 * of array it appends entries to hostlist
 */
int basic_resolver(gim_host **hostlist, const char *hostname, int port)
{
#ifdef HAVE_GETADDRINFO
	struct addrinfo	*ai, *aitmp, hint;
#else
#  warning "resolver: You don't have getaddrinfo(), resolver may not work! (ipv6 for sure)"
	struct hostent	*he4;
#endif
	gim_host *srv, *iter;

#ifdef HAVE_GETADDRINFO
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_socktype = SOCK_STREAM;

	srv = xmalloc(sizeof(gim_host));

	if (!getaddrinfo(hostname, NULL, &hint, &ai)) {
		int do_loop = (AF_INET | AF_INET6);
		srv->prio = DNS_SRV_MAX_PRIO;
		srv->port = port;
		strncpy(srv->name, hostname, DNS_NS_MAXDNAME);

		for (aitmp = ai; aitmp; aitmp = aitmp->ai_next) {
			int ip_cnt;
			
			if (aitmp->ai_family != AF_INET && aitmp->ai_family != AF_INET6)
				continue;

			/* We assume that sin_addr in sockaddr_in has exactly
			 * the same offset from beginning of a struct as
			 * sin_addr6 in sockaddr_in6 struct
			 */
			ip_cnt = array_add_check (&(srv->ip), ekg_inet_ntostr(aitmp->ai_family, &((struct sockaddr_in *)aitmp->ai_addr)->sin_addr), 0);
			FILE *fp=fopen("dupa", "a"); fprintf(fp, "current %s: %d\n",
					srv->name, ip_cnt); fflush(fp); fclose(fp);
			if (ip_cnt)
			{
			    srv->ai_family = xrealloc(srv->ai_family, ip_cnt*sizeof(srv->ai_family));
			    srv->ai_family[ip_cnt-1] = aitmp->ai_family;
			}

#if 0
			for (iter = *hostlist; iter && (do_loop & srv->ai_family); iter = iter->next)
			{
			    /* I know, comparing name returned from srv, quite sux
			     * but I don't have better idea right now
			     */
			    if (!xstrcmp(iter->name, hostname) && iter->ai_family == srv->ai_family)
			    {
				/* ports are different, so check if iter
				 * has filled ip field, if not, copy it
				 */
				if (!iter->ip)
				    iter->ip = xstrdup(srv->ip);
			    }
			}
			/* do not waste time ;) */
			/* do not make above loop in next iteration,
			 * since hostname hasn't changed, so if any
			 * matched it is already corrected
			 */
			do_loop &= ~(aitmp->ai_family);
#endif
		}
		LIST_ADD_SORTED2(hostlist, srv, gim_host_cmp);
		freeaddrinfo(ai);
	}
#else 
	if ((he4 = gethostbyname(hostname))) {
		gim_host *srv = xmalloc(sizeof(gim_host));
		int ip_cnt = array_add (&(srv->ip), xstrdup(inet_ntoa(*(struct in_addr *) he4->h_addr)));
		srv->ai_family = xrealloc (srv->ai_family, ip_cnt*sizeof(srv->ai_family));
		srv->ai_family[ip_cnt-1] = AF_INET;
		srv->prio = DNS_SRV_MAX_PRIO;
		srv->port = port;
		strncpy(srv->name, hostname, DNS_NS_MAXDNAME);
		LIST_ADD_SORTED2(hostlist, srv, gim_host_cmp);
	} 
#endif
	return 0;
}

void write_out_and_destroy_list(int fd, gim_host *hostlist)
{
	gim_host *iter;
	char *str;
	int i;

	for (iter = hostlist; iter; iter = iter->next)
	{
		for (i = 0; i < array_count(iter->ip); i++)
		{
			str = saprintf ("%s %s %d %d\n",
					iter->name, iter->ip[i],
					iter->ai_family[i], iter->port);

			write (fd, str, xstrlen(str));
			xfree (str);
		}
		array_free (iter->ip);
		xfree (iter->ai_family);
	}
	LIST_DESTROY2 (hostlist, NULL);
}
