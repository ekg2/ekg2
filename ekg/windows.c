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

#ifdef HAVE_REGEX_H
#	include <regex.h>
#endif

#include "commands.h"
#include "dynstuff.h"
#include "windows.h"
#include "userlist.h"
#include "sessions.h"
#include "themes.h"
#include "stuff.h"
#include "xmalloc.h"

#include "queries.h"

int window_last_id = -1;		/* ostatnio wy¶wietlone okno */

window_t *windows = NULL;		/* lista okien */
int config_display_crap = 1;		/* czy wy¶wietlaæ ¶mieci? */

window_t *window_current = NULL;	/* okno aktualne, zawsze na co¶ musi wskazywaæ! */
window_t *window_status  = NULL;	/* okno statusowe, zawsze musi miec dobry adres w pamieci [NULL jest ok] */
window_t *window_debug   = NULL;	/* okno debugowe, zawsze musi miec dobry adres w pamieci [NULL jest ok] */

window_lastlog_t *lastlog_current = NULL;

/**
 * window_find_ptr()
 *
 * it's search over window list and checks if param @a w is still on that list.
 *
 * @note It's possible to find another window with the same address as old one.. it's rather not possible.. however,
 *	It's better if you use other functions...
 *
 * @sa window_find()	- If you want to search for window target, instead of window ptr.
 * @sa window_find_s()	- If you want to search for window session && target, instead of window ptr.
 * @sa window_exist	- If you want to search for window id, instead of window ptr.
 *
 * @param w - window to look for.
 *
 * @return It returns @a w if window was found, otherwise NULL.
 */

window_t *window_find_ptr(window_t *w) {
	window_t *v;

	for (v = windows; v; v = v->next) {
		if (w == v)
			return w;
	}
	return NULL;
}

/**
 * window_find_sa()
 *
 * Search for an window with given @a target and @a session<br>
 *
 * @param session				- window session to search [See also @@ @a session_null_means_no_session]
 * @param target				- window target to search
 * @param session_null_means_no_session		- if you know that this window must belong to given session [NULL, or whatever] so this should be 1.
 * 						  else NULL @@ @a session will mean that you don't know which session should it be. And it'll search/check
 * 						  <b>all</b> sessions
 *
 * @sa window_find_ptr()        - If you want to search for given window ptr.
 * @sa window_find_s()          - macro to <code>window_find_sa(session, target, 1)</code>
 * @sa window_find()            - wrapper to <code>window_find_sa(NULL, target, 0)</code>
 *
 * @return pointer to window_t struct if window was founded, else NULL
 */

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

	if (xstrncmp(target, "__", 2)) {
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
					 * 	However userlist_find() also strip resources if preset.. here we don't have it. 
					 * 	maybe it's better, maybe not. Must think about it.
					 * 	For now leave this code.
					 */
					if (!xstrcasecmp(u->uid, w->target))
						return w;
				}
			}
		}
	}
	return NULL;
}

/**
 * window_find()
 *
 * Seeks for an window with given @a target<br>
 * Wrapper to: <code>window_find_sa(NULL, target, 0);</code>
 *
 * @note It's really slow, and you should avoid using it. You'll never know if you found good window... so use window_find_s()
 *
 * @param target - window target
 *
 * @sa window_find_s()		- If you know session.
 *
 * @return pointer to window_t struct if window was founded, else NULL
 *
 */

window_t *window_find(const char *target) {
	return window_find_sa(NULL, target, 0);
}

/*
 * window_switch()
 *
 * prze³±cza do podanego okna.
 *  
 *  - id - numer okna
 */
