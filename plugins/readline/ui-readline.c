/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Adam Osuchowski <adwol@polsl.gliwice.pl>
 *                          Wojtek Bojdo³ <wojboj@htcon.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#include <sys/ioctl.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <history.h>
#include <readline.h>

#include "commands.h"
#include "mail.h"
#ifndef HAVE_STRLCPY
#  include "../compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "ui.h"
#include "userlist.h"
#include "vars.h"
#include "xmalloc.h"

/* podstawmy ewentualnie brakuj±ce funkcje i definicje readline */

extern void rl_extend_line_buffer(int len);
extern char **completion_matches();

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
static char *rl_filename_completion_function()
{
	return NULL;
}
#endif

#ifndef HAVE_RL_SET_PROMPT
int rl_set_prompt(const char *foo)
{
	return -1;
}
#endif

#ifndef HAVE_RL_SET_KEY
int rl_set_key(const char *key, void *function, void *keymap)
{
	return -1;
}
#endif

struct window {
	int id, act;
	char *query_nick;
	char *line[MAX_LINES_PER_SCREEN];
};

static int window_last_id = -1;

/* deklaracje funkcji interfejsu */
static void ui_readline_loop();
static void ui_readline_print(const char *target, int separate, const char *line);
static void ui_readline_beep();
static int ui_readline_event(const char *event, ...);
static void ui_readline_deinit();
static void ui_readline_postinit();

static int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80;
static list_t windows = NULL;
struct window *window_current = NULL;

/* kod okienek napisany jest na podstawie ekg-windows nilsa */
static struct window *window_add();
static int window_remove(int id, int quiet);
static int window_switch(int id, int quiet);
static int window_refresh();
static int window_write(int id, const char *line);
static void window_clear();
static void window_list();
static int window_make_query(const char *nick);
static void window_free();
static struct window *window_find(int id);
static int window_find_query(const char *qnick);
static char *window_activity();

static int bind_sequence(const char *seq, const char *command, int quiet);
static int bind_handler_window(int a, int key);

/*
 * sigcont_handler() //XXX mo¿e wywalaæ
 *
 * os³uguje powrót z t³a poleceniem ,,fg'', ¿eby od¶wie¿yæ ekran.
 */
static void sigcont_handler()
{
	rl_forced_update_display();
	signal(SIGCONT, sigcont_handler);
}

/*
 * sigint_handler() //XXX mo¿e wywalaæ 
 *
 * obs³uguje wci¶niêcie Ctrl-C.
 */
static void sigint_handler()
{
	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	putchar('\n');
	rl_forced_update_display();
	signal(SIGINT, sigint_handler);
}

#ifdef SIGWINCH
/*
 * sigwinch_handler()
 *
 * obs³uguje zmianê rozmiaru okna.
 */
static void sigwinch_handler()
{
	ui_resize_term = 1;
	signal(SIGWINCH, sigwinch_handler);
}
#endif

/*
 * my_getc()
 *
 * pobiera znak z klawiatury obs³uguj±c w miêdzyczasie sieæ.
 */
static int my_getc(FILE *f)
{
	ekg_wait_for_key();

	if (ui_resize_term) {
		ui_resize_term = 0;
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

static char *command_generator(char *text, int state)
{
	static int len;
	static list_t l;
	int slash = 0;

	if (*text == '/') {
		slash = 1;
		text++;
	}

	if (!*rl_line_buffer) {
		char *nick, *ret;
		if (state)
			return NULL;
		if (send_nicks_count < 1)
			return saprintf((window_current->query_nick) ? "/%s" : "%s", (config_tab_command) ? config_tab_command : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;

		nick = ((xstrchr(send_nicks[0], ' ')) ? saprintf("\"%s\"", send_nicks[0]) : xstrdup(send_nicks[0])); 
		ret = saprintf((window_current->query_nick) ? "/%s %s" : "%s %s", (config_tab_command) ? config_tab_command : "chat", nick);
		xfree(nick);
		return ret;
	}

	if (!state) {
		l = commands;
		len = xstrlen(text);
	}

	for (; l; l = l->next) {
		struct command *c = l->data;

		if (!xstrncasecmp(text, c->name, len)) {
			l = l->next;
			return (window_current->query_nick) ? saprintf("/%s", c->name) : xstrdup(c->name);
		}
	}

	return NULL;
}

static char *known_uin_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

	while (l) {
		struct userlist *u = l->data;

		l = l->next;

		if (u->display && !xstrncasecmp(text, u->display, len))
			return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
	}

	return NULL;
}

static char *unknown_uin_generator(char *text, int state)
{
	static int index = 0, len;

	if (!state) {
		index = 0;
		len = xstrlen(text);
	}

	while (index < send_nicks_count)
		if (xisdigit(send_nicks[index++][0]))
			if (!xstrncasecmp(text, send_nicks[index - 1], len))
				return xstrdup(send_nicks[index - 1]);

	return NULL;
}

static char *variable_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = variables;
		len = xstrlen(text);
	}

	while (l) {
		ekg_var_t *v = l->data;
		
		l = l->next;
		
		if (v->type == VAR_FOREIGN)
			continue;

		if (*text == '-') {
			if (!xstrncasecmp(text + 1, v->name, len - 1))
				return saprintf("-%s", v->name);
		} else {
			if (!xstrncasecmp(text, v->name, len))
				return xstrdup(v->name);
		}
	}

	return NULL;
}

