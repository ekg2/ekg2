/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
                           Adam Osuchowski <adwol@polsl.gliwice.pl>
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

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <sys/ioctl.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <history.h>
#include <readline.h>

#include <ekg/commands.h>
#ifndef HAVE_STRLCPY
#  include <compat/strlcpy.h>
#endif
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/windows.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include "ui-readline.h"

int in_readline = 0, no_prompt = 0, pager_lines = -1, screen_lines = 24, screen_columns = 80;

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
char *rl_filename_completion_function()
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

char *command_generator(char *text, int state)
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
			return saprintf((window_current->target) ? "/%s" : "%s", (config_tab_command) ? config_tab_command : "msg");
		send_nicks_index = (send_nicks_count > 1) ? 1 : 0;

		nick = ((xstrchr(send_nicks[0], ' ')) ? saprintf("\"%s\"", send_nicks[0]) : xstrdup(send_nicks[0])); 
		ret = saprintf((window_current->target) ? "/%s %s" : "%s %s", (config_tab_command) ? config_tab_command : "chat", nick);
		xfree(nick);
		return ret;
	}

	if (!state) {
		l = commands;
		len = xstrlen(text);
	}

	for (; l; l = l->next) {
		command_t *c = l->data;
		char *without_sess_id = NULL;
		int plen = 0;
		session_t *session = session_current;

		if (session && !xstrncasecmp(c->name, session->uid, plen))
			without_sess_id = xstrchr(c->name, ':');

		if (!xstrncasecmp(text, c->name, len)) {
			l = l->next;
			return (window_current->target) ? saprintf("/%s", c->name) : xstrdup(c->name);
		}
		else if (without_sess_id && !xstrncasecmp(text, without_sess_id + 1, len)) {
			l = l->next;
			return (window_current->target) ? saprintf("%s", without_sess_id + 1) : xstrdup(without_sess_id + 1);
		}
			
	}

	return NULL;
}

char *known_uin_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

	while (l) {
		userlist_t *u = l->data;

		l = l->next;
#if 0 /* dark */
		if (u && !xstrncasecmp(text, u->display, len))
			return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
#endif
	}

	return NULL;
}

char *unknown_uin_generator(char *text, int state)
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

char *variable_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = variables;
		len = xstrlen(text);
	}

	while (l) {
		variable_t *v = l->data;
		
		l = l->next;
		
		if (v->type == VAR_FOREIGN || !v->ptr)
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

char *ignored_uin_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!session_current) 
		return NULL;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

	while (l) {
		userlist_t *u = l->data;

		l = l->next;

		if (!ignored_check(session_current, u->uid))
			continue;
#if 0 /* dark */
		if (!u->display) {
			if (!xstrncasecmp(text, itoa(u->uin), len))
				return xstrdup(itoa(u->uin));
		} else {
			if (!xstrncasecmp(text, u->display, len))
				return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
		}
#endif
	}

	return NULL;
}

char *blocked_uin_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = userlist;
		len = xstrlen(text);
	}

	while (l) {
		userlist_t *u = l->data;

		l = l->next;

		if (!ekg_group_member(u, "__blocked"))
			continue;
#if 0 /* dark */
		if (!u->display) {
			if (!xstrncasecmp(text, itoa(u->uin), len))
				return xstrdup(itoa(u->uin));
		} else {
			if (u->display && !xstrncasecmp(text, u->display, len))
				return ((xstrchr(u->display, ' ')) ? saprintf("\"%s\"", u->display) : xstrdup(u->display));
		}
#endif
	}

	return NULL;
}

char *window_generator(char *text, int state)
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

char *reason_generator(char *text, int state)
{
	static int len;
/* hm, look at ncurses/completion.c */
	if (!state) {
		len = xstrlen(text);
	}

	return NULL;
}
char *empty_generator(char *text, int state)
{
	return NULL;
}

char *possibilities_generator(char *text, int state)
{
	return NULL;
}

char *session_generator(char *text, int state)
{
	static list_t l;
	static int len;

	if (!state) {
		l = sessions;
		len = xstrlen(text);
	}
	while (l) {
		session_t *s = l->data;
		if (*text == '-') {
			if (!xstrncasecmp(text+1, s->uid, len-1))
				return saprintf("-%s", s->uid);
			if (!xstrncasecmp(text+1, s->alias, len-1))
				return saprintf("-%s", s->alias);
		} else {
			if (!xstrncasecmp(text, s->uid, len)) 
				return xstrdup(s->uid);
			if (!xstrncasecmp(text, s->alias, len)) 
				return xstrdup(s->alias);
		}
	}
	return NULL;
}

