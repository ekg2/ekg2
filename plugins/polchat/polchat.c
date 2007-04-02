/*
 *  (C) Copyright 2006	Jakub 'ABUKAJ' Kowalski
 *			Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
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

#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <sys/stat.h>
#define __USE_POSIX
#include <netdb.h>

#include <sys/time.h>

#ifdef __sun
#include <sys/filio.h>
#endif

#include <string.h>
#include <errno.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/userlist.h>

#define DEFQUITMSG "EKG2 - It's better than sex!"
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)

typedef struct {
	int fd;
	int connecting;
} polchat_private_t;

PLUGIN_DEFINE(polchat, PLUGIN_PROTOCOL, NULL);

#define POLCHAT_DEFAULT_HOST "polczat.pl"
#define POLCHAT_DEFAULT_PORT 14003
#define POLCHAT_DEFAULT_PORT_STR "14003"

/* HELPERS */
static inline char *dword_str(int dword) {	/* 4 bajty BE */
	static unsigned char buf[4];
	buf[0] = (dword & 0xff000000) >> 24;
	buf[1] = (dword & 0x00ff0000) >> 16;
	buf[2] = (dword & 0x0000ff00) >> 8;
	buf[3] = (dword & 0x000000ff);
	return &buf[0];
}

static inline char *word_str(short word) {	/* 2 bajty BE */
	static unsigned char buf[2];
	buf[0] = (word & 0xff00) >> 8;
	buf[1] = (word & 0x00ff);
	return &buf[0];
}

/* w data rozmiar danych do wyslania */

static WATCHER_LINE(polchat_handle_write) {
	static time_t t = 0;	/* last time_t execute of this function */
	watch_t *next_watch = NULL;
	list_t l;
	size_t fulllen = (size_t) data;
	int len = 0;

	if (type) 
		return 0;

	if (t == time(NULL)) return 0;	/* flood-protection */

	len = write(fd, watch, fulllen);

	if (len == fulllen) {	/* we sent all data, ok.. */
		/* turn on next watch */

		for (l = watches; l; l = l->next) {	/* watche sa od najnowszego po najstarszy.. dlatego musimy znalezc ostatni... */
			watch_t *w = l->data;

			if (w && w->fd == fd && w->type == WATCH_NONE) 
				next_watch = w;
		}

		if (next_watch) 
			next_watch->type = WATCH_WRITE;	/* turn on watch */
		t = time(NULL);
		errno = 0;
	} else if (len > 0) {
		for (l = watches; l; l = l->next) {
			watch_t *w = l->data;

			if (w && w->fd == fd && w->type == WATCH_WRITE_LINE && w->data == data) { /* this watch */
				w->data = (void *) fulllen - len;
			}
		}
	} 

	return (fulllen == len) ? -1 : len;
}


static watch_t *sendpkt(session_t *s, short headercode, ...)  {
	va_list ap;

	watch_t *w;
	polchat_private_t *j;
	int fd;

	char **arr = NULL;
	char *tmp;
	int size;
	int i;
/* XXX, headercode brzydko */

	if (!s || !(j = s->priv)) {
		debug("Invalid params\n");
		return NULL;
	}
	fd = j->fd;

	if (watch_find(&polchat_plugin, fd, WATCH_WRITE_LINE)) {
		w = watch_add_line(&polchat_plugin, fd, WATCH_WRITE_LINE, polchat_handle_write, NULL);
		w->type = WATCH_NONE;

	} else	w = watch_add_line(&polchat_plugin, fd, WATCH_WRITE_LINE, polchat_handle_write, NULL);

	size = 8;
	
	if (headercode) size += (2 * 1);

	va_start(ap, headercode);
	while ((tmp = va_arg(ap, char *))) {
	/* XXX przekoduj na utf-8 */
		array_add(&arr, xstrdup(tmp));
		size += strlen(tmp) + 3;
	}
	va_end(ap);

	string_append_raw(w->buf, dword_str(size), 4);
	string_append_raw(w->buf, word_str(headercode ? 1 : 0), 2);	/* headerlen / 256 + headerlen % 256 */
	string_append_raw(w->buf, word_str(array_count(arr)), 2);

/* headers */
	if (headercode)
		string_append_raw(w->buf, word_str(headercode), 2);

	if (arr) {
		for (i = 0; arr[i]; i++) {
			size_t len = xstrlen(arr[i]);
			string_append_raw(w->buf, word_str(len), 2);	/* LEN */
			string_append_n(w->buf, arr[i], len);		/* str */
			string_append_c(w->buf, '\0');			/* NUL */
		}
		array_free(arr);
	}
	
	w->data = (void *) w->buf->len;
	return w;
}


