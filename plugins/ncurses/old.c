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
#ifdef HAVE_REGEX_H
#	include <sys/types.h>
#	include <regex.h>
#endif

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
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

#include <ekg/queries.h>

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

int ncurses_noecho = 0;
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

/* typing */
int ncurses_typing_mod			= 0;	/* whether buffer was modified */
static int ncurses_typing_count		= 0;	/* last count sent */
window_t *ncurses_typing_win		= NULL;	/* last window for which typing was sent */
static time_t ncurses_typing_time	= 0;	/* time at which last typing was sent */

static char ncurses_funnything[5] = "|/-\\";

CHAR_T *ncurses_passbuf;

QUERY(ncurses_password_input) {
	char **buf		= va_arg(ap, char**);
	char *oldprompt;
	CHAR_T *oldline, *passa, *passb = NULL;
	CHAR_T **oldlines;
	int oldpromptlen;
	
	*buf				= NULL;
	ncurses_noecho			= 1;
	oldprompt			= ncurses_current->prompt;
	oldpromptlen			= ncurses_current->prompt_len;
	oldline				= ncurses_line;
	oldlines			= ncurses_lines;
	ncurses_current->prompt		= (char*) format_find("password_input");
	ncurses_current->prompt_len 	= xstrlen(ncurses_current->prompt);
	ncurses_update_real_prompt(ncurses_current);
	ncurses_lines			= NULL;
	ncurses_line			= xmalloc(LINE_MAXLEN * sizeof(CHAR_T));
	ncurses_line_adjust();
	ncurses_redraw_input(0);
	
	while (ncurses_noecho)
		ncurses_watch_stdin(0, 0, WATCH_READ, NULL);
	passa = ncurses_passbuf;
	
	if (xwcslen(passa)) {
		ncurses_current->prompt		= (char*) format_find("password_repeat");
		ncurses_current->prompt_len 	= xstrlen(ncurses_current->prompt);
		ncurses_noecho			= 1;
		ncurses_update_real_prompt(ncurses_current);
		ncurses_redraw_input(0);
		
		while (ncurses_noecho)
			ncurses_watch_stdin(0, 0, WATCH_READ, NULL);
		passb = ncurses_passbuf;
		
		if (xwcscmp(passa, passb))
			print("password_nomatch");
		else
#if USE_UNICODE
			*buf = wcs_to_normal(passa);
#else
			*buf = xstrdup(passa);
#endif
	} else
		print("password_empty");
	
	xfree(ncurses_line);
	ncurses_passbuf			= NULL;
	ncurses_line			= oldline;
	ncurses_lines			= oldlines;
	ncurses_current->prompt		= oldprompt;
	ncurses_current->prompt_len	= oldpromptlen;
	ncurses_update_real_prompt(ncurses_current);
	xfree(passa);
	xfree(passb);
	
	return -1;
}
int ncurses_lineslen() {
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

inline int ncurses_typingsend(const int len, const int first) {
	const char *sid	= session_uid_get(ncurses_typing_win->session);
	const char *uid	= get_uid(ncurses_typing_win->session, ncurses_typing_win->target);
	
	if (((first > 1) || (ncurses_typing_win->act & 8)) && uid)
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
	} else if (w->act & 24) { /* <gone/> or <active/> */
		window_t *tmp		= ncurses_typing_win;
		ncurses_typing_win	= w;

		if (!ncurses_typingsend(0, !(w->act & 16) ? 4 : 5) || (w->act & 16))
			w->act		^= 16;

		ncurses_typing_win	= tmp;
	}
}

	/* this one is meant to check whether we need to send some chatstate to disconnecting session,
	 * so jabber plugin doesn't need to care about this anymore */
QUERY(ncurses_session_disconnect_handler) {
	const char	*session	= *va_arg(ap, const char **);
	const session_t	*s		= session_find(session);
	list_t		l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (!w || w->session != s)
			continue;

		ncurses_window_gone(w);
	}

	return 0;
}

