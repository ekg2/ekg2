#include "ekg2-config.h"
#ifdef HAVE_EXPAT

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <string.h>

#include <ekg/dynstuff.h>
#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/sessions.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#ifdef HAVE_EXPAT_H
# include <expat.h>
#endif

#define RSS_DEFAULT_TIMEOUT 60

#include "feed.h"

typedef enum {
	RSS_PROTO_UNKNOWN = 0,
	RSS_PROTO_HTTP,
	RSS_PROTO_HTTPS,
	RSS_PROTO_FTP,
	RSS_PROTO_FILE,
} rss_proto_t;

typedef struct {
	char *url;
	char *uid;

	int connecting;		/* we wait for connect() ? */
	int getting;		/* we wait for read()	 ? */

	int headers_done;
	/* XXX headers_* */

	string_t buf;		/* buf with requested file */
/* PROTOs: */
	rss_proto_t proto;
	char *host;	/* protos: RSS_PROTO_HTTP, RSS_PROTO_HTTPS, RSS_PROTO_FTP			hostname 	*/
	char *ip;	/*		j/w								cached ip addr	*/
	int port;	/* 		j/w								port		*/
	char *file;	/* protos: 	j/w RSS_PROTO_FILE 						file		*/
} rss_feed_t;

typedef struct {
	list_t feeds;

} rss_private_t;

void rss_string_append(rss_feed_t *f, const char *str) {
	string_t buf		= f->buf;

	if (!buf) buf = f->buf = 	string_init(str);
	else				string_append(buf, str);
	string_append_c(buf, '\n');
}

rss_feed_t *rss_feed_find(session_t *s, const char *url) {
	rss_private_t *j = feed_private(s);
	list_t newsgroups = j->feeds;
	list_t l;
	rss_feed_t *feed;

	for (l = newsgroups; l; l = l->next) {
		feed = l->data;

		debug("rss_feed_find() %s %s\n", feed->url, url);
		if (!xstrcmp(feed->url, url)) 
			return feed;
	}
	debug("rss_feed_find() 0x%x NEW %s\n", newsgroups, url);

	feed		= xmalloc(sizeof(rss_feed_t));
	feed->uid	= saprintf("rss:%s", url);
	feed->url 	= xstrdup(url);

/*  URI: ^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))? */

	if (!xstrncmp(url, "https://", 8)) {
		url += 8;				/* skip https:// */
		feed->proto = RSS_PROTO_HTTPS;
	} else if (!xstrncmp(url, "ftp://", 6)) {
		url += 6;				/* skip ftp://	*/
		feed->proto = RSS_PROTO_FTP;
	} else if (!xstrncmp(url, "file://", 7)) {
		url += 7;				/* skip file:// */
		feed->proto = RSS_PROTO_FILE;
	} else if (!xstrncmp(url, "http://", 7)) {
		url += 7;				/* skip http:// */
		feed->proto = RSS_PROTO_HTTP;
	}

	if (feed->proto == RSS_PROTO_HTTP || feed->proto == RSS_PROTO_HTTPS || feed->proto == RSS_PROTO_FTP) {
		const char *req;
		char *host = NULL, *tmp;

		if ((req = xstrchr(url, '/')))	feed->host = xstrndup(url, req - url);
		else				feed->host = xstrdup(url);

		if ((tmp = xstrchr(host, ':'))) {	/* port http://www.cos:1234 */
			feed->port = atoi(tmp+1);
			*tmp = 0;
		} else {
			if (feed->proto == RSS_PROTO_FTP)	feed->port = 21;
			if (feed->proto == RSS_PROTO_HTTP)	feed->port = 80;
			if (feed->proto == RSS_PROTO_HTTPS)	feed->port = 443;
		}
		url = req;
	}
	if (feed->proto == RSS_PROTO_HTTP || feed->proto == RSS_PROTO_HTTPS || feed->proto == RSS_PROTO_FTP || feed->proto == RSS_PROTO_FILE) 
		feed->file = xstrdup(url);

	debug("[rss] proto: %d url: %s port: %d url: %s file: %s\n",
		feed->proto, feed->url, feed->port, feed->url, feed->file);

	list_add(&(j->feeds), feed, 0);
	return feed;
}

void rss_fetch_process(rss_feed_t *f, const char *str) {
	debug("rss_fetch_process() %s\n", str);

}

void rss_fetch_error(rss_feed_t *f, const char *str) {
	debug("rss_fetch_error() %s\n", str);
}

WATCHER(rss_fetch_handler) {
	rss_feed_t      *f = data;

	if (type) {
		if (f->buf) 
			rss_fetch_process(f, f->buf->str);
		else	rss_fetch_error(f, "Null f->buf");
		string_clear(f->buf);
		f->getting = 0;
		return 0;
	}

	if (f->headers_done) {
		rss_string_append(f, watch);
	} else {
		if (!xstrcmp(watch, "\r")) f->headers_done = 1;
		/* XXX, read some headers */
	}
	return 0;
}

