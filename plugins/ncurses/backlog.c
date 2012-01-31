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

static int get_word_width(gchar *str, fstr_attr_t *attr, int *eat) {
	int i, len = xstrlen(str);
	for (i=0; i<len; i++) {
		if ((attr[i] & FSTR_LINEBREAK)) {
			i++;
			break;
		}
	}
	*eat = i;
	return xmbswidth(str, i);
}

static int break_word(gchar *str, int width) {
	size_t i;
	int len = xstrlen(str);
#if USE_UNICODE
	int sum = 0;

	mbtowc(NULL, NULL, 0);
	for (i = 0; i < len && sum<width; ) {
		wchar_t ch;
		int ch_len = mbtowc(&ch, &str[i], len - i);
		if (ch_len!=-1) {
			int wc_width = wcwidth(ch);

			if (wc_width == -1)
				wc_width = 1;

			if (sum + wc_width > width)
				break;
			sum += wc_width;
			i += ch_len;
		} else {
			i++;
			sum++;
		}
	}
#else
	i = (len>=width ? width : len);
#endif
	return i;
}

static int wrap_line(window_t *w, int width, char *str, fstr_attr_t *attr, int *last_space) {
	int i, j, eat, printed, len = xstrlen(str);

	if (len==0 || xmbswidth(str, len)<width || width < 1)
		return len;

	if (w->nowrap)
		return break_word(str, width);

	for (i=0, printed=0; printed <= width && i < len; printed += j, i += eat) {
		j = get_word_width(str+i, attr+i, &eat);

		if (printed + j <= width)
			continue;

		if (printed+j == width+1 && str[i+eat-1]==' ') {
			*last_space = 1;
			return i + eat - 1;
		} else if (i==0) {
			return break_word(str, width);
		} else if ((j > width) && (width-printed>8)) {
			return i + break_word(str+i, width-printed);
		}
		return i;
	}
	return len;
}

static void calc_window_dimension(window_t *w) {
	ncurses_window_t *n = w->priv_data;

	n->x0 = n->margin_left;
	n->y0 = n->margin_top;

	if (w->frames & WF_LEFT) n->x0++;
	if (w->frames & WF_TOP) n->y0++;

	n->width = w->width - n->x0 - n->margin_right;
	n->height = w->height - n->y0 - n->margin_bottom;

	if (w->frames & WF_RIGHT) n->width--;
	if (w->frames & WF_BOTTOM) n->height--;
}

/*
 * backlog_split()
 *
 * dzieli linie tekstu w buforze na linie ekranowe.
 *
 *  - w - okno do podzielenia
 *
 * XXX function uses n->x0, y0, width, height !!! call calc_window_dimension(). !!!
 */
static int backlog_split(window_t *w, backlog_line_t *b, gboolean show, int y) {
	ncurses_window_t *n = w->priv_data;
	int rows_count = 0;
	/* timestamp */
	int ts_width		= 0;
	gchar *ts_str		= NULL;
	fstr_attr_t *ts_attr	= NULL;
	/* prompt */
	int prompt_width	= b->line->prompt_empty ? 0 : xmbswidth(b->line->str, b->line->prompt_len);
	/* text */
	char *str		= b->line->str  + b->line->prompt_len;
	fstr_attr_t *attr	= b->line->attr + b->line->prompt_len;

	/* set timestamp */
	if (b->line->ts && formated_config_timestamp && config_timestamp_show &&
	    (!w->floating || w->id == WINDOW_LASTLOG_ID))
	{
		fstring_t *s = fstring_new(timestamp_time(formated_config_timestamp, b->line->ts));
		ts_str	 = s->str;
		ts_attr	 = s->attr;
		xfree(s);
		ts_width = xmbswidth(ts_str, xstrlen(ts_str));
	}

	while (*str || rows_count==0) {
		int len, last_space = 0;
		int width = n->width - ts_width - prompt_width;

		len = wrap_line(w, width, str, attr, &last_space);

		if (show && (0 <= y && y < n->height)) {
			wmove(n->window, n->y0 + y, n->x0);

			if (ts_width) {		/* print timestamp */
				ncurses_fstring_print_fast(n->window, ts_str, ts_attr, -1);
			}

			if (prompt_width)	/* print prompt */
				ncurses_fstring_print_fast(n->window, b->line->str, b->line->attr, b->line->prompt_len);

			if (width > 0)		/* print text */
				ncurses_fstring_print_fast(n->window, str, attr, len);
		}

		rows_count++;

		if (w->nowrap)
			break;

		str	+= len + last_space;
		attr	+= len + last_space;
		y++;
	}

	xfree(ts_str);
	xfree(ts_attr);

	return rows_count;
}

