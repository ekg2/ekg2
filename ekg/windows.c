/* $Id$ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 * 		       2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "dynstuff.h"
#include "windows.h"
#include "userlist.h"
#include "sessions.h"
#include "themes.h"
#include "stuff.h"
#include "xmalloc.h"

list_t windows = NULL;			/* lista okien */
int config_display_crap = 1;		/* czy wy¶wietlaæ ¶mieci? */
int window_last_id = -1;		/* ostatnio wy¶wietlone okno */
window_t *window_current = NULL;	/* zawsze na co¶ musi wskazywaæ! */

/*
 * window_find()
 *
 * it seeks for an window with given target
 * returns structure describing it
 */
window_t *window_find(const char *target)
{
	int current = ((target) ? !xstrcasecmp(target, "__current") : 0);
	int debug = ((target) ? !xstrcasecmp(target, "__debug") : 0);
	int status = ((target) ? !xstrcasecmp(target, "__status") : 0);
	userlist_t *u = NULL;
	list_t l;

	if (!target || current) {
		if (window_current->id)
			return window_current;
		else
			status = 1;
	}

	if (xstrncmp(target, "__", 2)) {
		list_t sl;
		for (sl = sessions; sl; sl = sl->next) {
			session_t *s = sl->data;
			u = userlist_find(s, get_uid(s, target));
		}
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (!w->id && debug)
			return w;

		if (w->id == 1 && status)
			return w;

		if (w->target && target) {
			if (!xstrcasecmp(target, w->target))
				return w;

			if (u && u->nickname && !xstrcasecmp(u->nickname, w->target))
				return w;

			if (u && !xstrcasecmp(u->uid, w->target))
				return w;
		}
	}

	return NULL;
}

/*
 * window_find_s()
 *
 * it seeks for an window with given target and session
 * returns structure describing it
 */
window_t *window_find_s(session_t *session, const char *target)
{
	int current = ((target) ? !xstrcasecmp(target, "__current") : 0);
	int debug = ((target) ? !xstrcasecmp(target, "__debug") : 0);
	int status = ((target) ? !xstrcasecmp(target, "__status") : 0);
	userlist_t *u = NULL;
	list_t l;

	if (!target || current) {
		if (window_current->id)
			return window_current;
		else
			status = 1;
	}

	if (session && target) {
		u = userlist_find(session, get_uid(session, target));
	} else {
		if (target && strncmp(target, "__", 2)) {
			list_t sl;
			for (sl = sessions; sl; sl = sl->next) {
				session_t *s = sl->data;
				u = userlist_find(s, get_uid(s, target));
				break;
			}
		}
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (!w->id && debug)
			return w;

		if (w->id == 1 && status)
			return w;

		if (w->target && target && !session_compare(w->session, session)) {
			if (!xstrcasecmp(target, w->target)) {
				return w;
			}

			if (u && u->nickname && !xstrcasecmp(u->nickname, w->target)) {
				return w;
}

			if (u && !xstrcasecmp(u->uid, w->target)) {
				return w;
			}
		}
	}

	return NULL;
}

/*
 * window_switch()
 *
 * prze³±cza do podanego okna.
 *  
 *  - id - numer okna
 */
void window_switch(int id)
{
	list_t l;
	userlist_t *u;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (id != w->id || w->floating)
			continue;

		if (id != window_current->id)
			window_last_id = window_current->id;
		
		if (w->id != 0 && w->session)
			session_current = w->session;
	
		window_current = w;

		w->act = 0;
		if (w->target && w->session && (u=userlist_find(w->session, w->target)) && (u->xstate & EKG_XSTATE_BLINK)) 
			u->xstate &= ~EKG_XSTATE_BLINK;

		if (!config_make_window && w->id == 1) {
			list_t l;
	                session_t *s = session_current;

			for (l = s->userlist; l; l = l->next) {
                        	userlist_t *u = l->data;
				if (!window_find_s(s, u->uid))
		                        u->xstate &= ~EKG_XSTATE_BLINK;
			}
                }

		query_emit(NULL, ("ui-window-switch"), &w);	/* XXX */

		if (!w->id)
			w->session = session_current;

		break;
	}
}

/*
 * window_new_compare()  // funkcja wewnêtrzna
 *
 * do sortowania okienek.
 */
