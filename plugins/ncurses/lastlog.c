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

#include "ekg2.h"

#include "backlog.h"
#include "mouse.h"
#include "nc-stuff.h"

static int ncurses_ui_window_lastlog(window_t *lastlog_w, window_t *w) {
	const char *header;

	ncurses_window_t *n;
	window_lastlog_t *lastlog;

	int local_config_lastlog_case;

	int items = 0;
	int i;

	static int lock = 0;

	if (lock) {
		lastlog = w->lastlog;
		w	= lastlog->w;

	} else {
		lastlog = NULL;

		if (w == window_current || config_lastlog_display_all == 2)
			lastlog = lastlog_current;

		if (w->lastlog) {
			lock = 1;
			items += ncurses_ui_window_lastlog(lastlog_w, w);
			lock = 0;
		}
	}

	if (!lastlog)
		return items;

	if (lastlog == lastlog_current)	header = format_find("lastlog_title_cur");
	else				header = format_find("lastlog_title");


	if (!w || !(n = w->priv_data))
		return items;

	if (config_lastlog_noitems) { /* always add header */
		fstring_t *fstr = fstring_new_format(header, window_target(w), lastlog->expression);
		ncurses_backlog_add(lastlog_w, fstr);
		fstring_free(fstr);
	}

	local_config_lastlog_case = (lastlog->casense == -1) ? config_lastlog_case : lastlog->casense;

	for (i = n->backlog_size-1; i >= 0; i--) {
		gboolean found = FALSE;

		if (lastlog->isregex) {		/* regexp */
			found = g_regex_match(lastlog->reg, n->backlog[i]->str, 0, NULL);
		} else {				/* substring */
			if (local_config_lastlog_case)
				found = !!xstrstr(n->backlog[i]->str, lastlog->expression);
			else	
				found = !!xstrcasestr(n->backlog[i]->str, lastlog->expression);
		}

		if (!config_lastlog_noitems && found && !items) { /* add header only when found */
			fstring_t *fstr = fstring_new_format(header, window_target(w), lastlog->expression);
			ncurses_backlog_add(lastlog_w, fstr);
			fstring_free(fstr);
		}

		if (found) {
			ncurses_backlog_add_real(lastlog_w, fstring_dup(n->backlog[i]));
			items++;
		}
	}
	return items;
}

int ncurses_lastlog_update(window_t *w) {
	ncurses_window_t *n;
	window_t *ww;
	int retval = 0;

	int old_start;

	if (config_lastlog_lock) return 0;

	if (!w) w = window_exist(WINDOW_LASTLOG_ID);
	if (!w) return -1;

	n = w->priv_data;
#ifdef SCROLLING_FIXME
	old_start = n->start;
#endif

	ncurses_clear(w, 1);

/* XXX, it's bad orded now, need fix */

/* 1st, lookat current window.. */
	retval += ncurses_ui_window_lastlog(w, window_current);

/* 2nd, display lastlog from floating windows? (XXX) */

	if (config_lastlog_display_all) {
/* 3rd, other windows? */
		for (ww = windows; ww; ww = ww->next) {
			if (ww == window_current) continue;
			if (ww == w) continue; /* ;p */

			retval += ncurses_ui_window_lastlog(w, ww);
		}
	}
	{
		fstring_t *fstr = fstring_new("");
			/* double intentionally */
		ncurses_backlog_add(w, fstr);
		ncurses_backlog_add(w, fstr);
		fstring_free(fstr);
	}

#ifdef SCROLLING_FIXME
/* XXX fix n->start */
	n->start = old_start;
	if (n->start > n->lines_count - w->height + n->overflow)
		n->start = n->lines_count - w->height + n->overflow;

	if (n->start < 0)
		n->start = 0;
#endif

	n->redraw = 1;
	return retval;
}

void ncurses_lastlog_new(window_t *w) {
#define lastlog_edge		WF_BOTTOM
#define lastlog_margin		1
#define lastlog_frame		WF_TOP
#define lastlog_wrap		0

	ncurses_window_t *n = w->priv_data;
	int size = config_lastlog_size + lastlog_margin + ((lastlog_frame) ? 1 : 0);

	switch (lastlog_edge) {
		case WF_LEFT:
			w->width = size;
			n->margin_right = lastlog_margin;
			break;
		case WF_RIGHT:
			w->width = size;
			n->margin_left = lastlog_margin;
			break;
		case WF_TOP:
			w->height = size;
			n->margin_bottom = lastlog_margin;
			break;
		case WF_BOTTOM:
			w->height = size;
			n->margin_top = lastlog_margin;
			break;
	}
	w->frames = lastlog_frame;
	n->handle_redraw = ncurses_lastlog_update;
	n->handle_mouse = ncurses_lastlog_mouse_handler;
#ifdef SCROLLING_FIXME
	n->start = 0;
#endif
	w->edge = lastlog_edge;
	w->nowrap = !lastlog_wrap;
	w->floating = 1;
}