/* cut prompt to given width and recalculate its' width */
void ncurses_update_real_prompt(ncurses_window_t *n) {
	if (!n)
		return;

	const int maxlen = (n->window && n->window->_maxx ? n->window->_maxx : 80) / 3;
	xfree(n->prompt_real);

	if (maxlen <= 6) /* we assume the terminal is too narrow to display any input with prompt */
		n->prompt_real		= NULL;
	else {
#ifdef USE_UNICODE
		n->prompt_real		= normal_to_wcs(n->prompt);
#else
		n->prompt_real		= xstrdup(n->prompt);
#endif
	}
	n->prompt_real_len		= xwcslen(n->prompt_real);

	if (n->prompt_real_len > maxlen) { /* need to cut it */
		const CHAR_T *dots	= TEXT("...");
#ifdef USE_UNICODE
		const wchar_t udots[2]	= { 0x2026, 0 };
		if (config_use_unicode)	/* use unicode hellip, if using utf8 */
			dots		= udots;
#endif

		{
			const int dotslen	= xwcslen(dots);
			const int taillen	= (maxlen - dotslen) / 2; /* rounded down */
			const int headlen	= (maxlen - dotslen) - taillen; /* rounded up */

			CHAR_T *tmp		= xmalloc(sizeof(CHAR_T) * (maxlen + 1));

			xwcslcpy(tmp, n->prompt_real, headlen + 1);
			xwcslcpy(tmp + headlen, dots, dotslen + 1);
			xwcslcpy(tmp + headlen + dotslen, n->prompt_real + n->prompt_real_len - taillen, taillen + 1);

			xfree(n->prompt_real);
			n->prompt_real		= tmp;
			n->prompt_real_len	= maxlen;
		}
	}
}

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
#if USE_UNICODE
	{
		int rlen = xstrlen(str->str.b);
		wchar_t *temp = xmalloc((rlen + 1) * sizeof(CHAR_T));		/* new str->str */

		int cur = 0;
		int i = 0;

		mbtowc(NULL, NULL, 0);	/* reset */

		while (cur <= rlen) {
			wchar_t znak;
			int inv = 0;
			int len	= mbtowc(&znak, &(str->str.b[cur]), rlen-cur);

			/* BIG XXX:
			 * 	years ago, when I was writting support for unicode in ekg2, 
			 * 	I forgot to implement support for:
			 * 	  str->prompt_len && str->margin_left
			 *
			 * 	Bug was hidden, untill dmilith created theme with %| with utf-8 char :)
			 *
			 * 	Offsets isn't corrected here, so we have something like it: http://wafel.com/~darkjames/ekg2-prompt_len_bug.png
			 * 	(NOTE: 'AL' is beginning of message)
			 *
			 * 	Currently I'm not using unicode environment (honesty i never had :)) so I won't fix it.
			 *
			 * 	I don't even understand this code: http://geekandpoke.typepad.com/geekandpoke/2008/01/one-year-in-a-i.html (like always)
			 *	So again, I won't fix it. If someone want to fix it, feel free to apply && commit/send (if you're not developer) patch.
			 *
			 *	Workaround: 
			 *		Don't use utf-8 chars as prompt.
			 *
			 *	darkjames (@ 2oo8/ Feb/ o3)
			 */

			if (len == -1) {
/*				debug("[%s:%d] mbtowc() failed ?! (%d, %s) (%d)\n", __FILE__, __LINE__, errno, strerror(errno), i); */

				znak		= '?'; 
				inv 		= 1;
			}
			
			temp[i]		= znak;
			str->attr[i]	= str->attr[cur]; 

			if (inv) {
				str->attr[i] |= FSTR_REVERSE;
				len = 1;	/* ? */
			}

			cur += len;
			i++;

			if (!len)	/* NUL */
				break;
		}

	/* resize str->attr && str->str to match newlen */
		xfree(str->str.b); 

		str->str.w  = xrealloc(temp, (i+1) * sizeof(CHAR_T));		/* i ? */
		str->attr = xrealloc(str->attr, (i+1) * sizeof(short));		/* i ? */

	}
#endif
	n->backlog[0] = str;

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

		time_t ts;			/* current ts */
		time_t lastts = 0; 		/* last cached ts */
		char lasttsbuf[100];		/* last cached strftime() result */

		str = n->backlog[i]->str.w + n->backlog[i]->prompt_len;
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
				l->prompt_str = n->backlog[i]->str.w;
				l->prompt_attr = n->backlog[i]->attr;
			} else {
				l->prompt_str = NULL;
				l->prompt_attr = NULL;
			}

			if (!w->floating && config_timestamp && config_timestamp_show) {
				fstring_t *s = NULL;
				if (ts && lastts == ts) {	/* use cached value */
					s = fstring_new(lasttsbuf);

					l->ts = s->str.b;
					l->ts_len = xstrlen(l->ts);
					l->ts_attr = s->attr;

					xfree(s);
				} else if (xstrcmp(config_timestamp, "")) {
					struct tm *tm = localtime(&ts);
					char *tmp = NULL, *format;

					tmp = format_string(config_timestamp);
					format = saprintf("%s ", tmp);
					strftime(lasttsbuf, sizeof(lasttsbuf)-1, format, tm);

					s = fstring_new(lasttsbuf);

					l->ts = s->str.b;
					l->ts_len = xstrlen(l->ts);
					l->ts_attr = s->attr;

					xfree(s);
					xfree(tmp);
					xfree(format);

					lastts = ts;
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

				if (str[j] == CHAR(' ') && !w->nowrap)
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

			if (w->nowrap) {
				while (*str) {
					str++;
					attr++;
				}
				break;
			}
			str += l->len;
			attr += l->len;

			if (! *str)
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

		ncurses_update_real_prompt(n);
		n->redraw = 1;
	}

	ncurses_screen_width = width;
	ncurses_screen_height = height;
}

