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

#define MAXNS 3 /* I don't want to include resolv.h */

/**
 * resolver_query_t
 *
 * Mode of querying the nameservers by resolver.
 */
typedef enum {
	EKG_RESOLVER_SEQUENCE,		/**< Query first server 'retry' times, then second one, then third >*/
	EKG_RESOLVER_ROUNDROBIN,	/**< Query first server, then second, then third, then retry first >*/
	EKG_RESOLVER_PROGRESSIVE,	/**< Query first server, then retry first and query second, then retry second and query third, etc. >*/
	EKG_RESOLVER_ALLATONCE		/**< Query all servers at once, then retry them all */
} resolver_query_t;

#define EKG_RESOLVER_RETRIES	3
#define EKG_RESOLVER_QUERYMODE	EKG_RESOLVER_ROUNDROBIN

typedef struct {
	struct in_addr		nameservers[MAXNS];	/* available nameservers */
	query_handler_func_t	*handler;
	void			*userdata;
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
		debug("ekg_resolver_readconf: found ns %s\n", p);
		if (++ns >= &out->nameservers[MAXNS])
			break;
	}

	fclose(f);
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
 * @return	0 on success, else errno-like constant.
 */

int ekg_resolver(plugin_t *plugin, const char *hostname, int type, query_handler_func_t async, void *data) {
	resolver_t *res	= xmalloc(sizeof(resolver_t));
	int r;

	if ((r = ekg_resolver_readconf(res))) {
		xfree(res);
		return r;
	}

	res->handler	= async;
	res->userdata	= data;

#if 1 /* TMP */
	xfree(res);
#endif
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

