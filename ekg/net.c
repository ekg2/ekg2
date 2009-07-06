/* $Id$ */

/*
 *  (C) Copyright xxxx-2008 XXX
 *			Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
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
#ifndef NO_POSIX_SYSTEM
#include <netdb.h>	/* OK */
#endif

#ifdef __sun	  /* Solaris, thanks to Beeth */
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
 *	Includes were copied from jabber.c, where there's ? in comment, it's possibly not needed.
 *	It was done this way, to avoid regression.
 *	THX.
 */

#include "debug.h"
#include "dynstuff.h"
#include "net.h"
#include "plugins.h"
#include "sessions.h"
#include "xmalloc.h"
#include "srv.h"

#ifdef LIBIDN /* stolen from squid->url.c (C) Duane Wessels */
static const char valid_hostname_chars_u[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-._";
#endif

struct ekg_connect_data {
		/* internal data */
	char	**resolver_queue;	/* here we keep list of domains to be resolved		*/
	char	**connect_queue;	/* here we keep list of IPs to try to connect		*/
	char	**connect_queue2;	/* another list of IPs, this time unpreferred family	*/
	watch_t	*current_watch;
	watch_t	*internal_watch;	/* allowing user to abort connecting */

		/* data provided by user */
	int	prefer_family;		/* preferred address family */
	int	proto_port;		/* default port of a protocol */
	int	port;
	char	*session;
	watcher_handler_func_t *async;
};


/* Removes port from 'hostname' and returns it
 * WARN: hostname is modified */
static int ekg_resolver_split(char *hostname, const int defport) {
	char *p = xstrrchr(hostname, ':');
	int i;

	if (p && (i = atoi(p+1)) > 0 && (i <= 65535) && (xstrspn(p+1, "0123456789") == xstrlen(p+1))) {
		*p = '\0';
		return i;
	}

		/* remove braces from (IPv6) address,
		 * XXX: do we really need to check if it's IPv6? IPv4 doesn't use braces, domains neither. */
	if (*hostname == '[' && hostname[xstrlen(hostname) - 1] == ']') {
		hostname[xstrlen(hostname) - 1] = 0; /* shorten, i.e. remove ']' */
		memmove(hostname, hostname + 1, xstrlen(hostname)); /* xstrlen(hostname) gets trailing \0 too */
	}

	return defport;
}

/* 
 * with main part changed
 *
 * proto_port is used for srv resolver, since e.g.
 * jabber plugin using ssl uses port 5223, but for
 * srv resolving we need 5222, so as a proto_port you need
 * to supply default port for given protocol
 *
 * current implementation overwrites port returned by srv query with
 * port supplied in `port` argument, probably later I'll change it
 * to duplicate entries.
 *
 */
watch_t *ekg_resolver4(plugin_t *plugin, const char *server, watcher_handler_func_t async, void *data, const int proto_port, const int port, const int proto) {
	int res, fd[2];

	debug("ekg_resolver4() resolving: %s:%d [def proto port: %d]\n", server, port, proto_port);

	if (pipe(fd) == -1)
		return NULL;

	debug("ekg_resolver4() resolver pipes = { %d, %d }\n", fd[0], fd[1]);

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
			char *hostname = tmp, *nexthost;

			/* this loop iterates over hosts,
			 * GiM->peres, I know this is already in ekg_connect_loop,
			 * but right now irc doesn't use it, so leave this loop
			 * as it is [the two won't collide with each other]
			 */
			do {
				gim_host *gim_host_list = NULL;
				int sport;

				if ((nexthost = xstrchr(hostname, ','))) *nexthost = '\0';
				sport = ekg_resolver_split(hostname, port);

				srv_resolver (&gim_host_list, hostname, proto_port, sport, 0);
				basic_resolver (&gim_host_list, hostname, sport);
				resolve_missing_entries(&gim_host_list);

				hostname = nexthost+1;
				write_out_and_destroy_list(fd[1], gim_host_list);
			} while (nexthost);

			write(fd[1], "EOR\n", 4);
			sleep(3);
			close(fd[1]);
		}
		xfree(tmp);
		exit(0);
	}

	/* parent */
	close(fd[1]);

	/* XXX dodac dzieciaka do przegladania */
	return watch_add_line(plugin, fd[0], WATCH_READ_LINE, async, data);
}


