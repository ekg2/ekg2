/* $Id: windows.c 4413 2008-08-17 12:28:43Z peres $ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Pawe³ Maziarz <drg@infomex.pl>
 *		       2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include "ekg2-remote-config.h"

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "commands.h"
#include "debug.h"
#include "dynstuff.h"
#include "windows.h"
#include "userlist.h"
#include "sessions.h"
#include "themes.h"
#include "stuff.h"
#include "xmalloc.h"

#include "dynstuff_inline.h"
#include "queries.h"

int window_last_id = -1;		/* ostatnio wy¶wietlone okno */

window_t *windows = NULL;		/* lista okien */

static LIST_ADD_COMPARE(window_new_compare, window_t *) { return data1->id - data2->id; }
static LIST_FREE_ITEM(list_window_free, window_t *) { xfree(data->target); xfree(data->alias); userlists_destroy(&(data->userlist)); 
	xfree(data->irctopic); xfree(data->irctopicby); xfree(data->ircmode);
}

static __DYNSTUFF_LIST_ADD_SORTED(windows, window_t, window_new_compare);			/* windows_add() */
static __DYNSTUFF_LIST_REMOVE_SAFE(windows, window_t, list_window_free);			/* windows_remove() */
EXPORTNOT __DYNSTUFF_LIST_DESTROY(windows, window_t, list_window_free);				/* windows_destroy() */

EXPORTNOT int config_display_crap = 1;		/* czy wy¶wietlaæ ¶mieci? */

window_t *window_current = NULL;	/* okno aktualne, zawsze na co¶ musi wskazywaæ! */
window_t *window_status  = NULL;	/* okno statusowe, zawsze musi miec dobry adres w pamieci [NULL jest ok] */
EXPORTNOT window_t *window_debug	 = NULL;	/* okno debugowe, zawsze musi miec dobry adres w pamieci [NULL jest ok] */

window_lastlog_t *lastlog_current = NULL;

window_t *window_find_ptr(window_t *w) {
	window_t *v;

	for (v = windows; v; v = v->next) {
		if (w == v)
			return w;
	}
	return NULL;
}

window_t *window_find_sa(session_t *session, const char *target, int session_null_means_no_session) {
	userlist_t *u;
	window_t *w;

	if (!target || !xstrcasecmp(target, "__current"))
		return window_current->id ? window_current : window_status;

	if ((!xstrcasecmp(target, "__status")))
		return window_status;

	if (!xstrcasecmp(target, "__debug"))
		return window_debug;

	for (w = windows; w; w = w->next) {
		/* if targets match, and (sessions match or [no session was specified, and it doesn't matter to which session window belongs to]) */
		if (w->target && ((session == w->session) || (!session && !session_null_means_no_session)) && !xstrcasecmp(target, w->target))
			return w;
	}

	/* if we don't want session window, code below is useless */
	if (!session && session_null_means_no_session)
		return NULL;

	if (strncmp(target, "__", 2)) {
		session_t *s;
		for (s = sessions; s; s = s->next) {
		/* if sessions mishmash, and it wasn't NULL session, skip this session */
			if (session != s && session)
				continue;

		/* get_uid() was bad here. Because if even it's uid of user but we don't have it in userlist it'll do nothing. */
			if (!(u = userlist_find(s, target))) 
				continue;

			for (w = windows; w; w = w->next) {
				/* if there's target, and sessions match [no session specified, or sessions equal, check if entry (from userlist) match */
				if ((!session || session == w->session) && w->target) {
					if (u->nickname && !xstrcasecmp(u->nickname, w->target))
						return w;

					/* XXX, userlist_find() search only for u->nickname or u->uid.. so code below is useless? we can always return w; ?
					 *	However userlist_find() also strip resources if preset.. here we don't have it. 
					 *	maybe it's better, maybe not. Must think about it.
					 *	For now leave this code.
					 */
					if (!xstrcasecmp(u->uid, w->target))
						return w;
				}
			}
		}
	}
	return NULL;
}

window_t *window_find(const char *target) {
	return window_find_sa(NULL, target, 0);
}

static void window_switch_c(int id) {
	window_t *w;
	userlist_t *u;
	int ul_refresh = 0;

	for (w = windows; w; w = w->next) {
		if (id != w->id || w->floating)
			continue;

		if (id != window_current->id)
			window_last_id = window_current->id;
		
		if (w->id != 0 && w->session)
			session_current = w->session;
	
		window_current = w;
		query_emit_id(NULL, UI_WINDOW_SWITCH, &w);	/* XXX */

		w->act = 0;
		if (w->target && w->session && (u = userlist_find(w->session, w->target)) && u->blink) {
			u->blink	= 0;
			ul_refresh	= 1;
		}

		if (!(config_make_window & 3) && w->id == 1 && session_current) {
			userlist_t *ul;
			session_t *s = session_current;

			for (ul = s->userlist; ul; ul = ul->next) {
				userlist_t *u = ul;

				if (u->blink && !window_find_s(s, u->uid)) {
					u->blink	= 0;
					ul_refresh	= 1;
				}
			}
		}

		break;
	}

	if (ul_refresh)
		query_emit_id(NULL, USERLIST_REFRESH);
}

EXPORTNOT void remote_window_switch(int id) {
	/* XXX */
	window_switch_c(id);
}

static window_t *window_new_c(const char *target, session_t *session, int new_id) {
	window_t *w;
	
	w = xmalloc(sizeof(window_t));

	w->id = new_id;

	/* domy¶lne rozmiary zostan± dostosowane przez ui */
/*	w->top = 0;
	w->left = 0; */			/* xmalloc memset() to 0 memory */
	w->width = 1;
	w->height = 1;
	w->target = xstrdup(target);
	w->session = session;
/*	w->userlist = NULL; */		/* xmalloc memset() to 0 memory */

	windows_add(w);
	query_emit_id(NULL, UI_WINDOW_NEW, &w);	/* XXX */

	return w;
}

