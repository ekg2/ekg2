/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Pawe³ Maziarz <drg@infomex.pl>
			   Adam Osuchowski <adwol@polsl.gliwice.pl>
 *			    Wojtek Bojdo³ <wojboj@htcon.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
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

#include <sys/ioctl.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_READLINE_READLINE_H
#	include <readline/history.h>
#	include <readline/readline.h>
#else
#	include <history.h>
#	include <readline.h>
#endif

#include "ui-readline.h"

int ui_screen_height;
int ui_screen_width;
int ui_need_refresh;

int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80;

/* podstawmy ewentualnie brakuj±ce funkcje i definicje readline */

extern void rl_extend_line_buffer(int len);

#ifndef HAVE_RL_BIND_KEY_IN_MAP
int rl_bind_key_in_map(int key, void *function, void *keymap)
{
	return -1;
}
#endif

#ifndef HAVE_RL_GET_SCREEN_SIZE
int rl_get_screen_size(int *lines, int *columns)
{
#ifdef TIOCGWINSZ
	struct winsize ws;

	memset(&ws, 0, sizeof(ws));

	ioctl(0, TIOCGWINSZ, &ws);

	*columns = ws.ws_col;
	*lines = ws.ws_row;
#else
	*columns = 80;
	*lines = 24;
#endif
	return 0;
}
#endif

#ifndef HAVE_RL_FILENAME_COMPLETION_FUNCTION
char *rl_filename_completion_function()
{
	return NULL;
}
#endif

#ifndef HAVE_RL_SET_KEY
int rl_set_key(const char *key, void *function, void *keymap)
{
	return -1;
}
#endif

void set_prompt(const char *prompt) {
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt(prompt);
#else
	rl_expand_prompt((char *)prompt);
#endif
}

/*
 * my_getc()
 *
 * pobiera znak z klawiatury obs³uguj±c w miêdzyczasie sieæ.
 */
int my_getc(FILE *f)
{
	if (ui_need_refresh) {
		ui_need_refresh = 0;
		rl_get_screen_size(&screen_lines, &screen_columns);
		if (screen_lines < 1)
			screen_lines = 24;
		if (screen_columns < 1)
			screen_columns = 80;
		ui_screen_width = screen_columns;
		ui_screen_height = screen_lines;
	}
	return rl_getc(f);
}

/*
 * ui_readline_print()
 *
 * wy¶wietla dany tekst na ekranie, uwa¿aj±c na trwaj±ce w danych chwili
 * readline().
 */
void ui_readline_print(window_t *w, int separate, const char *xline)
{
	int old_end = rl_end, id = w->id;
	char *old_prompt = NULL, *line_buf = NULL;
	const char *line = NULL;

	if (config_timestamp) {
		string_t s = string_init(NULL);
		const char *p = xline;
		const char *buf = timestamp(format_string(config_timestamp));

		string_append(s, "\033[0m");
		string_append(s, buf);
		string_append_c(s, ' ');
		
		while (*p) {
			string_append_c(s, *p);
			if (*p == '\n' && *(p + 1)) {
				string_append(s, buf);
				string_append_c(s, ' ');
			}
			p++;
		}

		line = line_buf = string_free(s, 0);
	} else
		line = xline;

	/* je¶li nie piszemy do aktualnego, to zapisz do bufora i wyjd¼ */
	if (id != window_current->id) {
		window_write(id, line);

		/* XXX trzeba jeszcze waln±æ od¶wie¿enie prompta */
		goto done;
	}

	/* je¶li mamy ukrywaæ wszystko, wychodzimy */
	if (pager_lines == -2)
		goto done;

	window_write(window_current->id, line);

	/* ukryj prompt, je¶li jeste¶my w trakcie readline */
	if (in_readline) {
		int i;

		old_prompt = xstrdup(rl_prompt);
		rl_end = 0;
/*		set_prompt(NULL);*/
		rl_redisplay();
		printf("\r");
		for (i = 0; i < xstrlen(old_prompt); i++)
			printf(" ");
		printf("\r");
	}

	printf("%s", line);

	if (pager_lines >= 0) {
		pager_lines++;

		if (pager_lines >= screen_lines - 2) {
			const gchar *prompt = format_find("readline_more");
			char *lprompt = ekg_recode_to_locale(prompt);
			char *tmp;
				/* XXX: lprompt pretty const, make it static? */

			in_readline++;

			set_prompt(lprompt);

			pager_lines = -1;
			tmp = readline((char *) lprompt);
			g_free(lprompt);
			in_readline--;
			if (tmp) {
				xfree(tmp);
				pager_lines = 0;
			} else {
				printf("\n");
				pager_lines = -2;
			}
			printf("\033[A\033[K");		/* XXX brzydko */
		}
	}

	/* je¶li jeste¶my w readline, poka¿ z powrotem prompt */
	if (in_readline) {
		rl_end = old_end;
		set_prompt(old_prompt);
		xfree(old_prompt);
		rl_forced_update_display();
	}
	
done:
	if (line_buf)
		xfree(line_buf);
}
/*
 * current_prompt()
 *
 * zwraca wska¼nik aktualnego prompta. statyczny bufor, nie zwalniaæ.
 */
