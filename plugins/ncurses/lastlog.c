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

#ifdef HAVE_REGEX_H
#	include <sys/types.h>
#	include <regex.h>
#endif

#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include <ekg/stuff.h>

#include "backlog.h"
#include "mouse.h"
#include "nc-stuff.h"

static int ncurses_ui_window_lastlog(window_t *lastlog_w, window_t *w) {
	const char *header;
#if USE_UNICODE
	CHAR_T *wexpression;
#endif

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

	if (config_lastlog_noitems) /* always add header */
		ncurses_backlog_add(lastlog_w, fstring_new_format(header, window_target(w), lastlog->expression));

#if USE_UNICODE
	wexpression = normal_to_wcs(lastlog->expression);
#endif

	local_config_lastlog_case = (lastlog->casense == -1) ? config_lastlog_case : lastlog->casense;

	for (i = n->backlog_size-1; i >= 0; i--) {
		int found = 0;

		if (lastlog->isregex) {		/* regexp */
#ifdef HAVE_REGEX_H
			int rs;
#if USE_UNICODE
			char *tmp = wcs_to_normal(n->backlog[i]->str.w);
			rs = regexec(&(lastlog->reg), tmp, 0, NULL, 0);
			xfree(tmp);
#else
			rs = regexec(&(lastlog->reg), (char *) n->backlog[i]->str.w, 0, NULL, 0);
#endif
			if (!rs)
				found = 1;
			else if (rs != REG_NOMATCH) {
				char errbuf[512];
				/* blad wyrazenia? */
				regerror(rs, &(lastlog->reg), errbuf, sizeof(errbuf));	/* NUL-char-ok */
				print("regex_error", errbuf);

				/* XXX mh, dodac to do backloga? */
				/* XXX, przerwac wykonywanie? */
				break;
			}
#endif
		} else {				/* substring */
#if USE_UNICODE
			if (local_config_lastlog_case)
				found = !!wcsstr(n->backlog[i]->str.w, wexpression);
			else {
				char *tmp = wcs_to_normal(n->backlog[i]->str.w);
				found = !!xstrcasestr(tmp, lastlog->expression);
				xfree(tmp);
			}
#else
			if (local_config_lastlog_case)
				found = !!xstrstr((char *) n->backlog[i]->str.w, lastlog->expression);
			else	found = !!xstrcasestr((char *) n->backlog[i]->str.w, lastlog->expression);
#endif
		}

		if (!config_lastlog_noitems && found && !items) /* add header only when found */
			ncurses_backlog_add(lastlog_w, fstring_new_format(header, window_target(w), lastlog->expression));

		if (found) {
			fstring_t *dup;
			size_t len;

			dup = xmalloc(sizeof(fstring_t));

			len = xwcslen(n->backlog[i]->str.w);

			dup->str.w		= xmemdup(n->backlog[i]->str.w, sizeof(CHAR_T) * (len + 1));
			dup->attr		= xmemdup(n->backlog[i]->attr, sizeof(short) * (len + 1));
			dup->ts			= n->backlog[i]->ts;
			dup->prompt_len		= n->backlog[i]->prompt_len;
			dup->prompt_empty	= n->backlog[i]->prompt_empty;
			dup->margin_left	= n->backlog[i]->margin_left;
		/* org. window for example if we would like user allow move under that line with mouse and double-click.. or whatever */
/*			dup->priv_data		= (void *) w;	 */

			ncurses_backlog_add_real(lastlog_w, dup);
			items++;
		}
	}
#if USE_UNICODE
	xfree(wexpression);
#endif

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
	old_start = n->start;

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
	ncurses_backlog_add(w, fstring_new(""));
	ncurses_backlog_add(w, fstring_new(""));

/* XXX fix n->start */
	n->start = old_start;
	if (n->start > n->lines_count - w->height + n->overflow)
		n->start = n->lines_count - w->height + n->overflow;

	if (n->start < 0)
		n->start = 0;

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
	n->start = 0;
	w->edge = lastlog_edge;
	w->nowrap = !lastlog_wrap;
	w->floating = 1;
}