window_t *window_new(const char *target, session_t *session, int new_id) {
	if (target) {
		window_t *w;
		
		/* XXX, we don't check new_id here. stupido */
		if (!strcmp(target, "$")) {
			/* XXX, what about sessions, check if match? */
			return window_current;
		}
		
		w = window_find_s(session, target);

		if (w)
			return w;
	}
	/* XXX, check new_id ? */
/*	if (new_id != 0 && (w = window_exist(new_id)) 
		return w; */

	/* if no new_id given, than let's search for window id.. */
	if (new_id == 0) {
		window_t *v	= windows;	/* set to the beginning of the window list */
		int id		= 2;		/* [XXX] set to first valid id? */
		
		/* XXX, after it, we exactly know where to put new window to list, without list_add_sorted() we can do list_add_beggining() 
		 * but it'll ugly code. So not doing it :) */

		/* we can do this stuff. because windows are sorted by id */
		while (v) {
			window_t *w = v;
			v = v->next;		/* goto next window */

			if (w->id < 2)					/* [RESERVED CLASS: 0-1]	0 for __debug, 1 for __status */
				continue;

			/* if current window is larger than current id... than we found good id! */
			if (w->id > id)
				break;

			if (w->id >= 1000-1 && w->id < 2000 /* -1 */) {	/* [REVERVED CLASS: 1000-1999]	1k-1.999k windows reverved for special use. [1000 - __contacts, 1001 - __lastlog] */
				id = 2000;
				continue;
			}

			id = w->id+1;		/* current window+1 */
		}

		new_id = id;
	}

	/* debug window */
	if (new_id == -1)
		new_id = 0;

	return window_new_c(target, session, new_id);
}

EXPORTNOT window_t *remote_window_new(int id, const char *target) {
	window_t *w;

	if ((w = window_exist(id))) {
		debug_error("remote_window_new(%d) already registered!\n", id);
		/* XXX */
		return NULL;
	}

	return window_new_c(target, NULL, id);		/* session == NULL, XXX */
}

/* XXX, need check */
static void window_prev() {
	window_t *prev = NULL, *w;

	for (w = windows; w; w = w->next) {
		if (w->floating)
			continue;

		if (w == window_current && w != windows)
			break;

		prev = w;
	}

	if (!prev)
		return;

	if (!prev->id) {
		for (w = windows; w; w = w->next) {
			if (!w->floating)
				prev = w;
		}
	}

	window_switch(prev->id);
}

void window_kill(window_t *w) {
	int id;

	if (!w)
		return;
	
	id = w->id;

	if (id == 1 && w->target) {
		list_window_free(w);

		w->target	= NULL;
/*		w->session	= NULL; */

		query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &w);
		return;
	}

	if (w == window_status) {
		print("window_kill_status");
		return;
	}

	if (w == window_debug)
		return;
	
	if (w == window_current)	/* if it's current window. goto previous one. */
		window_prev();

	/* if config_sort_windows set, and it was not floating window... than resort stuff. */
	if (config_sort_windows && !w->floating) {
		window_t *w;

		for (w = windows; w; w = w->next) {
			if (w->floating)
				continue;
			/* XXX, i'm leaving it. however if we set sort_windows for example when we have: windows: 1, 3, 5, 7 and we will remove 3.. We'll still have: 1, 4, 6 not: 1, 2, 3 bug? */
			if (w->id > 1 && w->id > id)
				w->id--;
		}
	}

	query_emit_id(NULL, UI_WINDOW_KILL, &w);
	windows_remove(w);
}

EXPORTNOT void remote_window_kill(int id) {
	window_kill(window_exist(id));
	/* XXX */
}

window_t *window_exist(int id) {
	window_t *w;

	for (w = windows; w; w = w->next) {
		if (w->id == id) 
			return w;
	}

	return NULL;
}

char *window_target(window_t *window) {
	if (!window)			return "__current";

	if (window->target)		return window->target;
	else if (window->id == 1)	return "__status";
	else if (window->id == 0)	return "__debug";
	else				return "";
}

EXPORTNOT void window_session_set(window_t *w, session_t *new_session) {
	if (!w)
		return;

	if (w->session == new_session)
		return;

	w->session = new_session;

	if (w == window_current) {
		session_current = new_session;
		query_emit_id(NULL, SESSION_CHANGED);
	}

	query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &w);
}

void window_switch(int id) {
	/* XXX? */
	remote_request("REQWINDOW_SWITCH", itoa(id), NULL);
}

int window_session_cycle(window_t *w) {
	if (!w)
		return -1;

	/* XXX, assume w == window_current? */
	remote_request("REQSESSION_CYCLE", itoa(w->id), NULL);
	return 0;		/* it won't hurt */

	/* NOTE: SESSION_CHANGED emitowane gdy sie zmienia session_current */
	/* NOTE: UI_WINDOW_TARGET_CHANGED gdy sie zmienia w->target i/lub sesja (chyba) */
}

EXPORTNOT void windows_lock_all() {
	window_t *w;

	for (w = windows; w; w = w->next)
		w->lock = 1;
}

EXPORTNOT void windows_unlock_all() {
	window_t *w;

	for (w = windows; w; w = w->next)
		w->lock = 0;

	query_emit_id(NULL, UI_WINDOW_REFRESH);	/* powinno wystarczyc */
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
