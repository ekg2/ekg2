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

#include <ekg/char.h>
#include <ekg/sessions.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include "gg.h"

CHAR_T *gg_cp_to_locale(unsigned char *buf_);
#if USE_UNICODE
struct table_entry {
	unsigned char 	ch;
	wchar_t 	wc;
};
/* ripperd from http://interif.org/links/download/old-patches/links-1.00pre12.diff.gz patch (c Jakub Bogusz <qboosh@pld-linux.org> ) */
struct table_entry table_cp1250 [] = {
	{ 0x80, 0x20AC }, { 0x81, 0x0000 }, { 0x82, 0x201A }, { 0x83, 0x0000 }, { 0x84, 0x201E }, { 0x85, 0x2026 }, { 0x86, 0x2020 },
	{ 0x87, 0x2021 }, { 0x88, 0x0009 }, { 0x89, 0x2030 }, { 0x8A, 0x0160 }, { 0x8B, 0x2039 }, { 0x8C, 0x015A }, { 0x8D, 0x0164 }, 
	{ 0x8E, 0x017D }, { 0x8F, 0x0179 }, { 0x90, 0x0000 }, { 0x91, 0x2018 }, { 0x92, 0x2019 }, { 0x93, 0x201C }, { 0x94, 0x201D },
	{ 0x95, 0x2022 }, { 0x96, 0x2013 }, { 0x97, 0x2014 }, { 0x98, 0x0000 }, { 0x99, 0x2122 }, { 0x9A, 0x0161 }, { 0x9B, 0x203A },
	{ 0x9C, 0x015B }, { 0x9D, 0x0165 }, { 0x9E, 0x017E }, { 0x9F, 0x017A }, { 0xA0, 0x00A0 }, { 0xA1, 0x02C7 }, { 0xA2, 0x02D8 },
	{ 0xA3, 0x0141 }, { 0xA4, 0x00A4 }, { 0xA5, 0x0104 }, { 0xA6, 0x00A6 }, { 0xA7, 0x00A7 }, { 0xA8, 0x00A8 }, { 0xA9, 0x00A9 },
	{ 0xAA, 0x015E }, { 0xAB, 0x00AB }, { 0xAC, 0x00AC }, { 0xAD, 0x00AD }, { 0xAE, 0x00AE }, { 0xAF, 0x017B }, { 0xB0, 0x00B0 },
	{ 0xB1, 0x00B1 }, { 0xB2, 0x02DB }, { 0xB3, 0x0142 }, { 0xB4, 0x00B4 }, { 0xB5, 0x00B5 }, { 0xB6, 0x00B6 }, { 0xB7, 0x00B7 },
	{ 0xB8, 0x00B8 }, { 0xB9, 0x0105 }, { 0xBA, 0x015F }, { 0xBB, 0x00BB }, { 0xBC, 0x013D }, { 0xBD, 0x02DD }, { 0xBE, 0x013E }, 
	{ 0xBF, 0x017C }, { 0xC0, 0x0154 }, { 0xC1, 0x00C1 }, { 0xC2, 0x00C2 }, { 0xC3, 0x0102 }, { 0xC4, 0x00C4 }, { 0xC5, 0x0139 },
	{ 0xC6, 0x0106 }, { 0xC7, 0x00C7 }, { 0xC8, 0x010C }, { 0xC9, 0x00C9 }, { 0xCA, 0x0118 }, { 0xCB, 0x00CB }, { 0xCC, 0x011A }, 
	{ 0xCD, 0x00CD }, { 0xCE, 0x00CE }, { 0xCF, 0x010E }, { 0xD0, 0x0110 }, { 0xD1, 0x0143 }, { 0xD2, 0x0147 }, { 0xD3, 0x00D3 },
	{ 0xD4, 0x00D4 }, { 0xD5, 0x0150 }, { 0xD6, 0x00D6 }, { 0xD7, 0x00D7 }, { 0xD8, 0x0158 }, { 0xD9, 0x016E }, { 0xDA, 0x00DA },
	{ 0xDB, 0x0170 }, { 0xDC, 0x00DC }, { 0xDD, 0x00DD }, { 0xDE, 0x0162 }, { 0xDF, 0x00DF }, { 0xE0, 0x0155 }, { 0xE1, 0x00E1 },
	{ 0xE2, 0x00E2 }, { 0xE3, 0x0103 }, { 0xE4, 0x00E4 }, { 0xE5, 0x013A }, { 0xE6, 0x0107 }, { 0xE7, 0x00E7 }, { 0xE8, 0x010D },
	{ 0xE9, 0x00E9 }, { 0xEA, 0x0119 }, { 0xEB, 0x00EB }, { 0xEC, 0x011B }, { 0xED, 0x00ED }, { 0xEE, 0x00EE }, { 0xEF, 0x010F },
	{ 0xF0, 0x0111 }, { 0xF1, 0x0144 }, { 0xF2, 0x0148 }, { 0xF3, 0x00F3 }, { 0xF4, 0x00F4 }, { 0xF5, 0x0151 }, { 0xF6, 0x00F6 },
	{ 0xF7, 0x00F7 }, { 0xF8, 0x0159 }, { 0xF9, 0x016F }, { 0xFA, 0x00FA }, { 0xFB, 0x0171 }, { 0xFC, 0x00FC }, { 0xFD, 0x00FD }, 
	{ 0xFE, 0x0163 }, { 0xFF, 0x02D9 }, { 0x00, 0x0000 }, };

