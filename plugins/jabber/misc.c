/* $Id$ */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include "jabber.h"

char *jabber_attr(char **atts, const char *att)
{
	int i;

	for (i = 0; atts[i]; i += 2)
		if (!xstrcmp(atts[i], att))
			return atts[i + 1];
		
	return NULL;
}

static char iso_utf_length[256] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 0 - 15 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 16 - 31 */
	1, 1, 6, 1, 1, 1, 5, 6, 1, 1, 1, 1, 1, 1, 1, 1,		/* 32 - 47 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 1, 4, 1,		/* 48 - 63 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 64 - 79 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 80 - 95 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 96 - 111 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 112 - 127 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 128 - 143 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 144 - 159 */
	1, 2, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 2,		/* 160 - 175 */
	1, 2, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 2,		/* 176 - 191 */
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1,		/* 192 - 207 */
	1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 208 - 223 */
	1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1,		/* 224 - 239 */
	1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,		/* 240 - 255 */
};

static char *iso_utf_ent[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, "&quot;", 0, 0, 0, "&amp;", "&apos;", 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "&lt;", 0, "&gt;", 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, "\xC4\x84", 0, "\xC5\x81", 0, 0, "\xC5\x9A", 0, 0, 0, 0, 0, "\xC5\xB9", 0, 0, "\xC5\xBB",
	0, "\xC4\x85", 0, "\xC5\x82", 0, 0, "\xC5\x9B", 0, 0, 0, 0, 0, "\xC5\xBA", 0, 0, "\xC5\xBC",
	0, 0, 0, 0, 0, 0, "\xC4\x86", 0, 0, 0, "\xC5\x98", 0, 0, 0, 0, 0,
	0, "\xC5\x83", 0, "\xC3\x93", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, "\xC4\x87", 0, 0, 0, "\xC4\x99", 0, 0, 0, 0, 0,
	0, "\xC5\x84", 0, "\xC3\xB3", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/*
 * jabber_escape()
 *
 * zamienia tekst w iso-8859-2 na tekst w utf-8 z eskejpniêtymi znakami,
 * które bêd± przeszkadza³y XML-owi: ' " & < >.
 *
 *  - text
 *
 * zaalokowany bufor
 */
char *jabber_escape(const char *text)
{
	const unsigned char *p;
	unsigned char *res, *q;
	int len;

	if (!text)
		return NULL;

	for (p = text, len = 0; *p; p++)
		len += iso_utf_length[*p];

	res = xmalloc(len + 1);	
	memset(res, 0, len + 1);

	for (p = text, q = res; *p; p++) {
		char *ent = iso_utf_ent[*p];

		if (ent)
			xstrcpy(q, ent);
		else
			*q = *p;

		q += iso_utf_length[*p];
	}

	return res;
}

/*
 * jabber_unescape()
 *
 * zamienia tekst w utf-8 na iso-8859-2. xmlowe znaczki s± ju¿ zamieniane
 * przez expat, wiêc nimi siê nie zajmujemy.
 *
 *  - text
 *
 * zaalokowany bufor
 */
char *jabber_unescape(const char *text)
{
	const unsigned char *p;
	int len;
	char *res, *q;

	if (!text)
		return NULL;

	for (p = text, len = 0; *p; p++) {
		if (*p < 0x80 || *p >= 0xc0)
			len++;
	}

	res = xmalloc(len + 1);	
	memset(res, 0, len + 1);

	for (p = text, q = res; *p; p++) {
		if (*p >= 0x80 && *p < 0xc0)
			continue;

		if (*p >= 0xc0) {
			int i;

			*q = '?';

			for (i = 128; i < 256; i++) {
				if (iso_utf_ent[i] && !strncmp(p, iso_utf_ent[i], iso_utf_length[i])) {
					*q = i;
					break;
				}
			}

			q++;

			continue;
		}

		*q++ = *p;
	}

	return res;
}

/*
 * jabber_handle_write()
 *
 * obs³uga mo¿liwo¶ci zapisu do socketa. wypluwa z bufora ile siê da
 * i je¶li co¶ jeszcze zostanie, ponawia próbê.
 */
void jabber_handle_write(int type, int fd, int watch, void *data)
{
	jabber_private_t *j = data;
	int res;
	
	res = write(j->fd, j->obuf, j->obuf_len);

	if (res == -1) {
		debug("[jabber] write() failed: %s\n", strerror(errno));
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
		return;
	}

	if (res == j->obuf_len) {
		debug("[jabber] output buffer empty\n");
		xfree(j->obuf);
		j->obuf = NULL;
		j->obuf_len = 0;
		return;
	}

	memmove(j->obuf, j->obuf + res, j->obuf_len - res);
	j->obuf_len -= res;

	watch_add(&jabber_plugin, j->fd, WATCH_WRITE, 0, jabber_handle_write, j);
}

/*
 * jabber_write()
 *
 * wysy³a tekst do serwera, a je¶li nie da siê ca³ego wys³aæ, zapisuje
 * resztê do bufora i wy¶le przy najbli¿szej okazji.
 *
 *  - j
 *  - text
 *  - freetext - czy zwolniæ tekst po wys³aniu?
 */
int jabber_write(jabber_private_t *j, const char *format, ...)
{
	const char *buf;
	char *text;
	int len;
	va_list ap;

	if (!j || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);

	debug("[jabber] send %s\n", text);

	if (!j->obuf) {
		int res;

		len = xstrlen(text);
		res = write(j->fd, text, len);

		if (res == len) {
			xfree(text);
			return 0;
		}

		if (res == -1) {
			xfree(text);
			return -1;
		}

		buf = text + res;
	} else
		buf = text;

	len = xstrlen(buf);

	if (!j->obuf)
		watch_add(&jabber_plugin, j->fd, WATCH_WRITE, 0, jabber_handle_write, j);

	j->obuf = xrealloc(j->obuf, j->obuf_len + len);
	memcpy(j->obuf + j->obuf_len, buf, len);
	j->obuf_len += len;

	xfree(text);

	return 0;
}

