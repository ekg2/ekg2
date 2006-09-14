/* $Id$ */

/*
 *  (C) Copyright 2002-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Wojtek Bojdo³ <wojboj@htcon.pl>
 *                          Pawe³ Maziarz <drg@infomex.pl>
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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#ifdef WITH_ASPELL
#       include <aspell.h>
#endif

#include <ekg/debug.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include <ekg/commands.h>
#include <ekg/sessions.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>

#include "old.h"
#include "completion.h"
#include "bindings.h"
#include "contacts.h"
#include "mouse.h"

static WINDOW *ncurses_status	= NULL;		/* okno stanu */
static WINDOW *ncurses_header	= NULL;		/* okno nag³ówka */
WINDOW *ncurses_input	= NULL;		/* okno wpisywania tekstu */
WINDOW *ncurses_contacts= NULL;

CHAR_T *ncurses_history[HISTORY_MAX];	/* zapamiêtane linie */
int ncurses_history_index = 0;		/* offset w historii */

CHAR_T *ncurses_line = NULL;		/* wska¼nik aktualnej linii */
CHAR_T *ncurses_yanked = NULL;		/* bufor z ostatnio wyciêtym tekstem */
CHAR_T **ncurses_lines = NULL;		/* linie wpisywania wielolinijkowego */
int ncurses_line_start = 0;		/* od którego znaku wy¶wietlamy? */
int ncurses_line_index = 0;		/* na którym znaku jest kursor? */
int ncurses_lines_start = 0;		/* od której linii wy¶wietlamy? */
int ncurses_lines_index = 0;		/* w której linii jeste¶my? */
int ncurses_input_size = 1;		/* rozmiar okna wpisywania tekstu */
int ncurses_debug = 0;			/* debugowanie */

static int ncurses_screen_height;
static int ncurses_screen_width;

static struct termios old_tio;

int winch_pipe[2];
int have_winch_pipe = 0;
#ifdef WITH_ASPELL
#  define ASPELLCHAR 5
static AspellSpeller *spell_checker = NULL;
static AspellConfig  *spell_config  = NULL;
#endif

/*
 * ncurses_spellcheck_init()
 * 
 * it inializes dictionary
 */
#ifdef WITH_ASPELL
void ncurses_spellcheck_init(void) {
        AspellCanHaveError *possible_err;

        if (!config_aspell || !config_console_charset || !config_aspell_lang) {
	/* jesli nie chcemy aspella to wywalamy go z pamieci */
		if (spell_checker)	delete_aspell_speller(spell_checker);
		if (spell_config)	delete_aspell_config(spell_config);
		spell_checker = NULL;
		spell_config = NULL;
		debug("Maybe config_console_charset, aspell_lang or aspell variable is not set?\n");
		return;
	}

	wcs_print("aspell_init");
	
        if (spell_checker)	{ delete_aspell_speller(spell_checker);	spell_checker = NULL; }
	if (!spell_config) 	spell_config = new_aspell_config();
	aspell_config_replace(spell_config, "encoding", config_console_charset);
	aspell_config_replace(spell_config, "lang", config_aspell_lang);
	possible_err = new_aspell_speller(spell_config);
	/* delete_aspell_config(spell_config); ? */

	if (aspell_error_number(possible_err) != 0) {
		spell_checker = NULL;
		debug("Aspell error: %s\n", aspell_error_message(possible_err));
		print("aspell_init_error", aspell_error_message(possible_err));
		config_aspell = 0;
		delete_aspell_config(spell_config);
		spell_config = NULL;
	} else {
		spell_checker = to_aspell_speller(possible_err);
		wcs_print("aspell_init_success");
	}
}
#endif

/*
 * color_pair()
 *
 * zwraca numer COLOR_PAIR odpowiadaj±cej danej parze atrybutów: kolorze
 * tekstu (plus pogrubienie) i kolorze t³a.
 */
static int color_pair(int fg, int bold, int bg) {
	if (!config_display_color) {
		if (bg != COLOR_BLACK)
			return A_REVERSE;
		else
			return A_NORMAL | ((bold) ? A_BOLD : 0);
	}

	if (fg >= 8) {
		bold = 1;
		fg &= 7;
	}

	if (fg == COLOR_BLACK && bg == COLOR_BLACK) {
		fg = 7;
	} else if (fg == COLOR_WHITE && bg == COLOR_BLACK) {
		fg = 0;
	}

	return COLOR_PAIR(fg + 8 * bg) | ((bold) ? A_BOLD : 0);
}

/*
 * ncurses_commit()
 *
 * zatwierdza wszystkie zmiany w buforach ncurses i wy¶wietla je na ekranie.
 */
void ncurses_commit()
{
	ncurses_refresh();

	if (ncurses_header)
		wnoutrefresh(ncurses_header);

	wnoutrefresh(ncurses_status);

	wnoutrefresh(input);

	doupdate();
}

/* 
 * ncurses_main_window_mouse_handler()
 * 
 * handler for mouse events in main window 
 */