/**
 * current_prompt()
 *
 * Get the current prompt, locale-recoded.
 *
 * @return Static buffer pointer, non-NULL, locale-encoded.
 */
const char *current_prompt(void)
{
	static gchar *buf = NULL;
	session_t *s;
	char *tmp, *act, *sid;
	char *format, *format_act;

	if (no_prompt)
		return "";

	s = session_current;
	sid = s ? (s->alias?s->alias:s->uid) : "";

	if (window_current->id > 1) {
		format		= "rl_prompt_query";
		format_act	= "rl_prompt_query_act";
	} else if (s && (s && s->status == EKG_STATUS_INVISIBLE)) {
		format		= "rl_prompt_invisible";
		format_act	= "rl_prompt_invisible_act";
	} else if (s && (s->status < EKG_STATUS_AVAIL)) {
		format		= "rl_prompt_away";
		format_act	= "rl_prompt_away_act";
	} else {
		format		= "rl_prompt";
		format_act	= "rl_prompt_act";
	}

	act = window_activity();
	if (act)
		tmp = format_string(format_find(format_act), sid, ekg_itoa(window_current->id), act, window_current->target);
	else
		tmp = format_string(format_find(format), sid, ekg_itoa(window_current->id), window_current->target);

	g_free(buf);
	buf = ekg_recode_to_locale(tmp);
	g_free(tmp);
	g_free(act);

	return buf;
}

int my_loop() {
	extern void ekg_loop();
	ekg_loop();
	return 0;
}

/*
 * my_readline()
 *
 * malutki wrapper na readline(), który przygotowuje odpowiedniego prompta
 * w zale¿no¶ci od tego, czy jeste¶my zajêci czy nie i informuje resztê
 * programu, ¿e jeste¶my w trakcie czytania linii i je¶li chc± wy¶wietlaæ,
 * powinny najpierw sprz±tn±æ.
 */
char *my_readline()
{
	const char *prompt = current_prompt();
	char *res, *tmp;

	in_readline = 1;
	set_prompt(prompt);
	res = readline((char *) prompt);
	in_readline = 0;

	if (config_print_line) {
			/* XXX: this needs recoding back,
			 * maybe some internal API? */
		tmp = saprintf("%s%s\n", prompt, (res) ? res : "");
		window_write(window_current->id, tmp);
		xfree(tmp);
	} else {
		window_refresh();
	}

	return res;
}

/*
 * ui_readline_loop()
 *
 * g³ówna pêtla programu. wczytuje dane z klawiatury w miêdzyczasie
 * obs³uguj±c sieæ i takie tam.
 */