static char *ignored_uin_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

	while (l) {
		struct userlist *u = l->data;

		l = l->next;

		if (!ignored_check(u->uin))
			continue;

		if (!u->display) {
			if (!xstrncasecmp(text, itoa(u->uin), len))
				return xstrdup(itoa(u->uin));
		} else {
			if (u->display && !xstrncasecmp(text, u->display, len))
				return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
		}
	}

	return NULL;
}

static char *blocked_uin_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

	while (l) {
		struct userlist *u = l->data;

		l = l->next;

		if (!ekg_group_member(u, "__blocked"))
			continue;

		if (!u->display) {
			if (!xstrncasecmp(text, itoa(u->uin), len))
				return xstrdup(itoa(u->uin));
		} else {
			if (u->display && !xstrncasecmp(text, u->display, len))
				return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
		}
	}

	return NULL;
}

static char *dcc_generator(char *text, int state)
{
	char *commands[] = { "close", "get", "send", "list", "resume", "rsend", "rvoice", "voice", NULL };
	static int len, i;

	if (!state) {
		i = 0;
		len = xstrlen(text);
	}

	while (commands[i]) {
		if (!xstrncasecmp(text, commands[i], len))
			return xstrdup(commands[i++]);
		i++;
	}

	return NULL;
}

static char *window_generator(char *text, int state)
{
	char *commands[] = { "new", "kill", "next", "prev", "switch", "clear", "refresh", "list", "last", "active", NULL };
	static int len, i;

	if (!state) {
		i = 0;
		len = xstrlen(text);
	}

	while (commands[i]) {
		if (!xstrncasecmp(text, commands[i], len))
			return xstrdup(commands[i++]);
		i++;
	}

	return NULL;
}

static char *python_generator(char *text, int state)
{
	char *commands[] = { "load", "unload", "run", "exec", "list", NULL };
	static int len, i;

	if (!state) {
		i = 0;
		len = xstrlen(text);
	}

	while (commands[i]) {
		if (!xstrncasecmp(text, commands[i], len))
			return xstrdup(commands[i++]);
		i++;
	}

	return NULL;
}

static char *empty_generator(char *text, int state)
{
	return NULL;
}