void ncurses_main_window_mouse_handler(int x, int y, int mouse_state)
{
        if (mouse_state == EKG_SCROLLED_UP) {
	        ncurses_current->start -= 5;
	        if (ncurses_current->start < 0)
	                ncurses_current->start = 0;
        } else if (mouse_state == EKG_SCROLLED_DOWN) {
	        ncurses_current->start += 5;
	
	        if (ncurses_current->start > ncurses_current->lines_count - window_current->height + ncurses_current->overflow)
	                ncurses_current->start = ncurses_current->lines_count - window_current->height + ncurses_current->overflow;

	        if (ncurses_current->start < 0)
	                ncurses_current->start = 0;

	        if (ncurses_current->start == ncurses_current->lines_count - window_current->height + ncurses_current->overflow) {
	                window_current->more = 0;
	                update_statusbar(0);
        	}
        } else {
		return;
	}

        ncurses_redraw(window_current);
        ncurses_commit();
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
int ncurses_backlog_add(window_t *w, fstring_t *str)
{
	int i, removed = 0;
	ncurses_window_t *n = w->private;
	
	if (!w)
		return 0;

	if (n->backlog_size == config_backlog_size) {
		ncurses_fstring_t *line = n->backlog[n->backlog_size - 1];
		int i;

		for (i = 0; i < n->lines_count; i++) {
			if (n->lines[i].backlog == n->backlog_size - 1)
				removed++;
		}

		fstring_free((fstring_t *) line);

		n->backlog_size--;
	} else 
		n->backlog = xrealloc(n->backlog, (n->backlog_size + 1) * sizeof(ncurses_fstring_t *));

	memmove(&n->backlog[1], &n->backlog[0], n->backlog_size * sizeof(ncurses_fstring_t *));
#if USE_UNICODE
	if (config_use_unicode) {
	/* str */
		char *tmp = str->str;
		CHAR_T *ble	= normal_to_wcs(tmp);		/* jesli to padnie to klops */
	/* attr */
		int rlen = xstrlen(str->str);
		int cur = 0;
		int i = 0;
	/* dopasuj atrybuty do literek */
		while (cur < rlen) {
			int len = mblen(&(str->str[cur]), rlen-cur);

			if (len == -1) {
				debug("[%s:%d] mblen() failed ?!\n", __FILE__, __LINE__);
				i = 0;
				break;
			}
/*			if (len > 1) debug("mblen(): %d\n", len); */
			str->attr[i] = str->attr[cur];
			i++;	cur += len;
		}
	/* fstring */
		n->backlog[0] = (ncurses_fstring_t *) str;
		n->backlog[0]->str	= ble;
		if (i) str->attr = xrealloc(str->attr, (i+1) * sizeof(short));		/* resize */
		free_utf(tmp);
	} else 
#endif
		n->backlog[0] = (ncurses_fstring_t *) str;

	n->backlog_size++;

	for (i = 0; i < n->lines_count; i++)
		n->lines[i].backlog++;

	return ncurses_backlog_split(w, 0, removed);
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
	ncurses_window_t *n = w->private;

	if (!w)
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

	/* je¶li upgrade... je¶li pe³ne przebudowanie... */
	for (i = (!full) ? 0 : (n->backlog_size - 1); i >= 0; i--) {
		struct screen_line *l;
		CHAR_T *str; 
		short *attr;
		int j, margin_left, wrapping = 0;
		time_t ts;

		str = __SPTR(n->backlog[i]->str, n->backlog[i]->prompt_len);
		attr = n->backlog[i]->attr + n->backlog[i]->prompt_len;
		ts = n->backlog[i]->ts;
		margin_left = (!w->floating) ? n->backlog[i]->margin_left : -1;
		
		for (;;) {
			int word = 0, width;

			if (!i)
				res++;

			n->lines_count++;
			n->lines = xrealloc(n->lines, n->lines_count * sizeof(struct screen_line));
			l = &n->lines[n->lines_count - 1];

			l->str = str;
			l->attr = attr;
			l->len = xwcslen(str);
			l->ts = NULL;
			l->ts_len = 0;
			l->ts_attr = NULL;
			l->backlog = i;
			l->margin_left = (!wrapping || margin_left == -1) ? margin_left : 0;

			l->prompt_len = n->backlog[i]->prompt_len;
			if (!n->backlog[i]->prompt_empty) {
				l->prompt_str = n->backlog[i]->str;
				l->prompt_attr = n->backlog[i]->attr;
			} else {
				l->prompt_str = NULL;
				l->prompt_attr = NULL;
			}

			if (!w->floating && config_timestamp && config_timestamp_show) {
				struct tm *tm = localtime(&ts);
				char buf[100], *tmp = NULL, *format;
				fstring_t *s = NULL;

				if (xstrcmp(config_timestamp, "")) {
					tmp = format_string(config_timestamp);
					format = saprintf("%s ", tmp);
        	                        strftime(buf, sizeof(buf)-1, format, tm);
					
					s = fstring_new(buf);

					l->ts = s->str;
					l->ts_len = xstrlen(l->ts);
					l->ts_attr = s->attr;

					xfree(s);
					xfree(tmp);
					xfree(format);
				}
			}

			width = w->width - l->ts_len - l->prompt_len - n->margin_left - n->margin_right; 

			if ((w->frames & WF_LEFT))
				width -= 1;
			if ((w->frames & WF_RIGHT))
				width -= 1;

			if (l->len < width)
				break;
		
			for (j = 0, word = 0; j < l->len; j++) {

				if (__S(str, j) == ' ' && !w->nowrap)
					word = j + 1;

				if (j == width) {
					l->len = (word) ? word : width;
					if (__S(str, j) == ' ') {
						l->len--;
						__SN(&str, 1);		/* str++ */
						attr++;
					}
					break;
				}
			}

			if (w->nowrap) {
				while (__S(str, 0)) {		/* while (*str)  */
					__SN(&str, 1);			/* str++ */
					attr++;
				}
				break;
			}
			__SN(&str, l->len);	/* str += l->len */
			attr += l->len;

			if (!__S(str, 0))
				break;

			wrapping = 1;
		}
	}

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
 * ncurses_resize()
 *
 * dostosowuje rozmiar okien do rozmiaru ekranu, przesuwaj±c odpowiednio
 * wy¶wietlan± zawarto¶æ.
 */
void ncurses_resize()
{
	int left, right, top, bottom, width, height;
	list_t l;

	left = 0;
	right = stdscr->_maxx + 1;
	top = config_header_size;
	bottom = stdscr->_maxy + 1 - ncurses_input_size - config_statusbar_size;
	width = right - left;
	height = bottom - top;

	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!n)
			continue;

		if (!w->edge)
			continue;

		w->hide = 0;

		if ((w->edge & WF_LEFT)) {
			if (w->width * 2 > width)
				w->hide = 1;
			else {
				w->left = left;
				w->top = top;
				w->height = height;
				w->hide = 0;
				width -= w->width;
				left += w->width;
			}
		}

		if ((w->edge & WF_RIGHT)) {
			if (w->width * 2 > width)
				w->hide = 1;
			else {
				w->left = right - w->width;
				w->top = top;
				w->height = height;
				width -= w->width;
				right -= w->width;
			}
		}

		if ((w->edge & WF_TOP)) {
			if (w->height * 2 > height)
				w->hide = 1;
			else {
				w->left = left;
				w->top = top;
				w->width = width;
				height -= w->height;
				top += w->height;
			}
		}

		if ((w->edge & WF_BOTTOM)) {
			if (w->height * 2 > height)
				w->hide = 1;
			else {
				w->left = left;
				w->top = bottom - w->height;
				w->width = width;
				height -= w->height;
				bottom -= w->height;
			}
		}

		wresize(n->window, w->height, w->width);
		mvwin(n->window, w->top, w->left);

		n->redraw = 1;
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;
		int delta;

		if (!n || w->floating)
			continue;

		delta = height - w->height;

		if (n->lines_count - n->start == w->height) {
			n->start -= delta;

			if (delta < 0) {
				if (n->start > n->lines_count)
					n->start = n->lines_count;
			} else {
				if (n->start < 0)
					n->start = 0;
			}
		}

		if (n->overflow > height)
			n->overflow = height;

		w->height = height;

		if (w->height < 1)
			w->height = 1;

		if (w->width != width && !w->doodle) {
			w->width = width;
			ncurses_backlog_split(w, 1, 0);
		}

		w->width = width;
		
		wresize(n->window, w->height, w->width);

		w->top = top;
		w->left = left;

		if (w->left < 0)
			w->left = 0;
		if (w->left > stdscr->_maxx)
			w->left = stdscr->_maxx;

		if (w->top < 0)
			w->top = 0;
		if (w->top > stdscr->_maxy)
			w->top = stdscr->_maxy;

		mvwin(n->window, w->top, w->left);

		if (n->overflow) {
			n->start = n->lines_count - w->height + n->overflow;
			if (n->start < 0)
				n->start = 0;
		}

		n->redraw = 1;
	}

	ncurses_screen_width = width;
	ncurses_screen_height = height;
}

/*
 * ncurses_redraw()
 *
 * przerysowuje zawarto¶æ okienka.
 *
 *  - w - okno
 */
