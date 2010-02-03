/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
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
#include <ekg/debug.h>
#include <ekg/stuff.h>

#include "gg.h"

/*
 * gg_status_to_text()
 *
 * zamienia stan GG na enum ekg2 (dawniej: tekst, stad nazwa).
 *  
 *  - status
 */
int gg_status_to_text(int status)
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

#ifdef GG_FEATURE_DND_FFC
		case GG_STATUS_DND:
		case GG_STATUS_DND_DESCR:
			return EKG_STATUS_DND;

		case GG_STATUS_FFC:
		case GG_STATUS_FFC_DESCR:
			return EKG_STATUS_FFC;
#endif

		case GG_STATUS_BLOCKED:
			return EKG_STATUS_BLOCKED;
	}

	return EKG_STATUS_UNKNOWN;
}

/*
 * gg_text_to_status()
 *
 * zamienia stan enumowy (dawniej: tekstowy, stad nazwa) ekg2 na stan liczbowy ekg.
 *
 *  - status
 *  - descr
 */
int gg_text_to_status(const int status, const char *descr)
{
#define GG_TTS(x, y) if (status == EKG_STATUS_##x) return (descr ? GG_STATUS_##y##_DESCR : GG_STATUS_##y);
	GG_TTS(NA, NOT_AVAIL)
	GG_TTS(AVAIL, AVAIL)
	GG_TTS(AWAY, BUSY) /* I think we don't need to care about Jabber aways */
	GG_TTS(INVISIBLE, INVISIBLE)
#ifdef GG_FEATURE_DND_FFC
	GG_TTS(DND, DND)
	GG_TTS(FFC, FFC)
#endif
#undef GG_TTS
	if (status == EKG_STATUS_BLOCKED) return GG_STATUS_BLOCKED;

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

	if (!u || !s || !g)
		return -1;

	if (!ekg_group_member(u, "__blocked"))
		return -1;

	if (g->sess && g->sess->state == GG_STATE_CONNECTED)
		gg_remove_notify_ex(g->sess, atoi(u->uid + 3), gg_userlist_type(u));

	ekg_group_remove(u, "__blocked");

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

/**
 * gg_userlist_send()
 *
 * Send to server our user list.
 *
 * @param s - gg_session <b>[it's not session_t *]</b>
 * @param userlist - user list.
 *
 * @todo XXX, think about args here, maybe let's pass only session_t ?
 *
 * @return result of gg_notify_ex()
 */

int gg_userlist_send(struct gg_session *s, userlist_t *userlist) {
	int i, res;

	int count = LIST_COUNT2(userlist);
	userlist_t *ul;
	char *types;
	uin_t *uins;

	if (!count)
		return gg_notify(s, NULL, 0);
	
	uins = xmalloc(count * sizeof(uin_t));
	types = xmalloc(count * sizeof(char));

	for (ul = userlist, i = 0; ul; ul = ul->next, i++) {
		userlist_t *u = ul;

		uins[i] = atoi(u->uid + 3);
		types[i] = gg_userlist_type(u);
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
