#include "ekg2-config.h"
#include "win32.h"

#include <sys/types.h>
#include <sys/param.h> /* PATH_MAX, funny, I know */

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>	/* ? */
#endif

#include <errno.h>
#include <stdio.h>	/* ? */
#include <stdlib.h>	/* ? */
#include <string.h>
#include <stdarg.h>	/* ? */
#include <unistd.h>

#ifndef NO_POSIX_SYSTEM
#include <netdb.h>	/* OK */
#endif

#ifdef __sun      /* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif

#ifdef LIBIDN
# include <idna.h>
#endif

/* NOTE:
 * 	Includes were copied from jabber.c, where there's ? in comment, it's possibly not needed.
 * 	It was done this way, to avoid regression.
 * 	THX.
 */

#ifndef PATH_MAX
# ifdef MAX_PATH
#  define PATH_MAX MAX_PATH
# else
#  define PATH_MAX _POSIX_PATH_MAX
# endif
#endif

#ifndef HAVE_STRLCAT
#  include "compat/strlcat.h"
#endif

#include "debug.h"
#include "dynstuff.h"
#include "plugins.h"
#include "stuff.h"
#include "xmalloc.h"

#ifdef LIBIDN /* stolen from squid->url.c (C) Duane Wessels */
static const char valid_hostname_chars_u[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._";
#endif

#define MAXNS			3 /* I don't want to include resolv.h */
#define EKG_RESOLVER_RETRIES	3
#define EKG_RESOLVER_TIMEOUT	3

int ekg_resolver_fd	= -1;
list_t ekg_resolvers	= NULL;

/**
 * resolver_t
 *
 * Private resolver structure, containing inter-function data.
 */
typedef struct { /* this will be reordered when resolver is finished */
		/* user data */
	char			*hostname;		/**< Hostname to resolve. >*/
	query_handler_func_t	*handler;		/**< Result handler function. >*/
	void			*userdata;		/**< User data passed to result handler. >*/
		
		/* private data */
	int			retry;			/**< Current retry num. >*/
	struct in_addr		nameservers[MAXNS];	/**< Nameservers from resolv.conf. >*/
	uint16_t		id;			/**< ID of request, used to distinguish between resolved hostnames. >*/
	time_t			sent;			/**< Timestamp of last question sending, used for timeout detection. >*/
} resolver_t;

static uint16_t ekg_resolver_pkt[256]; /* 512 bytes is max DNS UDP packet size */

/**
 * ekg_resolver_readconf()
 *
 * Read ${SYSCONFDIR}/resolv.conf and fill in nameservers in given @a resolver_t.
 *
 * @param	out	- structure to be filled.
 *
 * @return	0 on success, errno-like constant on failure.
 */
int ekg_resolver_readconf(resolver_t *out) {
	FILE *f;
	char line[64]; /* this shouldn't be long */
	struct in_addr *ns = out->nameservers;

	if (!(f = fopen("/etc/resolv.conf", "r"))) /* XXX: stat() first? */
		return errno;

	while ((fgets(line, sizeof(line), f))) {
		char *p;

		if (!xstrchr(line, 10)) /* skip too long lines */
			continue;

		p = line + xstrspn(line, " \f\n\r\t\v");
		if (xstrncasecmp("nameserver", p, 10)) /* skip other keys */
			continue;

		p += 10;
		p += xstrspn(p, " \f\n\r\t\v");

		if (!((ns->s_addr = inet_addr(p))))
			continue;
		debug("ekg_resolver_readconf(), found ns %s", p); /* 'p' already contains LF */
		if (++ns >= &out->nameservers[MAXNS])
			break;
	}

	for (; ns < &out->nameservers[MAXNS]; ns++)
		ns->s_addr = INADDR_NONE;

	fclose(f);
	return 0;
}

/**
 * ekg_resolver_finish()
 *
 * Frees private resolver structure and sends final query.
 *
 * @param	res		- resolver_t struct.
 * @param	addr		- resolved IP address (and port, if SRV applies) or NULL, if failed.
 * @param	addrtype	- AF_* constant determining IP address type (AF_INET or AF_INET6, currently).
 */
void ekg_resolver_finish(resolver_t *res, struct sockaddr *addr, int type) {
	xfree(res->hostname);
	xfree(res);
	list_remove(&ekg_resolvers, res, 0);

	/* XXX: query */
}

/**
 * ekg_resolver_send()
 *
 * Send query to next nameserver in sequence.
 *
 * @param	res		- resolver_t struct.
 *
 * @return	0 on success, -1 on failure or when no servers (& tries) left.
 */
int ekg_resolver_send(resolver_t *res) {
	while (res->retry < EKG_RESOLVER_RETRIES * MAXNS) {
		uint16_t *pkt = ekg_resolver_pkt;
		struct in_addr *cfd;
		uint16_t old_id = 0;

		if ((cfd = &res->nameservers[(res->retry++) % MAXNS])->s_addr == INADDR_NONE)
			continue;

		memset(&pkt[1], 0, 510); /* leave id */
		if (res->id == 0) {
			if (!(++pkt[0])) /* id != 0 */
				pkt[0]++;
			res->id = pkt[0];
		} else {
			old_id	= pkt[0]; /* keep&restore old id, so new queries will get already used id after 65535 retries */
			pkt[0]	= res->id;
		}

		pkt[1]		= htons(256);	/* RA? 'host' sets this */
		pkt[2]		= htons(1);	/* 1 question */

		{
			char *p, *q, *r;
			uint16_t *z;

			for (p = res->hostname, q = (char*) &pkt[6], r = q + 1;; p++, r++) {
				if (*p == '.' || *p == 0) {
					*q = (r - q - 1);
					if (!*p)
						break;
					q = r;
				} else
					*r = *p;
			}

			z = (uint16_t*) r;

			*(z++)	= htons(1); /* QTYPE => A */
			*z	= htons(1); /* QCLASS => IN (internet) */
		}

		{
			struct sockaddr_in sin;

			sin.sin_family		= AF_INET;
			sin.sin_addr.s_addr	= cfd->s_addr;
			sin.sin_port		= htons(53);

			debug("ekg_resolver_send(), sending query for %s to %s:53, id: %04x\n", res->hostname, inet_ntoa(*cfd), pkt[0]);

			if ((sendto(ekg_resolver_fd, pkt, xstrlen(res->hostname) + 17, 0, (struct sockaddr*) &sin, sizeof(sin)) == -1)) {
				if (old_id)
					pkt[0]	= old_id;

				debug_error("ekg_resolver_send(), sendto() failed: %s\n", strerror(errno));
				continue; /* treat as no-response, i.e. retry */
			}
			res->sent = time(NULL);

			{
				watch_t *w = watch_find(NULL, ekg_resolver_fd, WATCH_READ);

				if (!w)
					debug_error("ekg_resolver_send(), watch not found - wtf?!\n");
				else if (w->timeout == 0) { /* don't overwrite awaiting timeouts */
					w->timeout = EKG_RESOLVER_TIMEOUT;
					w->started = res->sent;
				}
			}
		}

		if (old_id)
			pkt[0]	= old_id;

		return 0;
	}

	ekg_resolver_finish(res, NULL, 0);
	return -1;
}

/**
 * ekg_resolver_recv()
 *
 * Watch-triggered function, fetching data sent by DNS server and parsing it.
 */
WATCHER(ekg_resolver_recv) {
	if (type == 2) {
		list_t l;
		const time_t now	= time(NULL);
		int lowest		= 0;
		watch_t *w		= watch_find(NULL, fd, WATCH_READ);

		for (l = ekg_resolvers; l; l = l->next) {
			resolver_t *res = l->data;

			if (now - res->sent >= EKG_RESOLVER_TIMEOUT)
				ekg_resolver_send(res);
			lowest = EKG_RESOLVER_TIMEOUT - (now - res->sent);
		}

		if (w) {
			w->started = now;
			w->timeout = lowest;
		} else
			debug_error("ekg_resolver_recv(), can't find myself, WTF?!\n");
	} else if (type == 0) {
		uint16_t *pkt			= ekg_resolver_pkt;
		struct sockaddr_in from;
		socklen_t fromlen		= sizeof(from);
		resolver_t *res;
		struct in_addr *ns;

		if (recvfrom(ekg_resolver_fd, pkt, sizeof(ekg_resolver_pkt), 0, (struct sockaddr*) &from, &fromlen) == -1) {
			debug_error("ekg_resolver_recv(), recvfrom() failed: %s\n", strerror(errno));
			close(ekg_resolver_fd); /* if we don't clean up this mess, handler would be executed again and again, and again... */
			return -1;
		}

		if (fromlen != sizeof(struct sockaddr_in)) {
			debug_error("ekg_resolver_recv(), wrong 'fromlen' from recvfrom(), WTF?!\n");
			close(ekg_resolver_fd);
			return -1; /* XXX: not too hard? */
		}

		if (from.sin_addr.s_addr == INADDR_NONE) {
			debug_error("ekg_resolver_recv(), INADDR_NONE in 'from' from recvfrom(), WTF?!\n");
			close(ekg_resolver_fd);
			return -1;
		}

		{
			list_t l;

			for (l = ekg_resolvers; l; l = l->next) {
				res = l->data;
				if (res->id == pkt[0]) {
					for (ns = res->nameservers; ns < &res->nameservers[MAXNS]; ns++) {
						if (ns->s_addr == from.sin_addr.s_addr)
							break;
					}
					if (ns < &res->nameservers[MAXNS])
						break;
				}
			}

			if (!l) {
				debug("ekg_resolver_recv(), unknown id: %04x (junk?)\n", pkt[0]);
				return 0;
			}
		}

		{
			const uint8_t rcode = ntohs(pkt[1]) & 0xF;
			const char *rtext;

			switch (rcode) { /* short names shamelessly stolen from bind */
				case 1: rtext = "FORMERR";	break;
				case 2: rtext = "SERVFAIL";	break;
				case 3: rtext = "NXDOMAIN";	break;
				case 4: rtext = "NOTIMP";	break;
				case 5: rtext = "REFUSED";	break;
				default: rtext = NULL;
			}

			if (rcode) {
				debug_error("ekg_resolver_recv(), from: %s, flags: %04x, err: [%d] %s\n", inet_ntoa(from.sin_addr), ntohs(pkt[1]), rcode, rtext);
				ns->s_addr = INADDR_NONE; /* disable retry with this server */
				return 0;
			}
		}

		/* XXX: parse answer */

	}

	return 0;
}

/**
 * ekg_resolver()
 *
 * Simple watch-based resolver.
 *
 * @param	plugin		- calling plugin.
 * @param	hostname	- hostname to resolve.
 * @param	type		- bitmask of (1 << (PF_* - 1)), probably PF_INET & PF_INET6 will be supported.
 * @param	async		- function handling resolved data (XXX: create some query).
 * @param	data		- user data to pass to the function.
 *
 * @note	Given handler function MAY be executed BEFORE this routine returns, but that can happen
 * 		ONLY if resolving can't be done.
 *
 * @return	0 on success, else errno-like constant.
 */

int ekg_resolver(plugin_t *plugin, const char *hostname, int type, query_handler_func_t async, void *data) {
	resolver_t *res	= xmalloc(sizeof(resolver_t));
	int r;

	if (!hostname || !*hostname)

	return EINVAL;
	
		/* these two are needed for ekg_resolver_finish() to send failure query */
	res->handler	= async;
	res->userdata	= data;
	
	if ((r = ekg_resolver_readconf(res))) {
		ekg_resolver_finish(res, NULL, 0);
		return r;
	}

	if (res->nameservers[0].s_addr == INADDR_NONE) {
		ekg_resolver_finish(res, NULL, 0);
		return ENOENT;
	}

	if (ekg_resolver_fd == -1) {
		struct sockaddr_in sin;

		sin.sin_family		= AF_INET;
		sin.sin_port		= INADDR_ANY;
		sin.sin_addr.s_addr	= INADDR_ANY;

		if ((ekg_resolver_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 || bind(ekg_resolver_fd, (struct sockaddr *) &sin, sizeof(sin))) {
			const int err = errno;

			debug_error("ekg_resolver(), socket open/bind failed with: %s\n", strerror(err));
			ekg_resolver_finish(res, NULL, 0);
			return err;
		}

		watch_add(NULL, ekg_resolver_fd, WATCH_READ, &ekg_resolver_recv, NULL);

		debug("ekg_resolver(), socket open as fd %d\n", ekg_resolver_fd);
	}

	{
		const char *p = hostname;
		char *q;

		res->hostname	= xmalloc(xstrlen(hostname)+2);
		while (*p == '.') p++; /* skip leading dots */

		for (q = res->hostname; *p; p++, q++) {
			if (*p == '.') {
				while (*(p+1) == '.')
					p++; /* skip multiple dots */
			}
			*q = *p;
		}

		if (*(p-1) != '.') /* we are already sure that p is not empty */
			*(q++) = '.'; /* add trailing dot */
		*q = 0;
	}
#ifdef LIBIDN
	{
		char *tmp;

		if ((xstrspn(res->hostname, valid_hostname_chars_u) != xstrlen(res->hostname)) && /* need to escape */
			(idna_to_ascii_8z(res->hostname, &tmp, 0) == IDNA_SUCCESS)) {
			xfree(res->hostname);
			res->hostname = tmp;
		}
	}
#endif

	if ((r = ekg_resolver_send(res)))
		return r;

	list_add(&ekg_resolvers, res, 0);
	return 0;
}

/*
 * ekg_resolver2()
 *
 * Resolver copied from jabber plugin, 
 * it use gethostbyname()
 *
 *  - async	- watch handler.
 *  - data	- watch data handler.
 *
 *  in @a async watch you'll recv 4 bytes data with ip addr of @a server, or INADDR_NONE if gethostbyname() failed.
 *	you should return -1 (temporary watch) and in type == 1 close fd.
 *
 *  NOTE, EKG2-RESOLVER-API IS NOT STABLE.
 *  	IT'S JUST COPY-PASTE OF SOME FUNCTION FROM OTHER PLUGINS, TO AVOID DUPLICATION OF CODE (ALSO CLEANUP CODE A LITTLE)
 *  	AND TO AVOID REGRESSION. 
 *  THX.
 */

watch_t *ekg_resolver2(plugin_t *plugin, const char *server, watcher_handler_func_t async, void *data) {
	int res, fd[2];
	char *myserver;

	if (!server) {
		errno = EFAULT;
		return NULL;
	}

	debug("ekg_resolver2() resolving: %s\n", server);

	if (pipe(fd) == -1) {
		return NULL;
	}

	debug("ekg_resolver2() resolver pipes = { %d, %d }\n", fd[0], fd[1]);

	ekg_resolver(plugin, server, AF_INET, async, data);
	return NULL;

	myserver = xstrdup(server);
	if ((res = fork()) == -1) {
		int errno2 = errno;

		close(fd[0]);
		close(fd[1]);
		xfree(myserver);
		errno = errno2;
		return NULL;
	}

	if (!res) {
		/* child */
		struct in_addr a;

		close(fd[0]);

#ifdef LIBIDN
		{
			char *tmp;

			if ((xstrspn(myserver, valid_hostname_chars_u) != xstrlen(myserver)) && /* need to escape */
				(idna_to_ascii_8z(myserver, &tmp, 0) == IDNA_SUCCESS)) {
				xfree(myserver);
				myserver = tmp;
			}
		}
#endif
		if ((a.s_addr = inet_addr(myserver)) == INADDR_NONE) {
			struct hostent *he = gethostbyname(myserver);

			if (!he)
				a.s_addr = INADDR_NONE;
			else
				memcpy(&a, he->h_addr, sizeof(a));
		}
		write(fd[1], &a, sizeof(a));
		xfree(myserver);
		sleep(1);
		exit(0);
	}

	/* parent */
	close(fd[1]);
	xfree(myserver);
	/* XXX dodaæ dzieciaka do przegl±dania */
	return watch_add(plugin, fd[0], WATCH_READ, async, data);
}