void ncurses_redraw(window_t *w)
{
	int x, y, left, top, height, width;
	ncurses_window_t *n = w->private;
	const char *vertical_line_char = format_find("contacts_vertical_line_char");
	const char *horizontal_line_char = format_find("contacts_horizontal_line_char");
	
	if (!n)
		return;
	
	left = n->margin_left;
	top = n->margin_top;
	height = w->height - n->margin_top - n->margin_bottom;
	width = w->width - n->margin_left - n->margin_right;
	
	if (w->doodle) {
		n->redraw = 0;
		return;
	}

	if (n->handle_redraw) {
		/* handler mo¿e sam narysowaæ wszystko, wtedy zwraca -1.
		 * mo¿e te¿ tylko uaktualniæ zawarto¶æ okna, wtedy zwraca
		 * 0 i rysowaniem zajmuje siê ta funkcja. */
		if (n->handle_redraw(w) == -1)
			return;
	}
	
	werase(n->window);
	wattrset(n->window, color_pair(COLOR_BLUE, 0, COLOR_BLACK));

	if (w->floating) {
		if ((w->frames & WF_LEFT)) {
			left++;

			for (y = 0; y < w->height; y++)
				mvwaddch(n->window, y, n->margin_left, vertical_line_char[0]);
		}

		if ((w->frames & WF_RIGHT)) {
			for (y = 0; y < w->height; y++)
				mvwaddch(n->window, y, w->width - 1 - n->margin_right, vertical_line_char[0]);
		}
			
		if ((w->frames & WF_TOP)) {
			top++;
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(n->window, n->margin_top, x, horizontal_line_char[0]);
		}

		if ((w->frames & WF_BOTTOM)) {
			height--;

			for (x = 0; x < w->width; x++)
				mvwaddch(n->window, w->height - 1 - n->margin_bottom, x, horizontal_line_char[0]);
		}

		if ((w->frames & WF_LEFT) && (w->frames & WF_TOP))
			mvwaddch(n->window, 0, 0, ACS_ULCORNER);

		if ((w->frames & WF_RIGHT) && (w->frames & WF_TOP))
			mvwaddch(n->window, 0, w->width - 1, ACS_URCORNER);

		if ((w->frames & WF_LEFT) && (w->frames & WF_BOTTOM))
			mvwaddch(n->window, w->height - 1, 0, ACS_LLCORNER);

		if ((w->frames & WF_RIGHT) && (w->frames & WF_BOTTOM))
			mvwaddch(n->window, w->height - 1, w->width - 1, ACS_LRCORNER);
	}
 
	if (n->start < 0) 
		n->start = 0;
	for (y = 0; y < height && n->start + y < n->lines_count; y++) {
		struct screen_line *l = &n->lines[n->start + y];
		unsigned int x_real = 0;

		wattrset(n->window, A_NORMAL);
		for (x = 0; l->ts && l->ts[x] && x < l->ts_len; x++) { 
			int attr = A_NORMAL;
			short chattr = l->ts_attr[x];
			unsigned char ch = (unsigned char) l->ts[x];

                        if ((chattr & 64))
                                attr |= A_BOLD;

                        if ((chattr & 256))
                                attr |= A_BLINK;

                        if (!(chattr & 128))
                                attr |= color_pair(chattr & 7, 0, 
					config_display_transparent?COLOR_BLACK:
					(chattr>>3)&7);

			if ((chattr & 512))
				attr |= A_UNDERLINE;

			if ((chattr & 1024))
				attr |= A_REVERSE;
                        if (ch < 32) {
                                ch += 64;
                                attr |= A_REVERSE;
                        }
                        if (ch > 127 && ch < 160) {
                                ch = '?';
                                attr |= A_REVERSE;
                        }
			wattrset(n->window, attr);
			mvwaddch(n->window, top + y, left + x, ch);
		}
		for (x = 0; x < l->prompt_len + l->len; x++) {
			int attr = A_NORMAL;
			CHAR_T ch;
			short chattr;
			if (x < l->prompt_len) {
				if (!l->prompt_str)
					continue;
				
				ch = __S(l->prompt_str, x);
				chattr = l->prompt_attr[x];
			} else {
				ch = __S(l->str, x - l->prompt_len);
				chattr = l->attr[x - l->prompt_len];
			}
			if ((chattr & 64))
				attr |= A_BOLD;

                        if ((chattr & 256))
                                attr |= A_BLINK;

                        if (!(chattr & 128))
                                attr |= color_pair(chattr & 7, 0, 
					config_display_transparent?COLOR_BLACK:
					(chattr>>3)&7);

			if ((chattr & 512))
				attr |= A_UNDERLINE;

			if ((chattr & 1024))
				attr |= A_REVERSE;
			if (ch < 32) {
				ch += 64;
				attr |= A_REVERSE;
			}
			if (
#if USE_UNICODE
#warning "XXX ?"
				!config_use_unicode && 
#endif
				ch > 127 && ch < 160
			   ) 
			{
				ch = '?';
				attr |= A_REVERSE;
			}
			wattrset(n->window, attr);
			if (l->margin_left != -1 && x >= l->margin_left) 
				x_real = x - l->margin_left + config_margin_size;
			else 
				x_real = x;
#if USE_UNICODE
			if (config_use_unicode) {
				mvwaddnwstr(n->window, top + y, left + x_real + l->ts_len, &ch, 1);
			} else
#endif
				mvwaddch(n->window, top + y, left + x_real + l->ts_len, ch);
		}
	}

	n->redraw = 0;
}

/*
 * ncurses_clear()
 *
 * czy¶ci zawarto¶æ okna.
 */
void ncurses_clear(window_t *w, int full)
{
	ncurses_window_t *n = w->private;
		
	if (!full) {
		n->start = n->lines_count;
		n->redraw = 1;
		n->overflow = w->height;
		return;
	}

	if (n->backlog) {
		int i;

		for (i = 0; i < n->backlog_size; i++)
			fstring_free((fstring_t *) n->backlog[i]);

		xfree(n->backlog);

		n->backlog = NULL;
		n->backlog_size = 0;
	}

	if (n->lines) {
		int i;

		for (i = 0; i < n->lines_count; i++) {
			xfree(n->lines[i].ts);
			xfree(n->lines[i].ts_attr);
		}
		
		xfree(n->lines);

		n->lines = NULL;
		n->lines_count = 0;
	}

	n->start = 0;
	n->redraw = 1;
}

/*
 * window_floating_update()
 *
 * uaktualnia zawarto¶æ p³ywaj±cego okna o id == i
 * lub wszystkich okienek, gdy i == 0.
 */
static void window_floating_update(int i)
{
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (i && (w->id != i))
			continue;

		if (!w->floating)
			continue;

		/* je¶li ma w³asn± obs³ugê od¶wie¿ania, nie ruszamy */
		if (n->handle_redraw)
			continue;
		
		if (w->last_update == time(NULL))
			continue;

		w->last_update = time(NULL);

		ncurses_clear(w, 1);

		ncurses_redraw(w);
	}
}

/*
 * ncurses_refresh()
 *
 * wnoutrefresh()uje aktualnie wy¶wietlane okienko.
 */
void ncurses_refresh()
{
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!n)
			continue;

		if (w->floating || window_current->id != w->id)
			continue;

		if (n->redraw)
			ncurses_redraw(w);

		if (!w->hide)
			wnoutrefresh(n->window);
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!w->floating || w->hide)
			continue;

		if (n->handle_redraw)
			ncurses_redraw(w);
		else
			window_floating_update(w->id);

		touchwin(n->window);
		wnoutrefresh(n->window);
	}
	
	mvwin(ncurses_status, stdscr->_maxy + 1 - ncurses_input_size - config_statusbar_size, 0);
	wresize(input, ncurses_input_size, input->_maxx + 1);
	mvwin(input, stdscr->_maxy - ncurses_input_size + 1, 0);
}

/*
 * update_header()
 *
 * uaktualnia nag³ówek okna i wy¶wietla go ponownie.
 *
 *  - commit - czy wy¶wietliæ od razu?
 */
static void update_header(int commit)
{
	int y;

	if (!ncurses_header)
		return;

	wattrset(ncurses_header, color_pair(COLOR_WHITE, 0, COLOR_BLUE));

	for (y = 0; y < config_header_size; y++) {
		int x;
		
		wmove(ncurses_header, y, 0);

		for (x = 0; x <= ncurses_status->_maxx; x++)
			waddch(ncurses_header, ' ');
	}

	if (commit)
		ncurses_commit();
}
		
/*
 * window_printat()
 *
 * wy¶wietla dany tekst w danym miejscu okna.
 *	(w == ncurses_header || ncurses_status)
 *  - x, y - wspó³rzêdne, od których zaczynamy
 *  - format - co mamy wy¶wietliæ
 *  - data - dane do podstawienia w formatach
 *  - fgcolor - domy¶lny kolor tekstu
 *  - bold - domy¶lne pogrubienie
 *  - bgcolor - domy¶lny kolor t³a
 *
 * zwraca ilo¶æ dopisanych znaków.
 */

/* 13 wrz 06 	removed status cause it was always 1 (dj)  */
		