static QUERY(polchat_validate_uid) {
	char	*uid 	= *(va_arg(ap, char **));
	int	*valid 	= va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "polchat:", 8) && *(uid+8) != '\0') {
		(*valid)++;
		return -1;
	}

	return 0;
}

static QUERY(polchat_print_version) {
	 print("generic", "polchat plugin, proto code based on AmiX v0.2 (http://213.199.197.135/~kowalskijan/amix/) (c ABUKAJ), ekg2 <==> plugin code based on irc & jabber plugin");
	 return 0;
}

static void polchat_private_init(session_t *s) {
	polchat_private_t *j;

	if (!session_check(s, 0, "polchat"))
		return;

	if (s->priv) return;

	userlist_free(s);

	j = xmalloc(sizeof(polchat_private_t));
	j->fd = -1;

	session_connected_set(s, 0);

	s->priv = j;
}

static void polchat_private_destroy(session_t *s) {
	polchat_private_t *j = s->priv;

	if (!session_check(s, 1, "polchat"))
		return;

	xfree(j);
	s->priv = NULL;
}

static QUERY(polchat_session) {
	char		*session = *(va_arg(ap, char**));
	session_t	*s = session_find(session);

	if (!s)
		return 0;

	if (data)
		polchat_private_init(s);
	else
		polchat_private_destroy(s);

	return 0;
}

static void polchat_handle_disconnect(session_t *s, const char *reason, int type) {
	polchat_private_t *j = s->priv;
	char *__session, *__reason;
	list_t l;

	if (j->fd != -1) {
		for (l = watches; l; l = l->next) {
			watch_t *w = l->data;

			if (!w || w->fd != j->fd) continue;

			if (w->type == WATCH_NONE || w->type == WATCH_WRITE_LINE)
				watch_free(w);
		}

		close(j->fd);
		j->fd = -1;
	}
	s->connected = 0;
	j->connecting = 0;

	userlist_free(s);

/* notify */
	__session = xstrdup(session_uid_get(s));
	__reason = xstrdup(reason);
	query_emit(NULL, ("protocol-disconnected"), &__session, &__reason, &type);
	xfree(__session);
	xfree(__reason);
}

static WATCHER(polchat_handle_resolver) {
	if (type) {
		
	}

	return 0;
}

static int polchat_mode_to_ekg_mode(unsigned short status) {
	if (status & 0x0002) return EKG_STATUS_AVAIL;	/* OP */
	if (status & 0x0001) return EKG_STATUS_AWAY;	/* moderator ? */
	return EKG_STATUS_XA;				/* normal */
}

static int hex_to_dec(unsigned char ch1, unsigned char ch2) {
	int res = 0;

	if (xisdigit(ch1))	res = (ch1 - '0') << 4;
	else			res = ((tolower(ch1)-'a')+10) << 4;

	if (xisdigit(ch2))	res |= ch2 - '0';
	else			res |= ((tolower(ch2)-'a')+10);

	return res;
}