/*
 * fstring_attr2ncurses_attr()
 *
 * Convert internal ekg2 fstring attr value, to value which ncurses understand.
 *
 */

static inline int fstring_attr2ncurses_attr(short chattr) {
	int attr = A_NORMAL;

	if ((chattr & FSTR_BOLD))
		attr |= A_BOLD;

	if ((chattr & FSTR_BLINK))
		attr |= A_BLINK;

	if (!(chattr & FSTR_NORMAL))
		attr |= color_pair(chattr & FSTR_FOREMASK, 0, config_display_transparent ? COLOR_BLACK: (chattr>>3)&7);

	if ((chattr & FSTR_UNDERLINE))
		attr |= A_UNDERLINE;

	if ((chattr & FSTR_REVERSE))
		attr |= A_REVERSE;

	return attr;
}

/*
 * ncurses_fixchar()
 *
 * When we recv control character (ASCII code below 32), we can add 64 to it, and REVERSE attr.
 * When we recv ISO control character [and we're using console under iso charset] (ASCII code between 128..159), we can REVERSE attr, and return '?'
 */

static inline CHAR_T ncurses_fixchar(CHAR_T ch, int *attr) {
	if (ch < 32) {
		*attr |= A_REVERSE;
		return ch + 64;
	}

	if (ch > 127 && ch < 160 &&
#if USE_UNICODE
		!config_use_unicode &&
#endif
		config_use_iso)
	{
		*attr |= A_REVERSE;
		return '?';
	}

	return ch;
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
		const char *vertical_line_char	= format_find("contacts_vertical_line_char");
		const char *horizontal_line_char= format_find("contacts_horizontal_line_char");

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

	if (config_text_bottomalign && (!w->floating || config_text_bottomalign == 2)
			&& n->start == 0 && n->lines_count < height)
	{
		const int tmp = height - n->lines_count;

		if (tmp > top)
			top = tmp;
	}

	for (y = 0; y < height && n->start + y < n->lines_count; y++) {
		struct screen_line *l = &n->lines[n->start + y];

		int cur_y = (top + y);	
		int cur_x;

		int fixup = 0;

		wattrset(n->window, A_NORMAL);

		cur_x = (left);
		
		if (l->ts) {
			/* XXX,
			 * 	po co sprawdzamy l->ts[x] i l->ts_len? nie wystarczy jedno?
			 */
			for (x = 0; l->ts[x] && x < l->ts_len; x++, cur_x++) { 
				int attr = fstring_attr2ncurses_attr(l->ts_attr[x]);
				unsigned char ch = (unsigned char) ncurses_fixchar((CHAR_T) l->ts[x], &attr);

				wattrset(n->window, attr);
				mvwaddch(n->window, cur_y, cur_x, ch);
			}
		}

		cur_x = (left + l->ts_len);	/* XXX */

		if (l->prompt_str) {
			for (x = 0; x < l->prompt_len; x++, cur_x++) {
				int attr = fstring_attr2ncurses_attr(l->prompt_attr[x]);
				CHAR_T ch = ncurses_fixchar(l->prompt_str[x], &attr);

				wattrset(n->window, attr);

				if (!fixup && (l->margin_left != -1 && x >= l->margin_left))
					fixup = l->margin_left + config_margin_size;
#if USE_UNICODE
				if (config_use_unicode) {
					mvwaddnwstr(n->window, cur_y, cur_x - fixup, &ch, 1);
				} else
#endif
					mvwaddch(n->window, cur_y, cur_x - fixup, ch);
			}
		}

		cur_x = (left + l->ts_len + l->prompt_len);	/* XXX */

		for (x = 0; x < l->len; x++, cur_x++) {
			int attr = fstring_attr2ncurses_attr(l->attr[x]);
			CHAR_T ch = ncurses_fixchar(l->str[x], &attr);

			wattrset(n->window, attr);

			if (!fixup && (l->margin_left != -1 && (x + l->prompt_len) >= l->margin_left))
				fixup = l->margin_left + config_margin_size;
#if USE_UNICODE
			if (config_use_unicode) {
				mvwaddnwstr(n->window, cur_y, cur_x - fixup, &ch, 1);
			} else
#endif
				mvwaddch(n->window, cur_y, cur_x - fixup, ch);
		}
	}

	n->redraw = 0;

	if (w == window_current)
		ncurses_redraw_input(0);
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
			fstring_free(n->backlog[i]);

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
 * ncurses_refresh()
 *
 * wnoutrefresh()uje aktualnie wy¶wietlane okienko.
 */
void ncurses_refresh()
{
	list_t l;

	if (window_current && window_current->private /* !window_current->floating */) {
		ncurses_window_t *n = window_current->private;

		if (n->redraw)
			ncurses_redraw(window_current);

		if (!window_current->hide)
			wnoutrefresh(n->window);
	}

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		ncurses_window_t *n = w->private;

		if (!w->floating || w->hide)
			continue;

		if (n->handle_redraw) {
			ncurses_redraw(w);
		} else {
			/* window_floating_update() */

			if (w->last_update != time(NULL)) {
				w->last_update = time(NULL);

				ncurses_clear(w, 1);

				ncurses_redraw(w);
			}

		}
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
		
static int window_printat(WINDOW *w, int x, int y, const char *format, struct format_data *data, int fgcolor, int bold, int bgcolor) {
	int orig_x = x;
	int backup_display_color = config_display_color;
	char *ftext = NULL;		/* tekst do zwolnienia jesli !config_display_pl_chars */
	const char *p;			/* parsowanie format lub ftext jesli !config_display_pl_chars */

	if (!w)
		return -1;
	
	if (!config_display_pl_chars) {
		ftext = xstrdup(format);
		iso_to_ascii(ftext);
		p = ftext;
	} else	p = format;

	if (config_display_color == 2)	config_display_color = 0;

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

		for (i = 0; data[i].name; i++) {
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
						text++;	
						x++;
						continue;
					}
					text++;
					
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
					
					text++;
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

			for (i = 0; data[i].name; i++) {
				int len, matched = ((data[i].text) ? 1 : 0);

				if (neg)
					matched = !matched;

				len = xstrlen(data[i].name);

				if (!strncmp(p, data[i].name, len) && p[len] == ' ') {
					p += len + 1;

					if (matched)
						x += window_printat(w, x, y, p, data, fgcolor, bold, bgcolor);
					break; /* goto next; */
				}
			}
			/* goto next; */
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

	if (ftext) xfree(ftext);
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
	static const char empty_format[] = "";

	struct format_data formats[32];	/* if someone add his own format increment it. */
	int formats_count = 0, i = 0, y;
	session_t *sess = window_current->session;
	userlist_t *q = userlist_find(sess, window_current->target);
	plugin_t *plug;
	char *tmp;

	wattrset(ncurses_status, color_pair(COLOR_WHITE, 0, COLOR_BLUE));
	if (ncurses_header)
		wattrset(ncurses_header, color_pair(COLOR_WHITE, 0, COLOR_BLUE));

	/* inicjalizujemy wszystkie opisowe bzdurki */

	memset(&formats, 0, sizeof(formats));

#define __add_format(x, z) \
	{ \
		formats[formats_count].name = x; \
		formats[formats_count].text = z; \
		formats_count++; \
	} 

#define __add_format_emp(x, y)		__add_format(x, y ? (char *) empty_format : NULL)
#define __add_format_dup(x, y, z) 	__add_format(x, y ? xstrdup(z) : NULL)


	__add_format_dup("time", 1, timestamp(format_find("statusbar_timestamp")));

	__add_format_dup("window", window_current->id, itoa(window_current->id));
	__add_format_dup("session", (sess), (sess->alias) ? sess->alias : sess->uid);
	__add_format_dup("descr", (sess && sess->descr && session_connected_get(sess)), sess->descr);

	tmp = (sess && q) ? saprintf("%s/%s", q->nickname, q->uid) : xstrdup(window_current->target);
	__add_format("query", tmp);

	if ((plug = plugin_find(("mail")))) {
		int mail_count = -1;
		query_emit_id(plug, MAIL_COUNT, &mail_count);
		__add_format_dup("mail", (mail_count > 0), itoa(mail_count));
	}
	if (session_check(window_current->session, 1, "irc") && (plug = plugin_find(("irc")))) {
		char *t1 = NULL;
		char *t2 = NULL;
		char *t3 = NULL; 
		query_emit_id(plug, IRC_TOPIC, &t1, &t2, &t3);
		__add_format("irctopic", t1);
		__add_format("irctopicby", t2);
		__add_format("ircmode", t3);
	}

	{
		string_t s = string_init("");
		int act = 0;
		list_t l;

		for (l = windows; l; l = l->next) {
			char tmp[33];
			window_t *w = l->data;

			if (!(w->act & 7) || !w->id || (w == window_current)) 
				continue;

			if (act)
				string_append_c(s, ',');
			sprintf(tmp, "statusbar_act%s%s", (w->act & 2 ? "_important" : ""), (w->act & 4 ? "_typing" : ""));
			string_append(s, format_find(tmp));
			string_append(s, itoa(w->id));
			act = 1;
		}
		if (act) {
			__add_format("activity", string_free(s, 0));
		} else {
			__add_format("activity", NULL);
			string_free(s, 1);
		}
	}

	__add_format_dup("debug", (!window_current->id), "");

	if (sess && sess->connected) {
#define __add_format_emp_st(x, y) __add_format_emp(x, (sess->status == y))
		__add_format_emp_st("away", EKG_STATUS_AWAY);
		__add_format_emp_st("avail", EKG_STATUS_AVAIL);
		__add_format_emp_st("dnd", EKG_STATUS_DND);
		__add_format_emp_st("chat", EKG_STATUS_FFC);
		__add_format_emp_st("xa", EKG_STATUS_XA);
		__add_format_emp_st("invisible", EKG_STATUS_INVISIBLE);

		__add_format_emp_st("notavail", EKG_STATUS_NA);		/* XXX, session shouldn't be connected here */
#undef __add_format_emp_st
	} else
		__add_format_emp("notavail", 1);

	__add_format_emp("more", (window_current->more));

	if (q) {
#define __add_format_emp_st(x, y) __add_format_emp("query_" x, (q->status == y));
		__add_format_emp_st("away", EKG_STATUS_AWAY);
		__add_format_emp_st("avail", EKG_STATUS_AVAIL);
		__add_format_emp_st("invisible", EKG_STATUS_INVISIBLE);
		__add_format_emp_st("notavail", EKG_STATUS_NA);
		__add_format_emp_st("dnd", EKG_STATUS_DND);
		__add_format_emp_st("chat", EKG_STATUS_FFC);
		__add_format_emp_st("xa", EKG_STATUS_XA);
		/* XXX add unknown and likes!; maybe we could use ekg_status_string()? */
#undef __add_format_emp_st

		__add_format_emp("typing", (q->xstate & EKG_XSTATE_TYPING));
		__add_format_dup("query_descr", (q->descr), q->descr);
#if 0	
		__add_format_dup("query_ip", (q->ip), inet_ntoa(*((struct in_addr*)(&q->ip)))); 	/* XXX!!! */
#endif
	}

	__add_format_dup("url", 1, "http://www.ekg2.org/");
	__add_format_dup("version", 1, VERSION);

#undef __add_format_emp
#undef __add_format_dup
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
			char *tmp;
			case 0:
				window_printat(ncurses_status, 0, y, p, formats, COLOR_WHITE, 0, COLOR_BLUE);
				break;

			case 1:
				tmp = saprintf(" debug: lines_count=%d start=%d height=%d overflow=%d screen_width=%d", ncurses_current->lines_count, ncurses_current->start, window_current->height, ncurses_current->overflow, ncurses_screen_width);
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE);
				xfree(tmp);
				break;

			case 2:
				tmp = saprintf(" debug: lines(count=%d,start=%d,index=%d), line(start=%d,index=%d)", array_count((char **) ncurses_lines), lines_start, lines_index, line_start, line_index);
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE);
				xfree(tmp);
				break;

			case 3:
				tmp = saprintf(" debug: session=%p uid=%s alias=%s / target=%s session_current->uid=%s", sess, (sess && sess->uid) ? sess->uid : "", (sess && sess->alias) ? sess->alias : "", (window_current->target) ? window_current->target : "", (session_current && session_current->uid) ? session_current->uid : "");
				window_printat(ncurses_status, 0, y, tmp, formats, COLOR_WHITE, 0, COLOR_BLUE);
				xfree(tmp);
				break;
		}
	}

	for (i = 0; i < formats_count; i++) {
		if (formats[i].text != empty_format)
			xfree(formats[i].text);
	}

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
			fstring_free(n->backlog[i]);

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
	xfree(n->prompt_real);
	delwin(n->window);
	xfree(n);
	w->private = NULL;

