#include "jabber_dcc.h"

#include <ekg/debug.h>

int jabber_dcc_port = 0;
char *jabber_dcc_ip = NULL;

#if WITH_JABBER_DCC

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <ekg/plugins.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include "jabber.h"

WATCHER(jabber_dcc_handle_recv) {
	dcc_t *d = data;
	jabber_dcc_t *p;
	session_t *s;
	jabber_private_t *j;

/*	debug("jabber_dcc_handle_recv() data = %x type = %d\n", data, type); */
	if (type) {
		/* XXX, close DCC */
		return 0;
	}

	if (!d || !(p = d->priv)) return -1;
	if (!(s = p->session) || !(j = jabber_private(s))) return -1;

	switch (p->protocol) {
		case (JABBER_DCC_PROTOCOL_BYTESTREAMS): {
			jabber_dcc_bytestream_t *b = p->private.bytestream;
			char buf[16384];	/* dla data transfer */
			int len;

			if (b->validate != JABBER_DCC_PROTOCOL_BYTESTREAMS) return -1;	/* someone is doing mess with memory ? */

			if (b->step != SOCKS5_DATA) {
				char buf[200];		/* dla SOCKS5 */

				len = read(fd, &buf, sizeof(buf)-1);
				if (len == 0) { close(fd); return -1; }
				buf[len] = 0;

				if (buf[0] != 5) { debug_error("SOCKS5: protocol mishmash\n"); return -1; }
				if (buf[1] != 0) { debug_error("SOCKS5: reply error: %x\n", buf[1]); return -1; }

				switch (b->step) {
					case (SOCKS5_CONNECT): {
						char *ouruid;
						char req[47];
						char *digest;
						int i;

						req[0] = 0x05;	/* version */
						req[1] = 0x01;	/* req connect */
						req[2] = 0x00;	/* RSV */
						req[3] = 0x03;	/* ATYPE: 0x01-ipv4 0x03-DOMAINNAME 0x04-ipv6 */
						req[4] = 40;	/* length of hash. */

						/* generate SHA1 hash */
						ouruid = saprintf("%s/%s", s->uid+4, j->resource);
						digest = jabber_dcc_digest(p->sid, d->uid+4, ouruid);
						for (i=0; i < 40; i++) req[5+i] = digest[i];
						xfree(ouruid);

						req[45] = 0x00;	req[46] = 0x00; /* port = 0 */

						write(fd, &req, sizeof(req));
						b->step = SOCKS5_AUTH;
						return 0;
					}
					case (SOCKS5_AUTH): {
						jabber_write(p->session, 
							"<iq type=\"result\" to=\"%s\" id=\"%s\">"
							"<query xmlns=\"http://jabber.org/protocol/bytestreams\">"
							"<streamhost-used jid=\"%s\"/>"
							"</query></iq>", d->uid+4, p->req, b->streamhost->jid);

						b->step = SOCKS5_DATA;
						d->active = 1;
						return 0;
					}
				default:
					debug_error("SOCKS5: UNKNOWN STATE: %x\n", b->step);
					close(fd);
					return -1;
				}
			}
			
			len = read(fd, &buf, sizeof(buf)-1);
			if (len == 0) { close(fd); return -1; }
			buf[len] = 0;

			/* XXX, write to file @ d->offset */
			FILE *f = fopen("temp.dump", d->offset == 0 ? "w" : "a");
			fseek(f, d->offset, SEEK_SET);
			fwrite(&buf, len, 1, f);
			fclose(f);

			d->offset += len;
			if (d->offset == d->size) {
				print("dcc_done_get", format_user(p->session, d->uid), d->filename);
				dcc_close(d);
				close(fd);
				return -1;
			}

			break;
		}
		default:
			debug_error("jabber_dcc_handle_recv() UNIMPLEMENTED PROTOTYPE: %x\n", p->protocol);
	}
	return 0;
}

