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
 * ncurses_backlog_split()
 *
 * dzieli linie tekstu w buforze na linie ekranowe.
 *
 *  - w - okno do podzielenia
 *  - full - czy robimy pe³ne uaktualnienie?
 *  - removed - ile linii ekranowych z góry usuniêto?
 *
 * zwraca rozmiar w liniach ekranowych ostatnio dodanej linii.
 */
int ncurses_backlog_split(window_t *w, int full, int removed)
{
	int i, res = 0, bottom = 0;
	char *timestamp_format = NULL;
	ncurses_window_t *n;

	if (!w || !(n = w->priv_data))
		return 0;

	/* przy pe³nym przebudowaniu ilo¶ci linii nie musz± siê koniecznie
	 * zgadzaæ, wiêc nie bêdziemy w stanie pó¼niej stwierdziæ czy jeste¶my
	 * na koñcu na podstawie ilo¶ci linii mieszcz±cych siê na ekranie. */
	if (full && n->start == n->lines_count - w->height)
		bottom = 1;
	
	/* mamy usun±æ co¶ z góry, bo wywalono liniê z backloga. */
	if (removed) {
		for (i = 0; i < removed && i < n->lines_count; i++) {
			xfree(n->lines[i].ts);
			xfree(n->lines[i].ts_attr);
		}
		memmove(&n->lines[0], &n->lines[removed], sizeof(struct screen_line) * (n->lines_count - removed));
		n->lines_count -= removed;
	}

	/* je¶li robimy pe³ne przebudowanie backloga, czy¶cimy wszystko */
	if (full) {
		for (i = 0; i < n->lines_count; i++) {
			xfree(n->lines[i].ts);
			xfree(n->lines[i].ts_attr);
		}
		n->lines_count = 0;
		xfree(n->lines);
		n->lines = NULL;
	}

	if (config_timestamp && config_timestamp_show && config_timestamp[0])
		timestamp_format = format_string(config_timestamp);

	/* je¶li upgrade... je¶li pe³ne przebudowanie... */
	for (i = (!full) ? 0 : (n->backlog_size - 1); i >= 0; i--) {
		struct screen_line *l;
		char *str; 
		short *attr;
		int j, margin_left, wrapping = 0;

		time_t ts;			/* current ts */
		time_t lastts = 0;		/* last cached ts */
		char lasttsbuf[100];		/* last cached strftime() result */
		int prompt_width;

		str = n->backlog[i]->str + n->backlog[i]->prompt_len;
		attr = n->backlog[i]->attr + n->backlog[i]->prompt_len;
		ts = n->backlog[i]->ts;
		margin_left = (!w->floating) ? n->backlog[i]->margin_left : -1;

		prompt_width = xmbswidth(n->backlog[i]->str, n->backlog[i]->prompt_len);
		
		for (;;) {
			int word, width;
			int ts_width = 0;

			if (!i)
				res++;

			n->lines_count++;
			n->lines = xrealloc(n->lines, n->lines_count * sizeof(struct screen_line));
			l = &n->lines[n->lines_count - 1];

			l->str = (unsigned char *) str;
			l->attr = attr;
			l->len = xstrlen(str);
			l->ts = NULL;
			l->ts_attr = NULL;
			l->backlog = i;
			l->margin_left = (!wrapping || margin_left == -1) ? margin_left : 0;

			l->prompt_len = n->backlog[i]->prompt_len;
			if (!n->backlog[i]->prompt_empty) {
				l->prompt_str = (unsigned char *) n->backlog[i]->str;
				l->prompt_attr = n->backlog[i]->attr;
			} else {
				l->prompt_str = NULL;
				l->prompt_attr = NULL;
			}

			if ((!w->floating || (w->id == WINDOW_LASTLOG_ID && ts)) && timestamp_format) {
				fstring_t *s = NULL;

				if (!ts || lastts != ts) {	/* generate new */
					struct tm *tm = localtime(&ts);

					strftime(lasttsbuf, sizeof(lasttsbuf)-1, timestamp_format, tm);
					lastts = ts;
				}

				s = fstring_new(lasttsbuf);

				l->ts = s->str;
				ts_width = xmbswidth(l->ts, xstrlen(l->ts));
				ts_width++;			/* for separator between timestamp and text */
				l->ts_attr = s->attr;

				xfree(s);
			}

			width = w->width - ts_width - prompt_width - n->margin_left - n->margin_right; 

			if ((w->frames & WF_LEFT))
				width -= 1;
			if ((w->frames & WF_RIGHT))
				width -= 1;
#ifdef USE_UNICODE
			{
				int str_width = 0;

				mbtowc(NULL, NULL, 0);

				for (j = 0, word = 0; j < l->len;) {
					wchar_t ch;
					int ch_width;
					int ch_len;

					ch_len = mbtowc(&ch, &str[j], l->len - j);
					if (ch_len == -1) {
						ch = '?';
						ch_len = 1;
					}

					if (ch == CHAR(' '))
						word = j + 1;

					if (str_width >= width) {
						int old_len = l->len;

						l->len = (!w->nowrap && word) ? word : 		/* XXX, (str_width > width) ? word-1 : word? */
							(str_width > width && j) ? j /* - 1 */ : j;

						/* avoid dead loop -- always move forward */
						/* XXX, a co z bledami przy rysowaniu? moze lepiej str++; attr++; albo break? */
						if (!l->len)
							l->len = 1;

						if ((ch_len = mbtowc(&ch, &str[l->len], old_len - l->len)) > 0 && ch == CHAR(' ')) {
							l->len -= ch_len;
							str += ch_len;
							attr += ch_len;
						}
						break;
					}

					ch_width = wcwidth(ch);
					if (ch_width == -1) /* not printable? */
						ch_width = 1;		/* XXX: should be rendered as '?' with A_REVERSE. I hope wcwidth('?') is always 1. */
					str_width += ch_width;
					j += ch_len;
				}
				if (w->nowrap)
					break;
			}
#else
			if (l->len < width)
				break;

			if (w->nowrap) {
				l->len = width;		/* XXX, what for? for not drawing outside screen-area? ncurses can handle with it */

				if (str[width] == CHAR(' ')) {
					l->len--;
					/* str++; attr++; */
				}
				/* while (*str) { str++; attr++; } */
				break;
			}
		
			for (j = 0, word = 0; j < l->len; j++) {
				if (str[j] == CHAR(' '))
					word = j + 1;

				if (j == width) {
					l->len = (word) ? word : width;
					if (str[j] == CHAR(' ')) {
						l->len--;
						str++;
						attr++;
					}
					break;
				}
			}
#endif
			str += l->len;
			attr += l->len;

			if (! *str)
				break;

			wrapping = 1;
		}
	}
	xfree(timestamp_format);

	if (bottom) {
		n->start = n->lines_count - w->height;
		if (n->start < 0)
			n->start = 0;
	}

	if (full) {
		if (window_current && window_current->id == w->id) 
			ncurses_redraw(w);
		else
			n->redraw = 1;
	}

	return res;
}