//	ncurses_resize();

	ncurses_window_gone(w);

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
	int background;
	va_list dummy;

	ncurses_screen_width = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
	ncurses_screen_height = getenv("LINES") ? atoi(getenv("LINES")) : 24;

	initscr();
	cbreak();
	noecho();
	nonl();
#ifdef HAVE_NCURSES_ULC
	if (!config_use_iso
#if USE_UNICODE
			&& !config_use_unicode
#endif
			)
		use_legacy_coding(2);
#endif

	if (config_display_transparent) {
		background = COLOR_DEFAULT;
		use_default_colors();
	} else {
		background = COLOR_BLACK;
		assume_default_colors(COLOR_WHITE, COLOR_BLACK);
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

	ncurses_contacts_changed("contacts", dummy);
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
	const int prompt_len = (ncurses_lines) ? 0 : ncurses_current->prompt_real_len;

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

	/* Debian screen incomplete terminfo workaround */

	if (mouse_initialized == 2 && *ch == 27) { /* escape */
		int tmp;

		if ((tmp = wgetch(input)) != '[')
			ungetch(tmp);
		else if ((tmp = wgetch(input)) != 'M') {
			ungetch(tmp);
			ungetch('[');
		} else
			*ch = KEY_MOUSE;
	}

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
				case 1:
				case 2:
                                case 64:
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
	if (query_emit_id(NULL, UI_KEYPRESS, ch) == -1)  
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
	header_statusbar_resize(NULL);
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
	register int i = 0;     /* licznik */
	register int j = 0;     /* licznik */

	/* Sprawdzamy czy nie mamy doczynienia z 47 (wtedy nie sprawdzamy reszty ) */
	if (!what || *what == 47)
		return;       /* konczymy funkcje */
	    
	for (i = 0; what[i] != CHAR('\0') && what[i] != CHAR('\n') && what[i] != CHAR('\r'); i++) {
		if ((!isalpha_pl(what[i]) || i == 0 ) && what[i+1] != CHAR('\0')) { // separator/koniec lini/koniec stringu
			char *word;             /* aktualny wyraz */
			size_t wordlen;		/* dlugosc aktualnego wyrazu */
		
		/* szukamy jakiejs pierwszej literki */
			for (; what[i] != CHAR('\0') && what[i] != CHAR('\n') && what[i] != CHAR('\r'); i++) {
				if (isalpha_pl(what[i]))
					break;
			}

		/* trochê poprawiona wydajno¶æ */
			if (what[i] == '\0' || what[i] == '\n' || what[i] == '\r') {
				i--;
				continue;
			} 
		/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu www */ 
			if (what[i] == 'h' && what[i + 1] == 't' && what[i + 2] == 't' && what[i + 3] == 'p' && what[i + 4] == ':' && 
				what[i + 5] == '/' && what[i + 6] == '/') 
			{
				for(; what[i] != ' ' && what[i] != '\n' && what[i] != '\r' && what[i] != '\0'; i++);
				i--;
				continue;
			} 
		/* sprawdzanie czy nastêpny wyraz nie rozpoczyna adresu ftp */ 
			if (what[i] == 'f' && what[i + 1] == 't' && what[i + 2] == 'p' && what[i + 3] == ':' &&
				what[i + 4] == '/' && what[i + 5] == '/') 
			{
				for(; what[i] != ' ' && what[i] != '\n' && what[i] != '\r' && what[i] != '\0'; i++);
				i--;
				continue;
			}

/* XXX in unicode/unicode it's wrong... */
			word = xmalloc((xwcslen(what) + 1)*sizeof(char));

			/* wrzucamy aktualny wyraz do zmiennej word */
			for (j = 0; what[i] != '\n' && what[i] != '\0' && isalpha_pl(what[i]); i++) {
				if (isalpha_pl(what[i])) {
					word[j]= what[i];
					j++;
				} else 
					break;
			}
/*			word[j] = (char) 0; */	/* xmalloc() zero memory */
			wordlen = j;		/* xstrlen(word) */

			if (i > 0)
				i--;

/*			debug(GG_DEBUG_MISC, "Word: %s\n", word);  */

			/* sprawdzamy pisownie tego wyrazu */
			if (aspell_speller_check(spell_checker, word, xstrlen(word) ) == 0) { /* jesli wyraz jest napisany blednie */
				for (j = wordlen - 1; j >= 0; j--)
					where[i - j] = ASPELLCHAR;
			} else { /* jesli wyraz jest napisany poprawnie */
				for (j = wordlen - 1; j >= 0; j--)
					where[i - j] = ' ';
			}
			xfree(word);
		}
	}
}
#endif

