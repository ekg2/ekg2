/* $Id$ */

/*
 *  (C) Copyright XXX
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
#define __USE_GNU	/* glibc-2.8, needed for (struct hostent->h_addr) */
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
#include "dynstuff.h"
#include "net.h"
#include "plugins.h"
#include "sessions.h"
#include "xmalloc.h"

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

static int irc_resolver2(char ***arr, const char *hostname) {
#ifdef HAVE_GETADDRINFO
	struct addrinfo	*ai, *aitmp, hint;
	void		*tm = NULL;
#else
#warning "resolver: You don't have getaddrinfo(), resolver may not work! (ipv6 for sure)"
	struct hostent	*he4;
#endif

#ifdef HAVE_GETADDRINFO
	memset(&hint, 0, sizeof(struct addrinfo));
	hint.ai_socktype = SOCK_STREAM;

	if (!getaddrinfo(hostname, NULL, &hint, &ai)) {
		for (aitmp = ai; aitmp; aitmp = aitmp->ai_next) {
#ifdef HAVE_INET_NTOP
#define RESOLVER_MAXLEN INET6_ADDRSTRLEN
			static char	ip[RESOLVER_MAXLEN];
#else
			const char	*ip;
#endif

			if (aitmp->ai_family == AF_INET6)
				tm = &(((struct sockaddr_in6 *) aitmp->ai_addr)->sin6_addr);
			else if (aitmp->ai_family == AF_INET) 
				tm = &(((struct sockaddr_in *) aitmp->ai_addr)->sin_addr);
			else
				continue;
#ifdef HAVE_INET_NTOP
			inet_ntop(aitmp->ai_family, tm, ip, RESOLVER_MAXLEN);
#else
#warning "resolver: You have getaddrinfo() but no inet_ntop(), IPv6 won't work!"
			if (aitmp->ai_family == AF_INET6) {
				/* G: this doesn't have a sense since we're in child */
				/* print("generic_error", "You don't have inet_ntop() and family == AF_INET6. Please contact with developers if it happens."); */
				ip = "::";
			} else
				ip = inet_ntoa(*(struct in_addr *)tm);
#endif 
			array_add(arr, saprintf("%s %s %d\n", hostname, ip, aitmp->ai_family));
		}
		freeaddrinfo(ai);
	}
#else 
	if ((he4 = gethostbyname(hostname))) {
		/* copied from http://webcvs.ekg2.org/ekg2/plugins/irc/irc.c.diff?r1=1.79&r2=1.80 OLD RESOLVER VERSION...
		 * .. huh, it was 8 months ago..*/
		array_add(arr, saprintf("%s %s %d\n", hostname, inet_ntoa(*(struct in_addr *) he4->h_addr), AF_INET));
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

struct ekg_connect_data {
		/* internal data */
	char	**resolver_queue;	/* here we keep list of domains to be resolved	*/
	char	**connect_queue;	/* here we keep list of IPs to try to connect	*/

		/* data provided by user */
	int	port;
	char	*session;
	watcher_handler_func_t *async;
	int (*prefer_comparison)(const char *, const char *);
};

static void ekg_connect_data_free(struct ekg_connect_data *c) {
	/* XXX: call async with type==1? */

	array_free(c->resolver_queue);
	array_free(c->connect_queue);
	xfree(c->session);

	xfree(c);
}

	/* XXX: would we use it anywhere else? if yes, then move to dynstuff */
static char *array_shift(char ***array) {
	char *out	= NULL;
	int i		= 1;

	if (array && *array) {
		if (**array) {
			const int count = array_count(*array);

			out = (*array)[0];
			for (; i < count; i++)
				(*array)[i-1] = (*array)[i];
			(*array)[i-1] = NULL;
		}

		if (i == 1) { /* last element, free array */
			array_free(*array);
			*array = NULL;
		}
	}

	return out;
}

static int ekg_connect_loop(struct ekg_connect_data *c);

static WATCHER_LINE(ekg_connect_resolver_handler) {
	struct ekg_connect_data *c = (struct ekg_connect_data*) data;

	if (!data)
		return -1;

	if (type) {
		debug_function("ekg_connect_resolver_handler(), resolving done.\n");
		if (c->prefer_comparison)
			qsort(c->connect_queue, array_count(c->connect_queue), sizeof(char*),
					(void*) c->prefer_comparison);
		ekg_connect_loop(c);
		close(fd);
		return -1;
	}

	debug_function("ekg_connect_resolver_handler() = %s\n", watch);

	if (!xstrcmp(watch, "EOR"))
		return -1;
	
	array_add(&(c->connect_queue), xstrdup(watch));

	return 0;
}

static int ekg_build_sin(const char *data, const int defport, struct sockaddr **address, int *family) {
	struct sockaddr_in  *ipv4;
	struct sockaddr_in6 *ipv6;

	int len	= 0;
	int port;
	const char *addr;

	char **a = array_make(data, " ", 4, 1, 0);

	*address = NULL;

	if (array_count(a) < 3) 
		return 0;

	addr	= a[1];
	*family	= atoi(a[2]);
	port	= a[3] ? atoi(a[3]) : 0;

	if (port <= 0 || port > 66535)
		port = defport;

	if (*family == AF_INET) {
		len = sizeof(struct sockaddr_in);

		ipv4 = xmalloc(len);

		ipv4->sin_family = AF_INET;
		ipv4->sin_port   = htons(port);
#ifdef HAVE_INET_PTON
		inet_pton(AF_INET, addr, &(ipv4->sin_addr));
#else
#warning "irc: You don't have inet_pton() connecting to ipv4 hosts may not work"
#ifdef HAVE_INET_ATON /* XXX */
		if (!inet_aton(addr, &(ipv4->sin_addr))) {
			debug("inet_aton() failed on addr: %s.\n", addr);
		}
#else
#warning "irc: You don't have inet_aton() connecting to ipv4 hosts may not work"
#endif
#warning "irc: Yeah, You have inet_addr() connecting to ipv4 hosts may work :)"
		if ((ipv4->sin_addr.s_addr = inet_addr(co->address)) == -1) {
			debug("inet_addr() failed or returns 255.255.255.255? on %s\n", addr);
		}
#endif

		*address = (struct sockaddr *) ipv4;
	} else if (*family == AF_INET6) {
		len = sizeof(struct sockaddr_in6);

		ipv6 = xmalloc(len);
		ipv6->sin6_family  = AF_INET6;
		ipv6->sin6_port    = htons(port);
#ifdef HAVE_INET_PTON
		inet_pton(AF_INET6, addr, &(ipv6->sin6_addr));
#else
#warning "irc: You don't have inet_pton() connecting to ipv6 hosts may not work"
#endif

		*address = (struct sockaddr *) ipv6;
	} else
		debug_function("ekg_build_sin(), unknown addr family %d!\n", family);

	array_free(a);

	return len;
}

static WATCHER(ekg_connect_handler) {
	/* XXX */

	debug_function("ekg_connect_handler()\n");

	return -1;
}

static int ekg_connect_loop(struct ekg_connect_data *c) {
	char *host;
	session_t *s = session_find(c->session);

	if (!s) { /* session vanished! */
		debug_error("ekg_connect_loop(), looks like session '%s' vanished!\n", c->session);
		ekg_connect_data_free(c);
		return 0;
	}

	/* 1) if anything is in connect_queue, try to connect */
	if ((host = array_shift(&(c->connect_queue)))) {
		struct sockaddr *addr;
		int len, fd, family, connret;
		watch_t *w;

		do {
			const int one = 1;

			len = ekg_build_sin(host, c->port, &addr, &family);
			debug_function("ekg_connect_loop(), connect: %s, sinlen: %d\n", host, len);
			xfree(host);
			if (!len)
				break;

			if ((fd = socket(family, SOCK_STREAM, 0)) == -1) {
				const int err = errno;
				debug_error("ekg_connect_loop(), socket() failed: %s\n", strerror(err));
				break;
			}
			setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
			if (ioctl(fd, FIONBIO, &one) == -1) {
				const int err = errno;
				debug_error("ekg_connect_loop(), ioctl() failed: %s\n", strerror(err));
				break;
			}

			connret = connect(fd, addr, len);
			if (connret &&
#ifdef NO_POSIX_SYSTEM
					(WSAGetLastError() != WSAEWOULDBLOCK)
#else
					(errno != EINPROGRESS)
#endif
					) {
				const int err = errno;
				debug_error("ekg_connect_loop(), connect() failed: %s\n", strerror(err));
				break;
			}

			w = watch_add_session(s, fd, WATCH_WRITE, ekg_connect_handler);
			watch_timeout_set(w, 30 /* XXX */);

			return 1;
		} while (0);

		xfree(addr);
	}

	/* 2) if anything is in resolver_queue, try to resolve */
	if ((host = array_shift(&(c->resolver_queue)))) {
		debug_function("ekg_connect_loop(), resolve: %s\n", host);
		ekg_resolver3(s->plugin, host, (void*) ekg_connect_resolver_handler, c);
		xfree(host);

		return 1;
	}

	/* 3) fail */
//	c->async(0, 0, 0, c->session); /* XXX: pass error? */
	ekg_connect_data_free(c);
	return 0;
}

int ekg_connect(session_t *session, const char *server, const int port, int (*prefer_comparison)(const char *, const char *), watcher_handler_func_t async) {
	struct ekg_connect_data	*c;

	if (!session || !server || !async)
		return 0;
	c = xmalloc(sizeof(struct ekg_connect_data));

	/* 1) fill struct */
	c->resolver_queue	= array_make(server, ",", 0, 1, 1);
	c->session		= xstrdup(session_uid_get(session));
	c->async		= async;
	c->prefer_comparison	= prefer_comparison;
	c->port			= port;

	/* 2) call in the loop */
	return ekg_connect_loop(c);
}

