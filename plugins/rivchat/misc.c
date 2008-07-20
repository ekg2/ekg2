/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *                2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *                2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

#include <stdint.h>

#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

/* these stuff was stolen from ekg2 gadu-gadu plugin, and libgadu */

/* utf-8,iso-8859-2 <==> CP-1250 */

/* 80..9F = ?; here is A0..BF, C0..FF is the same */
static const unsigned char iso_to_cp_table[] = {
	0xa0, 0xa5, 0xa2, 0xa3, 0xa4, 0xbc, 0x8c, 0xa7,
	0xa8, 0x8a, 0xaa, 0x8d, 0x8f, 0xad, 0x8e, 0xaf,
	0xb0, 0xb9, 0xb2, 0xb3, 0xb4, 0xbe, 0x9c, 0xa1,
	0xb8, 0x9a, 0xba, 0x9d, 0x9f, 0xbd, 0x9e, 0xbf,
};

static const unsigned char cp_to_iso_table[] = {
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',
	 '?',  '?', 0xa9,  '?', 0xa6, 0xab, 0xae, 0xac,
	 '?',  '?',  '?',  '?',  '?',  '?',  '?',  '?',
	 '?',  '?', 0xb9,  '?', 0xb6, 0xbb, 0xbe, 0xbc,
	0xa0, 0xb7, 0xa2, 0xa3, 0xa4, 0xa1,  '?', 0xa7,
	0xa8,  '?', 0xaa,  '?',  '?', 0xad,  '?', 0xaf,
	0xb0,  '?', 0xb2, 0xb3, 0xb4,  '?',  '?',  '?',
	0xb8, 0xb1, 0xba,  '?', 0xa5, 0xbd, 0xb5, 0xbf,
};

/*
 * rivchat_cp_to_iso()
 *
 * zamienia na miejscu krzaczki pisane w cp1250 na iso-8859-2.
 *
 *  - buf.
 */
static unsigned char *rivchat_cp_to_iso(unsigned char *buf) {
	unsigned char *tmp = buf;

	if (!buf)
		return NULL;

	while (*buf) {
		if (*buf >= 0x80 && *buf < 0xC0)
			*buf = cp_to_iso_table[*buf - 0x80];

		buf++;
	}
	return tmp;
}

/*
 * rivchat_iso_to_cp()
 *
 * zamienia na miejscu iso-8859-2 na cp1250.
 *
 *  - buf.
 */
static unsigned char *rivchat_iso_to_cp(unsigned char *buf) {
	unsigned char *tmp = buf;
	if (!buf)
		return NULL;

	while (*buf) {
		if (*buf >= 0x80 && *buf < 0xA0)
			*buf = '?';
		else if (*buf >= 0xA0 && *buf < 0xC0)
			*buf = iso_to_cp_table[*buf - 0xA0];

		buf++;
	}
	return tmp;
}

#if (USE_UNICODE || HAVE_GTK)
static const uint16_t table_cp1250[] = {
	0x20ac, '?',    0x201a,    '?', 0x201e, 0x2026, 0x2020, 0x2021, 
	   '?', 0x2030, 0x0160, 0x2039, 0x015a, 0x0164, 0x017d, 0x0179, 
	   '?', 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 
	   '?', 0x2122, 0x0161, 0x203a, 0x015b, 0x0165, 0x017e, 0x017a, 
	0x00a0, 0x02c7, 0x02d8, 0x0141, 0x00a4, 0x0104, 0x00a6, 0x00a7, 
	0x00a8, 0x00a9, 0x015e, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x017b, 
	0x00b0, 0x00b1, 0x02db, 0x0142, 0x00b4, 0x00b5, 0x00b6, 0x00b7, 
	0x00b8, 0x0105, 0x015f, 0x00bb, 0x013d, 0x02dd, 0x013e, 0x017c, 
	0x0154, 0x00c1, 0x00c2, 0x0102, 0x00c4, 0x0139, 0x0106, 0x00c7, 
	0x010c, 0x00c9, 0x0118, 0x00cb, 0x011a, 0x00cd, 0x00ce, 0x010e, 
	0x0110, 0x0143, 0x0147, 0x00d3, 0x00d4, 0x0150, 0x00d6, 0x00d7, 
	0x0158, 0x016e, 0x00da, 0x0170, 0x00dc, 0x00dd, 0x0162, 0x00df, 
	0x0155, 0x00e1, 0x00e2, 0x0103, 0x00e4, 0x013a, 0x0107, 0x00e7, 
	0x010d, 0x00e9, 0x0119, 0x00eb, 0x011b, 0x00ed, 0x00ee, 0x010f, 
	0x0111, 0x0144, 0x0148, 0x00f3, 0x00f4, 0x0151, 0x00f6, 0x00f7, 
	0x0159, 0x016f, 0x00fa, 0x0171, 0x00fc, 0x00fd, 0x0163, 0x02d9, 
};