/* handluje polaczenie, wysyla to co ma wyslac, dodaje łocza do odczytu */
WATCHER(rss_fetch_handler_connect) {
	int		res = 0; 
	socklen_t	res_size = sizeof(res);
	rss_feed_t	*f = data;

	f->connecting = 0;

	if (type == 1) {
		debug ("[rss] handle_connect(): type %d\n", type);
		return 0;
	}

	if (type || getsockopt(fd, SOL_SOCKET, SO_ERROR, &res, &res_size) || res) {
		if (type) 
			debug("[rss] handle_connect(): SO_ERROR %s\n", strerror(res));
		if (type == 2); /* connection timeout */
		close(fd);
		return -1; /* ? */
	}
	
	if (!f) {
		debug("[rss] handle_connect() feed: %s not found\n", data);
		close(fd);
		return -1;
	}

	if (f->proto == RSS_PROTO_HTTP) {
		char *request = saprintf(
			"GET %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"User-Agent: Ekg2 - evilny klient gnu (rss feeder)\r\n"
			/* XXX, other headers */
			"Connection: close\r\n"
			"\r\n", f->file, f->host);
		write(fd, request, xstrlen(request));
		xfree(request);
	} else {	/* unknown proto here ? */
		close(fd);
		return -1;
	}
	f->getting = 1;
	watch_add(&feed_plugin, fd, WATCH_READ_LINE, rss_fetch_handler, f);
	return -1;
}

int rss_url_fetch(session_t *s, const char *url) {
	rss_feed_t *f = rss_feed_find(s, url);
	int fd = -1;

	debug("rss_url_fetch() url: %s\n", url);

	if (f->connecting) {
		debug("[rss] Already connecting....\n");
		return -1;
	}
	
	if (f->getting) {
		debug("[rss] Already getting...\n");
		return -1;
	}

	if (f->proto == RSS_PROTO_HTTPS) {
		debug("[rss_url_fetch] Currently we don't support https protocol, sorry\n");
		return -1;
	}

	if (f->proto == RSS_PROTO_FTP) {
		debug("[rss_url_fetch] Currently we don't support ftp porotocol, sorry\n");
		return -1;
	}

	if (f->proto == RSS_PROTO_FILE) {
		fd = open(f->file, O_RDONLY);

		if (fd == -1) {
			debug("rss_url_fetch FILE: %s (error: %s,%d)", f->file, strerror(errno), errno);
			return -1;
		}
	}

	if (f->proto == RSS_PROTO_HTTP) {
		debug("rss_url_fetch HTTP: host: %s port: %d file: %s\n", f->host, f->port, f->file);

		if (f->port <= 0 || f->port >= 65535) return -1;
		
		if (!f->ip) {
			/* some static IPs */
			if (!xstrcmp(f->host, "rss.7thguard.net")) f->ip = xstrdup("83.145.128.5");
		}

		if (f->ip) {
			struct sockaddr_in sin;
			int ret;
			debug("rss_url_fetch %s using previously cached IP address: %s\n", f->host, f->ip);

			fd = socket(AF_INET, SOCK_STREAM, 0);

			sin.sin_addr.s_addr 	= inet_addr(f->ip);
			sin.sin_port		= htons(f->port);
			sin.sin_family		= AF_INET;

			f->connecting = 1;

			ret = connect(fd, (struct sockaddr *) &sin, sizeof(sin));

			watch_add(&feed_plugin, fd, WATCH_WRITE, rss_fetch_handler_connect, f);
		} else {
			/* XXX, resolver */
		}
		return fd;
	}
	return -1;
}


COMMAND(rss_command_check) {
	list_t l;
	for (l = session->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		rss_feed_t *f = rss_feed_find(session, u->uid+4);

		rss_url_fetch(session, f->url);
	}
	return 0;
}

COMMAND(rss_command_get) {
	return rss_url_fetch(session, target);
}

COMMAND(rss_command_connect) {
	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	session_connected_set(session, 1);
	query_emit(NULL, "protocol-connected", &session->uid);
	xfree(session->status);	session->status = xstrdup(EKG_STATUS_AVAIL);

	return 0;
}

/* userlist_find() is wrong for URI's cause userlist_find() strips everything after first '/' and treat that as resource */
userlist_t *rss_userlist_find(session_t *s, const char *uid) {
	list_t l;
	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (!xstrcmp(u->uid, uid)) 
			return u;
	}
	return NULL;
}

COMMAND(rss_command_subscribe) {
	userlist_t *u;

	if ((u = rss_userlist_find(session, target))) {
		printq("none", "You already subscribe this group");
		return -1;
	}

	userlist_add(session, target, target);
	return 0;
}

COMMAND(rss_command_unsubscribe) {
	userlist_t *u; 
	if (!(u = rss_userlist_find(session, target))) {
		printq("none", "Subscribtion not found, cannot unsubscribe");
		return -1;
	}
	userlist_remove(session, u);
	return 0;
}

void *rss_protocol_init() {
	rss_private_t *p = xmalloc(sizeof(rss_private_t));
	return p;
}

void rss_init() {
	command_add(&feed_plugin, TEXT("rss:connect"), "?", rss_command_connect, RSS_ONLY, NULL);
	command_add(&feed_plugin, TEXT("rss:check"), "u", rss_command_check, RSS_FLAGS, NULL);
	command_add(&feed_plugin, TEXT("rss:get"), "!u", rss_command_get, RSS_FLAGS_TARGET, NULL);

	command_add(&feed_plugin, TEXT("rss:subscribe"), "!",	rss_command_subscribe, RSS_FLAGS_TARGET, NULL); 
	command_add(&feed_plugin, TEXT("rss:unsubscribe"), "!",	rss_command_unsubscribe, RSS_FLAGS_TARGET, NULL);
}
#endif