static int ncurses_backlog_calc_height(window_t *w, backlog_line_t *bl) {
	return (w->nowrap) ? 1 : backlog_split(w, bl, FALSE, 0);
}

static int ncurses_backlog_display_line(window_t *w, int y, backlog_line_t *bl) {
	return backlog_split(w, bl, TRUE, y);
}

#define ncurses_get_backlog_height(w,b,index) (b=g_ptr_array_index(n->backlog, index), b->height ? b->height : (b->height=ncurses_backlog_calc_height(w, b)))

void ncurses_backlog_display(window_t *w) {
	ncurses_window_t *n = w->priv_data;
	int y, n_rows, idx, dtrl;
	backlog_line_t *bl;

	werase(n->window);

	if (n->backlog->len <= 0)
		return;

	calc_window_dimension(w);

	dtrl = n->last_red_line ? 1 : 0;

	/* draw text */
	if (n->index == EKG_NCURSES_BACKLOG_END) {
		/* display from end of backlog */
		w->more = n->cleared = 0;
		for (y = n->height, idx = n->backlog->len - 1; idx >= 0 && y > 0; idx--) {
			n_rows = ncurses_get_backlog_height(w, bl, idx);
			if (dtrl && (bl->line->ts < n->last_red_line)) {
				dtrl = 0;
				draw_thin_red_line(w, --y);
				if (y == 0) break;
			}
			y -= n_rows;
			ncurses_backlog_display_line(w, y, bl);
		}

		if (y>0 && !(config_text_bottomalign && (!w->floating || config_text_bottomalign == 2))) {
			wmove(n->window, n->margin_top, 0);
			winsdelln(n->window, -y);
		}
	} else {
		/* display from line */
		idx = n->index;
		if (dtrl && idx > 0 ) {
			bl = g_ptr_array_index(n->backlog, idx - 1);
			dtrl = (bl->line->ts < n->last_red_line);
		}
		for (y = - n->first_row; idx < n->backlog->len && y < n->height; idx++) {
			n_rows = ncurses_get_backlog_height(w, bl, idx);
			if (dtrl && (bl->line->ts > n->last_red_line)) {
				dtrl = 0;
				draw_thin_red_line(w, y++);
				if (y == n->height) break;
			}
			ncurses_backlog_display_line(w, y, bl);
			y += n_rows;
		}

		if ((!n->cleared && !(idx < n->backlog->len)) ||
		    ( n->cleared && !(y < n->height)))
		{
			ncurses_backlog_seek_end(n);
			ncurses_backlog_display(w);
		}
	}
}

static void scroll_up(window_t *w, int count) {
	ncurses_window_t *n = w->priv_data;
	backlog_line_t *bl;

	if (n->index == EKG_NCURSES_BACKLOG_END)
		n->index = n->backlog->len;
	else {
		if (count <= n->first_row) {
			n->first_row -= count;
			return;
		}
		count -= n->first_row;
	}

	while (count > 0 && --n->index >= 0 )
		count -= ncurses_get_backlog_height(w, bl, n->index);

	if (n->index < 0)
		ncurses_backlog_seek_start(n);
	else
		n->first_row = -count;
}