int ui_readline_loop()
{
	char *line = my_readline();
	char *p;

	if (!line) {
		/* Ctrl-D handler */
		if (window_current->id == 0) {			/* debug window */
			window_switch(1);
		} else if (window_current->id == 1) {		/* status window */
			if (config_ctrld_quits)	{
				return 0;
			} else {
				printf("\n");
			}
		} else if (window_current->id > 1) {		/* query window */
			window_kill(window_current);
		}
		return 1;
	}

	if (xstrlen(line) > 0 && line[xstrlen(line) - 1] == '\\') {
		/* multi line handler */
		string_t s = string_init(NULL);

		line[xstrlen(line) - 1] = 0;

		string_append(s, line);

		xfree(line);

		no_prompt = 1;
		rl_bind_key(9, rl_insert);

		while ((line = my_readline())) {
			if (!xstrcmp(line, "."))
				break;
			string_append(s, line);
			string_append(s, "\r\n");
			xfree(line);
		}

		rl_bind_key(9, rl_complete);
		no_prompt = 0;

		if (line) {
			string_free(s, 1);
			xfree(line);
			return 1;
		}

		line = string_free(s, 0);
	}
		
	/* if no empty line and we save duplicate lines, add it to history */
	if (line && *line && (config_history_savedups || !history_length || xstrcmp(line, history_get(history_length)->line)))
		add_history(line);
	
	pager_lines = 0;
		
	for (p=line; *p && isspace(*p); p++);
	if (*p || config_send_white_lines)
		command_exec(window_current->target, window_current->session, line, 0);

	pager_lines = -1;

	xfree(line);
	return 1;
}

/*
 * wy¶wietla ponownie zawarto¶æ okna.
 *
 * XXX podpi±æ pod Ctrl-L.
 */
int window_refresh() {
	char **p;

	printf("\033[H\033[J"); /* XXX */

	for (p = readline_current->line; *p; p++)
		printf("%s", *p);

	return 0;
}

/*
 * window_write()
 *
 * dopisuje liniê do bufora danego okna.
 */
int window_write(int id, const char *line)
{
	window_t *w = window_exist(id);
	readline_window_t *r = readline_window(w);
	int i = 1;

	if (!line || !w)
		return -1;

	/* je¶li ca³y bufor zajêty, zwolnij pierwsz± liniê i przesuñ do góry */
	if (r->line[MAX_LINES_PER_SCREEN - 1]) {
		xfree(r->line[0]);
		memmove(&(r->line[0]), &(r->line[1]), sizeof(char *) * (MAX_LINES_PER_SCREEN - 1));
		r->line[MAX_LINES_PER_SCREEN - 1] = xstrdup(line);
	} else {
		/* znajd¼ pierwsz± woln± liniê i siê wpisz. */
		for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
			if (!r->line[i]) {
				r->line[i] = xstrdup(line);
				break;
			}
	}

	if (w != window_current) {
		set_prompt(current_prompt());
		rl_redisplay();
	}
	
	return 0;
}