static char **my_completion(char *text, int start, int end)
{
	char *params = NULL;
	int word = 0, i, abbrs = 0;
	CPFunction *func = known_uin_generator;
	list_t l;
	static int my_send_nicks_count = 0;

	if (start) {
		char *tmp = rl_line_buffer, *cmd = (config_tab_command) ? config_tab_command : "chat";
		int slash = 0;

		if (*tmp == '/') {
			slash = 1;
			tmp++;
		}

		if (!xstrncasecmp(tmp, cmd, xstrlen(cmd)) && tmp[xstrlen(cmd)] == ' ') {
			int in_quote = 0;
			word = 0;
			for (i = 0; i < xstrlen(rl_line_buffer); i++) {
				if (rl_line_buffer[i] == '"')
					in_quote = !in_quote;

				if (xisspace(rl_line_buffer[i]) && !in_quote)
					word++;
			}
			if (word == 2 && xisspace(rl_line_buffer[xstrlen(rl_line_buffer) - 1])) {
				if (send_nicks_count != my_send_nicks_count) {
					my_send_nicks_count = send_nicks_count;
					send_nicks_index = 0;
				}

				if (send_nicks_count > 0) {
					char buf[100], *tmp;

					tmp = ((xstrchr(send_nicks[send_nicks_index], ' ')) ? saprintf("\"%s\"", send_nicks[send_nicks_index]) : xstrdup(send_nicks[send_nicks_index]));
					snprintf(buf, sizeof(buf), "%s%s %s ", (slash) ? "/" : "", cmd, tmp);
					xfree(tmp);
					send_nicks_index++;
					rl_extend_line_buffer(xstrlen(buf));
					xstrcpy(rl_line_buffer, buf);
					rl_end = xstrlen(buf);
					rl_point = rl_end;
					rl_redisplay();
				}

				if (send_nicks_index == send_nicks_count)
					send_nicks_index = 0;
					
				return NULL;
			}
			word = 0;
		}
	}

	if (start) {
		int in_quote = 0;

		for (i = 1; i <= start; i++) {
			if (rl_line_buffer[i] == '"')
				in_quote = !in_quote;

			if (xisspace(rl_line_buffer[i]) && !xisspace(rl_line_buffer[i - 1]) && !in_quote)
				word++;
		}
		word--;

		for (l = commands; l; l = l->next) {
			struct command *c = l->data;
			int len = xstrlen(c->name);
			char *cmd = (*rl_line_buffer == '/') ? rl_line_buffer + 1 : rl_line_buffer;

			if (!xstrncasecmp(cmd, c->name, len) && xisspace(cmd[len])) {
				params = c->params;
				abbrs = 1;
				break;
			}
			
			for (len = 0; cmd[len] && cmd[len] != ' '; len++);

			if (!xstrncasecmp(cmd, c->name, len)) {
				params = c->params;
				abbrs++;
			} else
				if (params && abbrs == 1)
					break;
		}

		if (params && abbrs == 1) {
			if (word >= xstrlen(params))
				func = empty_generator;
			else {
				switch (params[word]) {
					case 'u':
						func = known_uin_generator;
	    					break;
					case 'U':
						func = unknown_uin_generator;
						break;
					case 'c':
						func = command_generator;
						break;
					case 's':	/* XXX */
						func = empty_generator;
						break;
					case 'i':
						func = ignored_uin_generator;
						break;
					case 'b':
						func = blocked_uin_generator;
						break;
					case 'v':
						func = variable_generator;
						break;
					case 'd':
						func = dcc_generator;
						break;
					case 'f':
						func = rl_filename_completion_function;
						break;
					case 'p':
						func = python_generator;
						break;
					case 'w':
						func = window_generator;
						break;
					default:
						func = empty_generator;
						break;
				}
			}
		}
	}

	if (start == 0)
		func = command_generator;

	return completion_matches(text, func);
}

/*
 * ui_readline_print()
 *
 * wy¶wietla dany tekst na ekranie, uwa¿aj±c na trwaj±ce w danych chwili
 * readline().
 */
static void ui_readline_print(const char *target, int separate, const char *xline)
{
        int old_end = rl_end, id = 0;
	char *old_prompt = NULL, *line_buf = NULL;
	const char *p, *line = NULL;
	string_t s = NULL;

	if (target && !xstrcmp(target, "__debug"))
		return;

	if (config_timestamp) {
		string_t s = string_init(NULL);
		const char *p = xline;
		char buf[80];
		struct tm *tm;
		time_t t;

		t = time(NULL);
		tm = localtime(&t);
		/* I KNOW THIS PLUGIN IS UNUSED, BUT THIS PIECE
		 * OF CODE IS SHITTY */
		if (!strftime(buf, sizeof(buf), config_timestamp, tm))
			xstrcpy(buf, "TOOLONG");

		string_append(s, buf);
		
		while (*p) {
			if (*p == '\n' && *(p + 1)) {
				string_append_c(s, '\n');
				string_append(s, buf);
			} else
				string_append_c(s, *p);

			p++;
		}

		line = line_buf = string_free(s, 0);
	} else
		line = xline;

	if (config_speech_app) {
		int in_esc_code = 0;
		
		s = string_init(NULL);

		for (p = line; *p; p++) {
			if (*p == 27) 
				in_esc_code = 1;

			/* zak³adamy, ¿e 'm' koñczy eskejpow± sekwencje */
			if (in_esc_code && *p == 'm') {
				in_esc_code = 0;
				continue;
			}
			
			if (!in_esc_code) 
				string_append_c(s, *p);
		}
	}

	/* znajd¼ odpowiednie okienko i ewentualnie je utwórz */
	if (target && separate)
		id = window_find_query(target);

	if (config_make_window > 0 && !id && strncmp(target, "__", 2) && separate)
		id = window_make_query(target);
	
	/* je¶li nie piszemy do aktualnego, to zapisz do bufora i wyjd¼ */
        if (id && id != window_current->id) {
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
/*		rl_set_prompt(NULL); */
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
			const char *prompt = format_find("readline_more");
			char *tmp;
			
			in_readline++;
		        rl_set_prompt((char *) prompt);
			pager_lines = -1;
			tmp = readline((char *) prompt);
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
		rl_set_prompt(old_prompt);
		xfree(old_prompt);
		rl_forced_update_display();
	}
	
done:
	if (line_buf)
		xfree(line_buf);

	/* say it! ;) */
	if (config_speech_app) {
		say_it(s->str);
		string_free(s, 1);
	}
}