static int window_new_compare(void *data1, void *data2)
{
	window_t *a = data1, *b = data2;

	if (!a || !b)
		return 0;

	return a->id - b->id;
}

/*
 * window_new()
 *
 * tworzy nowe okno o podanej nazwie i id.
 *
 *  - target - nazwa okienka
 *  - session - do jakiej sesji ma nale¿eæ
 *  - new_id - je¶li ró¿ne od zera, okno przyjmie taki numer
 *
 * zwraca strukturê opisuj±c± nowe okno.
 */
window_t *window_new(const char *target, session_t *session, int new_id)
{
	window_t *w;
	int id = 1, done = 0;
	list_t l;
/*	userlist_t *u = NULL; */

	if (target) {
		window_t *w = window_find_s(session, target);

		if (w)
			return w;

/*		u = get_uid(target); */

		if (!xstrcmp(target, "$"))
			return window_current;
	}

	while (!done) {
		done = 1;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (w->id == id) {
				done = 0;
				id++;
				break;
			}
		}
	}

	if (new_id != 0)
		id = new_id;

	/* okno z debug'iem */
	if (id == -1)
		id = 0;
	

	w = xmalloc(sizeof(window_t));

	w->id = id;

	/* domy¶lne rozmiary zostan± dostosowane przez ui */
	w->top = 0;
	w->left = 0;
	w->width = 1;
	w->height = 1;
	w->target = xstrdup(target);
	w->session = session;
	w->userlist = NULL;

	list_add_sorted(&windows, w, 0, window_new_compare);

	query_emit(NULL, ("ui-window-new"), &w);	/* XXX */

	return w;
}

/*
 * window_print()
 *
 * wy¶wietla w podanym okienku, co trzeba.
 * 
 *  - target - cel wy¶wietlanego tekstu.
 *  - separate - czy jest na tyle wa¿ne, ¿eby otwieraæ nowe okno?
 *  - line - sformatowana linia.
 */
void window_print(const char *target, session_t *session, int separate, fstring_t *line)
{
	window_t *w;
	list_t l;
	const char *who;
	userlist_t *uid = userlist_find(session, target);

	if (uid)
		who = uid->nickname;
	else 
		who = target;

	switch (config_make_window) {
		case 1:
			if ((w = window_find_s(session, target)))
				goto crap;

			if (!separate)
				w = window_find("__status");

			for (l = windows; l; l = l->next) {
				window_t *w = l->data;

				if (separate && !w->target && w->id > 1) {
					xfree(w->target);
					w->target = xstrdup(target);
					query_emit(NULL, ("ui-window-target-changed"), &w);	/* XXX */
					print("window_id_query_started", itoa(w->id), who, session_name(session));
					print_window(target, session, 1, "query_started", who, session_name(session));
					print_window(target, session, 1, "query_started_window", who);
/*					if (!(ignored_check(get_uid(target)) & IGNORE_EVENTS)) 
						event_check(EVENT_QUERY, get_uin(target), target);
 */
					break;
				}
			}

		case 2:
			if (!(w = window_find_s(session, target))) {
				if (!separate)
					w = window_find("__status");
				else {
					w = window_new(target, session, 0);
					print("window_id_query_started", itoa(w->id), who, session_name(session));
					print_window(target, session, 1, "query_started", who, session_name(session));
					print_window(target, session, 1, "query_started_window", who);
/*					if (!(ignored_check(get_uid(target)) & IGNORE_EVENTS))
						event_check(EVENT_QUERY, get_uin(target), target);
 */
				}
			}

crap:
			if (!config_display_crap && target && !xstrcmp(target, "__current"))
				w = window_find("__status");
			
			break;
			
		default:
			/* je¶li nie ma okna, rzuæ do statusowego. */
			if (!(w = window_find_s(session, target)))
				w = window_find("__status");
	}

	/* albo zaczynamy, albo koñczymy i nie ma okienka ¿adnego */
	if (!w) 
		return;
 
	if (w != window_current && !w->floating) {
		int oldact = w->act;
		if (separate)
			w->act = 2;
		else if (w->act != 2)
			w->act = 1;
		if (oldact != w->act)
			query_emit(NULL, ("ui-window-act-changed"));
	}

	if (!line->ts)
		line->ts = time(NULL);
	query_emit(NULL, ("ui-window-print"), &w, &line);	/* XXX */
}