extern volatile int sigint_count;

/* to jest tak, ncurses_redraw_input() jest wywolywany po kazdym nacisnieciu klawisza (gdy ch != 0)
   oraz przez ncurses_ui_window_switch() handler `window-switch`
   window_switch() moze byc rowniez wywolany jako binding po nacisnieciu klawisza (command_exec() lub ^n ^p lub inne)
   wtedy ncurses_redraw_input() bylby wywolywany dwa razy przez window_switch() oraz po nacisnieciu klawisza)
 */
static int ncurses_redraw_input_already_exec = 0;

/* 
 * wyswietla ponownie linie wprowadzenia tekstu		(prompt + aktualnie wpisany tekst) 
 * 	przy okazji jesli jest aspell to sprawdza czy tekst jest poprawny.
 */
void ncurses_redraw_input(unsigned int ch) {
	const int promptlen = ncurses_lines ? 0 : ncurses_current->prompt_real_len;
#ifdef WITH_ASPELL
	char *aspell_line = NULL;
	int mispelling = 0; /* zmienna pomocnicza */
#endif
	if (line_index - line_start > input->_maxx - 9 - promptlen)
		line_start += input->_maxx - 19 - promptlen;
	if (line_index - line_start < 10) {
		line_start -= input->_maxx - 19 - promptlen;
		if (line_start < 0)
			line_start = 0;
	}
	ncurses_redraw_input_already_exec = 1;

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
				if (aspell_line && aspell_line[line_start + j] == ASPELLCHAR && p[line_start + j] != ' ') /* jesli b³êdny to wy¶wietlamy podkre¶lony */
					print_char(input, i, j, p[line_start + j], A_UNDERLINE);
				else	/* jesli jest wszystko okey to wyswietlamy normalny */
#endif					/* lub nie mamy aspella */
					print_char(input, i, j, p[j + line_start], A_NORMAL);
			}