WATCHER(jabber_dcc_handle_send) {  /* XXX, try merge with jabber_dcc_handle_recv() */
	dcc_t *d = data;

	char buf[16384];
	int flen, len;

	if (!d) return -1;

	if (type) {
		return -1;
	}

	if (!d->active) return 0; /* we must wait untill stream will be accepted... BLAH! awful */
	debug_error("jabber_dcc_handle_send() ready: %d type: %d fd: %d filename: %s --- data = 0x%x\n", d->active, type, fd, d->filename, data);


	flen = sizeof(buf);

	if (d->offset + flen > d->size) flen = d->size - d->offset;

/*
	FILE *f = fopen(d->filename, "r");
        fseek(f, d->offset, SEEK_SET); 
	flen = fread((char *) &buf, 16384, 1, f); 
*/

	len = write(fd, (char *) &buf, flen);

	debug_error("jabber_dcc_handle_send() write(): %d offset: %d ", len, d->offset);

	d->offset += len;
	debug_function("rest: %d\n", d->size-d->offset);

	if (d->offset /* == */ >= d->size) {
		close(fd);
		return -1;
	}

	return 0;
}

WATCHER(jabber_dcc_handle_accepted) { /* XXX, try merge with jabber_dcc_handle_recv() */
	char buf[200];
	int len;

	if (type) {
		return -1;
	}
	
	len = read(fd, &buf, sizeof(buf)-1);

	if (len < 1) return -1;
	buf[len] = 0;
	debug_function("jabber_dcc_handle_accepted() read: %d bytes data: %s\n", len, buf);

	if (buf[0] != 0x05) { debug_error("SOCKS5: protocol mishmash\n"); return -1; }

	if (buf[1] == 0x02 /* SOCKS5 AUTH */) {
		char req[2];
		req[0] = 0x05;
		req[1] = 0x00;
		write(fd, (char *) &req, sizeof(req));
	}

	if (buf[1] == 0x01 /* REQ CONNECT */ && buf[2] == 0x00 /* RSVD */ && buf[3] == 0x03 /* DOMAINNAME */ && len == 47 /* plen == 47 */ ) {
		char *sha1 = &buf[5];
		char req[47];

		dcc_t *d = NULL;
		list_t l;
		int i;

		for (l=dccs; l; l = l->next) {
			dcc_t *D = l->data;
			jabber_dcc_t *p = D->priv;
			char *this_sha1;

			if (xstrncmp(D->uid, "jid:", 4)) continue; /* we skip not jabber dccs */
			if (p->protocol != JABBER_DCC_PROTOCOL_BYTESTREAMS) continue; /* prottype must be JABBER_DCC_PROTOCOL_BYTESTREAMS */

			{
				list_t l;
				for (l = sessions; l; l = l->data) {
					session_t *s = l->data;
					jabber_private_t *j = s->priv;
					char *fulluid;

					if (!s->connected) continue;
					if (!(session_check(s, 1, "jid"))) continue;

					fulluid = saprintf("%s/%s", s->uid+4, j->resource);

					/* XXX, take care about initiator && we		*/
					/* D->type == DCC_SEND initiator -- we 		*/
					/*            DCC_GET  initiator -- D->uid+4	*/
					this_sha1 = jabber_dcc_digest(p->sid, fulluid, D->uid+4);

					debug_function("[JABBER_DCC_ACCEPTED] SHA1: %s THIS: %s (session: %s)\n", sha1, this_sha1, fulluid);
					if (!xstrcmp(sha1, this_sha1)) {
						d = D;
						break;
					}
					xfree(fulluid);
				}
			}
		}

		if (!d) {
			debug_error("[JABBER_DCC_ACCEPTED] SHA1 HASH NOT FOUND: %s\n", sha1);
			/* XXX */
			return -1;
		}

	/* HEADER: */
		req[0] = 0x05; /* SOCKS5 */
		req[1] = 0x00; /* SUCC */
		req[2] = 0x00; /* RSVD */
		req[3] = 0x03; /* DOMAINNAME */
		req[4] = 40;   /* length of hash */
	/* SHA1 HASH: */
		for (i=0; i < 40; i++) req[5+i] = sha1[i];
	/* PORT: */
		req[45] = 0x00; 
		req[46] = 0x00;
	/* LET'S SEND IWIL (OK, AUTH NOT IWIL) PACKET: */
		write(fd, (char *) &req, sizeof(req));

		watch_add(&jabber_plugin, fd, WATCH_WRITE, jabber_dcc_handle_send, d);
		return -1;
	}
	return 0;
}