static unsigned char *html_to_ekg2(unsigned char *tekst) {
	string_t str = string_init(NULL);
	int bold = 0;
	int underline = 0;
	char color = '\0';

	while (*tekst) {
		if (*tekst == '<') {
			int reset = 0;

			unsigned char *btekst = tekst;
			while (*tekst != '>' && *tekst != '\0') 
				tekst++;

			if (*tekst == '\0') break;
			tekst++;

			if (btekst[1] == '/') {
				if (!xstrncmp("</u>", btekst, tekst-btekst))	underline = 0;
				if (!xstrncmp("</b>", btekst, tekst-btekst)) 	bold = 0;
				if (!xstrncmp("</font>", btekst, tekst-btekst))	color = 0;

				string_append(str, "%n");	reset = 1;
			}

			if ((reset && underline) || (!underline && !xstrncmp("<u>", btekst, tekst-btekst))) {
				underline = 1;
				string_append(str, "%U");
			}

			if (!reset && !xstrncmp("<font ", btekst, 6)) {
#define ishex(x) ((x >= '0' && x <= '9')  || (x >= 'A' && x <= 'F') || (x >= 'a' && x <= 'f'))
				unsigned char *fnt_color = xstrstr(btekst, " color=#");
				char new_color = color;

				if (fnt_color && fnt_color < tekst) {
					if (ishex(fnt_color[8]) && ishex(fnt_color[9]) && ishex(fnt_color[10]) && ishex(fnt_color[11]) && ishex(fnt_color[12]) && ishex(fnt_color[13])) 
						new_color = color_map(
								hex_to_dec(fnt_color[8], fnt_color[9]), 
								hex_to_dec(fnt_color[10], fnt_color[11]), 
								hex_to_dec(fnt_color[12], fnt_color[13]));
					if (new_color != color) {
						string_append_c(str, '%');
						string_append_c(str, bold ? toupper(new_color) : new_color);
						color = new_color;
					}
				}
#undef ishex
			} else if (reset && color) {
				string_append_c(str, '%');
				string_append_c(str, bold ? toupper(color) : color);
				continue;
			}

			if ((reset && bold) || (!bold && !xstrncmp("<b>", btekst, tekst-btekst))) {
				bold = 1;
				if (!color) string_append(str, "%T");
				else {
					string_append_c(str, '%');
					string_append_c(str, toupper(color));
				}
			}
			continue;

		} else if (*tekst == '&') {		/* eskejpniete */
			unsigned char *btekst = tekst;
			while (*tekst != ';' && *tekst != '\0') 
				tekst++;

			if (*tekst == '\0') break;
			tekst++;

			if (!xstrncmp("&amp;", btekst, tekst-btekst))	string_append_c(str, '&');
			if (!xstrncmp("&lt;", btekst, tekst-btekst))	string_append_c(str, '<');
			if (!xstrncmp("&gt;", btekst, tekst-btekst))	string_append_c(str, '>');
			if (!xstrncmp("&quot;", btekst, tekst-btekst))	string_append_c(str, '\"');
			/* ... */
			continue;
		}

		if (*tekst == '%' || *tekst == '\\') 
			string_append_c(str, '\\');

		string_append_c(str, *tekst);
		tekst++;
	}
	return string_free(str, 0);
}

