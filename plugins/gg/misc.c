/* $Id$ */

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

/*
 * gg_cp_to_iso()
 *
 * zamienia na miejscu krzaczki pisane w cp1250 na iso-8859-2.
 *
 *  - buf.
 */
void gg_cp_to_iso(unsigned char *buf)
{
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'¥') *buf = '¡';
		if (*buf == (unsigned char)'¹') *buf = '±';
		if (*buf == 140) *buf = '¦';
		if (*buf == 156) *buf = '¶';
		if (*buf == 143) *buf = '¬';
		if (*buf == 159) *buf = '¼';

		buf++;
	}
}

/*
 * gg_iso_to_cp()
 *
 * zamienia na miejscu iso-8859-2 na cp1250.
 *
 *  - buf.
 */
void gg_iso_to_cp(unsigned char *buf)
{
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'¡') *buf = '¥';
		if (*buf == (unsigned char)'±') *buf = '¹';
		if (*buf == (unsigned char)'¦') *buf = 'Œ';
		if (*buf == (unsigned char)'¶') *buf = 'œ';
		if (*buf == (unsigned char)'¬') *buf = '';
		if (*buf == (unsigned char)'¼') *buf = 'Ÿ';
		buf++;
	}
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
	status = GG_S(status);

	switch (status) {
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
		struct group *g = l->data;

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
	}

	return "?";
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