#ifdef WITH_ASPELL
			xfree(aspell_line);
#endif
		}

		{
			const int beforewin	= (lines_index < lines_start);
			const int outtawin	= (beforewin || lines_index > lines_start + 4);
			
			wmove(input, (beforewin ? 0 : outtawin ? 4 : lines_index - lines_start),
					outtawin ? stdscr->_maxx : line_index - line_start);
			curs_set(!outtawin);
		}
	} else {
		int i;
		/* const */size_t linelen	= xwcslen(ncurses_line);

		if (ncurses_current->prompt)
#ifdef USE_UNICODE /* XXX: should we check config_use_unicode here? */
			mvwaddwstr(input, 0, 0, ncurses_current->prompt_real);
#else
			mvwaddstr(input, 0, 0, ncurses_current->prompt_real);
#endif

		if (ncurses_noecho) {
			const int x		= promptlen + 1;
			static char *funnything	= ncurses_funnything;

			mvwaddch(input, 0, x, *funnything);
			wmove(input, 0, x);
			if (!*(++funnything))
				funnything = ncurses_funnything;
			return;
		}

#ifdef WITH_ASPELL		
		if (spell_checker) {
			aspell_line = xmalloc(linelen + 1);
			if (line_start == 0) 
				mispelling = 0;
	
			spellcheck(ncurses_line, aspell_line);
		}
#endif
		for (i = 0; i < input->_maxx + 1 - promptlen && i < linelen - line_start; i++) {
#ifdef WITH_ASPELL
			if (spell_checker && aspell_line[line_start + i] == ASPELLCHAR && ncurses_line[line_start + i] != ' ') /* jesli b³êdny to wy¶wietlamy podkre¶lony */
				print_char(input, 0, i + promptlen, ncurses_line[line_start + i], A_UNDERLINE);
			else 	/* jesli jest wszystko okey to wyswietlamy normalny */
#endif				/* lub gdy nie mamy aspella */
				print_char(input, 0, i + promptlen, ncurses_line[line_start + i], A_NORMAL);
		}
#ifdef WITH_ASPELL
		xfree(aspell_line);
#endif
		/* this mut be here if we don't want 'timeout' after pressing ^C */
		if (ch == 3) ncurses_commit();
		wattrset(input, color_pair(COLOR_BLACK, 1, COLOR_BLACK));
		if (line_start > 0)
			mvwaddch(input, 0, promptlen, '<');
		if (linelen - line_start > input->_maxx + 1 - promptlen)
			mvwaddch(input, 0, input->_maxx, '>');
		wattrset(input, color_pair(COLOR_WHITE, 0, COLOR_BLACK));
		wmove(input, 0, line_index - line_start + promptlen);
	}
}