static void processpkt(session_t *s, unsigned short nheaders, unsigned short nstrings, unsigned char *data, size_t len) {
	polchat_private_t *j = s->priv;

	unsigned short *headers;
	unsigned char **strings;
	int unk = 0;
	int pok = 0;
	int i;

	debug("processpkt() pkt->headerlen: %d nstrings: %d len: %d\n", nheaders, nstrings, len);
	if (!len) return;

	headers = xcalloc(nheaders, sizeof(short));
	strings	= xcalloc(nstrings+1, sizeof(char *));

/* x naglowkow po 2 bajty kazdy (short) BE */
	for (i = 0; i < nheaders; i++) {
		if (len < 2) goto invalid_packet; len -= 2;

		headers[i] = data[0] << 8 | data[1];
		data += 2;
	}

/* x stringow w &data[2] data[0..1]  -> rozmiar, stringi NUL terminated */
	for (i = 0; i < nstrings; i++) {
		unsigned short strlen;
		
		if (len < 2) goto invalid_packet; len -= 2;
		
		strlen = (data[0] << 8 | data[1]);

		if (len < strlen+1) goto invalid_packet; len -= (strlen+1);

/* XXX, przekonwertowac z utf-8 na locale */
		strings[i] = xstrndup(&data[2], strlen);
		data += (strlen + 3);
	}

	if (len) 
		debug("processpkt() headers && string parsed but len left: %d\n", len);

	pok = 1;

	if (nheaders) {
#define HEADER0_ECHOREQUEST	0x0001
#define HEADER0_MSG		0x0262
#define HEADER0_PRIVMSG		0x0263
#define HEADER0_CLIENTCONFIG	0x0266
#define HEADER0_JOIN		0x0267
#define HEADER0_PART		0x0268
#define HEADER0_ROOMINFO	0x0271
#define HEADER0_ROOMCONFIG	0x0272
#define HEADER0_WELCOMEMSG	0x0276
#define HEADER0_GOODBYEMSG	0x0277
#define HEADER0_NICKLIST	0x026b
#define HEADER0_ERRORMSG	0xffff
		/* move to another .c file or create callbacks...  ( IF IT'LL BE MORE THAN 500 LINES... )*/

		switch (headers[0] & 0xffff) {	/* '&' what for? */
			case HEADER0_ECHOREQUEST:
				if (nheaders == 1 && !nstrings)	{
					sendpkt(s, 0x00, NULL);

					debug_function("processpkt() HEADER0_ECHOREQUEST\n");
				} else		unk = 1;
				break;

			case HEADER0_MSG:
				if (nheaders == 1 && nstrings == 1) {
					char *tmp = html_to_ekg2(strings[0]);
					char *tmp2= format_string(tmp);
					xfree(tmp);

					print("none", tmp2);
					xfree(tmp2);

					debug_function("processpkt() HEADER0_MSG: %s\n", strings[0]);
				} else		unk = 1;
				break;

			case HEADER0_CLIENTCONFIG:
				if (nheaders == 1 && nstrings == 1)
					debug("HEADER0_CLIENTCONFIG: %s\n", strings[0]);
				else		unk = 1;
				break;

			case HEADER0_ROOMINFO:
				if (nheaders == 2 && nstrings == 2) {
					debug_function("processpkt() HEADER0_ROOMINFO: NAME: %s DESC: %s\n", strings[0], strings[1]);
			/* XXX, update j-> & use in ncurses header like irc-topic */
#if 0
					xfree(roomname);
					roomname = xstrdup(strings[0]);
					xfree(roomdesc);
					roomdesc = xstrdup(strings[1]);
#endif
				} else		unk = 1;
			break;

			case HEADER0_JOIN:
				if (nheaders == 2 && nstrings == 1) {
					userlist_t *u;
					char *uid = saprintf("polchat:%s", strings[0]);
					char *tmp;

					u = userlist_add(s, uid, strings[0]);
					
					u->status = polchat_mode_to_ekg_mode(headers[1]);

					xfree(uid);

					if ((headers[1] & 0x00ff8c) != 0x0000)
						debug_error("Unknown status of: %s data: %.4x\n", strings[0], headers[1]);

				} else 		unk = 1;
				break;

			case HEADER0_PART:
				if (nheaders == 1 && nstrings == 1) {
					debug_function("processpkt() HEADER0_PART: %s\n", strings[0]);
					userlist_remove(s, userlist_find(s, strings[0]));
				} else		unk = 1;
				break;

			case HEADER0_NICKLIST:
				if (nheaders >= 5 && 
						headers[1] == 0x0001 && 
						headers[2] == 0x0001 && 
						headers[3] == 0x0000 && 
						headers[4] == 0x0000) {

					for (i = 0; i < nstrings; i++) {
						debug_function("processpkt() HEADER0_NICKLIST: %s\n", strings[i]);
#if 0
						addnick(ppart->strings[i], ppart->header[2 * i + 5], ppart->header[2 * i + 6]);
						if (((ppart->header[2 * i + 5] & 0x00ff8c) != 0x0000 || ppart->header[2 * i + 6] != 0x0000) && debug)
							debug_error("Unknown status of: %s data1: %.4x data2: %.4x\n", strings[i], headers[2 * i + 5], headers[2 * i + 6]);
#endif
					}
				} else		unk = 1;
				break;

			case HEADER0_ROOMCONFIG:
				if (nheaders == 1 && nstrings == 2) {
					debug_function("HEADER0_ROOMCONFIG: %s\n", strings[0]);
#if 0
					if (NULL != (ptr = strstr(ppart->strings[0], "color_user=")))
					{
						ptr += 11;
						sscanf(ptr, "#%x", &tmp);
						colourt[0] = transformrgb((tmp >> 16) & 0x00FF, (tmp >> 8) & 0x00FF, tmp & 0x00FF);
					}
					if (NULL != (ptr = strstr(ppart->strings[0], "color_op=")))
					{
						ptr += 9;
						sscanf(ptr, "#%x", &tmp);
						colourop = transformrgb((tmp >> 16) & 0x00FF, (tmp >> 8) & 0x00FF, tmp & 0x00FF);
					}
					if (NULL != (ptr = strstr(ppart->strings[0], "color_guest=")))
					{
						ptr += 12;
						tmp = sscanf(ptr, "#%x #%x #%x #%x #%x #%x #%x", &tempt[0],
								&tempt[1], &tempt[2], &tempt[3], &tempt[4], &tempt[5],
								&tempt[6]);
						for (i = 0; i <tmp; i++)
						{
							colourt[i + 1] = transformrgb((tempt[i] >> 16) & 0x00FF, (tempt[i] >> 8) & 0x00FF, tempt[i] & 0x00FF);
						}
#endif
				} else		unk = 1;
				break;

			case HEADER0_WELCOMEMSG:
				if (nheaders == 1 && nstrings == 1) {
			/* new-status */
					s->status = EKG_STATUS_AVAIL;
			/* connected */
					j->connecting = 0;
					s->connected = 1;
			/* notify */
					char *__session = xstrdup(s->uid);
					query_emit(NULL, "protocol-connected", &__session);
					xfree(__session);

					debug_function("processpkt() HEADER0_WELCOMEMSG: %s\n", strings[0]);
				} else		unk = 1;
				break;

			case HEADER0_GOODBYEMSG:
				if (nheaders == 1 && nstrings == 1) {
					userlist_free(s);

					debug_function("HEADER0_GOODBYEMSG: %s\n", strings[0]);
				} else		unk = 1;
				break;

			case HEADER0_PRIVMSG:
				if (nheaders == 1 && nstrings == 2) {
					debug("processpkt() HEADER0_PRIVMSG INC(?) : NICK: %s MSG: %s\n", strings[1], strings[0]);
				} else if (nheaders == 1 && nstrings == 3) {
					debug("processpkt() HEADER0_PRIVMSG OUT(?) : UNK[0]: %s\nUNK[1]: %s\nMSG: %s\n", strings[0], strings[1], strings[2]);
				} else		unk = 1;
				break;
#if 0
			case 0x0269:/*NICK update*/
				if (nheaders == 2 && nstrings == 1) {
					/* addnick(ppart->strings[0], ppart->header[1], 0x0000); */

					if ((headers[1] & 0x00ff8c) != 0x0000) {
						debug_error("Unknown status of: %s data: %.4x\n", strings[0], headers[1]);
				} else		unk = 1;
				break;

			case 0x026a:/*I have absolutly no idea - chyba ze wlazlem jako ja???*/
				if (nheaders == 2 && nstrings == 1)  {
					if (headers[1] != 0x0004) 
						debug_error("0x0004 != %.4x NICK: %s\n", headers[1], strings[0]);
				} else		unk = 1;
				break;
#endif

			case HEADER0_ERRORMSG:
				if (nheaders == 1 && nstrings == 1) {
				/* XXX, utf -> locale */
					polchat_handle_disconnect(s, strings[0], EKG_DISCONNECT_FAILURE);
				} else 		unk = 1;
				break;

			case 0x0000:
			default:
				unk = 1;
		}
	} else 	debug_error("processpkt() XXX nheaders == 0 !!!\n");

	if (unk) {
		int i;
		debug_error("processpkt() XXX nheaders: %d nstrings: %d\n\t", nheaders, nstrings);
		for (i = 0; i < nheaders; i++) 
			debug_error("headers[%d]: %.4x ", i, headers[i]);

		debug_error("\n");
		for (i = 0; i < nstrings; i++)
			debug_error("\tstrings[%d]: %s\n", i, strings[i]);

		debug_error("\n");
	}

invalid_packet: 
	if (!pok)
		debug_error("invalid len packet!! exploit warning?\n");

	xfree(headers);
	array_free((char **) strings);
}

static WATCHER(polchat_handle_stream) {
	session_t *s = session_find(data);
	polchat_private_t *j; 

	unsigned char buffer[4];
	unsigned char *result = NULL;
	int len;

	if (type) {
		if (s && session_connected_get(s))
			polchat_handle_disconnect(s, NULL, EKG_DISCONNECT_NETWORK);
		xfree(data);
		return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	/* XXX tutaj fragmentacja... sprawdzic czy mamy j->buffer .. jesli mamy j->buffer to dodajemy do bufora i sprawdzamy czy dl. == j->bufferlen  */

	if ((len = read(fd, buffer, 4)) == 4) {
		unsigned int rlen = (buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3])-4;
		debug("STEP1: Read 4 bytes from fd, ok rlen: %d (%d)\n", rlen);

		result = xmalloc(rlen);

		if ((len = read(fd, result, rlen)) < rlen)  {
			debug("STEP2: Err Read %d bytes from fd, fragmented packed?\n", len);
			xfree(result);
			return -1;
		} else {
			struct {
				short headerlen		__attribute__((__packed__));
				short nstrings		__attribute__((__packed__));
				unsigned char data[]	__attribute__((__packed__));
			} *pkt = (void *) result;

			if (!pkt->headerlen && !pkt->nstrings) {
				debug("<blink> CONNECTION LOST :-( </blink>");
				xfree(result);
				return -1;
			}

			processpkt(s, ntohs(pkt->headerlen), ntohs(pkt->nstrings), pkt->data, len-4);
		}
		xfree(result);
	} else if (len > 0)  {
		debug("Read %d bytes from fd: %d not good, fragmented packed?\n", len, fd);
		return -1; /* XXX */
	} else {
		debug("Connection closed/ error XXX\n");
		return -1;
	}
	return 0;
}

static WATCHER(polchat_handle_connect) {
	session_t *s = session_find(data);
	polchat_private_t *j;

	if (type) {
		xfree(data);
		return 0;
	}

	if (!s || !(j = s->priv)) {
		debug("session: %s deleted\n", data);
		return -1;
	}
	j->connecting = 2;
/* XXX, oproznij watche? */

	sendpkt(s, 0x0578, 
		"darkjames", "", "", "aaaaaszcxzczxcz", 
		/* XXX j->nick, j->pass, j->roompass, j->room, */
		"http://www.polchat.pl/chat/room.phtml/?room=AmiX", "polchat.pl", "nlst=1&nnum=1&jlmsg=true&ignprv=false", "ekg2-CVS-polchat", NULL);

	watch_add(&polchat_plugin, fd, WATCH_READ, polchat_handle_stream, xstrdup(data));
	return -1;
}

static COMMAND(polchat_command_connect) {
	polchat_private_t *j = session->priv;
	const char *server;
	const char *nick;

	int fd;
	int port;
	struct sockaddr_in sin;
	int one = 1;
	int res;

	if (j->connecting) {
		printq("during_connect", session_name(session));
		return -1;
	}

	if (session_connected_get(session)) {
		printq("already_connected", session_name(session));
		return -1;
	}

	if (!(server = session_get(session, "server"))) {
		server = POLCHAT_DEFAULT_HOST;
		return -1;
	}

	if (!(nick = session_get(session, "nickname"))) {
		printq("generic_error", "gdzie lecimy ziom ?! [/session nickname]");
		return -1;
	}

	port = session_int_get(session, "port");
	if (port < 0 || port > 65535) 
		port = POLCHAT_DEFAULT_PORT;


	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		debug("[polchat] socket() failed: %s\n", strerror(errno));
		polchat_handle_disconnect(session, strerror(errno), EKG_DISCONNECT_FAILURE); 
		return -1;
	}

	j->connecting = 1;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr("64.34.174.225");

        if (ioctl(fd, FIONBIO, &one) == -1) 					debug("[polchat] ioctl() FIONBIO failed: %s\n", strerror(errno));
        if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one)) == -1)	debug("[polchat] setsockopt() SO_KEEPALIVE failed: %s\n", strerror(errno));

	res = connect(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in)); 

	if (res == -1 && errno != EINPROGRESS) {
                debug("[polchat] connect() failed: %s (errno=%d)\n", strerror(errno), errno);
		polchat_handle_disconnect(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		return -1;
	}

	j->fd = fd;

	watch_add(&polchat_plugin, fd, WATCH_WRITE, polchat_handle_connect, xstrdup(session->uid));

	return 0;
}