void window_switch(int id) {
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
			list_t l;
	                session_t *s = session_current;

			for (l = s->userlist; l; l = l->next) {
                        	userlist_t *u = l->data;

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

/**
 * window_new_compare()
 *
 * internal function to sort windows by id
 * used by LIST_ADD_SORTED()
 *
 * @param data1 - first window_t to compare
 * @param data2 - second window_t to compare
 *
 * @return It returns result of window id subtractions.
 */
static LIST_ADD_COMPARE(window_new_compare, window_t *) { return data1->id - data2->id; }

/**
 * window_new()
 *
 * Create new window_t, with given @a new_id (if @a new_id != 0)
 *
 * @note 	If target == "$" than it return current window. [POSSIBLE BUG]
 * 		If window with such target [it can also be u->uid/u->nickname combination] exists.
 * 		than it'll return it.
 * 
 * @note 	You shouldn't pass @a new_id here. Because it can broke UI stuff. don't ask. it's wrong. Just don't use it.
 * 		It'll be possible removed... Really eventually you can talk with devs, and ask for id from class: 1000 to 1999
 *
 * @todo	See XXX's
 *
 * @param target 	- name of window
 * @param session 	- session of this window
 * @param new_id	- if different than 0, than window will take this id.
 *
 * @return window_t struct
 */

window_t *window_new(const char *target, session_t *session, int new_id) {
	window_t *w;

	if (target) {
		window_t *w;
		
		/* XXX, we don't check new_id here. stupido */
		if (!xstrcmp(target, "$")) {
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

			if (w->id < 2)					/* [RESERVED CLASS: 0-1] 	0 for __debug, 1 for __status */
				continue;

			/* if current window is larger than current id... than we found good id! */
			if (w->id > id)
				break;

			if (w->id >= 1000-1 && w->id < 2000 /* -1 */) {	/* [REVERVED CLASS: 1000-1999] 	1k-1.999k windows reverved for special use. [1000 - __contacts, 1001 - __lastlog] */
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

	LIST_ADD_SORTED2(&windows, w, window_new_compare);
	query_emit_id(NULL, UI_WINDOW_NEW, &w);	/* XXX */

	return w;
}

/**
 * window_print()
 *
 * Print fstring_t @a line to window
 *
 * @todo If UI_WINDOW_PRINT is not handled by ui-plugin, we should free @a line, or we'll have memleaks.
 *
 * @param w - window
 * @param line - line
 *
 */

void window_print(window_t *w, fstring_t *line) {
	if (!w || !line) {
		fstring_free(line);
		return;
	}

	if (!line->ts)
		line->ts = time(NULL);
	query_emit_id(NULL, UI_WINDOW_PRINT, &w, &line);	/* XXX */
}

/*
 * window_next()
 *
 * przechodzi do kolejnego okna.
 */
void window_next() {
	window_t *next = NULL, *w;
	int passed = 0;

	for (w = windows; w; w = w->next) {
		if (passed && !w->floating) {
			next = w;
			break;
		}

		if (w == window_current)
			passed = 1;
	}

	if (!next)
		next = window_status;

	window_switch(next->id);
}

/*
 * window_prev()
 *
 * przechodzi do poprzedniego okna.
 */

/* XXX, need check */
void window_prev() {
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

/**
 * window_kill()
 *
 * Remove given window.<br>
 * If it's __status window, and w->target than display nice message about closing talk, else display message about no possibility to close status window<br>
 *
 * @note 	You cannot remove here __status and __debug windows.<br>
 * 		You must do it by hand like in ekg_exit() but if you want do it.<br>
 * 		Set @a window_debug and window_status for proper values.<br>
 * 		ekg2 core need them.
 *
 * @bug		Possible bug with sort_windows. Can anyone who wrote it look at it?
 *
 * @param w - given window.
 */

void window_kill(window_t *w) {
	int id;

	if (!w)
		return;
	
	id = w->id;

	if (id == 1 && w->target) {
		print("query_finished", w->target, session_name(w->session));
		xfree(window_current->target);
		w->target	= NULL;
/*		w->session	= NULL; */
		userlist_free_u(&(w->userlist));	/* wtf? */

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

	xfree(w->target);
	userlist_free_u(&(w->userlist));
	LIST_REMOVE2(&windows, w, NULL);	/* XXX: LIST_ITEM_FREE ? */
}

/**
 * window_exist()
 *
 * check if window with @a id exist 
 *
 * @param id - id of window.
 *
 * @sa window_find()		- If you want to search for window target, instead of window id.
 * @sa window_find_s()		- If you want to search for window session && target, instead of window id.
 * @sa window_find_ptr()	- if you want to search for window pointer, instead of window id.
 *
 * @return window_t *, with id specified by @a id, or NULL if such window doesn't exists.
 */

window_t *window_exist(int id) {
	window_t *w;

        for (w = windows; w; w = w->next) {
                if (w->id == id) 
			return w;
        }

	return NULL;
}

/**
 * window_move()
 * 
 * swap windows (swap windows @a id -> change sequence of them in UI)
 *
 * @param first		- 1st window id.
 * @param second 	- 2nd window id.
 *
 * @todo XXX: Rename to _swap, and make some real move.
 */

static void window_move(int first, int second) {
	window_t *w1, *w2;

	if (!(w1 = window_exist(first)) || !(w2 = window_exist(second)))
		return;

        LIST_UNLINK2(&windows, w1);
	w1->id = second;

        LIST_UNLINK2(&windows, w2);
	w2->id = first;

	LIST_ADD_SORTED2(&windows, w1, window_new_compare);
	LIST_ADD_SORTED2(&windows, w2, window_new_compare);
}

/* really move window, i.e. insert it at given id and move right all other windows */
static void window_real_move(int source, int dest) {
	window_t *ws, *wd;

	if (!(ws = window_exist(source)))
		return;

	if ((wd = window_exist(dest))) { /* need to move ids */
		window_t *wl;

		if (dest < source) {
			for (wl = windows; wl; wl = wl->next) {
				window_t *w = wl;

				if (w->id >= dest && w->id < source)
					(w->id)++;	/* XXX: move only when ids overlap? */
			}
		} else {
			for (wl = windows; wl; wl = wl->next) {
				window_t *w = wl;

				if (w->id <= dest && w->id > source)
					(w->id)--;	/* XXX: move only when ids overlap? */
			}
		}
	}
	ws->id = dest;

	LIST_UNLINK2(&windows, ws);
	LIST_ADD_SORTED2(&windows, ws, window_new_compare);
}

/**
 * window_target()
 *
 * @param window - window
 * @todo Make it const?
 *
 * @return 	Never NULL pointer [to don't care about it] look below for more details:
 * 		if @a window->target is not NULL return it<br>
 * 		else: <br>
 * 		- __current	if @a window is NULL<br>
 * 		- __status	if @a window->id == 1<br>
 * 		- __debug	if @a window->id == 0<br>
 * 		else return ""
 */

char *window_target(window_t *window) {
	if (!window)			return "__current";

	if (window->target)       	return window->target;
	else if (window->id == 1)	return "__status";
	else if (window->id == 0)	return "__debug";
        else                            return "";
}

/*
 *
 * komenda ekg obs³uguj±ca okna
 */
COMMAND(cmd_window) {
	if (!xstrcmp(name, ("clear")) || (params[0] && !xstrcasecmp(params[0], ("clear")))) {
		window_t *w = xmemdup(window_current, sizeof(window_t));
		query_emit_id(NULL, UI_WINDOW_CLEAR, &w);
		xfree(w);
		return 0;
	}

	if (!params[0] || !xstrcasecmp(params[0], ("list"))) {
		window_t *w;

		for (w = windows; w; w = w->next) {
			if (w->id) {
				if (w->target) {
					if (!w->floating)
						printq("window_list_query", itoa(w->id), w->target);
					else
						printq("window_list_floating", itoa(w->id), itoa(w->left), itoa(w->top), itoa(w->width), itoa(w->height), w->target);
				} else
					printq("window_list_nothing", itoa(w->id));
			}
		}
		return 0;
	}

	if (!xstrcasecmp(params[0], ("active"))) {
		window_t *w;
		int a,id = 0;

		for (a=2; !id && a>0; a--)
			for (w = windows; w; w = w->next) {
				if ((w->act==a) && !w->floating && w->id) {
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
			w->session = sessions;

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
			printq("not_enough_params", name);
		else
			window_switch(atoi(params[1]));
		return 0;
	}			

	if (!xstrcasecmp(params[0], ("last"))) {
		window_switch(window_last_id);
		return 0;
	}

	if (!xstrcmp(params[0], "lastlog")) {
		static window_lastlog_t lastlog_current_static;

		window_lastlog_t *lastlog;

		const char *str;
		window_t *w = NULL;

		int iscase	= -1;	/* default-default variable */
		int isregex	= 0;	/* constant, make variable? */
		int islock	= 0;	/* constant, make variable? */

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[2]) {
			char **arr = array_make(params[1], " ", 0, 1, 1);
			int i;

	/* parse configuration */
			for (i = 0; arr[i]; i++) {
				if (match_arg(arr[i], 'r', "regex", 2)) 
					isregex = 1;
				else if (match_arg(arr[i], 'R', "extended-regex", 2))
					isregex = 2;
				else if (match_arg(arr[i], 's', "substring", 2))
					isregex = 0;

				else if (match_arg(arr[i], 'C', "CaseSensitive", 2))
					iscase = 1;
				else if (match_arg(arr[i], 'c', "caseinsensitive", 2))
					iscase = 0;

				else if (match_arg(arr[i], 'w', "window", 2) && arr[i+1]) {
					w = window_exist(atoi(arr[++i]));
					
					if (!w) {
						printq("window_doesnt_exist", arr[i]);
						array_free(arr);
						return -1;
					}
				} else {
					printq("invalid_params", name);
					array_free(arr);
					return -1;
				}
			}
			array_free(arr);
			str = params[2];

		} else	str = params[1];

		lastlog = w ? window_current->lastlog : &lastlog_current_static;

		if (!lastlog) 
			lastlog = xmalloc(sizeof(window_lastlog_t));

		if (w || lastlog_current) {
#ifdef HAVE_REGEX_H
			if (lastlog->isregex)
				regfree(&lastlog->reg);
			xfree(lastlog->expression);
#endif

		}

/* compile regexp if needed */
		if (isregex) {
#ifdef HAVE_REGEX_H
			int rs, flags = REG_NOSUB;
			char errbuf[512];

			if (isregex == 2)
				flags |= REG_EXTENDED;

/* XXX, when config_lastlog_case is toggled.. we need to recompile regex's */
			if (!lastlog->casense || (lastlog->casense == -1 && !config_lastlog_case))
				flags |= REG_ICASE;

			if ((rs = regcomp(&lastlog->reg, str, flags))) {
				regerror(rs, &lastlog->reg, errbuf, sizeof(errbuf));
				printq("regex_error", errbuf);
				/* XXX, it was copied from ekg1, although i don't see much sense to free if regcomp() failed.. */
				regfree(&(lastlog->reg));
				return -1;
			}
#else
			printq("generic_error", "you don't have regex.h !!!!!!!!!!!!!!!!!!!11111");
/*			isrgex = 0; */
			return -1;
#endif
		}

		lastlog->w 		= w;
		lastlog->casense 	= iscase;
		lastlog->lock		= islock;
		lastlog->isregex	= isregex;
		lastlog->expression	= xstrdup(str);

		if (w)  window_current->lastlog	= lastlog;
		else	lastlog_current		= lastlog;
			
		return query_emit_id(NULL, UI_WINDOW_UPDATE_LASTLOG);
	}
	
	if (!xstrcasecmp(params[0], ("kill"))) {
		window_t *w = window_current;

		if (params[1]) {
			window_t *ww;

			for (w = NULL, ww = windows; ww; ww = ww->next) {
				if (ww->id == atoi(params[1])) {
					w = ww;
					break;
				}
			}

			if (!w) {
				printq("window_noexist");
				return -1;
			}
		}

		window_kill(w);
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

        if (!xstrcasecmp(params[0], ("move")) || !xstrcasecmp(params[0], "swap")) {
		int source, dest;

		if (!window_current)
			return -1;

		if (!params[1]) {
			printq("invalid_params", name);
			return -1;
		}

		source = (params[2]) ? atoi(params[2]) : window_current->id;

		if (!source) {
                        printq("window_invalid_move", itoa(source));
			return -1;
		}

		if (!window_exist(source)) {
			printq("window_doesnt_exist", itoa(source));
			return -1;
		}

		if (source == 1) {
			printq("window_cannot_move_status");
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
			printq("window_invalid_move", itoa(dest));
			return -1;
		}

                if (dest == 1) {
                        printq("window_cannot_move_status");
			return -1;
                }

		if (dest == source)
			return 0;

		if (!xstrcasecmp(params[0], "swap")) {
	                if (!window_exist(dest)) 
				window_new(NULL, NULL, dest);

			window_move(source, dest);
		} else
			window_real_move(source, dest);
		window_switch(dest);
		return 0;
        }

	
	if (!xstrcasecmp(params[0], ("refresh"))) {
		query_emit_id(NULL, UI_REFRESH);
		return 0;
	}


	printq("invalid_params", name);

	return 0;
}

/**
 * window_session_cycle()
 *
 * Change session of given window to next good one (based on @a config_window_session_allow value) 
 *
 * @note	behaviour of window_session_cycle() based on values of config_window_session_allow:
 * 		 0 - change session only if w->target == NULL
 * 		 1 - like 0 + if w->target is set than new session must accept that uid	[default && other values]
 * 		 2 - change to any next session
 * 		 4 - jump to status window before cycling.
 *
 * @note	If w->session was changed than UI_WINDOW_TARGET_CHANGED will be emited.
 * 		If w == window_current than SESSION_CHANGED will be emited also.
 *
 * @todo	Gdy config_window_session_allow == 2, to najpierw sprobowac znalezc dobra sesje a potem jesli nie to 
 * 		nastepna?
 * 
 * @todo	Create window_session_set() for some stuff here.
 *
 * @param	w - window
 *
 * @return	 0 - if session of window was changed
 * 		-1 - if not
 */

int window_session_cycle(window_t *w) {
	session_t *s;
	session_t *new_session = NULL;
	int once = 0;
	char *uid;
	char *nickname;

	if (!w || !sessions) {
		return -1;
	}

	/* @ab config_window_session_allow == 4: don't change session when we have open talk in __status window */
	/* 	XXX, change to __status? */
	if ((config_window_session_allow == 0 && w->target) || (config_window_session_allow == 4 && window_status->target)) {
		print("session_cannot_change");
		return -1;
	}

	if (config_window_session_allow == 4) { /* higher level of magic */
			/* switch to status window */
		command_exec(NULL, NULL, "/window switch 1", 1);	/* XXX, window_switch(window_status->id); */

			/* and change switching order
			 * we don't need to emit anything, because this function will (should?) change it again */
		window_status->session	= w->session;
		w			= window_status;
	}


	/* find sessions->(...next..) == w->session */
	for (s = sessions; s; s = s->next) {
		if (w->session == s) {
			s = s->next;
			break;
		}
	}

	if (!(uid = get_uid(w->session, w->target)))	/* try to get old uid, because in w->target we can have nickname, and it could be other person */
		uid = w->target;

again:
	if (s) {
		session_t *k;

		for (k = s; k; k = k->next) {
			if (k == w->session)
				break;

			once = 1;

			if (config_window_session_allow == 2 || !w->target || (config_window_session_allow != 0 && get_uid(k, uid))) {
				new_session = k;
				break;
			}
		}
	} 
		
	if (!new_session && s != sessions) {
		s = sessions;
		goto again;
	}

	if (!new_session) {	/* not found */
		if (once) {		/* here config_window_session_allow don't allow to change session */
			print("session_cannot_change");
		}
		return -1;
	}

	w->session = new_session;

	if ((nickname = get_nickname(new_session, uid))) {		/* if we've got nickname for old uid, than use it as w->target */
		char *tmp = w->target;
		w->target = xstrdup(nickname);
		xfree(tmp);
	} else if (w->target != uid) {					/* if not, than change w->target (possibility nickname) with uid value [XXX, untested behavior] */
		char *tmp = w->target;
		w->target = xstrdup(uid);
		xfree(tmp);
	}

	if (w == window_current) {
		session_current = new_session;
        	query_emit_id(NULL, SESSION_CHANGED);
	}
	query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &w);

	{	/* here sync window_status->session with window_debug->session */
		if (w == window_status) {
			if (window_debug->session != new_session) {
				window_debug->session = new_session;
				if (window_current == window_debug) {
					session_current = new_session;
					query_emit_id(NULL, SESSION_CHANGED);
				}
				query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &window_debug);
			}
		}

		if (w == window_debug) {
			if (window_status->session != new_session) {
				window_status->session = new_session;
				if (window_current == window_status) {
					session_current = new_session;
					query_emit_id(NULL, SESSION_CHANGED);
				}
				query_emit_id(NULL, UI_WINDOW_TARGET_CHANGED, &window_status);
			}
		}
	}

	return 0;
}

int window_lock_inc(window_t *w) {
	if (!w)
		return -1;

	w->lock++;

	return 0;
}

int window_lock_dec(window_t *w) {
	if (!w || w->lock < 1)
		return -1;

	w->lock--;

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