static void ekg_connect_data_free(struct ekg_connect_data *c) {
	if (c->internal_watch) {
		c->internal_watch->data = NULL;	/* avoid double free */
		watch_free(c->internal_watch);
	}
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

static int ekg_connect_loop(struct ekg_connect_data *c); /* predeclared */

static WATCHER_LINE(ekg_connect_resolver_handler) {
	struct ekg_connect_data *c = (struct ekg_connect_data*) data;

	if (!c)
		return -1;

	if (type) {
		if (session_find(c->session))
			debug_function("ekg_connect_resolver_handler(), resolving done.\n");
		ekg_connect_loop(c);
		close(fd);
		return -1;
	} else if (!session_find(c->session))
		return -1;

	debug_function("ekg_connect_resolver_handler() = %s\n", watch);

	if (!xstrcmp(watch, "EOR"))
		return -1;
	else if (c->prefer_family) {			/* determine address family */
		const char *ax = xstrchr(watch, ' ');
		int af;

		if (ax)
			ax = xstrchr(ax+1, ' ');
		if (!ax) { /* WTF?! */
			debug_error("ekg_connect_resolver_handler(), unable to determine address family with '%s'\n", watch);
			return 0;
		}

		af = atoi(ax+1);

		if (af != c->prefer_family) { /* unpreferred go to queue2 */
			array_add(&(c->connect_queue2), xstrdup(watch));
			return 0;
		}
	}
	/* if preferred or no preference */
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

	if (array_count(a) < 3) {
		array_free(a);
		return 0;
	}

	addr	= a[1];
	*family	= atoi(a[2]);
	port	= a[3] ? atoi(a[3]) : 0;

	if (port <= 0 || port > 66535)
		port = defport;

	if (*family == AF_INET) {
		len = sizeof(struct sockaddr_in);

		ipv4 = xmalloc(len);

		ipv4->sin_family = AF_INET;
		ipv4->sin_port	 = htons(port);
#ifdef HAVE_INET_PTON
		inet_pton(AF_INET, addr, &(ipv4->sin_addr));
#else
#warning "irc: You don't have inet_pton() connecting to ipv4 hosts may not work"
#ifdef HAVE_INET_ATON /* XXX */
		if (!inet_aton(addr, &(ipv4->sin_addr)))
			debug_error("inet_aton() failed on addr: %s.\n", addr);
#else
#warning "irc: You don't have inet_aton() connecting to ipv4 hosts may not work"
#endif
#warning "irc: Yeah, You have inet_addr() connecting to ipv4 hosts may work :)"
		if ((ipv4->sin_addr.s_addr = inet_addr(addr)) == -1)
			debug_error("inet_addr() failed or returns 255.255.255.255? on %s\n", addr);
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
	struct ekg_connect_data *c = (struct ekg_connect_data*) data;
	int res = 0; 
	socklen_t res_size = sizeof(res);
	session_t *s;

	if (!c)
		return -1;

	debug_function("ekg_connect_handler(), type = %d.\n", type);

	if (type == 1)
		return 0;

	if (!((s = session_find(c->session)))) {
		ekg_connect_loop(c);
		close(fd);
		return -1;
	}

	if (type || getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		if (res)
			debug_error("ekg_connect_handler(), error: %s\n", strerror(res));
		ekg_connect_loop(c);
		close(fd);

		return -1;
	} 

	if (s && c->async(type, fd, WATCH_WRITE, s) > 0) {
		debug_error("ekg_connect_handler(), looks like caller didn't like our job.\n");
		ekg_connect_loop(c);
		close(fd);
	} else
		ekg_connect_data_free(c);

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
	/* 1b) if connect_queue is empty, try unpreferred families too
	 *	if anyone would like ekg_connect() to try other servers with pref family too,
	 *	please let me know */
	if ((host = array_shift(&(c->connect_queue))) || (host = array_shift(&(c->connect_queue2)))) {
		struct sockaddr *addr;
		int len, fd, family, connret;
		watch_t *w;

		const int timeout = session_int_get(s, "connect_timeout");

		do {
			int one = 1;

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
				close(fd);
				break;
			}

			connret = connect(fd, addr, len);
			if (connret && errno != EINPROGRESS) {
				const int err = errno;
				debug_error("ekg_connect_loop(), connect() failed: %s\n", strerror(err));
				close(fd);
				break;
			}

			w = watch_add(s->plugin, fd, WATCH_WRITE, ekg_connect_handler, c);

			if (timeout > 0)
				watch_timeout_set(w, timeout);

			xfree(addr);
			c->current_watch = w;
			return 1;
		} while (0);

		xfree(addr);
	}

	/* 2) if anything is in resolver_queue, try to resolve */
	if ((host = array_shift(&(c->resolver_queue)))) {
		watch_t *w;

		debug_function("ekg_connect_loop(), resolve: %s\n", host);
		w = ekg_resolver4(s->plugin, host, (void*) ekg_connect_resolver_handler, c, c->proto_port, c->port, IPPROTO_TCP);
		xfree(host);

		c->current_watch = w;
		return 1;
	}

	/* 3) fail */
	if (s)
		c->async(-1, -1, WATCH_WRITE, s);
	ekg_connect_data_free(c);
	return 0;
}

static WATCHER(ekg_connect_abort) {
	struct ekg_connect_data *c = (struct ekg_connect_data*) data;

	if (type == 1) {
		if (data) {
			c->internal_watch		= NULL;	/* avoid freeing twice */
			c->current_watch->data		= NULL;	/* avoid running handler, just make watch disappear */
			watch_free(c->current_watch);
			ekg_connect_data_free(c);

			debug_function("ekg_connect_abort(), data freed.\n");
		}
	} else
		debug_error("ekg_connect_abort() called with incorrect type!\n");

	return -1;
}

/* default comparison function, based on 'prefer_family' sessionvar */
watch_t *ekg_connect(session_t *session, const char *server, const int proto_port, const int port, watcher_handler_func_t async) {
	const int pref	= session_int_get(session, "prefer_family");
	struct ekg_connect_data	*c;

	if (!session || !server || !async)
		return 0;
	c = xmalloc(sizeof(struct ekg_connect_data));

	/* 1) fill struct */
	c->resolver_queue	= array_make(server, ",", 0, 1, 1);
	c->session		= xstrdup(session_uid_get(session));
	c->async		= async;
	c->port			= port;
	c->proto_port		= proto_port;

	c->internal_watch		= xmalloc(sizeof(watch_t));
	c->internal_watch->type		= WATCH_NONE;
	c->internal_watch->handler	= ekg_connect_abort;
	c->internal_watch->data		= c;

	if (pref == 4)
		c->prefer_family = AF_INET;
	else if (pref == 6)
		c->prefer_family = AF_INET6;

	/* 2) call in the loop */
	ekg_connect_loop(c);

	/* 3) return internal watch, allowing caller to abort */
	return c->internal_watch;
}



/**** OLD STUFFF *****/


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
 *	IT'S JUST COPY-PASTE OF SOME FUNCTION FROM OTHER PLUGINS, TO AVOID DUPLICATION OF CODE (ALSO CLEANUP CODE A LITTLE)
 *	AND TO AVOID REGRESSION. 
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
				memcpy(&a, he->h_addr_list[0], sizeof(a));
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

