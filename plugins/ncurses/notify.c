/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Wojtek Bojdo³ <wojboj@htcon.pl>
 *			    Pawe³ Maziarz <drg@infomex.pl>
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

#include <ekg/windows.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>

#include <ekg/queries.h>

#include "input.h"
#include "notify.h"
#include "nc-stuff.h"

/* typing */

/* vars */
int config_typing_interval	= 1;
int config_typing_timeout	= 10;
int config_typing_timeout_inactive = 120;

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

int ncurses_typingsend(window_t *w, int chatstate) {
	const char *sid, *uid;
	userlist_t *u;
	session_t *s;

	if (!w || (w->id<2))
		return -1;

	if (w->last_chatstate == chatstate)
		return -1;

	w->last_chatstate = chatstate;

	if (!(s=w->session) || !s->connected)
		return -1;

	if (!(uid = get_uid(s, w->target)))
		return -1;

	u = userlist_find(s, uid);
	if (!u || (u->status <= EKG_STATUS_NA))
		return -1;

	sid = session_uid_get(s);

	return query_emit(NULL, "protocol-typing-out", &sid, &uid, &chatstate);
}

TIMER(ncurses_typing) {

	if (type)
		return 0;

	const int curlen	= ncurses_lineslen();
	const int winchange	= (ncurses_typing_win != window_current);

	if (winchange && ncurses_typing_win && ncurses_typing_win->target) {
		ncurses_typingsend(ncurses_typing_win, EKG_CHATSTATE_INACTIVE);	/* previous window */
		ncurses_typing_time	= 0;
		ncurses_typing_mod	= 0;
		ncurses_typing_count	= curlen;
		ncurses_typing_win	= window_current;
		return 0;
	}

	if ((ncurses_typing_mod > 0) && window_current && window_current->target) { /* need to update status */
		ncurses_typing_win	= window_current;
		if (!curlen)
			ncurses_typingsend(ncurses_typing_win, EKG_CHATSTATE_ACTIVE);
		else if (ncurses_typing_count != curlen)
			ncurses_typingsend(ncurses_typing_win, EKG_CHATSTATE_COMPOSING);

		ncurses_typing_time	= time(NULL);
		ncurses_typing_count	= curlen;
		ncurses_typing_mod	= 0;

	} else if (ncurses_typing_win) {
		int chat = 0;
		if (ncurses_typing_time) {
			int timeout = time(NULL) - ncurses_typing_time;
			if (curlen && config_typing_timeout && (timeout>config_typing_timeout))
				chat = EKG_CHATSTATE_PAUSED;
			else if (config_typing_timeout_inactive && (timeout>config_typing_timeout_inactive))
				chat = EKG_CHATSTATE_INACTIVE;
		}
		if (chat)
			ncurses_typingsend(ncurses_typing_win, chat);
	}

	return 0;
}