#if 0 /* TODO */
	/* nowe okno w == window_current ? */ {
		set_prompt(current_prompt());
#endif

/*
 * window_activity()
 *
 * zwraca string z actywnymi oknami 
 */
char *window_activity() 
{
	string_t s = string_init("");
	int first = 1;
	window_t *w;

	for (w = windows; w; w = w->next) {
/* we cannot make it colorful with default formats because grey on black doesn't look so good... */
		if (!w->act || !w->id) 
			continue;

		if (!first)
			string_append_c(s, ',');
		string_append(s, ekg_itoa(w->id));
		first = 0;
	}

	if (!first) {
		char *tmp = string_free(s, 0);
		char *act = tmp;
		return act;
	}
	string_free(s, 1);
	return NULL;
}
		
/*
 * bind_find_command()
 *
 * szuka komendy, któr± nale¿y wykonaæ dla danego klawisza.
 */
char *bind_find_command(const char *seq)
{
	struct binding *s;

	if (!seq)
		return NULL;
	
	for (s = bindings; s; s = s->next) {
		if (s->key && !xstrcasecmp(s->key, seq))
			return s->action;
	}

	return NULL;
}

/*
 * bind_handler_ctrl()
 *
 * obs³uguje klawisze Ctrl-A do Ctrl-Z, wywo³uj±c przypisane im akcje.
 */
int bind_handler_ctrl(int a, int key)
{
	char *tmp = saprintf("Ctrl-%c", 'A' + key - 1);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(window_current->target, window_current->session, bind_find_command(tmp), 0);
	if (foo < 0)
		pager_lines = foo;
	xfree(tmp);

	return 0;
}

/*
 * bind_handler_alt()
 *
 * obs³uguje klawisze z Altem, wywo³uj±c przypisane im akcje.
 */
int bind_handler_alt(int a, int key)
{
	char *tmp = saprintf("Alt-%c", key);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(window_current->target, window_current->session, bind_find_command(tmp), 0);
	if (foo < 0)
		pager_lines = foo;
	xfree(tmp);

	return 0;
}

/*
 * bind_handler_window()
 *
 * obs³uguje klawisze Alt-1 do Alt-0, zmieniaj±c okna na odpowiednie.
 */
int bind_handler_window(int a, int key)
{
	if (key > '0' && key <= '9')
		window_switch(key - '0');
	else
		window_switch(10);

	return 0;
}

int bind_sequence(const char *seq, const char *command, int quiet)
{
	char *nice_seq = NULL;
	
	if (!seq)
		return -1;

	if (command && bind_find_command(seq)) {
		printq("bind_seq_exist", seq);
		return -1;
	}
	
	if (!xstrncasecmp(seq, "Ctrl-", 5) && xstrlen(seq) == 6 && xisalpha(seq[5])) {
		int key = CTRL(xtoupper(seq[5]));

		if (command) {
			rl_bind_key(key, bind_handler_ctrl);
			nice_seq = xstrdup(seq);
			nice_seq[0] = xtoupper(nice_seq[0]);
			nice_seq[1] = xtolower(nice_seq[1]);
			nice_seq[2] = xtolower(nice_seq[2]);
			nice_seq[3] = xtolower(nice_seq[3]);
			nice_seq[5] = xtoupper(nice_seq[5]);
		} else
			rl_unbind_key(key);

	} else if (!xstrncasecmp(seq, "Alt-", 4) && xstrlen(seq) == 5) {

		if (command) {
			rl_bind_key_in_map(xtolower(seq[4]), bind_handler_alt, emacs_meta_keymap);
			nice_seq = xstrdup(seq);
			nice_seq[0] = xtoupper(nice_seq[0]);
			nice_seq[1] = xtolower(nice_seq[1]);
			nice_seq[2] = xtolower(nice_seq[2]);
			nice_seq[4] = xtoupper(nice_seq[4]);
		} else
			rl_unbind_key_in_map(xtolower(seq[4]), emacs_meta_keymap);
		
	} else {
		printq("bind_seq_incorrect", seq);

		return -1;
	}

	if (command) {
		struct binding *s;

		s = xmalloc(sizeof(struct binding));
		
		s->key = nice_seq;
		s->action = xstrdup(command);
		s->internal = 0;

		LIST_ADD2(&bindings, s);

		if (!quiet) {
			print("bind_seq_add", s->key);
			config_changed = 1;
		}
	} else {
		struct binding *s;

		for (s = bindings; s; s = s->next) {
			if (s->key && !xstrcasecmp(s->key, seq)) {
				LIST_REMOVE2(&bindings, s, NULL);
				if (!quiet) {
					print("bind_seq_remove", seq);
					config_changed = 1;
				}
				return 0;
			}
		}
	}

	return 1;
}