#endif

/*
 * gg_cp_to_iso()
 *
 * zamienia na miejscu krzaczki pisane w cp1250 na iso-8859-2.
 *
 *  - buf.
 */
unsigned char *gg_cp_to_iso(unsigned char *buf)
{
	unsigned char *tmp = buf;
	if (!buf)
		return NULL;

	while (*buf) {
		if (*buf == (unsigned char)'¥') *buf = '¡';
		if (*buf == (unsigned char)'¹') *buf = '±';
		if (*buf == 140) *buf = '¦';
		if (*buf == 156) *buf = '¶';
		if (*buf == 143) *buf = '¬';
		if (*buf == 159) *buf = '¼';

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
unsigned char *gg_iso_to_cp(unsigned char *buf)
{
	unsigned char *tmp = buf;
	if (!buf)
		return NULL;

	while (*buf) {
		if (*buf == (unsigned char)'¡') *buf = '¥';
		if (*buf == (unsigned char)'±') *buf = '¹';
		if (*buf == (unsigned char)'¦') *buf = 'Œ';
		if (*buf == (unsigned char)'¶') *buf = 'œ';
		if (*buf == (unsigned char)'¬') *buf = '';
		if (*buf == (unsigned char)'¼') *buf = 'Ÿ';
		buf++;
	}
	return tmp;
}

unsigned char *gg_locale_to_cp(CHAR_T *buf_) {
	if (!buf_)
		return NULL;
#if USE_UNICODE
	unsigned char *buf = xmalloc(xwcslen(buf_) * sizeof(unsigned char));
	unsigned char *tmp = buf;

	while (*buf_) {
		struct table_entry *enc = (struct table_entry *) &(table_cp1250);
		if (*buf_ >= 0x80) {
			unsigned char a = '?'; /* jesli nie mozemy przekonwertowac znaczka unicodowego na cp1250 - nie mamy czegos takiego w tablicy to dajemy '?' ? :>*/
			while (enc->ch) {
				if (enc->wc == *buf_) {
					a = enc->ch;
					break;
				}
				enc++;
			}
			*buf = a;
		} else {
			*buf = *buf_;
		}
		buf_++;
		buf++;
	}
	return tmp;
#else
	return gg_iso_to_cp(buf_);
#endif
}

CHAR_T *gg_cp_to_locale(unsigned char *buf_) {
	if (!buf_)
		return NULL;
#if USE_UNICODE
	CHAR_T *buf = xmalloc((xstrlen(buf_)+1) * sizeof(CHAR_T));
	CHAR_T *tmp = buf;
	unsigned char cur;

	while ((cur = *buf_)) {
		if (cur >= 0x80 && cur <= 0xFF) {
			*buf = table_cp1250[cur-0x80].wc;
		} else *buf = *buf_;
		buf_++;
		buf++;
	}
	return tmp;
#else
	return gg_cp_to_iso(buf_);
#endif
}

/*
 * gg_status_to_text()
 *
 * zamienia stan GG na opis.
 *  
 *  - status
 */
const CHAR_T *gg_status_to_text(int status)
{
	switch (GG_S(status)) {
		case GG_STATUS_AVAIL:
		case GG_STATUS_AVAIL_DESCR:
			return WCS_EKG_STATUS_AVAIL;

		case GG_STATUS_NOT_AVAIL:
		case GG_STATUS_NOT_AVAIL_DESCR:
			return WCS_EKG_STATUS_NA;

		case GG_STATUS_BUSY:
		case GG_STATUS_BUSY_DESCR:
			return WCS_EKG_STATUS_AWAY;
				
		case GG_STATUS_INVISIBLE:
		case GG_STATUS_INVISIBLE_DESCR:
			return WCS_EKG_STATUS_INVISIBLE;

		case GG_STATUS_BLOCKED:
			return WCS_EKG_STATUS_BLOCKED;
	}

	return WCS_EKG_STATUS_UNKNOWN;
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