/*
 * window_next()
 *
 * przechodzi do kolejnego okna.
 */
void window_next()
{
	window_t *next = NULL;
	int passed = 0;
	list_t l;

	for (l = windows; l; l = l->next) {
		if (l->data == window_current)
			passed = 1;

		if (passed && l->next) {
			window_t *w = l->next->data;

			if (!w->floating) {
				next = w;
				break;
			}
		}
	}

	if (!next)
		next = window_find("__status");

	window_switch(next->id);
}

/*
 * window_prev()
 *
 * przechodzi do poprzedniego okna.
 */
void window_prev()
{
	window_t *prev = NULL;
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (w->floating)
			continue;

		if (w == window_current && l != windows)
			break;

		prev = l->data;
	}

	if (!prev->id) {
		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (!w->floating)
				prev = l->data;
		}
	}

	window_switch(prev->id);
}

/*
 * window_kill()
 *
 * usuwa podane okno.
 *
 *  - w - struktura opisuj±ca okno
 *  - quiet - czy ma byæ po cichu?
 */
void window_kill(window_t *w, int quiet)
{
	int id = w->id;
	window_t *tmp;

	if (quiet) 
		goto cleanup;

	if (id == 1 && w->target) {
		printq("query_finished", window_current->target, session_name(window_current->session));
		xfree(window_current->target);
		window_current->target = NULL;
		userlist_free_u(&(window_current->userlist));

		tmp = xmemdup(w, sizeof(window_t));
		query_emit(NULL, ("ui-window-target-changed"), &tmp);
		xfree(tmp);

		return;
	}
	
	if (id == 1) {
		wcs_printq("window_kill_status");
		return;
	}

	if (id == 0)
		return;
	
	if (w == window_current)
		window_prev();

	if (config_sort_windows) {
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (w->floating)
				continue;

			if (w->id > 1 && w->id > id)
				w->id--;
		}
	}

cleanup:
	tmp = xmemdup(w, sizeof(window_t));
	query_emit(NULL, ("ui-window-kill"), &tmp);
	xfree(tmp);

	xfree(w->target);
	userlist_free_u(&(w->userlist));
	list_remove(&windows, w, 1);
}

/* 
 * window_exist()
 *
 * check if window exist 
 */
window_t *window_exist(int id)
{
	list_t l;

        for (l = windows; l; l = l->next) {
	        window_t *w = l->data;

                if (w->id == id) 
			return w;
        }

	return NULL;
}

/*
 * window_move()
 * 
 * swap windows 
 */
void window_move(int first, int second)
{
	window_t *w1, *w2;

	if (!(w1 = window_exist(first)) || !(w2 = window_exist(second)))
		return;

        list_remove(&windows, w1, 0);
	w1->id = second;

        list_remove(&windows, w2, 0);
	w2->id = first;

	list_add_sorted(&windows, w1, sizeof(window_t), window_new_compare);
        list_add_sorted(&windows, w2, sizeof(window_t), window_new_compare);
}

/*
 * window_target()
 *
 * return window target
 */

char *window_target(window_t *window) {
	if (!window)			return "__current";
	if (window->id == 1)		return "__status";
	else if (window->id == 0)	return "__debug";
	else if (window->target)        return window->target;
        else                            return "";
}

/*
 *
 * komenda ekg obs³uguj±ca okna
 */
