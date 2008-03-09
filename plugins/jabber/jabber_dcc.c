#include "jabber_dcc.h"

#include <ekg/debug.h>

int jabber_dcc = 0;
int jabber_dcc_port = 0;
char *jabber_dcc_ip = NULL;

static int jabber_dcc_fd = -1;

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <ekg/plugins.h>
#include <ekg/userlist.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include "jabber.h"

WATCHER(jabber_dcc_handle_recv) {
	dcc_t *d = data;
	jabber_dcc_t *p;
	session_t *s;
	jabber_private_t *j;

/*	debug("jabber_dcc_handle_recv() data = %x type = %d\n", data, type); */
	if (type) {
		if (d && d->priv) dcc_close(d);
		return 0;
	}

	if (!d || !(p = d->priv))
		return -1;

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
						ouruid = saprintf("%s/%s", s->uid+5, j->resource);
						digest = jabber_dcc_digest(p->sid, d->uid+5, ouruid);
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
							"</query></iq>", d->uid+5, p->req, b->streamhost->jid);
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

			fwrite(&buf, len, 1, p->fd);

			d->offset += len;
			if (d->offset == d->size) {
				/* moze sie zdarzyc ze klient chce nam wyslac wiecej danych niz na poczatku zadeklarowal...
				 * ale takie psi tego nie umie... my tez nie.
				 */

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
	jabber_dcc_t *p = d->priv;
 
	char buf[16384];
	int flen, len;

	if (!d || !(p = d->priv)) {
		debug_error("jabber_dcc_handle_send() d == NULL 0x%x || d->priv == NULL 0x%x\n", d, d ? d->priv : NULL);
		return -1;
	}

	if (type) {
		p->sfd = -1;
		dcc_close(d);
		return -1;
	}

	if (!d->active) {
		debug_error("jabber_dcc_handle_send() d->active = 0\n");
		return 0; /* we must wait untill stream will be accepted... BLAH! awful */
	}

	if (!p->fd) {
		debug_error("jabber_dcc_handle_send() p->fd == NULL\n");
		return -1;
	}
	
	if (p->sfd != fd) {
		debug_error("jabber_dcc_handle_send() p->sfd != fd\n");
		return -1;
	}

	flen	= sizeof(buf);
	if (d->offset + flen > d->size)
		flen = d->size - d->offset;

	flen	= fread(&buf[0], 1, flen, p->fd);
	len	= write(fd, &buf[0], flen);

	if (len < 1 && len != flen) {
		debug_error("jabber_dcc_handle_send() len: %d\n", len);
		close(fd);
		return -1;
	}
	d->offset += len;
	
	if (d->offset == d->size) {
		if (!feof(p->fd)) debug_error("d->offset > d->size... file changes size?\n");
		print("dcc_done_send", format_user(p->session, d->uid), d->filename);

	/* Zamykamy to polaczenie... i tak takie psi nie przyjmie wiecej danych niz wyslalismy mu ... */
		close(fd);
		return -1;
	}

	return 0;
}

WATCHER(jabber_dcc_handle_accepted) { /* XXX, try merge with jabber_dcc_handle_recv() */
	char buf[200];
	int len;

	if (type)
		return -1;
	
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
		dcc_t *D;
		int i;

		for (D=dccs; D; D = D->next) {
			jabber_dcc_t *p = D->priv;

			char *this_sha1;
			list_t k;

			if (xstrncmp(D->uid, "xmpp:", 5)) continue; /* we skip not jabber dccs */

			if (!p) 		{ debug_error("[%s:%d] D->priv == NULL ?\n", __FILE__, __LINE__); continue; }			/* we skip invalid dccs */
			if (p->sfd != -1) 	{ debug_error("[%s:%d] p->sfd  != -1, already associated ?\n", __FILE__, __LINE__); continue; }	/* we skip associated dccs */
			if (p->protocol != JABBER_DCC_PROTOCOL_BYTESTREAMS) continue; 								/* we skip not BYTESTREAMS dccs */

			for (k = sessions; k; k = k->next) {
				session_t *s = k->data;
				jabber_private_t *j = s->priv;
				char *fulluid;

				if (!s->connected) continue;
				if (!(session_check(s, 1, "jid"))) continue;

				fulluid = saprintf("%s/%s", s->uid+5, j->resource);

				/* XXX, take care about initiator && we		*/
				/* D->type == DCC_SEND initiator -- we 		*/
				/*            DCC_GET  initiator -- D->uid+5	*/
				this_sha1 = jabber_dcc_digest(p->sid, fulluid, D->uid+5);

				debug_function("[JABBER_DCC_ACCEPTED] SHA1: %s THIS: %s (session: %s)\n", sha1, this_sha1, fulluid);
				if (!xstrcmp(sha1, this_sha1)) {
					d = D;
					p->sfd = fd;	/* associate client FD... */
					break;
				}
				xfree(fulluid);
			}
		}

		if (!d) {
			debug_error("[JABBER_DCC_ACCEPTED] SHA1 HASH NOT FOUND: %s\n", sha1);
			close(fd);
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

		watch_add(&jabber_plugin, fd, WATCH_NONE, jabber_dcc_handle_send, d);
		return -1;
	}
	return 0;
}