/*
 *
 */
int ncurses_backlog_add_real(window_t *w, fstring_t *str) {
	int i, removed = 0;
	ncurses_window_t *n = w->priv_data;
	
	if (!w)
		return 0;

	if (n->backlog_size == config_backlog_size) {
		fstring_t *line = n->backlog[n->backlog_size - 1];
		int i;

		for (i = 0; i < n->lines_count; i++) {
			if (n->lines[i].backlog == n->backlog_size - 1)
				removed++;
		}

		fstring_free(line);

		n->backlog_size--;
	} else 
		n->backlog = xrealloc(n->backlog, (n->backlog_size + 1) * sizeof(fstring_t *));

	memmove(&n->backlog[1], &n->backlog[0], n->backlog_size * sizeof(fstring_t *));
	n->backlog[0] = str;

	n->backlog_size++;

	for (i = 0; i < n->lines_count; i++)
		n->lines[i].backlog++;

	return ncurses_backlog_split(w, 0, removed);
}

/*
 * ncurses_backlog_add()
 *
 * dodaje do bufora okna. zak³adamy dodawanie linii ju¿ podzielonych.
 * je¶li doda siê do backloga liniê zawieraj±c± '\n', bêdzie ¼le.
 *
 *  - w - wska¼nik na okno ekg
 *  - str - linijka do dodania
 *
 * zwraca rozmiar dodanej linii w liniach ekranowych.
 */
int ncurses_backlog_add(window_t *w, const fstring_t *str) {
#if USE_UNICODE
	{
		int rlen = xstrlen(str->str);

		int cur = 0;
		int i;

		mbtowc(NULL, NULL, 0);	/* reset */

		for (i = 0; cur < rlen; i++) {
			wchar_t znak;
			int len	= mbtowc(&znak, &(str->str[cur]), rlen-cur);

			if (!len)	/* shouldn't happen -- cur < rlen */
				break;

			if (len == -1) {
				znak = '?';
				len  = 1;		/* always move forward */

				str->str[cur] = '?';
				str->attr[cur]  = str->attr[cur] | FSTR_REVERSE; 
			}
/*
			if (cur == str->prompt_len)
				str->prompt_len = i;

			if (cur == str->margin_left)
				str->margin_left = i;
 */
			cur += len;
		}
	}
#endif
	return ncurses_backlog_add_real(w, fstring_dup(str));
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

		ncurses_backlog_split(w, 1, 0);
	}
}

