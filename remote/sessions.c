/* $Id: sessions.c 4593 2008-09-01 19:34:02Z peres $ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include "debug.h"
#include "dynstuff_inline.h"
#include "sessions.h"
#include "userlist.h"
#include "queries.h"
#include "windows.h"
#include "xmalloc.h"

// #include "themes.h"

session_t *sessions = NULL;

static LIST_ADD_COMPARE(session_compare, session_t *) { return xstrcasecmp(data1->uid, data2->uid); }

static __DYNSTUFF_LIST_ADD_SORTED(sessions, session_t, session_compare);	/* sessions_add() */

session_t *session_current = NULL;

session_t *session_find(const char *uid)
{
	session_t *s;

	if (!uid)
		return NULL;

	for (s = sessions; s; s = s->next) {
		if (!xstrcasecmp(s->uid, uid) || (s->alias && !xstrcasecmp(s->alias, uid)))
			return s;
	}

	return NULL;
}

EXPORTNOT session_t *remote_session_add(const char *uid, const char *plugin) {
	session_t *s;
	plugin_t *pl;

	if (!(pl = plugin_find(plugin))) {
		debug_error("remote_session_add() plugin == NULL\n");
		return NULL;
	}
		
	s = xmalloc(sizeof(session_t));
	s->uid		= xstrdup(uid);
	s->status	= EKG_STATUS_NA;
	s->plugin	= pl;

	sessions_add(s);

	/* XXX, session_var_default() */

	query_emit_id(NULL, SESSION_ADDED, &(s->uid));
	return s;
}

static LIST_FREE_ITEM(session_free_item, session_t *) {
	xfree(data->alias);
	xfree(data->uid);
	xfree(data->descr);

	userlist_free(data);
}

static __DYNSTUFF_LIST_REMOVE_SAFE(sessions, session_t, session_free_item);	/* sessions_remove() */
static __DYNSTUFF_LIST_DESTROY(sessions, session_t, session_free_item);	/* sessions_destroy() */

static int session_remove(const char *uid) {
	session_t *s;
	window_t *w;
	char *tmp;

	if (!(s = session_find(uid)))
		return -1;
	if (s == session_current)
		session_current = NULL;

	for (w = windows; w; w = w->next) {
		if (w->session == s) {
			w->session = NULL;
			if (sessions && sessions->next)
				window_session_cycle(w);
		} 
	}
	
	tmp = xstrdup(uid);
	query_emit_id(NULL, SESSION_CHANGED);
	query_emit_id(NULL, SESSION_REMOVED, &tmp);
	xfree(tmp);

	sessions_remove(s);
	return 0;
}

EXPORTNOT int remote_session_remove(const char *uid) {
	/* ekg2-remote: @ SESSION_REMOVED -> sessions_remove(s); */


	return 0;
}

const char *session_uid_get(session_t *s) {
	return (s) ? s->uid : NULL;
}

EXPORTNOT void sessions_free() {
	window_t *wl;

	if (!sessions)
		return;

	for (wl = windows; wl; wl = wl->next)
		wl->session = NULL;

	sessions_destroy();
	session_current = NULL;
	window_current->session = NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