/*
 * ui_readline_beep()
 *
 * wydaje d¼wiêk na konsoli.
 */
static void ui_readline_beep()
{
	printf("\a");
	fflush(stdout);
}

/*
 * current_prompt()
 *
 * zwraca wska¼nik aktualnego prompta. statyczny bufor, nie zwalniaæ.
 */
static const char *current_prompt()
{
	static char buf[80];
	const char *prompt = buf;
	int count = list_count(windows);
	char *tmp, *act = window_activity();

        if (window_current->query_nick) {
		if (count > 1 || window_current->id != 1) {
			if (act) {
				tmp = format_string(format_find("readline_prompt_query_win_act"), window_current->query_nick, itoa(window_current->id), act);
				xfree(act);
			} else
				tmp = format_string(format_find("readline_prompt_query_win"), window_current->query_nick, itoa(window_current->id));
		} else
			tmp = format_string(format_find("readline_prompt_query"), window_current->query_nick, NULL);
		strlcpy(buf, tmp, sizeof(buf));
		xfree(tmp);
        } else {
		char *format_win = "readline_prompt_win", *format_nowin = "readline_prompt", *format_win_act = "readline_prompt_win_act";
			
		if (GG_S_B(config_status)) {
			format_win = "readline_prompt_away_win";
			format_nowin = "readline_prompt_away";
			format_win_act = "readline_prompt_away_win_act";
		}

		if (GG_S_I(config_status)) {
			format_win = "readline_prompt_invisible_win";
			format_nowin = "readline_prompt_invisible";
			format_win_act = "readline_prompt_invisible_win_act";
		}
		
		if (count > 1 || window_current->id != 1) {
			if (act) {
				tmp = format_string(format_find(format_win_act), itoa(window_current->id), act);
				xfree(act);
			} else
				tmp = format_string(format_find(format_win), itoa(window_current->id));
			strlcpy(buf, tmp, sizeof(buf));
			xfree(tmp);
		} else
			strlcpy(buf, format_find(format_nowin), sizeof(buf));
        }

        if (no_prompt)
                prompt = "";

        return prompt;
}

/*
 * my_readline()
 *
 * malutki wrapper na readline(), który przygotowuje odpowiedniego prompta
 * w zale¿no¶ci od tego, czy jeste¶my zajêci czy nie i informuje resztê
 * programu, ¿e jeste¶my w trakcie czytania linii i je¶li chc± wy¶wietlaæ,
 * powinny najpierw sprz±tn±æ.
 */
static char *my_readline()
{
	const char *prompt = current_prompt();
        char *res, *tmp;

        in_readline = 1;
	rl_set_prompt(prompt);
        res = readline((char *) prompt);
        in_readline = 0;

	tmp = saprintf("%s%s\n", prompt, (res) ? res : "");
        window_write(window_current->id, tmp);
	xfree(tmp);

        return res;
}

/*
 * ui_readline_init()
 *
 * inicjalizacja interfejsu readline.
 */
