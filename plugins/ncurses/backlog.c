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

#include "ecurses.h"

#include <stdlib.h>
#include <string.h>

#include "nc-stuff.h"

static inline int xmbswidth(const char *s, size_t n) {
#ifdef USE_UNICODE
	size_t i;
	int res = 0;

	mbtowc(NULL, NULL, 0);
	for (i = 0; i < n; ) {
		wchar_t ch;
		int ch_len; 

		ch_len = mbtowc(&ch, &s[i], n - i);
		if (ch_len != -1) {
			int wc_width = wcwidth(ch);

			if (wc_width == -1)
				wc_width = 1;

			res += wc_width;
			i += ch_len;
		} else {
			i++;
			res++;
		}
	}
	return res;
#else
	return n;
#endif
}

/*
 *
 */
int ncurses_backlog_add_real(window_t *w, /*locale*/ fstring_t *str) {
	ncurses_window_t *n = w->priv_data;
	
	if (!w)
		return 0;

	if (n->backlog_size == config_backlog_size) {
		fstring_t *line = n->backlog[n->backlog_size - 1];
		fstring_free(line);
		n->backlog_size--;
	} else 
		n->backlog = xrealloc(n->backlog, (n->backlog_size + 1) * sizeof(fstring_t *));

	memmove(&n->backlog[1], &n->backlog[0], n->backlog_size * sizeof(fstring_t *));
	n->backlog[0] = str;
	n->backlog_size++;

	return 0;
}

/**
 * ncurses_backlog_add()
 *
 * Add an utf8-encoded line to window backlog, recoding it whenever
 * necessary. The line should not contain \n. It will be duplicated, so
 * caller needs to free it.
 *
 * @param w - target window
 * @param str - an utf8-encoded fstring_t to add
 *
 * @return The return value is going to be changed, thou shalt not rely
 * upon it.
 */
int ncurses_backlog_add(window_t *w, const fstring_t *str) {
	return ncurses_backlog_add_real(w, ekg_recode_fstr_to_locale(str));
}


/*
 * changed_backlog_size()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,backlog_size''.
 */
void changed_backlog_size(const char *var)
{
	window_t *w;

	if (config_backlog_size < ncurses_screen_height)
		config_backlog_size = ncurses_screen_height;

	for (w = windows; w; w = w->next) {
		ncurses_window_t *n = w->priv_data;
		int i;
				
		if (n->backlog_size <= config_backlog_size)
			continue;
				
		for (i = config_backlog_size; i < n->backlog_size; i++)
			fstring_free(n->backlog[i]);

		n->backlog_size = config_backlog_size;
		n->backlog = xrealloc(n->backlog, n->backlog_size * sizeof(fstring_t *));
	}
}