static int window_printat(WINDOW *w, int x, int y, const char *format_, void *data_, int fgcolor, int bold, int bgcolor) {
	int orig_x = x;
	int backup_display_color = config_display_color;
	char *format = (char*) format_;
	const char *p;
	struct format_data *data = data_;

	if (!config_display_pl_chars) {
		format = xstrdup(format);
		iso_to_ascii(format);
	}

	p = format;

	if (config_display_color == 2)	config_display_color = 0;

	if (!w)
		return -1;
	
	if (x == 0) {
		int i;

		wattrset(w, color_pair(fgcolor, 0, bgcolor));

		wmove(w, y, 0);

		for (i = 0; i <= w->_maxx; i++)
			waddch(w, ' ');
	}

	wmove(w, y, x);
			
	while (*p && *p != '}' && x <= w->_maxx) {
		int i, nest;

		if (*p != '%') {
			waddch(w, (unsigned char) *p);
			p++;
			x++;
			continue;
		}

		p++;
		if (!*p)
			break;

#define __fgcolor(x,y,z) \
		case x: fgcolor = z; bold = 0; break; \
		case y: fgcolor = z; bold = 1; break;
#define __bgcolor(x,y) \
		case x: bgcolor = y; break;

		if (*p != '{') {
			switch (*p) {
				__fgcolor('k', 'K', COLOR_BLACK);
				__fgcolor('r', 'R', COLOR_RED);
				__fgcolor('g', 'G', COLOR_GREEN);
				__fgcolor('y', 'Y', COLOR_YELLOW);
				__fgcolor('b', 'B', COLOR_BLUE);
				__fgcolor('m', 'M', COLOR_MAGENTA);
				__fgcolor('c', 'C', COLOR_CYAN);
				__fgcolor('w', 'W', COLOR_WHITE);
				__bgcolor('l', COLOR_BLACK);
				__bgcolor('s', COLOR_RED);
				__bgcolor('h', COLOR_GREEN);
				__bgcolor('z', COLOR_YELLOW);
				__bgcolor('e', COLOR_BLUE);
				__bgcolor('q', COLOR_MAGENTA);
				__bgcolor('d', COLOR_CYAN);
				__bgcolor('x', COLOR_WHITE);
				case 'n':
					bgcolor = COLOR_BLUE;
					fgcolor = COLOR_WHITE;
					bold = 0;
					break;
			}
			p++;

			wattrset(w, color_pair(fgcolor, bold, bgcolor));
			
			continue;
		}

		if (*p != '{' && !config_display_color)
			continue;

		p++;
		if (!*p)
			break;

		for (i = 0; data && data[i].name; i++) {
			int len;

			if (!data[i].text)
				continue;

			len = xstrlen(data[i].name);

			if (!strncmp(p, data[i].name, len) && p[len] == '}') {
				char *text = data[i].text;
				char *ftext = NULL;
                             	
				if (!config_display_pl_chars) {
                                	ftext = text = xstrdup(text);
                                	iso_to_ascii(text);
                              	}

				while (*text) {
					if (*text != '%') {
						waddch(w, (unsigned char) *text);
						*text++;	
						x++;
						continue;
					}
					*text++;
					
					if (!*text)	
						break;

		                        switch (*text) {
		                                __fgcolor('k', 'K', COLOR_BLACK);
                		                __fgcolor('r', 'R', COLOR_RED);
		                                __fgcolor('g', 'G', COLOR_GREEN);
                		                __fgcolor('y', 'Y', COLOR_YELLOW);
                                		__fgcolor('b', 'B', COLOR_BLUE);
		                                __fgcolor('m', 'M', COLOR_MAGENTA);
		                                __fgcolor('c', 'C', COLOR_CYAN);
		                                __fgcolor('w', 'W', COLOR_WHITE);
		                                __bgcolor('l', COLOR_BLACK);
		                                __bgcolor('s', COLOR_RED);
		                                __bgcolor('h', COLOR_GREEN);
		                                __bgcolor('z', COLOR_YELLOW);
		                                __bgcolor('e', COLOR_BLUE);
		                                __bgcolor('q', COLOR_MAGENTA);
		                                __bgcolor('d', COLOR_CYAN);
		                                __bgcolor('x', COLOR_WHITE);
		                                case 'n':
		                                        bgcolor = COLOR_BLUE;
		                                        fgcolor = COLOR_WHITE;
		                                        bold = 0;
		                                        break;
                		        }
					
					*text++;
		                        wattrset(w, color_pair(fgcolor, bold, bgcolor));
				}

//				waddstr(w, text);
				
				p += len;
				xfree(ftext);
				goto next;
			}
		}
#undef __fgcolor
#undef __bgcolor
		if (*p == '?') {
			int neg = 0;

			p++;
			if (!*p)
				break;

			if (*p == '!') {
				neg = 1;
				p++;
			}

			for (i = 0; data && data[i].name; i++) {
				int len, matched = ((data[i].text) ? 1 : 0);

				if (neg)
					matched = !matched;

				len = xstrlen(data[i].name);

				if (!strncmp(p, data[i].name, len) && p[len] == ' ') {
					p += len + 1;

					if (matched)
						x += window_printat(w, x, y, p, data, fgcolor, bold, bgcolor);
					goto next;
				}
			}

			goto next;
		}

next:
		/* uciekamy z naszego poziomu zagnie¿d¿enia */

		nest = 1;

		while (*p && nest) {
			if (*p == '}')
				nest--;
			if (*p == '{')
				nest++;
			p++;
		}
	}

	config_display_color = backup_display_color;

	if (!config_display_pl_chars)
		xfree(format);
	
	return x - orig_x;
}

/*
 * update_statusbar()
 *
 * uaktualnia pasek stanu i wy¶wietla go ponownie.
 *
 *  - commit - czy wy¶wietliæ od razu?
 */
void update_statusbar(int commit)
{
	userlist_t *q = userlist_find(window_current->session, window_current->target);
	struct format_data formats[32];	/* if someone add his own format increment it. */
	int formats_count = 0, i = 0, y;
	plugin_t *plug;
	session_t *sess = window_current->session;
	userlist_t *u;
	char *tmp;

	wattrset(ncurses_status, color_pair(COLOR_WHITE, 0, COLOR_BLUE));
	if (ncurses_header)
		wattrset(ncurses_header, color_pair(COLOR_WHITE, 0, COLOR_BLUE));

	/* inicjalizujemy wszystkie opisowe bzdurki */

	memset(&formats, 0, sizeof(formats));

#define __add_format(x, y, z) \
	{ \
		formats[formats_count].name = x; \
		formats[formats_count].text = (y) ? xstrdup(z) : NULL; \
		formats_count++; \
/* jak robimy memset(&formats, 0, sizeof(formats)); to po co to ? */\
	} 

	__add_format("time", 1, timestamp(format_find("statusbar_timestamp")));

	__add_format("window", window_current->id, itoa(window_current->id));
	__add_format("session", (sess), (sess->alias) ? sess->alias : sess->uid);
	__add_format("descr", (sess && sess->descr && session_connected_get(sess)), sess->descr);
	tmp = (sess && (u = userlist_find(sess, window_current->target))) ? saprintf("%s/%s", u->nickname, u->uid) : xstrdup(window_current->target);
	__add_format("query", tmp, tmp);
	xfree(tmp); 

	if ((plug = plugin_find(("mail")))) {
		int mail_count = -1;
		query_emit(plug, ("mail-count"), &mail_count);
		__add_format("mail", (mail_count > 0), itoa(mail_count));
	}
	if (session_check(window_current->session, 1, "irc") && (plug = plugin_find(("irc")))) {
		/* yeah, I know, shitty way */
		char *t2 = NULL;
		char *t3 = NULL; 
		query_emit(plug, ("irc-topic"), &tmp, &t2, &t3);
		__add_format("irctopic", tmp, tmp);
		__add_format("irctopicby", t2, t2);
		__add_format("ircmode", t3, t3);
		xfree(tmp);
		xfree(t2);
		xfree(t3);
	}

	{
		string_t s = string_init("");
		int first = 1, act = 0;
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;
			char *tmp;

			if (!w->act || !w->id) 
				continue;

			if (!first)
				string_append_c(s, ',');
		
			tmp = saprintf("statusbar_act%s", (w->act == 1) ? "" :  "_important");
			string_append(s, format_find(tmp));
			string_append(s, itoa(w->id));
			first = 0;
			act = 1;
			xfree(tmp);
		}
		
		__add_format("activity", (act), s->str);

		string_free(s, 1);
	}

	__add_format("debug", (!window_current->id), "");
	__add_format("away", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_AWAY)), "");
	__add_format("avail", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_AVAIL)), "");
        __add_format("dnd", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_DND)), "");
        __add_format("chat", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_FREE_FOR_CHAT)), "");
        __add_format("xa", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_XA)), "");
	__add_format("invisible", (sess && sess->connected && !xstrcasecmp(sess->status, EKG_STATUS_INVISIBLE)), "");
	__add_format("notavail", (!sess || !sess->connected || !xstrcasecmp(sess->status, EKG_STATUS_NA)), "");
	__add_format("more", (window_current->more), "");

	__add_format("query_descr", (q && q->descr), q->descr);
	__add_format("query_away", (q && !xstrcasecmp(q->status, EKG_STATUS_AWAY)), "");
	__add_format("query_avail", (q && !xstrcasecmp(q->status, EKG_STATUS_AVAIL)), "");
	__add_format("query_invisible", (q && !xstrcasecmp(q->status, EKG_STATUS_INVISIBLE)), "");
	__add_format("query_notavail", (q && !xstrcasecmp(q->status, EKG_STATUS_NA)), "");
	__add_format("query_dnd", (q && !xstrcasecmp(q->status, EKG_STATUS_DND)), "");
	__add_format("query_chat", (q && !xstrcasecmp(q->status, EKG_STATUS_FREE_FOR_CHAT)), "");
	__add_format("query_xa", (q && !xstrcasecmp(q->status, EKG_STATUS_XA)), "");
	__add_format("query_ip", (q && q->ip), inet_ntoa(*((struct in_addr*)(&q->ip)))); 

	__add_format("url", 1, "http://www.ekg2.org/");
	__add_format("version", 1, VERSION);