WATCHER(jabber_dcc_handle_accept) {
	struct sockaddr_in sin;
	int newfd;
	socklen_t sin_len = sizeof(sin);

	if (type) {
		close(fd);
		jabber_dcc_fd	= -1;
		jabber_dcc_port = 0;
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

/* zwraca watcha */
static watch_t *jabber_dcc_init(int port) {
	struct sockaddr_in sin;
	int fd;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug_error("jabber_dcc_init() socket() FAILED (%s)\n", strerror(errno));
		return NULL;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	while (bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))) {
		debug_error("jabber_dcc_init() bind() port: %d FAILED (%s)\n", port, strerror(errno));
		port++;
		if (port > 65535) {
			close(fd);
			return NULL;
		}

		sin.sin_port = htons(port);
	}
	if (listen(fd, 10)) {
		debug_error("jabber_dcc_init() listen() FAILED (%s)\n", strerror(errno));
		close(fd);
		return NULL;
	}
	debug_function("jabber_dcc_init() SUCCESSED fd:%d port:%d\n", fd, port);

	jabber_dcc_port = port;
	jabber_dcc_fd	= fd;
	return watch_add(&jabber_plugin, fd, WATCH_READ, jabber_dcc_handle_accept, NULL);
}

void jabber_dcc_close_handler(struct dcc_s *d) {
	jabber_dcc_t *p = d->priv;

	debug_error("jabber_dcc_close_handler() d->priv: 0x%x\n", d->priv);

	if (!p)
		return;

	if (!d->active && d->type == DCC_GET) {
		session_t *s = p->session;
		jabber_private_t *j;
		
		if (!s || !(j= session_private_get(s))) return;

		watch_write(j->send_watch, "<iq type=\"error\" to=\"%s\" id=\"%s\"><error code=\"403\">Declined</error></iq>", 
			d->uid+5, p->req);
	}

	d->priv = NULL;

	if (p) {
		if (p->protocol == JABBER_DCC_PROTOCOL_BYTESTREAMS) {
			/* XXX, free protocol-specified data */

		}

		if (p->sfd != -1) close(p->sfd);

		if (p->fd) fclose(p->fd);
		xfree(p->req);
		xfree(p->sid);
		xfree(p);
	} else {
		debug_error("[jabber] jabber_dcc_close_handler() d->priv == NULL ?! wtf?\n");
	}
}

dcc_t *jabber_dcc_find(const char *uin, /* without xmpp: */ const char *id, const char *sid) {
#define DCC_RULE(x) (!xstrncmp(x->uid, "xmpp:", 5) && !xstrcmp(x->uid+5, uin))
	dcc_t *d;
	if (!id && !sid) { debug_error("jabber_dcc_find() neither id nor sid passed.. Returning NULL\n"); return NULL; }

	for (d = dccs; d; d = d->next) {
		jabber_dcc_t *p = d->priv;

		if (DCC_RULE(d) && (!sid || !xstrcmp(p->sid, sid)) && (!id || !xstrcmp(p->req, id))) {
			debug_function("jabber_dcc_find() %s sid: %s id: %s founded: 0x%x\n", __(uin), __(sid), __(id), d);
			return d;
		}
	}
	debug_error("jabber_dcc_find() %s %s not founded. Possible abuse attempt?!\n", __(uin), __(sid));
	return NULL;
}

QUERY(jabber_dcc_postinit) {
	static watch_t *dcc_watch = NULL;

	debug("jabber_dcc_postinit() dcc: %d fd: %d dcc_watch: 0x%x\n", jabber_dcc, jabber_dcc_fd, dcc_watch);

	if (jabber_dcc_fd == -1) dcc_watch = NULL;

	if (jabber_dcc && !dcc_watch)
		dcc_watch = jabber_dcc_init(JABBER_DEFAULT_DCC_PORT); 
	else if (!jabber_dcc) {
		watch_free(dcc_watch);
		dcc_watch = NULL;
	}
	if (!dcc_watch) {
		jabber_dcc = 0;
		jabber_dcc_fd = -1;
	}
	return 0;
}

