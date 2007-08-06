#include "ekg2-config.h"
#include <ekg/win32.h>

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
#include "plugins.h"
#include "stuff.h"
#include "xmalloc.h"

#ifdef LIBIDN /* stolen from squid->url.c (C) Duane Wessels */
static const char valid_hostname_chars_u[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._";
#endif

/**
 * resolver_query_t
 *
 * Mode of querying the nameservers by resolver. Currently only EKG_RESOLVER_ROUNDROBIN implemented and always used,
 * it is possible that we even remove all this mode-stuff and stay with this one.
 */
typedef enum {
	EKG_RESOLVER_SEQUENCE,		/**< Query first server 'retry' times, then second one, then third >*/
	EKG_RESOLVER_ROUNDROBIN,	/**< Query first server, then second, then third, then retry first >*/
	EKG_RESOLVER_PROGRESSIVE,	/**< Query first server, then retry first and query second, then retry second and query third, etc. >*/
	EKG_RESOLVER_ALLATONCE		/**< Query all servers at once, then retry them all >*/
} resolver_query_t;

#define MAXNS 3 /* I don't want to include resolv.h */
#define EKG_RESOLVER_RETRIES	3
#define EKG_RESOLVER_QUERYMODE	EKG_RESOLVER_ROUNDROBIN /* currently no other supported */

/**
 * resolver_t
 *
 * Private resolver structure, containing inter-function data.
 */
typedef struct { /* this will be reordered when resolver is finished */
		/* user data */
	char			*hostname;		/**< Hostname to resolve. */
	query_handler_func_t	*handler;		/**< Result handler function. >*/
	void			*userdata;		/**< User data passed to result handler. >*/
		
		/* private data */
	int			retry;			/**< Current retry num. */
	struct resolver_ns {				/**< Nameservers from resolv.conf. >*/
		struct in_addr	addr;
		int		fd;
	}			nameservers[MAXNS];
} resolver_t;

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
	struct resolver_ns *ns = out->nameservers;

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

		if (!((ns->addr.s_addr = inet_addr(p))))
			continue;
		ns->fd = 0;
		debug("ekg_resolver_readconf: found ns %s\n", p);
		if (++ns >= &out->nameservers[MAXNS])
			break;
	}

	for (; ns < &out->nameservers[MAXNS]; ns++)
		ns->addr.s_addr = INADDR_NONE;

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
	struct resolver_ns *cfd;

	for (cfd = res->nameservers; cfd < &res->nameservers[MAXNS]; cfd++) {
		if (cfd->fd)
			close(cfd->fd);
	}
	xfree(res->hostname);
	xfree(res);

	/* XXX: query */
}

/**
 * ekg_resolver_send()
 *
 * Send query to next nameserver in sequence.
 *
 * @param	res		- resolver_t struct.
 *
 * @return	New watch_t pointer on success, NULL on failure or when no nameservers left (after all retries).
 */
watch_t *ekg_resolver_send(resolver_t *res) {
	struct resolver_ns *cfd;
	struct sockaddr_in sin;

	do {
		if ((cfd = &res->nameservers[(res->retry++) % MAXNS])->addr.s_addr == INADDR_NONE)
			continue;

		sin.sin_family		= AF_INET;
		sin.sin_port		= INADDR_ANY;
		sin.sin_addr.s_addr	= INADDR_ANY;

		if (!(cfd->fd = socket(AF_INET, SOCK_DGRAM, 0)) || bind(cfd->fd, (struct sockaddr *) &sin, sizeof(sin))) {
			if (cfd->fd)
				close(cfd->fd);
			continue;
		}

		/* XXX: prepare & send packet, create watch */

	} while (res->retry <= EKG_RESOLVER_RETRIES * MAXNS);

	ekg_resolver_finish(res, NULL, 0);
	return NULL;
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
 * @return	0 on success, else errno-like constant.
 */

int ekg_resolver(plugin_t *plugin, const char *hostname, int type, query_handler_func_t async, void *data) {
	resolver_t *res	= xmalloc(sizeof(resolver_t));
	int r;

	if ((r = ekg_resolver_readconf(res))) {
		xfree(res);
		return r;
	}

	if (res->nameservers[0].addr.s_addr == INADDR_NONE)
		ekg_resolver_finish(res, NULL, 0);

	res->hostname	= xstrdup(hostname);
	res->handler	= async;
	res->userdata	= data;
	res->retry	= 0;

	return ENOSYS;
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