#undef __add_format

	for (y = 0; y < config_header_size; y++) {
		const char *p;

		if (!y) {
			p = format_find("header1");

			if (!xstrcmp(p, ""))
				p = format_find("header");
		} else {
			char *tmp = saprintf("header%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		window_printat(ncurses_header, 0, y, p, formats, COLOR_WHITE, 0, COLOR_BLUE);
	}

	for (y = 0; y < config_statusbar_size; y++) {
		const char *p;

		if (!y) {
			p = format_find("statusbar1");

			if (!xstrcmp(p, ""))
				p = format_find("statusbar");
		} else {
			char *tmp = saprintf("statusbar%d", y + 1);
			p = format_find(tmp);
			xfree(tmp);
		}

		switch (ncurses_debug) {
			case 0:
				window_printat(ncurses_status, 0, y, p, formats, COLOR_WHITE, 0, COLOR_BLUE);
				break;
				
			case 1:
			{
				char *tmp = saprintf(" debug: lines_count=%d start=%d height=%d overflow=%d screen_width=%d", ncurses_current->lines_count, ncurses_current->start, window_current->height, ncurses_current->overflow, ncurses_screen_width);
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE);
				xfree(tmp);
				break;
			}

			case 2:
			{
				char *tmp = saprintf(" debug: lines(count=%d,start=%d,index=%d), line(start=%d,index=%d)", array_count((char **) ncurses_lines), lines_start, lines_index, line_start, line_index);
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE);
				xfree(tmp);
				break;
			}

			case 3:
			{
				session_t *s = window_current->session;
				char *tmp = saprintf(" debug: session=%p uid=%s alias=%s / target=%s session_current->uid=%s", s, (s && s->uid) ? s->uid : "", (s && s->alias) ? s->alias : "", (window_current->target) ? window_current->target : "", (session_current && session_current->uid) ? session_current->uid : "");
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE);
				xfree(tmp);
				break;
			}
		}
	}

	for (i = 0; i < formats_count; i++)
		xfree(formats[i].text);

	query_emit(NULL, ("ui-redrawing-header"));
	query_emit(NULL, ("ui-redrawing-statusbar"));
	
	if (commit)
		ncurses_commit();
}

/*
 * ncurses_window_kill()
 *
 * usuwa podane okno.
 */
int ncurses_window_kill(window_t *w)
{
	ncurses_window_t *n = w->private;

	if (!n) 
		return -1;

	if (n->backlog) {
		int i;

		for (i = 0; i < n->backlog_size; i++)
			fstring_free((fstring_t *) n->backlog[i]);

		xfree(n->backlog);
	}

	if (n->lines) {
		int i;

		for (i = 0; i < n->lines_count; i++) {
			xfree(n->lines[i].ts);
			xfree(n->lines[i].ts_attr);
		}
		
		xfree(n->lines);
	}
		
	xfree(n->prompt);
	n->prompt = NULL;
	delwin(n->window);
	n->window = NULL;
	xfree(n);
	w->private = NULL;

//	ncurses_resize();

	return 0;
}

#ifdef SIGWINCH
static void sigwinch_handler()
{
	signal(SIGWINCH, sigwinch_handler);
	if (have_winch_pipe) {
		char c = ' ';
		write(winch_pipe[1], &c, 1);
	}
}
#endif

/*
 * ncurses_init()
 *
 * inicjalizuje ca³± zabawê z ncurses.
 */
void ncurses_init()
{
	int background = COLOR_BLACK;

	ncurses_screen_width = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	ncurses_screen_height = getenv("LINES") ? atoi(getenv("LINES")) : 24;

	initscr();
	cbreak();
	noecho();
	nonl();
	
	if (config_display_transparent) {
		background = COLOR_DEFAULT;
		use_default_colors();
	}

	ncurses_screen_width = stdscr->_maxx + 1;
	ncurses_screen_height = stdscr->_maxy + 1;
	
	ncurses_status = newwin(1, stdscr->_maxx + 1, stdscr->_maxy - 1, 0);
	input = newwin(1, stdscr->_maxx + 1, stdscr->_maxy, 0);
	keypad(input, TRUE);
	nodelay(input, TRUE);

	start_color();

	init_pair(7, COLOR_BLACK, background);	/* ma³e obej¶cie domy¶lnego koloru */
	init_pair(1, COLOR_RED, background);
	init_pair(2, COLOR_GREEN, background);
	init_pair(3, COLOR_YELLOW, background);
	init_pair(4, COLOR_BLUE, background);
	init_pair(5, COLOR_MAGENTA, background);
	init_pair(6, COLOR_CYAN, background);

#define __init_bg(x, y) \
	init_pair(x, COLOR_BLACK, y); \
	init_pair(x + 1, COLOR_RED, y); \
	init_pair(x + 2, COLOR_GREEN, y); \
	init_pair(x + 3, COLOR_YELLOW, y); \
	init_pair(x + 4, COLOR_BLUE, y); \
	init_pair(x + 5, COLOR_MAGENTA, y); \
	init_pair(x + 6, COLOR_CYAN, y); \
	init_pair(x + 7, COLOR_WHITE, y);

	__init_bg(8, COLOR_RED);
	__init_bg(16, COLOR_GREEN);
	__init_bg(24, COLOR_YELLOW);
	__init_bg(32, COLOR_BLUE);
	__init_bg(40, COLOR_MAGENTA);
	__init_bg(48, COLOR_CYAN);
	__init_bg(56, COLOR_WHITE);

#undef __init_bg

	ncurses_contacts_changed("contacts", NULL);
	ncurses_commit();

	/* deaktywujemy klawisze INTR, QUIT, SUSP i DSUSP */
	if (!tcgetattr(0, &old_tio)) {
		struct termios tio;

		memcpy(&tio, &old_tio, sizeof(tio));
		tio.c_cc[VINTR] = _POSIX_VDISABLE;
		tio.c_cc[VQUIT] = _POSIX_VDISABLE;
#ifdef VDSUSP
		tio.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
#ifdef VSUSP
		tio.c_cc[VSUSP] = _POSIX_VDISABLE;
#endif

		tcsetattr(0, TCSADRAIN, &tio);
	}

#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#endif

	memset(ncurses_history, 0, sizeof(ncurses_history));

	ncurses_binding_init();

#ifdef WITH_ASPELL
	if (config_aspell)
		ncurses_spellcheck_init();
#endif

	ncurses_line = xmalloc(LINE_MAXLEN * sizeof(CHAR_T));

	ncurses_history[0] = ncurses_line;
}

/*
 * ncurses_deinit()
 *
 * zamyka, robi porz±dki.
 */