void ncurses_backlog_scroll(window_t *w, int offset) {
	ncurses_window_t *n;
	backlog_line_t *bl;

/* XXX: add thin red line correction */

	if (!w || !(n = w->priv_data) || !n->backlog->len)
		return;

	n->cleared = 0;

	calc_window_dimension(w);

	if (n->index == EKG_NCURSES_BACKLOG_END) {
		if (offset > 0)
			return;
		/* move to first line on screen */
		scroll_up(w, n->height);
	}

	if (offset < 0) {
		scroll_up(w, -offset);
	} else {
		int h = ncurses_get_backlog_height(w, bl, n->index);
		if (n->first_row + offset < h) {
			n->first_row += offset;
			return;
		}
		
		offset -= (h - n->first_row);
		while (offset >= 0 && ++n->index < n->backlog->len) {
			h = ncurses_get_backlog_height(w, bl, n->index);
			offset -= h;
		}
		if (!(n->index < n->backlog->len)) {
			ncurses_backlog_seek_end(n);
			return;
		}
		n->first_row = h + offset;
	}
}

backlog_line_t *ncurses_backlog_mouse_click(window_t *w, int click_y) {
	ncurses_window_t *n = w->priv_data;
	backlog_line_t *bl = NULL;
	int i, h, y;

/* XXX: add thin red line correction */
	if (n->backlog->len < 1)
		return NULL;

	calc_window_dimension(w);
	click_y -= n->y0;

	i = n->index;

	if (i == EKG_NCURSES_BACKLOG_END) {
		/* move to first line */
		h = n->height;
		for (i = n->backlog->len; h > 0 && --i >= 0; )
			h -= ncurses_get_backlog_height(w, bl, i);

		if (i < 0)
			h = i = 0;
	} else
		h = - n->first_row;

	for (y = h; i < n->backlog->len && y < click_y; i++)
		y += ncurses_get_backlog_height(w, bl, i);

	if (click_y > y)
		return NULL;

	return bl;
}
/*
 *
 */
void ncurses_backlog_add_real(window_t *w, /*locale*/ fstring_t *str) {
	ncurses_window_t *n = w->priv_data;
	backlog_line_t *b;
	
	if (!w)
		return;

	/* add line */
	b = xmalloc(sizeof(backlog_line_t));
	b->line = str;
	if (w->nowrap)
		b->height = 1;

	g_ptr_array_add(n->backlog, b);

	/* check backlog length */
	if (n->backlog->len >= config_backlog_size) {
		g_ptr_array_remove_index(n->backlog, 0);

		if (n->index == 0)
			ncurses_backlog_seek_start(n);
		else if (n->index > 0)
			n->index--;
	}

	return;
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
void ncurses_backlog_add(window_t *w, const fstring_t *str) {
	ncurses_backlog_add_real(w, ekg_recode_fstr_to_locale(str));
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
		if (n->backlog->len > config_backlog_size) {
			int removed = n->backlog->len - config_backlog_size;
			g_ptr_array_remove_range(n->backlog, 0, removed);
			if (n->index == EKG_NCURSES_BACKLOG_END)
				continue;
			n->index -= removed;
			if (n->index < 0)
				ncurses_backlog_seek_start(n);
		}
	}
}

/*
 * ncurses_backlog_reset_heights()
 *
 */
void ncurses_backlog_reset_heights(window_t *w, int height) {
	ncurses_window_t *n = w->priv_data;

	if (!w || !n)
		return;

	void set_height(gpointer data, gpointer user_data) {
		backlog_line_t *b = data;
		
		b->height = height;
	}

	g_ptr_array_foreach(n->backlog, set_height, NULL);
}


static void backlog_line_destroy(gpointer data) {
	backlog_line_t *b = data;
	fstring_free(b->line);
	g_free(b);
}
/*
 * ncurses_backlog_new()
 *
 */
void ncurses_backlog_new(window_t *w) {
	ncurses_window_t *n = w->priv_data;
	n->backlog = g_ptr_array_new_with_free_func(backlog_line_destroy);
	ncurses_backlog_seek_end(n);
}
/*
 * ncurses_backlog_destroy()
 *
 */
void ncurses_backlog_destroy(window_t *w) {
	ncurses_window_t *n = w->priv_data;
	g_ptr_array_free(n->backlog, TRUE);
	n->backlog = NULL;
	ncurses_backlog_seek_start(n);
}