static COMMAND(polchat_command_disconnect) {
	polchat_private_t   *j = session->priv;
	const char      *reason = params[0]?params[0]:QUITMSG(session);

	if (timer_remove(&polchat_plugin, "reconnect") == 0) {		/* XXX here, coz we can remove wrong reconnect timer for more than one polchat session */
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!j->connecting && !session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (reason && session_connected_get(session)) {
		char *quitmsg = saprintf("/quit %s", reason);
		sendpkt(session, 0x019a, quitmsg, NULL);
		xfree(quitmsg);
	}

	if (j->connecting)
		polchat_handle_disconnect(session, reason, EKG_DISCONNECT_STOPPED);
	else    
		polchat_handle_disconnect(session, reason, EKG_DISCONNECT_USER);

	return 0;
}


static COMMAND(polchat_command_reconnect) {
	polchat_private_t   *j = session->priv;

	if (j->connecting || session_connected_get(session))
		polchat_command_disconnect(name, params, session, target, quiet);
	return polchat_command_connect(name, params, session, target, quiet);
}

static COMMAND(polchat_command_msg) {
	/* w target -> target */
	/* XXX, params[1] MUST BE in utf-8 otherwise it'll disconnect session, likes some jabberd do. */
	sendpkt(session, 0x019a, params[1], NULL);
/* NOTE: sending `/quit` msg disconnect session */	/* XXX, escape? */

	return 0;
}

static COMMAND(polchat_command_inline_msg) {
	const char	*p[2] = { NULL, params[0] };
	if (!target || !params[0])
		return -1;
	return polchat_command_msg(("msg"), p, session, target, quiet);
}

static plugins_params_t polchat_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		SESSION_VAR_ALIAS, VAR_STR, 0, 0, NULL), 
	PLUGIN_VAR_ADD("auto_connect", 		SESSION_VAR_AUTO_CONNECT, VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("log_formats", 		SESSION_VAR_LOG_FORMATS, VAR_STR, "irssi", 0, NULL),
	PLUGIN_VAR_ADD("nickname", 		0, VAR_STR, NULL, 0, NULL), 
	PLUGIN_VAR_ADD("port", 			SESSION_VAR_PORT, VAR_INT, POLCHAT_DEFAULT_PORT_STR, 0, NULL),
	PLUGIN_VAR_ADD("server", 		SESSION_VAR_SERVER, VAR_STR, POLCHAT_DEFAULT_HOST, 0, NULL),
	PLUGIN_VAR_END()
};