void ncurses_deinit()
{
	static int done = 0;
	list_t l;
	int i;

#ifdef SIGWINCH
	signal(SIGWINCH, SIG_DFL);
#endif
	if (have_winch_pipe) {
		close(winch_pipe[0]);
		close(winch_pipe[1]);
	}

	for (l = windows; l; ) {
		window_t *w = l->data;

		l = l->next;

		ncurses_window_kill(w);
	}

	tcsetattr(0, TCSADRAIN, &old_tio);

	keypad(input, FALSE);

	werase(input);
	wnoutrefresh(input);
	doupdate();

	delwin(input);
	delwin(ncurses_status);
	if (ncurses_header)
		delwin(ncurses_header);
	endwin();

	for (i = 0; i < HISTORY_MAX; i++)
		if (ncurses_history[i] != ncurses_line) {
			xfree(ncurses_history[i]);
			ncurses_history[i] = NULL;
		}

	if (ncurses_lines) {
		for (i = 0; ncurses_lines[i]; i++) {
			if (ncurses_lines[i] != ncurses_line)
				xfree(ncurses_lines[i]);
			ncurses_lines[i] = NULL;
		}

		xfree(ncurses_lines);
		ncurses_lines = NULL;
	}

#ifdef WITH_ASPELL
        delete_aspell_speller(spell_checker);
#endif

	xfree(ncurses_line);
	xfree(ncurses_yanked);

	done = 1;
}

/*
 * line_adjust()
 *
 * ustawia kursor w odpowiednim miejscu ekranu po zmianie tekstu w poziomie.
 */
void ncurses_line_adjust()
{
	int prompt_len = (ncurses_lines) ? 0 : ncurses_current->prompt_len;

	line_index = xwcslen(ncurses_line);
	if (line_index < input->_maxx - 9 - prompt_len)
		line_start = 0;
	else
		line_start = line_index - line_index % (input->_maxx - 9 - prompt_len);
}

/*
 * lines_adjust()
 *
 * poprawia kursor po przesuwaniu go w pionie.
 */
void ncurses_lines_adjust() {
	size_t linelen;
	if (lines_index < lines_start)
		lines_start = lines_index;

	if (lines_index - 4 > lines_start)
		lines_start = lines_index - 4;

	ncurses_line = ncurses_lines[lines_index];

	linelen = xwcslen(ncurses_line);
	if (line_index > linelen)	
		line_index = linelen;
}

/*
 * ncurses_input_update()
 *
 * uaktualnia zmianê rozmiaru pola wpisywania tekstu -- przesuwa okienka
 * itd. je¶li zmieniono na pojedyncze, czy¶ci dane wej¶ciowe.
 */
void ncurses_input_update()
{
	if (ncurses_input_size == 1) {
		int i;
		
		for (i = 0; ncurses_lines[i]; i++)
			xfree(ncurses_lines[i]);
		xfree(ncurses_lines);
		ncurses_lines = NULL;
		ncurses_line = xmalloc(LINE_MAXLEN*sizeof(CHAR_T));

		ncurses_history[0] = ncurses_line;

		line_start = 0;
		line_index = 0; 
		lines_start = 0;
		lines_index = 0;
	} else {
		ncurses_lines = xmalloc(2 * sizeof(CHAR_T *));
		ncurses_lines[0] = xmalloc(LINE_MAXLEN*sizeof(CHAR_T));
/*		ncurses_lines[1] = NULL; */
		xwcscpy(ncurses_lines[0], ncurses_line);
		xfree(ncurses_line);
		ncurses_line = ncurses_lines[0];
		ncurses_history[0] = NULL;
		lines_start = 0;
		lines_index = 0;
	}
	
	ncurses_resize();

	ncurses_redraw(window_current);
	touchwin(ncurses_current->window);

	ncurses_commit();
}

/*
 * print_char()
 *
 * wy¶wietla w danym okienku znak, bior±c pod uwagê znaki ,,niewy¶wietlalne''.
 * 	gdy attr A_UNDERLINE wtedy podkreslony
 */
static void print_char(WINDOW *w, int y, int x, CHAR_T ch, int attr)
{
	wattrset(w, attr);

	if (ch < 32) {
		wattrset(w, A_REVERSE | attr);
		ch += 64;
	}

	if (ch >= 128 && ch < 160) {
		ch = '?';
		wattrset(w, A_REVERSE | attr);
	}
#if USE_UNICODE
	if (config_use_unicode)
		mvwaddnwstr(w, y, x, &ch, 1);
	else
#endif
		mvwaddch(w, y, x, ch);
	wattrset(w, A_NORMAL);
}

/* 
 * ekg_getch()
 *
 * czeka na wci¶niêcie klawisza i je¶li wkompilowano obs³ugê pythona,
 * przekazuje informacjê o zdarzeniu do skryptu.
 *
 *  - meta - przedrostek klawisza.
 *
 * zwraca kod klawisza lub -2, je¶li nale¿y go pomin±æ.
 */
static int ekg_getch(int meta, unsigned int *ch) {
#if USE_UNICODE
	int retcode;
	if (config_use_unicode) {
		retcode = wget_wch(input, ch);
		if (retcode == ERR) *ch = ERR;
	} else retcode = 
#endif
		*ch = wgetch(input);

	/* 
	 * conception is borrowed from Midnight Commander project 
	 *    (www.ibiblio.org/mc/) 
	 */	
#define GET_TIME(tv)    (gettimeofday(&tv, (struct timezone *)NULL))
#define DIF_TIME(t1,t2) ((t2.tv_sec -t1.tv_sec) *1000+ \
                         (t2.tv_usec-t1.tv_usec)/1000)
	if (*ch == KEY_MOUSE) {
		int btn, mouse_state = 0, x, y;
		static struct timeval tv1 = { 0, 0 }; 
		static struct timeval tv2;
		static int clicks;
		static int last_btn = 0;

		btn = wgetch (input) - 32;
    
		if (btn == 3 && last_btn) {
			last_btn -= 32;

			switch (last_btn) {
                                case 0: mouse_state = (clicks) ? EKG_BUTTON1_DOUBLE_CLICKED : EKG_BUTTON1_CLICKED;	break;
                                case 1: mouse_state = (clicks) ? EKG_BUTTON2_DOUBLE_CLICKED : EKG_BUTTON2_CLICKED;	break;
                                case 2: mouse_state = (clicks) ? EKG_BUTTON3_DOUBLE_CLICKED : EKG_BUTTON3_CLICKED;	break;
				default:										break;
			}

	 		last_btn = 0;
			GET_TIME (tv1);
			clicks = 0;

    		} else if (!last_btn) {
			GET_TIME (tv2);
			if (tv1.tv_sec && (DIF_TIME (tv1,tv2) < 250)){
				clicks++;
	    			clicks %= 3;
			} else
	    			clicks = 0;
	
			switch (btn) {
				case 0:
					btn += 32;
					break;
				case 1:
					btn += 32;
					break;
				case 2:
					btn += 32;
					break;
                                case 64:
                                        btn += 32;
                                        break;
                                case 65:
                                        btn += 32;
                                        break;
				default:
					btn = 0;
					break;
			}

			last_btn = btn;
		} else {
			switch (btn) {
				case 64:
					mouse_state = EKG_SCROLLED_UP;
					break;
				case 65:
					mouse_state = EKG_SCROLLED_DOWN;
					break;
			}
		}
		
		/* 33 based */
                x = wgetch(input) - 32; 
		y = wgetch(input) - 32;

		if (mouse_state)
			ncurses_mouse_clicked_handler(x, y, mouse_state);
	} 
#undef GET_TIME
#undef DIF_TIME
	if (query_emit(NULL, ("ui-keypress"), ch, NULL) == -1)  
		return -2; /* -2 - ignore that key */
#if USE_UNICODE
	if (retcode == KEY_CODE_YES) return KEY_CODE_YES;
#endif
	return *ch;
}

/* XXX: deklaracja ncurses_watch_stdin nie zgadza sie ze
 * sposobem wywolywania watchow.
 * todo brzmi: dorobic do tego jakis typ (typedef watch_handler_t),
 * zeby nie bylo niejasnosci
 * (mp)
 *
 * I've changed the declaration of ncurses_watch_stdin,
 * and added if (last) return; but I'm not sure how it is supposed to work...
 *
 */