WATCHER(jabber_dcc_handle_accept) {
	struct sockaddr_in sin;
	int newfd, sin_len = sizeof(sin);

	if (type) {
		return -1;
	}

	if ((newfd = accept(fd, (struct sockaddr *) &sin, &sin_len)) == -1) {
		debug_error("jabber_dcc_handle_accept() accept() FAILED (%s)\n", strerror(errno));
		return -1;
	}

	debug_function("jabber_dcc_handle_accept() accept() fd: %d\n", newfd);
	watch_add(&jabber_plugin, newfd, WATCH_READ, jabber_dcc_handle_accepted, NULL);
	return 0;
}

/* zwraca strukture z fd / infem XXX */
int jabber_dcc_init(int port) {
	struct sockaddr_in sin;
	int fd;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug_error("jabber_dcc_init() socket() FAILED (%s)\n", strerror(errno));
		return -1;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	while (bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))) {
		debug_error("jabber_dcc_init() bind() port: %d FAILED (%s)\n", port, strerror(errno));
		port++;
		if (port > 65535) {
			close(fd);
			return -1;
		}

		sin.sin_port = htons(port);
	}
	if (listen(fd, 10)) {
		debug_error("jabber_dcc_init() listen() FAILED (%s)\n", strerror(errno));
		close(fd);
		return -1;
	}
	debug_function("jabber_dcc_init() SUCCESSED fd:%d port:%d\n", fd, port);

	watch_add(&jabber_plugin, fd, WATCH_READ, jabber_dcc_handle_accept, NULL);

	jabber_dcc_port = port;
	return fd;
}

void jabber_dcc_close_handler(struct dcc_s *d) {
	jabber_dcc_t *p = d->priv;
	if (!d->active && d->type == DCC_GET) {
		session_t *s = p->session;
		jabber_private_t *j;
		
		if (!s || !(j= session_private_get(s))) return;

		watch_write(j->send_watch, "<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"403\">Declined</error></iq>", 
			d->uid+4, p->req);
	}
	if (p) {
		xfree(p->req);
		xfree(p->sid);
		xfree(p);
	}
	/* XXX, free ALL data */
	d->priv = NULL;
}

dcc_t *jabber_dcc_find(const char *uin, /* without jid: */ const char *id, const char *sid) {
#define DCC_RULE(x) (!xstrncmp(x->uid, "jid:", 4) && !xstrcmp(x->uid+4, uin))
	list_t l;
	if (!id && !sid) { debug_error("jabber_dcc_find() neither id nor sid passed.. Returning NULL\n"); return NULL; }

	for (l = dccs; l; l = l->next) {
		dcc_t *d = l->data;
		jabber_dcc_t *p = d->priv;

		if (DCC_RULE(d) && (!sid || !xstrcmp(p->sid, sid)) && (!id || !xstrcmp(p->req, id))) {
			debug_function("jabber_dcc_find() %s sid: %s id: %s founded: 0x%x\n", uin, sid, id, d);
			return d;
		}
	}
	debug_error("jabber_dcc_find() %s %s not founded. Possible abuse attempt?!\n", uin, sid);
	return NULL;
}
#endif

QUERY(jabber_dcc_postinit) {
#if WITH_JABBER_DCC
	jabber_dcc_init(JABBER_DEFAULT_DCC_PORT); /* XXX */
#else
	debug_error("[jabber] compilated without WITH_JABBER_DCC=1, disabling JABBER DCC.\n");
#endif
	return 0;
}