void ui_readline_init()
{
	char c;

        window_current = window_add();
        window_refresh();

	ui_print = ui_readline_print;
	ui_postinit = ui_readline_postinit;
	ui_loop = ui_readline_loop;
	ui_beep = ui_readline_beep;
	ui_event = ui_readline_event;
	ui_deinit = ui_readline_deinit;
		
	rl_initialize();
	rl_getc_function = my_getc;
	rl_readline_name = "gg";
	rl_attempted_completion_function = (CPPFunction *) my_completion;
	rl_completion_entry_function = (void*) empty_generator;

	rl_set_key("\033[[A", binding_help, emacs_standard_keymap);
	rl_set_key("\033OP", binding_help, emacs_standard_keymap);
	rl_set_key("\033[11~", binding_help, emacs_standard_keymap);
	rl_set_key("\033[M", binding_help, emacs_standard_keymap);
	rl_set_key("\033[[B", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033OQ", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033[12~", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033[N", binding_quick_list, emacs_standard_keymap);
	
	//rl_set_key("\033[24~", binding_toggle_debug, emacs_standard_keymap);

	for (c = '0'; c <= '9'; c++)
		rl_bind_key_in_map(c, bind_handler_window, emacs_meta_keymap);
	
	signal(SIGINT, sigint_handler);
	signal(SIGCONT, sigcont_handler);

#ifdef SIGWINCH
	signal(SIGWINCH, sigwinch_handler);
#endif

	rl_get_screen_size(&screen_lines, &screen_columns);

	if (screen_lines < 1)
		screen_lines = 24;
	if (screen_columns < 1)
		screen_columns = 80;

	ui_screen_width = screen_columns;
	ui_screen_height = screen_lines;
	ui_resize_term = 0;
}

/*
 * ui_readline_postinit()
 *
 * funkcja uruchamiana po wczytaniu konfiguracji.
 */
static void ui_readline_postinit()
{

}

/*
 * ui_readline_deinit()
 *
 * zamyka to, co zwi±zane z interfejsem.
 */
static void ui_readline_deinit()
{
	window_free();
}

/*
 * ui_readline_loop()
 *
 * g³ówna pêtla programu. wczytuje dane z klawiatury w miêdzyczasie
 * obs³uguj±c sieæ i takie tam.
 */
static void ui_readline_loop()
{
	for (;;) {
		char *line = my_readline();

		/* je¶li wci¶niêto Ctrl-D i jeste¶my w query, wyjd¼my */
		if (!line && window_current->query_nick) {
			ui_event("command", 0, "query", NULL);
			continue;
		}

		/* je¶li wci¶niêto Ctrl-D, to zamknij okienko */
		if (!line && list_count(windows) > 1) {
			window_remove(window_current->id, 0);
			continue;
		}

		if (!line) {
			if (config_ctrld_quits)	
				break;
			else {
				printf("\n");
				continue;
			}
		}

		if (xstrlen(line) > 0 && line[xstrlen(line) - 1] == '\\') {
			string_t s = string_init(NULL);

			line[xstrlen(line) - 1] = 0;

			string_append(s, line);

			xfree(line);

			no_prompt = 1;
			rl_bind_key(9, rl_insert);

			while ((line = my_readline())) {
				if (!xstrcmp(line, ".")) {
					xfree(line);
					break;
				}
				string_append(s, line);
				string_append(s, "\r\n");
				xfree(line);
			}

			rl_bind_key(9, rl_complete);

			no_prompt = 0;

			if (!line) {
				printf("\n");
				string_free(s, 1);
				continue;
			}

			line = string_free(s, 0);
		}
		
		/* je¶li linia nie jest pusta, dopisz do historii */
		if (*line)
			add_history(line);
		
		pager_lines = 0;
		
		command_exec(window_current->query_nick, line, 0);

		pager_lines = -1;

		xfree(line);
	}

	if (!batch_mode && !quit_message_send) {
		putchar('\n');
		print("quit");
		putchar('\n');
		quit_message_send = 1;
	}

}

/*
 * ui_readline_event()
 *
 * obs³uga zdarzeñ wysy³anych z ekg do interfejsu.
 */
static int ui_readline_event(const char *event, ...)
{
	va_list ap;
	int result = 0;

	va_start(ap, event);
	
        if (!xstrcmp(event, "variable_changed")) {
		char *name = va_arg(ap, char*);

		if (!xstrcasecmp(name, "sort_windows") && config_sort_windows) {
			list_t l;
			int id = 1;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				w->id = id++;
			}
		}
	}

	if (!xstrcasecmp(event, "command")) {
		int quiet = va_arg(ap, int);
		char *command = va_arg(ap, char*);

		if (!xstrcasecmp(command, "query-current")) {
			int *param = va_arg(ap, uin_t*);

			if (window_current->query_nick)
				*param = get_uin(window_current->query_nick);
			else
				*param = 0;

			goto cleanup;
		}

		if (!xstrcasecmp(command, "query-nicks")) {
			char ***param = va_arg(ap, char***);
			list_t l;

			for (l = windows; l; l = l->next) {
				struct window *w = l->data;

				if (!w->query_nick)
					continue;
				else
					array_add(param, xstrdup(w->query_nick));
			}

			goto cleanup;
		}

		if (!xstrcasecmp(command, "query")) {
			char *param = va_arg(ap, char*);	
			
			if (!param && !window_current->query_nick)
				goto cleanup;

			if (param) {
				int id;

				if ((id = window_find_query(param))) {
					printq("query_exist", param, itoa(id));
					return 1;
				}
				
				printq("query_started", param);
				xfree(window_current->query_nick);
				window_current->query_nick = xstrdup(param);

			} else {
				if (!quiet)
					printf("\n");	/* XXX brzydkie */
				printq("query_finished", window_current->query_nick);
				xfree(window_current->query_nick);
				window_current->query_nick = NULL;
			}

			result = 1;
		}

		if (!xstrcasecmp(command, "find")) {
			char *tmp = NULL;
			
			if (window_current->query_nick) {
				struct userlist *u = userlist_find(0, window_current->query_nick);
				struct conference *c = conference_find(window_current->query_nick);
				int uin;

				if (u && u->uin)
					tmp = saprintf("find %d", u->uin);

				if (c && c->name)
					tmp = saprintf("/conference --find %s", c->name);

				if (!u && (uin = atoi(window_current->query_nick)))
					tmp = saprintf("find %d", uin);
			}

			if (!tmp)
				tmp = saprintf("find %d", config_uin);

			command_exec(NULL, tmp, 0);

			xfree(tmp);

			goto cleanup;
		}

		if (!xstrcasecmp(command, "window")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*);

			if (!p1 || !xstrcasecmp(p1, "list")) {
				if (!quiet)
					window_list();
			} else if (!xstrcasecmp(p1, "last")) {
				if (window_last_id != -1)
					window_switch(window_last_id, quiet);
			} else if (!xstrcasecmp(p1, "active")) {
				list_t l;

				for (l = windows; l; l = l->next) {
					struct window *w = l->data;

					if (w->act)
						window_switch(w->id, quiet);
				}

		        } else if (!xstrcasecmp(p1, "new")) {
		                window_add();
 
			} else if (!xstrcasecmp(p1, "next")) {
		                window_switch(window_current->id + 1, quiet);

			} else if (!xstrcasecmp(p1, "prev")) {
		                window_switch(window_current->id - 1, quiet);
				
		        } else if (!xstrcasecmp(p1, "kill")) {
		                int id = (p2) ? atoi(p2) : window_current->id;

				window_remove(id, quiet);
				
		        } else if (!xstrcasecmp(p1, "switch")) {
		                if (!p2) {
		                        printq("not_enough_params", "window");
		                } else
		                	window_switch(atoi(p2), quiet);

			} else if (!xstrcasecmp(p1, "refresh")) {
		                window_refresh();

			} else if (!xstrcasecmp(p1, "clear")) {
		                window_clear();
				window_refresh();

			} else
				printq("invalid_params", "window");

			result = 1;
		}

		if (!xstrcasecmp(command, "bind")) {
			char *p1 = va_arg(ap, char*), *p2 = va_arg(ap, char*), *p3 = va_arg(ap, char*);
			
			if (p1 && (!xstrcasecmp(p1, "-a") || !xstrcasecmp(p1, "--add"))) {
				if ((!p2 || !p3) && !quiet)
					print("not_enough_params", "bind");
				else
					bind_sequence(p2, p3, quiet);
			
			} else if (p1 && (!xstrcasecmp(p1, "-d") || !xstrcasecmp(p1, "--del"))) {
				if (!p2 && !quiet)
					print("not_enough_params", "bind");
				else
					bind_sequence(p2, NULL, quiet);
			
			} else {
				if (p1 && (!xstrcasecmp(p1, "-l") || !xstrcasecmp(p1, "--list")))
					binding_list(quiet, p2, 0);
				else
					binding_list(quiet, p1, 0);
			}

			result = 1;
		}
	}

	if (!xstrcasecmp(event, "check_mail"))
		check_mail();

cleanup:
	va_end(ap);

	return result;
}

