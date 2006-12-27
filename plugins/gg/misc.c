/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *                2004 Piotr Kupisiewicz <deletek@ekg2.org>
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <libgadu.h>

#include <ekg/sessions.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include "gg.h"

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
 * gg_cp_to_iso()
 *
 * zamienia na miejscu krzaczki pisane w cp1250 na iso-8859-2.
 *
 *  - buf.
 */
static unsigned char *gg_cp_to_iso(unsigned char *buf) {
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
 * gg_iso_to_cp()
 *
 * zamienia na miejscu iso-8859-2 na cp1250.
 *
 *  - buf.
 */
static unsigned char *gg_iso_to_cp(unsigned char *buf) {
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

#if USE_UNICODE
extern int config_use_unicode;	/* stuff.h */

static const unsigned short table_cp1250[] = {
	0x20ac, 0x0000, 0x201a, 0x0000, 0x201e, 0x2026, 0x2020, 0x2021, 
	0x0000, 0x2030, 0x0160, 0x2039, 0x015a, 0x0164, 0x017d, 0x0179, 
	0x0000, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 
	0x0000, 0x2122, 0x0161, 0x203a, 0x015b, 0x0165, 0x017e, 0x017a, 
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

#endif

unsigned char *gg_locale_to_cp(unsigned char *buf) {
	if (!buf)
		return NULL;
#if USE_UNICODE
	if (config_use_unicode) {	/* why not iconv? iconv is too big for recoding only utf-8 <==> cp1250 */
		int len 	= mbstowcs(NULL, buf, 0)+1;	/* it's safe mbstowcs() can return -1 */
		wchar_t *tmp	= xmalloc(len*sizeof(wchar_t)); /* so here we malloc(0) it returns NULL */
		int i;
		
		if (len == 0 || (mbstowcs(tmp, buf, len-1)) == -1) {	/* invalid multibyte seq */
			if (len) debug("[%s:%d] mbstowcs() failed with: %s (%d)\n", errno, strerror(errno));
			xfree(tmp);
			return buf;		/* return `unicode` seq? */
		}
		buf = xrealloc(buf, len * sizeof(char));

		for (i = 0; i < len-1; i++) {
			int j = 0;
			buf[i] = '?';
			if (tmp[i] < 0x80) 
				buf[i] = tmp[i];
			else while (j < 8 * 16) {
				if (table_cp1250[j++] != tmp[i]) continue;
				buf[i] = 0x7F + j;
				break;
			}
		}
		xfree(tmp);
		buf[len-1] = 0;
		return buf;
	} else
#endif
		return gg_iso_to_cp(buf);
}

char *gg_cp_to_locale(unsigned char *buf) {
	if (!buf)
		return NULL;
#if USE_UNICODE
	if (config_use_unicode) { /* shitty way with string_t */
	/* wchar */
		int len = xstrlen(buf);
		wchar_t *tmp = xmalloc((len+1)*sizeof(wchar_t));	/* optimize len ? DELAYED */
	/* char */
		char *newbuf;
		int n, i;

		for (i=0; i < len; i++) {
			if (buf[i] < 0x80)		tmp[i] = buf[i];						/* ASCII */
			else if (buf[i] == 0x81 || buf[i] == 0x83 || buf[i] == 0x88 || buf[i] == 0x90 || buf[i] == 0x98) /* undefined chars in cp1250 */
							tmp[i] = '?';
			else				tmp[i] = table_cp1250[buf[i]-0x80];				/* unicode <==> cp1250 */
		}
		
		n	= wcstombs(NULL, tmp, 0)+1;
		newbuf	= xmalloc(n+1);

		if ((wcstombs(newbuf, tmp, n)) == -1) {
			debug("[%s:%d] wcstombs() failed with: %s (%d)\n", errno, strerror(errno));
			xfree(newbuf);
			xfree(tmp);
			return buf;		/* return `cp` seq ? */
		}
		xfree(tmp);
		xfree(buf); 			/* XXX, need testing */
		return newbuf;
	} else
#endif
		return gg_cp_to_iso(buf);
}

/*
 * gg_status_to_text()
 *
 * zamienia stan GG na opis.
 *  
 *  - status
 */
const char *gg_status_to_text(int status)
{
	switch (GG_S(status)) {
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			return EKG_STATUS_AVAIL;

		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
			return EKG_STATUS_NA;

		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			return EKG_STATUS_AWAY;
				
		case GG_STATUS_INVISIBLE:
		case GG_STATUS_INVISIBLE_DESCR:
			return EKG_STATUS_INVISIBLE;

		case GG_STATUS_BLOCKED:
			return EKG_STATUS_BLOCKED;
	}

	return EKG_STATUS_UNKNOWN;
}

/*
 * gg_text_to_status()
 *
 * zamienia stan tekstowy ekg na stan liczbowy ekg.
 *
 *  - text
 *  - descr
 */
int gg_text_to_status(const char *text, const char *descr)
{
	if (!xstrcasecmp(text, EKG_STATUS_NA))
		return (descr) ? GG_STATUS_NOT_AVAIL_DESCR : GG_STATUS_NOT_AVAIL;

	if (!xstrcasecmp(text, EKG_STATUS_AVAIL))
		return (descr) ? GG_STATUS_AVAIL_DESCR : GG_STATUS_AVAIL;

	if (!xstrcasecmp(text, EKG_STATUS_AWAY) || !xstrcasecmp(text, EKG_STATUS_DND) || !xstrcasecmp(text, EKG_STATUS_XA))
		return (descr) ? GG_STATUS_BUSY_DESCR : GG_STATUS_BUSY;

	if (!xstrcasecmp(text, EKG_STATUS_INVISIBLE))
		return (descr) ? GG_STATUS_INVISIBLE_DESCR : GG_STATUS_INVISIBLE;

	if (!xstrcasecmp(text, EKG_STATUS_BLOCKED))
		return GG_STATUS_BLOCKED;

	return GG_STATUS_NOT_AVAIL;
}

/*
 * gg_userlist_type()
 *
 * zwraca typ u¿ytkownika.
 */
char gg_userlist_type(userlist_t *u)
{
	if (u && ekg_group_member(u, "__blocked"))
		return GG_USER_BLOCKED;
	
	if (u && ekg_group_member(u, "__offline"))
		return GG_USER_OFFLINE;
	
	return GG_USER_NORMAL;
}

/*
 * gg_blocked_remove()
 *
 * usuwa z listy blokowanych numerków.
 *
 *  - uid
 */
int gg_blocked_remove(session_t *s, const char *uid)
{
	userlist_t *u = userlist_find(s, uid);
	gg_private_t *g = session_private_get(s);
	list_t l;

	if (!u || !s || !g)
		return -1;

	if (!ekg_group_member(u, "__blocked"))
		return -1;

	if (g->sess && g->sess->state == GG_STATE_CONNECTED)
		gg_remove_notify_ex(g->sess, atoi(u->uid + 3), gg_userlist_type(u));

	for (l = u->groups; l; ) {
		struct ekg_group *g = l->data;

		l = l->next;

		if (xstrcasecmp(g->name, "__blocked"))
			continue;

		xfree(g->name);
		list_remove(&u->groups, g, 1);
	}

	if (!u->nickname && !u->groups)
		userlist_remove(s, u);
	else {
		if (g->sess && g->sess->state == GG_STATE_CONNECTED)
			gg_add_notify_ex(g->sess, atoi(u->uid + 3), gg_userlist_type(u));
	}

	return 0;
}

/*
 * gg_blocked_add()
 *
 * dopisuje do listy blokowanych numerków.
 *
 *  - uid
 */
int gg_blocked_add(session_t *s, const char *uid)
{
	userlist_t *u = userlist_find(s, uid);
	gg_private_t *g = session_private_get(s);

	if (!s || !g)
		return -1;

	if (u && ekg_group_member(u, "__blocked"))
		return -1;
	
	if (!u)
		u = userlist_add(s, uid, NULL);
	else {
		if (g->sess && g->sess->state == GG_STATE_CONNECTED)
			gg_remove_notify_ex(g->sess, atoi(u->uid + 3), gg_userlist_type(u));
	}

	ekg_group_add(u, "__blocked");

	if (g->sess && g->sess->state == GG_STATE_CONNECTED)
		gg_add_notify_ex(g->sess, atoi(u->uid + 3), gg_userlist_type(u));
	
	return 0;
}

/*
 * gg_http_error_string()
 *
 * zwraca tekst opisuj±cy powód b³êdu us³ug po http.
 *
 *  - h - warto¶æ gg_http->error lub 0, gdy funkcja zwróci³a NULL.
 */
const char *gg_http_error_string(int h)
{
	switch (h) {
		case 0:
			return format_find((errno == ENOMEM) ? "http_failed_memory" : "http_failed_connecting");
		case GG_ERROR_RESOLVING:
			return format_find("http_failed_resolving");
		case GG_ERROR_CONNECTING:
			return format_find("http_failed_connecting");
		case GG_ERROR_READING:
			return format_find("http_failed_reading");
		case GG_ERROR_WRITING:
			return format_find("http_failed_writing");
		default:
			return "?";
	}
}

/*
 * gg_userlist_send()
 *
 * wysy³a do serwera listê u¿ytkowników.
 */
int gg_userlist_send(struct gg_session *s, list_t userlist)
{
	int i, res, count = 0;
	uin_t *uins;
	char *types;
	list_t l;

	for (l = userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (!xstrncasecmp(u->uid, "gg:", 3))
			count++;
	}

	uins = xmalloc(count * sizeof(uin_t));
	types = xmalloc(count * sizeof(char));

	for (i = 0, l = userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (xstrncasecmp(u->uid, "gg:", 3))
			continue;

		uins[i] = atoi(u->uid + 3);
		types[i] = gg_userlist_type(u);
		i++;
	}

	res = gg_notify_ex(s, uins, types, count);

	xfree(uins);
	xfree(types);
	return res;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