WATCHER(ncurses_watch_winch)
{
	char c;
	if (type) return 0;
	read(winch_pipe[0], &c, 1);

	/* skopiowalem ponizsze z ncurses_watch_stdin.
	 * problem polegal na tym, ze select czeka na system, a ungetc
	 * ungetuje w bufor libca.
	 * (mp)
	 */
	endwin();
	refresh();
	keypad(input, TRUE);
	/* wywo³a wszystko, co potrzebne */
	header_statusbar_resize();
	changed_backlog_size(("backlog_size"));
	return 0;
}

/* 
 * spellcheck()
 *
 * it checks if the given word is correct
 */
#ifdef WITH_ASPELL
static void spellcheck(CHAR_T *what, char *where) {
        char *word;             /* aktualny wyraz */
	register int i = 0;     /* licznik */
	register int j = 0;     /* licznik */

	/* Sprawdzamy czy nie mamy doczynienia z 47 (wtedy nie sprawdzamy reszty ) */
	if (!what || __S(what, 0) == 47)
		return;       /* konczymy funkcje */
	    
	for (i = 0; __S(what, i) != '\0' && __S(what, i) != '\n' && __S(what, i) != '\r'; i++) {
		if ((!isalpha_pl(__S(what, i)) || i == 0 ) && __S(what, i+1) != '\0' ) { // separator/koniec lini/koniec stringu
			word = xmalloc((xwcslen(what) + 1)*sizeof(char));
			size_t wordlen;		/* len of word */
		
			for (; __S(what, i) != '\0' && __S(what, i) != '\n' && __S(what, i) != '\r'; i++) {
				if (isalpha_pl(__S(what, i))) /* szukamy jakiejs pierwszej literki */
					break; 
			}

			/* trochê poprawiona wydajno¶æ */
			if (__S(what, i) == '\0' || __S(what, i) == '\n' || __S(what, i) == '\r') {
				i--;
				goto aspell_loop_end; /* 
						       * nie powinno siê u¿ywaæ goto, aczkolwiek s± du¿o szybsze
						       * ni¿ instrukcje warunkowe i w tym przypadku nie psuj± bardzo
						       * czytelno¶ci kodu
						       */
				/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu www */ 
			} else if (
							    __S(what, i) == 'h' && 
					__S(what, i + 1) && __S(what, i + 1) == 't' && 
					__S(what, i + 2) && __S(what, i + 2) == 't' && 
					__S(what, i + 3) && __S(what, i + 3) == 'p' && 
					__S(what, i + 4) && __S(what, i + 4) == ':' &&
					__S(what, i + 5) && __S(what, i + 5) == '/' && 
					__S(what, i + 6) && __S(what, i + 6) == '/') {

				for(; __S(what, i) != ' ' && __S(what, i) != '\n' && __S(what, i) != '\r' && __S(what, i) != '\0'; i++);
				i--;
				goto aspell_loop_end;

				/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu ftp */ 
			} else if (
							    __S(what, i) == 'f' && 
					__S(what, i + 1) && __S(what, i + 1) == 't' && 
					__S(what, i + 2) && __S(what, i + 2) == 'p' && 
					__S(what, i + 3) && __S(what, i + 3) == ':' && 
					__S(what, i + 4) && __S(what, i + 4) == '/' && 
					__S(what, i + 5) && __S(what, i + 5) == '/') {

				for(; __S(what, i) != ' ' && __S(what, i) != '\n' && __S(what, i) != '\r' && __S(what, i) != '\0'; i++);
				i--;
				goto aspell_loop_end;
			}

			/* wrzucamy aktualny wyraz do zmiennej word */		    
			for (j = 0; __S(what, i) != '\n' && __S(what, i) != '\0' && isalpha_pl(__S(what, i)); i++) {
				if (isalpha_pl(__S(what, i))) {
					word[j]= __S(what, i);
					j++;
				} else 
					break;
			}
			word[j] = (char) 0;
			if (i > 0)
				i--;

/*			debug(GG_DEBUG_MISC, "Word: %s\n", word);  */
			wordlen = j;			/* xstrlen(word) */

			/* sprawdzamy pisownie tego wyrazu */
			if (aspell_speller_check(spell_checker, word, xstrlen(word) ) == 0) { /* jesli wyraz jest napisany blednie */
				for (j = wordlen - 1; j >= 0; j--)
					where[i - j] = ASPELLCHAR;
			} else { /* jesli wyraz jest napisany poprawnie */
				for (j = wordlen - 1; j >= 0; j--)
					where[i - j] = ' ';
			}
aspell_loop_end:
			xfree(word);
		}	
	}
}
#endif

extern volatile int sigint_count;
/*
 * ncurses_watch_stdin()
 *
 * g³ówna pêtla interfejsu.
 */