int polchat_plugin_init(int prio) {
	polchat_plugin.params = polchat_plugin_vars;
	plugin_register(&polchat_plugin, prio);
	query_connect(&polchat_plugin, "protocol-validate-uid", polchat_validate_uid, NULL);
	query_connect(&polchat_plugin, "session-added",		polchat_session, (void*) 1);
	query_connect(&polchat_plugin, "session-removed",	polchat_session, (void*) 0);
	query_connect(&polchat_plugin, "plugin-print-version",	polchat_print_version, NULL);
#if 0
	query_connect(&irc_plugin, ("ui-window-kill"),	irc_window_kill, NULL);
	query_connect(&irc_plugin, ("irc-topic"),	irc_topic_header, NULL);
	query_connect(&irc_plugin, ("status-show"),	irc_status_show_handle, NULL);
#endif

#define POLCHAT_ONLY 		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define POLCHAT_FLAGS 		POLCHAT_ONLY | SESSION_MUSTBECONNECTED
#define POLCHAT_FLAGS_TARGET	POLCHAT_FLAGS | COMMAND_ENABLEREQPARAMS | COMMAND_PARAMASTARGET
	
	command_add(&polchat_plugin, "polchat:", "?",		polchat_command_inline_msg, POLCHAT_FLAGS, NULL);
	command_add(&polchat_plugin, "polchat:msg", "!uUw !",	polchat_command_msg,	    POLCHAT_FLAGS_TARGET, NULL);
	command_add(&polchat_plugin, "polchat:connect", NULL,   polchat_command_connect,    POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:disconnect", "r ?",polchat_command_disconnect,POLCHAT_ONLY, NULL);
	command_add(&polchat_plugin, "polchat:reconnect", "r ?", polchat_command_reconnect, POLCHAT_ONLY, NULL);

	return 0;
}

static int polchat_plugin_destroy() {
	list_t  l;
	for (l = sessions; l; l = l->next)
		polchat_private_destroy((session_t*) l->data);

	plugin_unregister(&polchat_plugin);
	return 0;
}