COMMAND(cmd_window)
{
	if (!xstrcmp(name, ("clear")) || (params[0] && !xstrcasecmp(params[0], ("clear")))) {
		window_t *w = xmemdup(window_current, sizeof(window_t));
		query_emit(NULL, ("ui-window-clear"), &w);
		xfree(w);
		return 0;
	}

	if (!params[0] || !xstrcasecmp(params[0], ("list"))) {
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (w->id) {
				if (w->target) {
					if (!w->floating)	
						printq("window_list_query", itoa(w->id), w->target);
					else
						printq("window_list_floating", itoa(w->id), itoa(w->left), itoa(w->top), itoa(w->width), itoa(w->height), w->target);
				} else
					wcs_printq("window_list_nothing", itoa(w->id));
			}
		}
		return 0;
	}

	if (!xstrcasecmp(params[0], ("active"))) {
		list_t l;
		int id = 0;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (w->act && !w->floating && w->id) {
				id = w->id;
				break;
			}
		}

		if (id)
			window_switch(id);
		return 0;
	}

	if (!xstrcasecmp(params[0], ("new"))) {
		window_t *w = window_new(params[1], session, 0);

		w->session = window_current->session;

		if (!w->session && sessions)
			w->session = (session_t*) sessions->data;

		if (!w->floating)
			window_switch(w->id);

		return 0;
	}

	if (atoi(params[0])) {
		window_switch(atoi(params[0]));
		return 0;
	}

	if (!xstrcasecmp(params[0], ("switch"))) {
		if (!params[1] || (!atoi(params[1]) && xstrcmp(params[1], ("0"))))
			wcs_printq("not_enough_params", name);
		else
			window_switch(atoi(params[1]));
		return 0;
	}			

	if (!xstrcasecmp(params[0], ("last"))) {
		window_switch(window_last_id);
		return 0;
	}
	
	if (!xstrcasecmp(params[0], ("kill"))) {
		window_t *w = window_current;

		if (params[1]) {
			list_t l;

			for (w = NULL, l = windows; l; l = l->next) {
				window_t *ww = l->data;

				if (ww->id == atoi(params[1])) {
					w = ww;
					break;
				}
			}

			if (!w) {
				wcs_printq("window_noexist");
				return -1;
			}
		}

		window_kill(w, 0);
		window_switch(window_current->id);
		return 0;
	}

	if (!xstrcasecmp(params[0], ("next"))) {
		window_next();
		return 0;
	}
	
	if (!xstrcasecmp(params[0], ("prev"))) {
		window_prev();
		return 0;
	}

        if (!xstrcasecmp(params[0], ("move"))) {
		int source, dest;

		if (!window_current)
			return -1;

		if (!params[1]) {
			wcs_printq("invalid_params", name);
			return -1;
		}

		source = (params[2]) ? atoi(params[2]) : window_current->id;

		if (!source) {
                        wcs_printq("window_invalid_move", itoa(source));
			return -1;
		}

		if (!window_exist(source)) {
			wcs_printq("window_doesnt_exist", itoa(source));
			return -1;
		}

		if (source == 1) {
			wcs_printq("window_cannot_move_status");
			return -1;
		}

		/* source is okey, now we are checking destination window */
		
		if (!xstrcasecmp(params[1], ("left")))
			dest = source - 1;
		else if (!xstrcasecmp(params[1], ("right")))
			dest = source + 1;
		else
			dest = atoi(params[1]);


		if (!dest) {
			wcs_printq("window_invalid_move", itoa(dest));
			return -1;
		}

                if (dest == 1) {
                        wcs_printq("window_cannot_move_status");
			return -1;
                }

                if (!window_exist(dest)) {
			window_new(NULL, NULL, dest);
                }

		if (dest == source)
			return 0;

		window_move(source, dest);
		window_switch(dest);
		return 0;
        }

	
	if (!xstrcasecmp(params[0], ("refresh"))) {
		query_emit(NULL, ("ui-window-refresh"));
		return 0;
	}


	wcs_printq("invalid_params", name);

	return 0;
}

int window_session_cycle(window_t *w)
{
	list_t l;

	if (!w || !sessions)
		return -1;

	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;

		if (w->session != s)
			continue;

		if (l->next) {
			w->session = (session_t*) l->next->data;
			if (w == window_current)
				session_current = window_current->session;
        		query_emit(NULL, ("session-changed"));
			return 0;
		} else
			break;
	}

	w->session = (session_t*) sessions->data;	
        if (w == window_current)
                 session_current = window_current->session;

        query_emit(NULL, ("session-changed"));

	return 0;
}

int window_session_set(window_t *w, session_t *s)
{
	if (!w)
		return -1;

	w->session = s;
	if (window_current == w)
		session_current = s;

	return 0;
}

session_t *window_session_get(window_t *w)
{
	return (w) ? w->session : NULL;
}

int window_lock_set(window_t *w, int lock)
{
	if (!w)
		return -1;

	w->lock = lock;

	return 0;
}

int window_lock_inc(window_t *w)
{
	if (!w)
		return -1;

	w->lock++;

	return 0;
}

int window_lock_dec(window_t *w)
{
	if (!w || w->lock < 1)
		return -1;

	w->lock--;

	return 0;
}

int window_lock_get(window_t *w)
{
	return (w) ? w->lock : 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