char **my_completion(char *text, int start, int end)
{
	char **params = NULL;
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
					strlcpy(rl_line_buffer, buf, xstrlen(buf) + 1);
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
			command_t *c = l->data;
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
			if (word >= /* xstrlen(params) */ array_count(params))
				func = empty_generator;
			else {
				switch (params[word][0]) {
					case 'u':
						func = known_uin_generator;
	    					break;
					case 'U':
						func = unknown_uin_generator;
						break;
					case 'c':
						func = command_generator;
						break;
					case 'p':
						func = possibilities_generator;
						break;
					case 's':
						func = session_generator;
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
					case 'f':
						func = rl_filename_completion_function;
						break;
					case 'r':
						func = reason_generator;
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
void ui_readline_print(window_t *w, int separate, const char *xline)
{
        int old_end = rl_end, id = w->id;
	char *old_prompt = NULL, *line_buf = NULL;
	const char *p, *line = NULL;
	string_t s = NULL;
	char *target = w->target;
	char *linetmp;

	if (!xstrcmp(target, "__debug"))
		return;

	if (config_timestamp) {
		string_t s = string_init(NULL);
		const char *p = xline;
		const char *buf = timestamp(format_string(config_timestamp));

		string_append(s, "\033[0m");
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
/* awful */
	linetmp = (char *) line;
	line = line_buf = saprintf("%s\n", linetmp);
	xfree(linetmp);

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
#ifdef HAVE_RL_SET_PROMPT
/*		rl_set_prompt(NULL); */
#else
/*		rl_expand_prompt(NULL); */
#endif
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
/*		rl_set_prompt(NULL); */
#ifdef HAVE_RL_SET_PROMPT
		        rl_set_prompt((char *) prompt);
#else
		        rl_expand_prompt((char *) prompt);
#endif
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
#ifdef HAVE_RL_SET_PROMPT
		rl_set_prompt(old_prompt);
#else
		rl_expand_prompt(old_prompt);
#endif
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
 * current_prompt()
 *
 * zwraca wska¼nik aktualnego prompta. statyczny bufor, nie zwalniaæ.
 */
const char *current_prompt()
{
	static char buf[80];
	const char *prompt = buf;
	int count = list_count(windows);
	char *tmp, *act = window_activity();

        if (window_current->target) {
		if (count > 1 || window_current->id != 1) {
			if (act) {
				tmp = format_string(format_find("readline_prompt_query_win_act"), window_current->target, itoa(window_current->id), act);
				xfree(act);
			} else
				tmp = format_string(format_find("readline_prompt_query_win"), window_current->target, itoa(window_current->id));
		} else
			tmp = format_string(format_find("readline_prompt_query"), window_current->target, NULL);
		strlcpy(buf, tmp, sizeof(buf));
		xfree(tmp);
        } else {
		char *format_win = "readline_prompt_win", *format_nowin = "readline_prompt", *format_win_act = "readline_prompt_win_act";
		if (/* GG_S_B(config_status) */ 1) {
			format_win = "readline_prompt_away_win";
			format_nowin = "readline_prompt_away";
			format_win_act = "readline_prompt_away_win_act";
		}

		if (/* GG_S_I(config_status)*/ 0) {
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

int my_loop() {
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
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt(prompt);
#else
	rl_expand_prompt(prompt);
#endif
        res = readline((char *) prompt);
        in_readline = 0;

	tmp = saprintf("%s%s\n", prompt, (res) ? res : "");
        window_write(window_current->id, tmp);
	xfree(tmp);

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
	char *line;
	line = my_readline();

	/* je¶li wci¶niêto Ctrl-D i jeste¶my w query, wyjd¼my */
	if (!line && window_current->target) {
//		ui_event("command", 0, "query", NULL); /* dark */
	}

	/* je¶li wci¶niêto Ctrl-D, to zamknij okienko */
	if (!line && list_count(windows) > 1) {
		window_kill(window_current, 0);
	}

	if (!line) {
/*
		if (config_ctrld_quits)	
			return;
		else */ {
			printf("\n");
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
				return 1;
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
		}

		line = string_free(s, 0);
	}
		
	/* je¶li linia nie jest pusta, dopisz do historii */
	if (*line)
		add_history(line);
	
	pager_lines = 0;
		
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
int window_refresh()
{
        int i;

        printf("\033[H\033[J"); /* XXX */

	for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
		if (readline_current->line[i])
			printf("%s", readline_current->line[i]);

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
		for (i = 1; i < MAX_LINES_PER_SCREEN; i++)
			r->line[i - 1] = r->line[i];
		r->line[MAX_LINES_PER_SCREEN - 1] = NULL;
	}

	/* znajd¼ pierwsz± woln± liniê i siê wpisz. */
	for (i = 0; i < MAX_LINES_PER_SCREEN; i++)
		if (!r->line[i]) {
			r->line[i] = xstrdup(line);
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

#if 0 /* TODO */
	/* nowe okno w == window_current ? */ {
#ifdef HAVE_RL_SET_PROMPT
		rl_set_prompt((char *) current_prompt());
#else
		rl_expand_prompt((char *) current_prompt());
#endif
		
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
	list_t l;
	char *act = NULL;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		
		if (w->act) {
			if (!first)
				string_append_c(s, ',');
			string_append(s, itoa(w->id));
			first = 0;
		}
	}

	if (!first)
		return act = string_free(s, 0);
	
	return NULL;
}
		
/*
 * bind_find_command()
 *
 * szuka komendy, któr± nale¿y wykonaæ dla danego klawisza.
 */
char *bind_find_command(const char *seq)
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

