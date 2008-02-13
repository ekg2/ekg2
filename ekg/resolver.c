#include "ekg2-config.h"
#include <ekg/win32.h>

#include <sys/types.h>

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

#define __USE_POSIX
#ifndef NO_POSIX_SYSTEM
#include <netdb.h>	/* OK */
#endif

#ifdef __sun      /* Solaris, thanks to Beeth */
#include <sys/filio.h>
#endif

#ifdef LIBIDN
# include <idna.h>
#endif

/*
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
 */

/* NOTE:
 * 	Includes were copied from jabber.c, where there's ? in comment, it's possibly not needed.
 * 	It was done this way, to avoid regression.
 * 	THX.
 */

#include "debug.h"
#include "plugins.h"
#include "xmalloc.h"

#ifndef INADDR_NONE		/* XXX, xmalloc.h (?) */
#  define INADDR_NONE (unsigned long) 0xffffffff
#endif

#ifdef LIBIDN /* stolen from squid->url.c (C) Duane Wessels */
static const char valid_hostname_chars_u[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._";
#endif

/*
 * ekg_resolver2()
 *
 * Resolver copied from jabber plugin, 
 * it uses gethostbyname()
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

	if (pipe(fd) == -1)
		return NULL;

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
	/* XXX dodac dzieciaka do przegladania */
	return watch_add(plugin, fd[0], WATCH_READ, async, data);
}

static int irc_resolver2(char ***arr, char *hostname) {
#ifdef HAVE_GETADDRINFO
	struct  addrinfo *ai, *aitmp, hint;
	void *tm = NULL;
#else
#warning "irc: You don't have getaddrinfo() resolver may not work! (ipv6 for sure)"
	struct hostent *he4;
#endif	

#ifdef HAVE_GETADDRINFO
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_socktype=SOCK_STREAM;
	if (!getaddrinfo(hostname, NULL, &hint, &ai)) {
		for (aitmp = ai; aitmp; aitmp = aitmp->ai_next) {
			char *ip = NULL, *buf;

			if (aitmp->ai_family == AF_INET6)
				tm = &(((struct sockaddr_in6 *) aitmp->ai_addr)->sin6_addr);
			if (aitmp->ai_family == AF_INET) 
				tm = &(((struct sockaddr_in *) aitmp->ai_addr)->sin_addr);
#ifdef HAVE_INET_NTOP
			ip = xmalloc(100);
			inet_ntop(aitmp->ai_family, tm, ip, 100);
#else
			if (aitmp->ai_family == AF_INET6) {
				/* G: this doesn't have a sense since we're in child */
				/* print("generic_error", "You don't have inet_ntop() and family == AF_INET6. Please contact with developers if it happens."); */
				ip =  xstrdup("::");
			} else
				ip = xstrdup(inet_ntoa(*(struct in_addr *)tm));
#endif 
			buf = saprintf("%s %s %d\n", hostname, ip, aitmp->ai_family);
			array_add(arr, buf);
			xfree(ip);
		}
		freeaddrinfo(ai);
	}
#else 
	if ((he4 = gethostbyname(hostname))) {
		/* copied from http://webcvs.ekg2.org/ekg2/plugins/irc/irc.c.diff?r1=1.79&r2=1.80 OLD RESOLVER VERSION...
		 * .. huh, it was 8 months ago..*/
		char *ip = xstrdup(inet_ntoa(*(struct in_addr *) he4->h_addr));
		array_add(arr, saprintf("%s %s %d\n", hostname, ip, AF_INET));
	} else array_add(arr, saprintf("%s : no_host_get_addrinfo()\n", hostname));
#endif

	return 0;
}

/*
 * ekg_resolver3()
 *
 * Resolver copied from irc plugin, 
 * it uses getaddrinfo() [or gethostbyname() if you don't have getaddrinfo]
 *
 *  - async	- watch handler.
 *  - data	- watch data handler.
 *
 *  in @a async watch you'll recv lines:
 *  	HOSTNAME IPv4 PF_INET 
 *  	HOSTNAME IPv4 PF_INET
 *  	HOSTNAME IPv6 PF_INET6
 *  	....
 *  	EOR means end of resolving, you should return -1 (temporary watch) and in type == 1 close fd.
 *
 *  NOTE, EKG2-RESOLVER-API IS NOT STABLE.
 *  	IT'S JUST COPY-PASTE OF SOME FUNCTION FROM OTHER PLUGINS, TO AVOID DUPLICATION OF CODE (ALSO CLEANUP CODE A LITTLE)
 *  	AND TO AVOID REGRESSION. 
 *  THX.
 */

watch_t *ekg_resolver3(plugin_t *plugin, const char *server, watcher_handler_func_t async, void *data) {
	int res, fd[2];

	debug("ekg_resolver3() resolving: %s\n", server);

	if (pipe(fd) == -1)
		return NULL;

	debug("ekg_resolver3() resolver pipes = { %d, %d }\n", fd[0], fd[1]);

	if ((res = fork()) == -1) {
		int errno2 = errno;

		close(fd[0]);
		close(fd[1]);

		errno = errno2;
		return NULL;
	}

	if (!res) {
		char *tmp	= xstrdup(server);

		/* Child */
		close(fd[0]);

		if (tmp) {
			char *tmp1 = tmp, *tmp2;
			char **arr = NULL;

			/* G->dj: I'm changing order, because
			 * we should connect first to first specified host from list...
			 * Yeah I know code look worse ;)
			 */
			do {
				if ((tmp2 = xstrchr(tmp1, ','))) *tmp2 = '\0';
				irc_resolver2(&arr, tmp1);
				tmp1 = tmp2+1;
			} while (tmp2);

			tmp2 = array_join(arr, NULL);
			array_free(arr);

			write(fd[1], tmp2, xstrlen(tmp2));
			write(fd[1], "EOR\n", 4);

			sleep(3);

			close(fd[1]);
			xfree(tmp2);
		}
		xfree(tmp);
		exit(0);
	}

	/* parent */
	close(fd[1]);

	/* XXX dodac dzieciaka do przegladania */
	return watch_add_line(plugin, fd[0], WATCH_READ_LINE, async, data);
}