/*
 * window_find()
 *
 * szuka struct window dla okna o podanym numerku.
 *
 *  - id - numer okna.
 *
 * struct window dla danego okna.
 */
static struct window *window_find(int id)
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->id == id)
			return w;
	}

	return NULL;
}

/*
 * window_add()
 *
 * tworzy nowe okno.
 *
 * zwraca zaalokowan± struct window.
 */
static struct window *window_add()
{
        struct window w;
	int id = 1;

	/* wyczy¶æ. */
	memset(&w, 0, sizeof(w));

	/* znajd¼ pierwszy wolny id okienka. */
	while (window_find(id))
		id++;
	w.id = id;

	/* dopisz, zwróæ. */
        return list_add(&windows, &w, sizeof(w));
}

/*
 * window_remove()
 *
 * usuwa okno o podanym numerze.
 */
static int window_remove(int id, int quiet)
{
	struct window *w;
	int i;

	/* je¶li zosta³o jedno okienko, nie usuwaj niczego. */
        if (list_count(windows) < 2) {
                printq("window_no_windows");
                return -1;
        }

	/* je¶li nie ma takiego okienka, id¼ sobie. */
	if (!(w = window_find(id))) {
        	printq("window_noexist");
		return -1;
	}

	/* je¶li usuwano aktualne okienko, nie kombinuj tylko ustaw 1-sze. */
	if (window_current == w) {
		struct window *newwin = windows->data;

		/* je¶li usuwane jest pierwszy, we¼ drugie. */
		if (newwin == w) 
			newwin = windows->next->data;
		
		window_switch(newwin->id, 0);
	}

	/* i sortujemy okienka, w razie potrzeby... */
	if (config_sort_windows) {
		list_t l;
		int wid = w->id;

		for (l = windows; l; l = l->next) {
			struct window *wtmp = l->data;
			
			if (wtmp->id > wid)
				wtmp->id--;
		}
	}

	/* usuñ dane zajmowane przez okno. */
	for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
		xfree(w->line[i]);
		w->line[i] = NULL;
	}
	xfree(w->query_nick);
	w->query_nick = NULL;
	
	list_remove(&windows, w, 1);

        return 0;
}

