/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Wojtek Bojdo³ <wojboj@htcon.pl>
 *			    Pawe³ Maziarz <drg@infomex.pl>
 *			    Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
 *		  2008-2010 Wies³aw Ochmiñski <wiechu@wiechu.com>
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

#include <ekg/debug.h>
#include <ekg/windows.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>

#include <ekg/queries.h>

#include "notify.h"
#include "old.h"

/* typing */
int ncurses_typing_mod			= 0;	/* whether buffer was modified */
window_t *ncurses_typing_win		= NULL;	/* last window for which typing was sent */
static int ncurses_typing_count		= 0;	/* last count sent */
static time_t ncurses_typing_time	= 0;	/* time at which last typing was sent */

static int ncurses_lineslen() {
	if (ncurses_lines) {
		int n = -1;
		CHAR_T **l;

		if (ncurses_lines[0][0] == '/')
			return 0;

		for (l = ncurses_lines; *l; l++)
			n += xwcslen(*l) + 1;

		return n;
	} else
		return (ncurses_line[0] == '/' ? 0 : xwcslen(ncurses_line));
}

static inline int ncurses_typingsend(const int len, const int first) {
	const char *sid	= session_uid_get(ncurses_typing_win->session);
	const char *uid	= get_uid(ncurses_typing_win->session, ncurses_typing_win->target);
	
debug_ok("%s len:%d first:%d in_a:%d [%d]\n", uid, len, first, ncurses_typing_win->in_active,(((first > 1) || (ncurses_typing_win->in_active)) && uid));
	if (((first > 1) || (ncurses_typing_win->in_active)) && uid)
		return query_emit_id(NULL, PROTOCOL_TYPING_OUT, &sid, &uid, &len, &first);
	else
		return -1;
}

TIMER(ncurses_typing) {
	window_t *oldwin	= NULL;

	if (type)
		return 0;

	if (ncurses_typing_mod > 0) { /* need to update status */
		const int curlen		= ncurses_lineslen();
		const int winchange		= (ncurses_typing_win != window_current);

		if (winchange && ncurses_typing_win && ncurses_typing_win->target)
			ncurses_typing_time	= 0; /* this should force check below */
		else
			ncurses_typing_time	= time(NULL);

		if (window_current && window_current->target && curlen &&
				(winchange || ncurses_typing_count != curlen)) {

#if 0
			debug_function("ncurses_typing(), [UNIMPL] got change for %s [%s] vs %s [%s], %d vs %d!\n",
					window_current->target, session_uid_get(window_current->session),
					ncurses_typing_win ? ncurses_typing_win->target : NULL,
					ncurses_typing_win ? session_uid_get(ncurses_typing_win->session) : NULL,
					curlen, ncurses_typing_count);
#endif
			if (winchange)
				oldwin		= ncurses_typing_win;

			ncurses_typing_win	= window_current;
			ncurses_typing_count	= curlen;
			ncurses_typingsend(curlen, winchange);
		}

		ncurses_typing_mod		= 0;
	}

	{
		const int isempty = (ncurses_lines
				? (!ncurses_lines[0][0] && !ncurses_lines[1]) || ncurses_lines[0][0] == '/'
				: !ncurses_line[0] || ncurses_line[0] == '/');
		const int timeout = (config_typing_timeout_empty && isempty ?
					config_typing_timeout_empty : config_typing_timeout);

		if (ncurses_typing_win && (!ncurses_typing_time || (timeout && time(NULL) - ncurses_typing_time > timeout))) {
			window_t *tmpwin = NULL;

			if (oldwin) {
				tmpwin			= ncurses_typing_win;
				ncurses_typing_win	= oldwin;
			}
			ncurses_typingsend(0, (ncurses_typing_mod == -1 ? 3 : 1));
#if 0
			debug_function("ncurses_typing(), [UNIMPL] disabling for %s [%s]\n",
					ncurses_typing_win->target, session_uid_get(ncurses_typing_win->session));
#endif
			if (oldwin)
				ncurses_typing_win	= tmpwin;
			else
				ncurses_typing_win	= NULL;
		}
	}

	return 0;
}

void ncurses_window_gone(window_t *w) {
	if (w == ncurses_typing_win) { /* don't allow timer to touch removed window */
		const int tmp		= ncurses_typing_mod;

		ncurses_typing_time	= 0;
		ncurses_typing_mod	= -1; /* prevent ncurses_typing_time modification & main loop behavior */

		ncurses_typing(0, NULL);

		ncurses_typing_mod	= tmp;
	} else if (w->in_active || w->out_active) { /* <gone/> or <active/> */
		window_t *tmp		= ncurses_typing_win;
		ncurses_typing_win	= w;

		if (!ncurses_typingsend(0, !w->out_active ? 4 : 5) || w->out_active)
			w->out_active	^= 1;

		ncurses_typing_win	= tmp;
	}
}