/*
 * ncurses_watch_stdin()
 *
 * g³ówna pêtla interfejsu.
 */
WATCHER(ncurses_watch_stdin)
{
	static int lock = 0;
	struct binding *b = NULL;
	int tmp;
	unsigned int ch;

	ncurses_redraw_input_already_exec = 0;
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
		if ((ekg_getch(27, &ch)) < 0)
			goto loop;
		
		if (ch == 27)
			b = ncurses_binding_map[27];
		else if (ch > KEY_MAX) {
			debug_error("%s:%d INTERNAL NCURSES/EKG2 FAULT. KEY-PRESSED: %d>%d TO PROTECT FROM SIGSEGV\n", __FILE__, __LINE__, ch, KEY_MAX);
			goto then;
		} else	b = ncurses_binding_map_meta[ch];

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
			!config_use_unicode && 
#endif
			ch > KEY_MAX) {
			
			debug_error("%s:%d INTERNAL NCURSES/EKG2 FAULT. KEY-PRESSED: %d>%d TO PROTECT FROM SIGSEGV\n", __FILE__, __LINE__, ch, KEY_MAX);
			goto then;
		}

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
			memmove(ncurses_line + line_index + 1, ncurses_line + line_index, sizeof(CHAR_T) * (LINE_MAXLEN - line_index - 1));
					/* put in ncurses_line[lindex_index] current char */
			ncurses_line[line_index++] = ch;

			ncurses_typing_mod = 1;
		}
	}
then:
	if (ncurses_plugin_destroyed)
		return 0;

	/* je¶li siê co¶ zmieni³o, wygeneruj dope³nienia na nowo */
	if (!b || (b && b->function != ncurses_binding_complete))
		ncurses_complete_clear();
	
	if (!ncurses_redraw_input_already_exec || (b && b->function == ncurses_binding_accept_line)) 
		ncurses_redraw_input(ch);
