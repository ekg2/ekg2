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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ekg/completion.h>

#include "bindings.h"
#include "input.h"
#include "mouse.h"
#include "notify.h"
#include "nc-stuff.h"
#include "spell.h"


CHAR_T *ncurses_line = NULL;		/* wska¼nik aktualnej linii */
CHAR_T *ncurses_yanked = NULL;		/* bufor z ostatnio wyciêtym tekstem */
CHAR_T **ncurses_lines = NULL;		/* linie wpisywania wielolinijkowego */
int ncurses_line_start = 0;		/* od którego znaku wy¶wietlamy? */
int ncurses_line_index = 0;		/* na którym znaku jest kursor? */
int ncurses_lines_start = 0;		/* od której linii wy¶wietlamy? */
int ncurses_lines_index = 0;		/* w której linii jeste¶my? */
int ncurses_input_size = 1;		/* rozmiar okna wpisywania tekstu */

int ncurses_noecho = 0;


#if 0
static char ncurses_funnything[5] = "|/-\\";

CHAR_T *ncurses_passbuf;

QUERY(ncurses_password_input) {
	char **buf		= va_arg(ap, char**);
	const char *prompt	= *va_arg(ap, const char**);
	const char **rprompt	= va_arg(ap, const char**);

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
	ncurses_current->prompt		= (char*) (prompt ? prompt : format_find("password_input"));
	ncurses_current->prompt_len	= xstrlen(ncurses_current->prompt);
	ncurses_update_real_prompt(ncurses_current);
	ncurses_lines			= NULL;
	ncurses_line			= xmalloc(LINE_MAXLEN * sizeof(CHAR_T));
	line_index = line_start = 0;
	ncurses_redraw_input(0);

	while (ncurses_noecho)
		ncurses_watch_stdin(0, 0, WATCH_READ, NULL);
	passa = ncurses_passbuf;

	if (xwcslen(passa)) {
		if (rprompt) {
			ncurses_current->prompt		= (char*) (*rprompt ? *rprompt : format_find("password_repeat"));
			ncurses_current->prompt_len	= xstrlen(ncurses_current->prompt);
			ncurses_noecho			= 1;
			ncurses_update_real_prompt(ncurses_current);
			ncurses_redraw_input(0);

			while (ncurses_noecho)
				ncurses_watch_stdin(0, 0, WATCH_READ, NULL);
			passb = ncurses_passbuf;
		}

		if (passb && xwcscmp(passa, passb))
			print("password_nomatch");
		else
#if USE_UNICODE
			*buf = wcs_to_normal(passa);
#else
			*buf = xstrdup((char *) passa);
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
#endif

/* cut prompt to given width and recalculate its' width */
void ncurses_update_real_prompt(ncurses_window_t *n) {
	g_assert(n);

#if 0 /* XXX: shortening */

	const int _maxlen = n->window && n->window->_maxx ? n->window->_maxx : 80;
	const int maxlen = ncurses_noecho ? _maxlen - 3 : _maxlen / 3;
	xfree(n->prompt_real);

	if (maxlen <= 6) /* we assume the terminal is too narrow to display any input with prompt */
		n->prompt_real		= NULL;
	else {
#ifdef USE_UNICODE
		n->prompt_real		= normal_to_wcs(n->prompt);
#else
		n->prompt_real		= (CHAR_T *) xstrdup(n->prompt);
#endif
	}
	n->prompt_real_len		= xwcslen(n->prompt_real);

	if (n->prompt_real_len > maxlen) { /* need to cut it */
		const CHAR_T *dots	= (CHAR_T *) TEXT("...");
#ifdef USE_UNICODE
		const wchar_t udots[2]	= { 0x2026, 0 };
		if (console_charset_is_utf8)	/* use unicode hellip, if using utf8 */
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
#endif
}

/*
 * lines_adjust()
 *
 * poprawia kursor po przesuwaniu go w pionie.
 */
void ncurses_lines_adjust(void) {
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
void ncurses_input_update(int new_line_index)
{
	if (ncurses_input_size == 1) {
		g_strfreev((char **) ncurses_lines);
		ncurses_lines = NULL;
		ncurses_line = xmalloc(LINE_MAXLEN*sizeof(CHAR_T));

		ncurses_history[0] = ncurses_line;

	} else {
		ncurses_lines = xmalloc(2 * sizeof(CHAR_T *));
		ncurses_lines[0] = xmalloc(LINE_MAXLEN*sizeof(CHAR_T));
/*		ncurses_lines[1] = NULL; */
		xwcscpy(ncurses_lines[0], ncurses_line);
		xfree(ncurses_line);
		ncurses_line = ncurses_lines[0];
		ncurses_history[0] = NULL;
	}
	line_start = 0;
	line_index = new_line_index;
	lines_start = 0;
	lines_index = 0;

	ncurses_resize();

	ncurses_redraw(window_current);
	touchwin(ncurses_current->window);

	ncurses_commit();
}

/*
 * print_char()
 *
 * wy¶wietla w danym okienku znak, bior±c pod uwagê znaki ,,niewy¶wietlalne''.
 *	gdy attr A_UNDERLINE wtedy podkreslony
 */
static void print_char(WINDOW *w, CHAR_T ch, int attr) {
	ch = ncurses_fixchar(ch, &attr);

	wattrset(w, attr);

#if USE_UNICODE
	waddnwstr(w, &ch, 1);
#else
	waddch(w, ch);
#endif
}

/*
 * ekg_getch()
 *
 * czeka na wci¶niêcie klawisza i je¶li wkompilowano obs³ugê pythona,
 * przekazuje informacjê o zdarzeniu do skryptu.
 *
 *  - meta - przedrostek klawisza.
 *
 * @returns:
 *	-2		- ignore that key
 *	ERR		- error
 *	OK		- report a (wide) character
 *	KEY_CODE_YES	- report the pressing of a function key
 *
 */
static int ekg_getch(int meta, unsigned int *ch) {
	int retcode;
#if USE_UNICODE
	retcode = wget_wch(input, ch);
#else
	*ch = wgetch(input);
	retcode = *ch >= KEY_MIN ? KEY_CODE_YES : OK;
#endif

	if (retcode == ERR) return ERR;
	if ((retcode == KEY_CODE_YES) && (*(int *)ch == -1)) return ERR;		/* Esc (delay) no key */

#ifndef HAVE_USABLE_TERMINFO
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
#endif

	/*
	 * conception is borrowed from Midnight Commander project
	 *    (www.ibiblio.org/mc/)
	 */
#define GET_TIME(tv)	(g_get_current_time(&tv))
#define DIF_TIME(t1,t2) ((t2.tv_sec -t1.tv_sec) *1000+ \
			 (t2.tv_usec-t1.tv_usec)/1000)
	if (*ch == KEY_MOUSE) {
		int btn, mouse_state = 0, x, y;
		static GTimeVal tv1 = { 0, 0 };
		static GTimeVal tv2;
		static int clicks;
		static int last_btn = 0;

		btn = wgetch (input) - 32;

		if (btn == 3 && last_btn) {
			last_btn -= 32;

			switch (last_btn) {
				case 0: mouse_state = (clicks) ? EKG_BUTTON1_DOUBLE_CLICKED : EKG_BUTTON1_CLICKED;	break;
				case 1: mouse_state = (clicks) ? EKG_BUTTON2_DOUBLE_CLICKED : EKG_BUTTON2_CLICKED;	break;
				case 2: mouse_state = (clicks) ? EKG_BUTTON3_DOUBLE_CLICKED : EKG_BUTTON3_CLICKED;	break;
				case 64: mouse_state = EKG_SCROLLED_UP;							break;
				case 65: mouse_state = EKG_SCROLLED_DOWN;						break;
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

		/* XXX query_emit UI_MOUSE ??? */
		if (mouse_state)
			ncurses_mouse_clicked_handler(x, y, mouse_state);

	}
#undef GET_TIME
#undef DIF_TIME
	if (query_emit(NULL, "ui-keypress", ch) == -1)
		return -2; /* -2 - ignore that key */

	return retcode;
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

extern volatile int sigint_count;

/* to jest tak, ncurses_redraw_input() jest wywolywany po kazdym nacisnieciu klawisza (gdy ch != 0)
   oraz przez ncurses_ui_window_switch() handler `window-switch`
   window_switch() moze byc rowniez wywolany jako binding po nacisnieciu klawisza (command_exec() lub ^n ^p lub inne)
   wtedy ncurses_redraw_input() bylby wywolywany dwa razy przez window_switch() oraz po nacisnieciu klawisza)
 */
static int ncurses_redraw_input_already_exec = 0;

/*
 * returns currsor x position
 */
static int ncurses_redraw_input_line(CHAR_T *text) {
#ifdef HAVE_LIBASPELL
	char *aspell_line = NULL;
#endif
	int i, stop, cur_posx = 0;
	int attr = A_NORMAL;
	const int linelen = xwcslen(text);
	int promptlen = getcurx(input);
	int width = input->_maxx + 1 - promptlen;
	int y = getcury(input);

#ifdef HAVE_LIBASPELL
	if (spell_checker) {
		aspell_line = xmalloc(linelen + 1);
		spellcheck(text, aspell_line);
	}
#endif
	stop = linelen < width+line_start ? linelen : width + line_start;
	for (i = line_start; i < stop; i++) {
		if (line_index == i) {
			cur_posx = getcurx(input);
		}
#ifdef HAVE_LIBASPELL
		if (aspell_line && aspell_line[i] == ASPELLCHAR && text[i] != ' ') /* jesli b³êdny to wy¶wietlamy podkre¶lony */
			attr = A_UNDERLINE;
		else
			attr = A_NORMAL;
#endif
		print_char(input, text[i], attr);
	}
	if (line_index >= i) {
		cur_posx = getcurx(input);
	}
#ifdef HAVE_LIBASPELL
	xfree(aspell_line);
#endif

	if (width>2) {
		wattrset(input, color_pair(COLOR_BLACK, COLOR_BLACK) | A_BOLD);
		if (line_start > 0)
			mvwaddch(input, y, promptlen, '<');
		if (linelen && linelen - line_start > width)
			mvwaddch(input, y, input->_maxx, '>');
	}
	wattrset(input, A_NORMAL);
	return cur_posx;
}

/*
 * wyswietla ponownie linie wprowadzenia tekstu		(prompt + aktualnie wpisany tekst)
 *	przy okazji jesli jest aspell to sprawdza czy tekst jest poprawny.
 */
void ncurses_redraw_input(unsigned int ch) {
	int x, y;
	/* draw prompt */
	werase(input);
	wmove(input, 0, 0);
	if (!ncurses_lines) {
		if (ncurses_current->prompt)
			ncurses_common_print(input, ncurses_current->prompt->str,
					ncurses_current->prompt->attr, -1 /* XXX */);
			
	}
	getyx(input, y, x);
	ncurses_current->prompt_len = x;

	/* XXX: cleanup, optimize */
	{
		int cur_posx = -1, cur_posy = 0;
		const int width = input->_maxx - x;

		if ((line_index - line_start >= width) || (line_index - line_start < 2))
			line_start = line_index - width/2;
		if (line_start < 0)
			line_start = 0;

		ncurses_redraw_input_already_exec = 1;

		wattrset(input, color_pair(COLOR_WHITE, COLOR_BLACK));

		if (ncurses_lines) {
			int i, x;

			cur_posy = lines_index - lines_start;
			for (i = 0; i < MULTILINE_INPUT_SIZE; i++) {
				if (!ncurses_lines[lines_start + i])
					break;

				wmove(input, i, 0);
				x = ncurses_redraw_input_line(ncurses_lines[lines_start + i]);
				if (lines_index == (lines_start + i))
					cur_posx = x;
			}
			wattrset(input, color_pair(COLOR_BLACK, COLOR_BLACK) | A_BOLD);
			if (lines_start>0)
				mvwaddch(input, 0, input->_maxx, '^');
			if (g_strv_length((char **) ncurses_lines)-lines_start > MULTILINE_INPUT_SIZE)
				mvwaddch(input, MULTILINE_INPUT_SIZE-1, input->_maxx, 'v');
			wattrset(input, A_NORMAL);
		} else {
#if 0
			if (ncurses_noecho) {
				static char *funnything	= ncurses_funnything;

				waddch(input, ' ');		/* XXX why here? If you want to add space after propt, add it in theme */
				waddch(input, *funnything);
				wmove(input, 0, getcurx(input)-1);
				if (!*(++funnything))
					funnything = ncurses_funnything;
				return;
			}
#endif

			cur_posx = ncurses_redraw_input_line(ncurses_line);

		}
		/* this mut be here if we don't want 'timeout' after pressing ^C */
		if (ch == 3) ncurses_commit();
		if (cur_posx != -1) {
			wmove(input, cur_posy, cur_posx);
			curs_set(1);
		} else {
			wmove(input, 0, 0);	// XXX ???
			curs_set(0);
		}
	}
}


static void bind_exec(struct binding *b) {
	if (b->function)
		b->function(b->arg);
	else {
		command_exec_format(window_current->target, window_current->session, 0,
				("%s%s"), ((b->action[0] == '/') ? "" : "/"), b->action);
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
	unsigned int ch;
	int getch_ret;

	ncurses_redraw_input_already_exec = 0;
	if (type)
		return 0;

	switch ((getch_ret = ekg_getch(0, &ch))) {
		case ERR:
		case(-2):	/* przeszlo przez query_emit i mamy zignorowac (pytthon, perl) */
			return 0;
		case OK:
			if (ch != 3) sigint_count = 0;
			if (ch == 0) return 0;	/* Ctrl-Space, g³upie to */
			break;
		case KEY_CODE_YES:
		default:
			break;
	}

	if (bindings_added && ch != KEY_MOUSE) {
		char **chars = NULL, *joined;
		int i = 0, count = 0, success = 0;
		binding_added_t *d;
		int c;
		array_add(&chars, xstrdup(ekg_itoa(ch)));

		while (count <= bindings_added_max && (c = wgetch(input)) != ERR) {
			array_add(&chars, xstrdup(ekg_itoa(c)));
			count++;
		}

		joined = g_strjoinv(" ", chars);

		for (d = bindings_added; d; d = d->next) {
			if (!xstrcasecmp(d->sequence, joined)) {
				bind_exec(d->binding);
				success = 1;
				goto end;
			}
		}

		for (i = count; i > 0; i--) {
			ungetch(atoi(chars[i]));
		}

end:
		xfree(joined);
		g_strfreev(chars);
		if (success)
			goto then;
	}

	if (ch == 27) {
		if ((ekg_getch(27, &ch)) < OK)
			goto loop;

		if (ch == 27)
			b = ncurses_binding_map[27];
		else if (ch > KEY_MAX) {
			/* XXX shouldn't happen */
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
			bind_exec(b);
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
		if ( ((getch_ret == KEY_CODE_YES && ch <= KEY_MAX) ||
			(getch_ret == OK && ch < 0x100)) &&
			(b = ncurses_binding_map[ch]) && b->action )
		{
			bind_exec(b);
		} else if (getch_ret == OK && xwcslen(ncurses_line) < LINE_MAXLEN - 1) {
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
		ekg2_complete_clear();

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