/*
 * window_switch()
 *
 * prze³±cza do okna o podanym id.
 */
static int window_switch(int id, int quiet)
{
	struct window *w = window_find(id);

	if (!w) {
		printq("window_noexist");
		return -1;
	}

	if (id != window_current->id)
		window_last_id = window_current->id;

	window_current = w;
	w->act = 0;
	window_refresh();
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt((char *) current_prompt());
#else
	rl_expand_prompt((char *) current_prompt());
#endif
	rl_initialize();

	return 0;
}

/*
 * window_refresh()
 *
 * wy¶wietla ponownie zawarto¶æ okna.
 *
 * XXX podpi±æ pod Ctrl-L.
 */
static int window_refresh()
{
        int i;

        printf("\033[H\033[J"); /* XXX */

	for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
		if (window_current->line[i])
			printf("%s", window_current->line[i]);

        return 0;
}

/*
 * window_write()
 *
 * dopisuje liniê do bufora danego okna.
 */
static int window_write(int id, const char *line)
{
        struct window *w = window_find(id);
        int i = 1;

        if (!line || !w)
                return -1;

	/* je¶li ca³y bufor zajêty, zwolnij pierwsz± liniê i przesuñ do góry */
	if (w->line[MAX_LINES_PER_SCREEN - 1]) {
		xfree(w->line[0]);
		for (i = 1; i < MAX_LINES_PER_SCREEN; i++)
			w->line[i - 1] = w->line[i];
		w->line[MAX_LINES_PER_SCREEN - 1] = NULL;
	}

	/* znajd¼ pierwsz± woln± liniê i siê wpisz. */
	for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
		if (!w->line[i]) {
			w->line[i] = xstrdup(line);
			break;
		}

	if (w != window_current) {
		w->act = 1;
#ifdef HAVE_RL_SET_PROMPT
		rl_set_prompt((char *) current_prompt());
#else
		rl_expand_prompt((char *) current_prompt());
#endif
		rl_redisplay();
	}
	
        return 0;
}

/*
 * window_clear()
 *
 * czy¶ci zawarto¶æ aktualnego okna.
 */
static void window_clear()
{
        int i;

        for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
		xfree(window_current->line[i]);
                window_current->line[i] = NULL;
	}
}

/*
 * window_find_query()
 *
 * znajduje id okna, w którym prowadzona jest rozmowa z dan± osob±. je¶li
 * nie ma takiego, zwraca zero.
 */