WATCHER(ncurses_watch_stdin)
{
	struct binding *b = NULL;
	unsigned int tmp;
	unsigned int ch;
#ifdef WITH_ASPELL
	char *aspell_line = NULL;
	int mispelling = 0; /* zmienna pomocnicza */
#endif

	/* GiM: I'm not sure if this should be like that
	 * deletek you should take a look at this.
	 */
	if (type)
		return 0;

	switch ((tmp = ekg_getch(0, &ch))) {
		case(-1):	/* dziwna kombinacja, która by blokowa³a */
		case(-2):	/* przeszlo przez query_emit i mamy zignorowac (pytthon, perl) */
		case(0):	/* Ctrl-Space, g³upie to */
			return 0;
		case(3):
		default:
			if (ch != 3) sigint_count = 0;
	}
	ekg_stdin_want_more = 1;

	if (bindings_added && ch != KEY_MOUSE) {
		char **chars = NULL, *joined;
		int i = 0, count = 0, success = 0;
		list_t l;
		int c;
		array_add(&chars, xstrdup(itoa(ch)));

        	while (count <= bindings_added_max && 
				(c = wgetch(input)) != ERR
				) {
	                array_add(&chars, xstrdup(itoa(c)));
			count++;
	        }

		joined = array_join(chars, (" "));

		for (l = bindings_added; l; l = l->next) {
			binding_added_t *d = l->data;

			if (!xstrcasecmp(d->sequence, joined)) {
				struct binding *b = d->binding;

	                        if (b->function)
	                                b->function(b->arg);
	                        else {
	                                command_exec_format(window_current->target, window_current->session, 0, ("%s%s"), 
							((b->action[0] == '/') ? "" : "/"), b->action);
	                        }

				success = 1;
				goto end;
			}
		}

                for (i = count; i > 0; i--) {
                        ungetch(atoi(chars[i]));
                }

end:
		xfree(joined);
		array_free(chars);
		if (success)
			goto then;
	} 

	if (ch == 27) {
		if ((ekg_getch(27, &ch)) == -2)
			return 0;

                b = ncurses_binding_map_meta[ch];
		
		if (ch == 27)
			b = ncurses_binding_map[27];

		/* je¶li dostali¶my \033O to albo mamy Alt-O, albo
		 * pokaleczone klawisze funkcyjne (\033OP do \033OS).
		 * ogólnie rzecz bior±c, nieciekawa sytuacja ;) */

		if (ch == 'O') {
			unsigned int tmp;
			if ((ekg_getch(ch, &tmp)) > -1) {
				if (tmp >= 'P' && tmp <= 'S')
					b = ncurses_binding_map[KEY_F(tmp - 'P' + 1)];
				else if (tmp == 'H')
					b = ncurses_binding_map[KEY_HOME];
				else if (tmp == 'F')
					b = ncurses_binding_map[KEY_END];
				else if (tmp == 'M')
					b = ncurses_binding_map[13];
				else
					ungetch(tmp);
			}
		}

		if (b && b->action) {
			if (b->function)
				b->function(b->arg);
			else {
				command_exec_format(window_current->target, window_current->session, 0,
						("%s%s"), ((b->action[0] == '/') ? "" : "/"), b->action
						);
			}
		} else {
			/* obs³uga Ctrl-F1 - Ctrl-F12 na FreeBSD */
			if (ch == '[') {
				ch = wgetch(input);

				if (ch == '4' && wgetch(input) == '~' && ncurses_binding_map[KEY_END])
					ncurses_binding_map[KEY_END]->function(NULL);

				if (ch >= 107 && ch <= 118)
					window_switch(ch - 106);
			}
		}
	} else {
		if (
#if USE_UNICODE
			( (config_use_unicode && (tmp == KEY_CODE_YES || ch < 0x100 /* TODO CHECK */)) || !config_use_unicode) &&
#endif
			(b = ncurses_binding_map[ch]) && b->action) {

			if (b->function)
				b->function(b->arg);
			else {
				command_exec_format(window_current->target, window_current->session, 0,
						("%s%s"), ((b->action[0] == '/') ? "" : "/"), b->action
						);
			}
		} else if (
#if USE_UNICODE
				((config_use_unicode && ch != KEY_MOUSE) || (!config_use_unicode && ch < 255)) &&
#else
				ch < 255 && 
#endif
				xwcslen(ncurses_line) < LINE_MAXLEN - 1) {

					/* move &ncurses_line[index_line] to &ncurses_line[index_line+1] */
			memmove(__SPTR(ncurses_line, line_index + 1), __SPTR(ncurses_line, line_index), sizeofchart*LINE_MAXLEN - sizeofchart*line_index - sizeofchart);
					/* put in ncurses_line[lindex_index] current char */
			__SREP(ncurses_line, line_index++, ch);		/* ncurses_line[line_index++] = ch */
		}
	}
then:
	if (ncurses_plugin_destroyed)
		return 0; /* -1 */

	/* je¶li siê co¶ zmieni³o, wygeneruj dope³nienia na nowo */
	if (!b || (b && b->function != ncurses_binding_complete))
		ncurses_complete_clear();

	if (line_index - line_start > input->_maxx - 9 - ncurses_current->prompt_len)
		line_start += input->_maxx - 19 - ncurses_current->prompt_len;
	if (line_index - line_start < 10) {
		line_start -= input->_maxx - 19 - ncurses_current->prompt_len;
		if (line_start < 0)
			line_start = 0;
	}

	werase(input);
	wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));

	if (ncurses_lines) {
		int i;
		
		for (i = 0; i < 5; i++) {
			CHAR_T *p;
			int j;
			size_t plen;

			if (!ncurses_lines[lines_start + i])
				break;

			p = ncurses_lines[lines_start + i];
			plen = xwcslen(p);
#ifdef WITH_ASPELL
			if (spell_checker) {
				aspell_line = xmalloc(plen);
				if (line_start == 0) 
					mispelling = 0;
					    
				spellcheck(p, aspell_line);
	                }
#endif
			for (j = 0; j + line_start < plen && j < input->_maxx + 1; j++) { 
#ifdef WITH_ASPELL
				if (aspell_line && aspell_line[line_start + j] == ASPELLCHAR && __S(p, line_start + j) != ' ') /* jesli b³êdny to wy¶wietlamy podkre¶lony */
					print_char(input, i, j, __S(p, line_start + j), A_UNDERLINE);
				else	/* jesli jest wszystko okey to wyswietlamy normalny */
#endif					/* lub nie mamy aspella */
					print_char(input, i, j, __S(p, j + line_start), A_NORMAL);
			}
#ifdef WITH_ASPELL
			xfree(aspell_line);
#endif
		}
		wmove(input, lines_index - lines_start, line_index - line_start);
	} else {
		int i;
		/* const */size_t linelen = xwcslen(ncurses_line);

		if (ncurses_current->prompt)
			mvwaddstr(input, 0, 0, ncurses_current->prompt);
#ifdef WITH_ASPELL		
		if (spell_checker) {
			aspell_line = xmalloc(linelen + 1);
			if (line_start == 0) 
				mispelling = 0;
	
			spellcheck(ncurses_line, aspell_line);
		}
#endif
                for (i = 0; i < input->_maxx + 1 - ncurses_current->prompt_len && i < linelen - line_start; i++) {
#ifdef WITH_ASPELL
			if (spell_checker && aspell_line[line_start + i] == ASPELLCHAR && __S(ncurses_line, line_start + i) != ' ') /* jesli b³êdny to wy¶wietlamy podkre¶lony */
                        	print_char(input, 0, i + ncurses_current->prompt_len, __S(ncurses_line, line_start + i), A_UNDERLINE);
                        else 	/* jesli jest wszystko okey to wyswietlamy normalny */
#endif				/* lub gdy nie mamy aspella */
                                print_char(input, 0, i + ncurses_current->prompt_len, __S(ncurses_line, line_start + i), A_NORMAL);
		}
#ifdef WITH_ASPELL
		xfree(aspell_line);
#endif
		/* this mut be here if we don't want 'timeout' after pressing ^C */
		if (ch == 3) ncurses_commit();
		wattrset(input, color_pair(COLOR_BLACK, 1, COLOR_BLACK));
		if (line_start > 0)
			mvwaddch(input, 0, ncurses_current->prompt_len, '<');
		if (linelen - line_start > input->_maxx + 1 - ncurses_current->prompt_len)
			mvwaddch(input, 0, input->_maxx, '>');
		wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));
		wmove(input, 0, line_index - line_start + ncurses_current->prompt_len);
	}
	return 0;
}

/*
 * header_statusbar_resize()
 *
 * zmienia rozmiar paska stanu i/lub nag³ówka okna.
 */
void header_statusbar_resize()
{
	if (!ncurses_status)
		return;
	
	if (config_header_size < 0)
		config_header_size = 0;

	if (config_header_size > 5)
		config_header_size = 5;

	if (config_statusbar_size < 1)
		config_statusbar_size = 1;

	if (config_statusbar_size > 5)
		config_statusbar_size = 5;

	if (config_header_size) {
		if (!header)
			header = newwin(config_header_size, stdscr->_maxx + 1, 0, 0);
		else
			wresize(header, config_header_size, stdscr->_maxx + 1);

		update_header(0);
	}

	if (!config_header_size && header) {
		delwin(header);
		header = NULL;
	}

	ncurses_resize();

	wresize(ncurses_status, config_statusbar_size, stdscr->_maxx + 1);
	mvwin(ncurses_status, stdscr->_maxy + 1 - ncurses_input_size - config_statusbar_size, 0);

	update_statusbar(0);

	ncurses_commit();
}

/*
 * changed_backlog_size()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,backlog_size''.
 */
void changed_backlog_size(const char *var)
{
	list_t l;

	if (config_backlog_size < ncurses_screen_height)
		config_backlog_size = ncurses_screen_height;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;
		int i;
				
		if (n->backlog_size <= config_backlog_size)
			continue;
				
		for (i = config_backlog_size; i < n->backlog_size; i++)
			fstring_free((fstring_t *) n->backlog[i]);

		n->backlog_size = config_backlog_size;
		n->backlog = xrealloc(n->backlog, n->backlog_size * sizeof(ncurses_fstring_t *));

		ncurses_backlog_split(w, 1, 0);
	}
}

/*
 * ncurses_window_new()
 *
 * tworzy nowe okno ncurses do istniej±cego okna ekg.
 */
int ncurses_window_new(window_t *w)
{
	ncurses_window_t *n;

	if (w->private)
		return 0;

	w->private = n = xmalloc(sizeof(ncurses_window_t));

	if (!xstrcmp(w->target, "__contacts"))
		ncurses_contacts_new(w);

	if (w->target) {
		const char *f = format_find("ncurses_prompt_query");

		n->prompt = format_string(f, w->target);
		n->prompt_len = xstrlen(n->prompt);
	} else {
		const char *f = format_find("ncurses_prompt_none");

		if (xstrcmp(f, "")) {
			n->prompt = format_string(f);
			n->prompt_len = xstrlen(n->prompt);
		}
	}

 	n->window = newwin(w->height, w->width, w->top, w->left);

	ncurses_resize();

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