loop:
	if (!lock) {
		lock = 1;
		while ((ncurses_watch_stdin(type, fd, watch, NULL)) == 1) ;		/* execute handler untill all data from fd 0 will be readed */
		lock = 0;
	}

	return 1;
}

/*
 * header_statusbar_resize()
 *
 * zmienia rozmiar paska stanu i/lub nag³ówka okna.
 */
void header_statusbar_resize(const char *dummy)
{
/*	if (in_autoexec) return; */
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
			fstring_free(n->backlog[i]);

		n->backlog_size = config_backlog_size;
		n->backlog = xrealloc(n->backlog, n->backlog_size * sizeof(fstring_t *));

		ncurses_backlog_split(w, 1, 0);
	}
}

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


	if (!w || !(n = w->private))
		return items;

	if (config_lastlog_noitems) {	/* always add header */
		char *tmp = format_string(header, window_target(w), lastlog->expression);
		ncurses_backlog_add(lastlog_w, fstring_new(tmp));
		xfree(tmp);
	}

	local_config_lastlog_case = (lastlog->casense == -1) ? config_lastlog_case : lastlog->casense;

	for (i = n->backlog_size-1; i >= 0; i--) {
		int found = 0;

		if (lastlog->isregex) {		/* regexp */
#ifdef HAVE_REGEX_H
			int rs;
			if (!(rs = regexec(&(lastlog->reg), n->backlog[i]->str.w, 0, NULL, 0))) 
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
			if (local_config_lastlog_case) 
				found = !!xstrstr(n->backlog[i]->str.w, lastlog->expression);
			else	found = !!xstrcasestr(n->backlog[i]->str.w, lastlog->expression);
		}

		if (!config_lastlog_noitems && found && !items) { /* add header only when found */
			char *tmp = format_string(header, window_target(w), lastlog->expression);
			ncurses_backlog_add(lastlog_w, fstring_new(tmp));
			xfree(tmp);
		}
		
		if (found) {
			fstring_t *dup;
			size_t len;
#if 0
(USE_UNICODE)
			#warning "ncurses_ui_window_lastlog_find() won't work with USE_UNICODE sorry. no time for it. feel free"
			continue;
#endif

			dup = xmalloc(sizeof(fstring_t));

			len = xwcslen(n->backlog[i]->str.w);

			dup->str.w		= xmemdup(n->backlog[i]->str.w, sizeof(CHAR_T) * (len + 1));
			dup->attr		= xmemdup(n->backlog[i]->attr, sizeof(short) * (len + 1));
			dup->ts			= n->backlog[i]->ts;
			dup->prompt_len		= n->backlog[i]->prompt_len;
			dup->prompt_empty	= n->backlog[i]->prompt_empty;
			dup->margin_left	= n->backlog[i]->margin_left;
		/* org. window for example if we would like user allow move under that line with mouse and double-click.. or whatever */
/*			dup->private		= (void *) w;	 */

			ncurses_backlog_add(lastlog_w, dup);
			items++;
		}
	}

	return items;
}

int ncurses_lastlog_update(window_t *w) {
	ncurses_window_t *n;
	list_t l;
	int retval = 0;

	int old_start;

	if (config_lastlog_lock) return 0;

	if (!w) w = window_find_sa(NULL, "__lastlog", 1);
	if (!w) return -1;

	n = w->private;
	old_start = n->start;

	ncurses_clear(w, 1);

/* XXX, it's bad orded now, need fix */

/* 1st, lookat current window.. */
	retval += ncurses_ui_window_lastlog(w, window_current);

/* 2nd, display lastlog from floating windows? (XXX) */

	if (config_lastlog_display_all) {
/* 3rd, other windows? */
		for (l = windows; l; l = l->next) {
			window_t *ww = l->data;
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

	ncurses_window_t *n = w->private;
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
/*
	n->handle_mouse = ncurses_lastlog_mouse_handler;
 */
	n->start = 0;
	w->edge = lastlog_edge;
	w->nowrap = !lastlog_wrap;
	w->floating = 1;
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

	if (!xstrcmp(w->target, "__contacts")) {
		ncurses_contacts_new(w);

	} else if (!xstrcmp(w->target, "__lastlog")) {
		ncurses_lastlog_new(w);

	} else if (w->target) {
		const char *f = format_find("ncurses_prompt_query");

		n->prompt = format_string(f, w->target);
		n->prompt_len = xstrlen(n->prompt);

		ncurses_update_real_prompt(n);
	} else {
		const char *f = format_find("ncurses_prompt_none");

		if (xstrcmp(f, "")) {
			n->prompt = format_string(f);
			n->prompt_len = xstrlen(n->prompt);

			ncurses_update_real_prompt(n);
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