static int window_find_query(const char *nick)
{
        list_t l;

        if (!nick)
                return 0;

        for (l = windows; l; l = l->next) {
                struct window *w = l->data;

		if (w->query_nick && !xstrcasecmp(w->query_nick, nick))
			return w->id;
        }

        return 0;
}

/*
 * window_list()
 *
 * wy¶wietla listê okien.
 */
static void window_list()
{
	list_t l;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;

		if (w->query_nick)
			print("window_list_query", itoa(w->id), w->query_nick);
		else
			print("window_list_nothing", itoa(w->id));
	}
}		

/*
 * window_make_query()
 *
 * tworzy nowe okno rozmowy w zale¿no¶ci od aktualnych ustawieñ.
 */
static int window_make_query(const char *nick)
{
	/* szuka pierwszego wolnego okienka i je zajmuje */
	if (config_make_window == 1) {
		list_t l;

		for (l = windows; l; l = l->next) {
			struct window *w = l->data;
			
			if (!w->query_nick) {
				w->query_nick = xstrdup(nick);
				
				if (w == window_current) {
					print("query_started", nick);
#ifdef HAVE_RL_SET_PROMPT
					rl_set_prompt((char *) current_prompt());
#else
					rl_expand_prompt((char *) current_prompt());
#endif									
				} else
					print("window_id_query_started", itoa(w->id), nick);

				if (!(ignored_check(get_uin(nick)) & IGNORE_EVENTS))
					event_check(EVENT_QUERY, get_uin(nick), nick);
				
				return w->id;
			}
		}
	}
	
	if (config_make_window == 1 || config_make_window == 2) {
		struct window *w;

		if (!(w = window_add()))
			return 0;
		
		w->query_nick = xstrdup(nick);
		
		print("window_id_query_started", itoa(w->id), nick);

		if (!(ignored_check(get_uin(nick)) & IGNORE_EVENTS))
			event_check(EVENT_QUERY, get_uin(nick), nick);
			
		return w->id;
	}

	return 0;
}

/*
 * window_free()
 *
 * zwalnia pamiêæ po wszystkich strukturach okien.
 */
static void window_free()
{
	list_t l;

	window_current = NULL;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;
		int i;

		xfree(w->query_nick);
		w->query_nick = NULL;

		for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
			xfree(w->line[i]);
			w->line[i] = NULL;
		}
	}

	list_destroy(windows, 1);
	windows = NULL;
}
/*
 * window_activity()
 *
 * zwraca string z actywnymi oknami 
 */
static char *window_activity() 
{
	string_t s = string_init("");
	int first = 1;
	list_t l;
	char *act = NULL;

	for (l = windows; l; l = l->next) {
		struct window *w = l->data;
		
		if (w->act) {
			if (!first)
				string_append_c(s, ',');
			string_append(s, itoa(w->id));
			first = 0;
		}
	}

	if (!first)
		act = xstrdup(s->str);
	
	string_free(s, 1);
	
	return act;
}
		
/*
 * bind_find_command()
 *
 * szuka komendy, któr± nale¿y wykonaæ dla danego klawisza.
 */
static char *bind_find_command(const char *seq)
{
	list_t l;

	if (!seq)
		return NULL;
	
	for (l = bindings; l; l = l->next) {
		struct binding *s = l->data;

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
static int bind_handler_ctrl(int a, int key)
{
	char *tmp = saprintf("Ctrl-%c", 'A' + key - 1);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(NULL, bind_find_command(tmp), 0);
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
static int bind_handler_alt(int a, int key)
{
	char *tmp = saprintf("Alt-%c", key);
	int foo = pager_lines;

	if (foo < 0)
		pager_lines = 0;
	command_exec(NULL, bind_find_command(tmp), 0);
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
static int bind_handler_window(int a, int key)
{
	if (key > '0' && key <= '9')
		window_switch(key - '0', 0);
	else
		window_switch(10, 0);

	return 0;
}
		
static int bind_sequence(const char *seq, const char *command, int quiet)
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
		struct binding s;
		
		s.key = nice_seq;
		s.action = xstrdup(command);
		s.internal = 0;
		s.arg = s.default_action = s.default_arg = NULL;

		list_add(&bindings, &s, sizeof(s));

		if (!quiet) {
			print("bind_seq_add", s.key);
			config_changed = 1;
		}
	} else {
		list_t l;

		for (l = bindings; l; l = l->next) {
			struct binding *s = l->data;

			if (s->key && !xstrcasecmp(s->key, seq)) {
				list_remove(&bindings, s, 1);
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