/* some code based on libiconv utf8_mbtowc() && utf8_wctomb() from utf8.h under LGPL-2.1 */

static int rivchat_utf8_helper(unsigned char *s, int n, uint16_t *ch) {
	unsigned char c = s[0];

	if (c < 0x80) {
		*ch = c;
		return 1;
	}

	if (c < 0xc2) 
		goto invalid;

	if (c < 0xe0) {
		if (n < 2)
			goto invalid;
		if (!((s[1] ^ 0x80) < 0x40))
			goto invalid;
		*ch = ((uint16_t) (c & 0x1f) << 6) | (uint16_t) (s[1] ^ 0x80);
		return 2;
	} 
	
	if (c < 0xf0) {
		if (n < 3)
			goto invalid;
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
			goto invalid;
		*ch = ((uint16_t) (c & 0x0f) << 12) | ((uint16_t) (s[1] ^ 0x80) << 6) | (uint16_t) (s[2] ^ 0x80);
		return 3;
	}

invalid:
	*ch = '?';
	return 1;
}
#endif

char *rivchat_locale_to_cp(char *b) {
	unsigned char *buf = (unsigned char *) b;

	if (!buf)
		return NULL;
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode) {	/* why not iconv? iconv is too big for recoding only utf-8 <==> cp1250 */
		char *newbuf;
		int newlen = 0;
		int len;
		int i, j;

		len = xstrlen(b);

		for (i = 0; i < len; newlen++) {
			uint16_t discard;

			i += rivchat_utf8_helper(&buf[i], len - i, &discard);
		}

		newbuf = xmalloc(newlen+1);

		for (i = 0, j = 0; buf[i]; j++) {
			uint16_t znak;
			int k;

			i += rivchat_utf8_helper(&buf[i], len - i, &znak);

			if (znak < 0x80) {
				newbuf[j] = znak;
				continue;
			}

			newbuf[j] = '?';

			for (k = 0; k < (sizeof(table_cp1250)/sizeof(table_cp1250[0])); k++) {
				if (table_cp1250[k] == znak) {
					newbuf[j] = (0x80 | k);
					break;
				}
			}
		}
		newbuf[j] = '\0';

		xfree(buf);

		return newbuf;
	} else
#endif
		return (char *) rivchat_iso_to_cp(buf);
}

char *rivchat_cp_to_locale(char *b) {
	unsigned char *buf = (unsigned char *) b;

	if (!buf)
		return NULL;
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode) { /* shitty way with string_t */
		char *newbuf;
		int newlen = 0;
		int i, j;

		for (i = 0; buf[i]; i++) {
			uint16_t znak = (buf[i] < 0x80) ? buf[i] : table_cp1250[buf[i]-0x80];

			if (znak < 0x80)	newlen += 1;
			else if (znak < 0x800)	newlen += 2;
			else			newlen += 3;
		}

		newbuf = xmalloc(newlen+1);

		for (i = 0, j = 0; buf[i]; i++) {
			uint16_t znak = (buf[i] < 0x80) ? buf[i] : table_cp1250[buf[i]-0x80];
			int count;

			if (znak < 0x80)	count = 1;
			else if (znak < 0x800)	count = 2;
			else			count = 3;

			switch (count) {
				case 3: newbuf[j+2] = 0x80 | (znak & 0x3f); znak = znak >> 6; znak |= 0x800;
				case 2: newbuf[j+1] = 0x80 | (znak & 0x3f); znak = znak >> 6; znak |= 0xc0;
				case 1: newbuf[j] = znak;
			}
			j += count;
		}
		newbuf[j] = '\0';

		xfree(buf);

		return newbuf;
	} else
#endif
		return (char *) rivchat_cp_to_iso(buf);
}

