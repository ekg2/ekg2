/* $Id$ */

/*
 *  (C) Copyright 2002-2004 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Wojtek Bojdo³ <wojboj@htcon.pl>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *			    Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ekg/stuff.h>
#include <ekg/metacontacts.h>
#include <ekg/xmalloc.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include "ecurses.h"
#include "bindings.h"
#include "completion.h"
#include "old.h"
#include "contacts.h"

struct binding *ncurses_binding_map[KEY_MAX + 1];	/* mapa klawiszy */
struct binding *ncurses_binding_map_meta[KEY_MAX + 1];	/* j.w. z altem */

void *ncurses_binding_complete = NULL;

#define line ncurses_line
#define lines ncurses_lines

static void binding_backward_word(const char *arg)
{
	while (line_index > 0 && line[line_index - 1] == ' ')
		line_index--;
	while (line_index > 0 && line[line_index - 1] != ' ')
		line_index--;
}

static void binding_forward_word(const char *arg)
{
	while (line_index < xstrlen(line) && line[line_index] == ' ')
		line_index++;
	while (line_index < xstrlen(line) && line[line_index] != ' ')
		line_index++;
}

static void binding_kill_word(const char *arg)
{
	char *p = line + line_index;
	int eaten = 0;

	while (*p && *p == ' ') {
		p++;
		eaten++;
	}

	while (*p && *p != ' ') {
		p++;
		eaten++;
	}

	memmove(line + line_index, line + line_index + eaten, xstrlen(line) - line_index - eaten + 1);
}

static void binding_toggle_input(const char *arg)
{
	if (input_size == 1) {
		input_size = 5;
		ncurses_input_update();
	} else {
		string_t s = string_init("");
		int i;
		char **tmp, *tmp2;
	
		tmp = xmalloc((array_count(lines) + 1 )* sizeof(char *));

		for (i = 0; lines[i]; i++) {
			if (!xstrcmp(lines[i], "") && !lines[i + 1])
				break;

			string_append(s, lines[i]);
			string_append(s, "\r\n");
			if (!window_current->target)
				tmp[i] = xstrdup(lines[i]);
		}

		line = string_free(s, 0);
		tmp2 = xstrdup(line);

		if (history[0] != line)
                        xfree(history[0]);
                history[0] = array_join(lines, "\015");
                xfree(history[HISTORY_MAX - 1]);
                memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));

		history[0] = line;
	        history_index = 0;

		input_size = 1;
		ncurses_input_update();

		if (window_current->target) {
			command_exec(window_current->target, window_current->session, tmp2, 0);
		} else {
			for (i = 0; tmp[i]; i++) {
				command_exec(window_current->target, window_current->session, tmp[i], 0);
				xfree(tmp[i]);
			}
		}
		xfree(tmp);
		xfree(tmp2);
	}
}

static void binding_cancel_input(const char *arg)
{
	if (input_size == 5) {
		input_size = 1;
		ncurses_input_update();
	}
}

static void binding_backward_delete_char(const char *arg)
{
	if (lines && line_index == 0 && lines_index > 0 && xstrlen(lines[lines_index]) + xstrlen(lines[lines_index - 1]) < LINE_MAXLEN) {
		int i;

		line_index = xstrlen(lines[lines_index - 1]);
		xstrcat(lines[lines_index - 1], lines[lines_index]);
		
		xfree(lines[lines_index]);

		for (i = lines_index; i < array_count(lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

		lines_index--;
		lines_adjust();

		return;
	}

	if (xstrlen(line) > 0 && line_index > 0) {
		memmove(line + line_index - 1, line + line_index, LINE_MAXLEN - line_index);
		line[LINE_MAXLEN - 1] = 0;
		line_index--;
	}
}

static void binding_kill_line(const char *arg)
{
	line[line_index] = 0;
}

static void binding_yank(const char *arg)
{
	if (yanked && xstrlen(yanked) + xstrlen(line) + 1 < LINE_MAXLEN) {
		memmove(line + line_index + xstrlen(yanked), line + line_index, LINE_MAXLEN - line_index - xstrlen(yanked));
		memcpy(line + line_index, yanked, xstrlen(yanked));
		line_index += xstrlen(yanked);
	}
}

static void binding_delete_char(const char *arg)
{
	if (line_index == xstrlen(line) && lines_index < array_count(lines) - 1 && xstrlen(line) + xstrlen(lines[lines_index + 1]) < LINE_MAXLEN) {
		int i;

		xstrcat(line, lines[lines_index + 1]);

		xfree(lines[lines_index + 1]);

		for (i = lines_index + 1; i < array_count(lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

		lines_adjust();
	
		return;
	}
				
	if (line_index < xstrlen(line)) {
		memmove(line + line_index, line + line_index + 1, LINE_MAXLEN - line_index - 1);
		line[LINE_MAXLEN - 1] = 0;
	}
}
				
static void binding_accept_line(const char *arg)
{
	if (lines) {
		int i;

		lines = xrealloc(lines, (array_count(lines) + 2) * sizeof(char*));

		for (i = array_count(lines); i > lines_index; i--)
			lines[i + 1] = lines[i];

		lines[lines_index + 1] = xmalloc(LINE_MAXLEN);
		xstrcpy(lines[lines_index + 1], line + line_index);
		line[line_index] = 0;
		
		line_index = 0;
		line_start = 0;
		lines_index++;

		lines_adjust();
	
		return;
	}
				
	command_exec(window_current->target, window_current->session, line, 0);

	if (ncurses_plugin_destroyed)
		return;

	if (xstrcmp(line, "")) {
		if (history[0] != line)
			xfree(history[0]);
		history[0] = xstrdup(line);
		xfree(history[HISTORY_MAX - 1]);
		memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));
	} else {
		if (config_enter_scrolls)
			print("none", "");
	}

	history[0] = line;
	history_index = 0;
	line[0] = 0;
	line_adjust();
}

static void binding_line_discard(const char *arg)
{
	xfree(yanked);
	yanked = xstrdup(line);
	line[0] = 0;
	line_adjust();

	if (lines && lines_index < array_count(lines) - 1) {
		int i;

		xfree(lines[lines_index]);

		for (i = lines_index; i < array_count(lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count(lines) + 1) * sizeof(char*));

		lines_adjust();
	}
}

static void binding_quoted_insert(const char *arg)
{
/* XXX
 * naprawiæ
 */
}

static void binding_word_rubout(const char *arg)
{
	char *p;
	int eaten = 0;

	if (!line_index)
		return;
	
	xfree(yanked);

	p = line + line_index;
	
	if (xisspace(*(p-1))) {
		while (p > line && xisspace(*(p-1))) {
			p--;
			eaten++;
		}
	} else {
                while (p > line && !xisalpha(*(p-1)) && !xisspace(*(p-1))) {
                        p--;
                        eaten++;
                }
        }

	if (p > line) {
		while (p > line && !xisspace(*(p-1)) && xisalpha(*(p-1))) {
			p--;
			eaten++;
		}
	}

	yanked = xmalloc(eaten + 1);
	strlcpy(yanked, p, eaten + 1);

	memmove(p, line + line_index, xstrlen(line) - line_index + 1);
	line_index -= eaten;
}

static void binding_complete(const char *arg)
{
	if (!lines)
		ncurses_complete(&line_start, &line_index, line);
	else {
		int i, count = 8 - (line_index % 8);

		if (xstrlen(line) + count >= LINE_MAXLEN - 1)
			return;

		memmove(line + line_index + count, line + line_index, LINE_MAXLEN - line_index - count);

		for (i = line_index; i < line_index + count; i++)
			line[i] = ' ';

		line_index += count;
	}
}

static void binding_backward_char(const char *arg)
{
	if (lines) {
		if (line_index > 0)
			line_index--;
		else {
			if (lines_index > 0) {
				lines_index--;
				lines_adjust();
				line_adjust();
			}
		}

		return;
	}

	if (line_index > 0)
		line_index--;
}

static void binding_forward_char(const char *arg)
{
	if (lines) {
		if (line_index < xstrlen(line))
			line_index++;
		else {
			if (lines_index < array_count(lines) - 1) {
				lines_index++;
				line_index = 0;
				line_start = 0;
			}
			lines_adjust();
		}

		return;
	}

	if (line_index < xstrlen(line))
		line_index++;
}

static void binding_end_of_line(const char *arg)
{
	line_adjust();
}

static void binding_beginning_of_line(const char *arg)
{
	line_index = 0;
	line_start = 0;
}

static void binding_previous_only_history(const char *arg)
{
        if (history[history_index + 1]) {
                if (history_index == 0)
                        history[0] = xstrdup(line);
                history_index++;
		if (xstrchr(history[history_index], '\015')) {
			char **tmp;
			int i;
			
                        if (input_size == 1) {
                                input_size = 5;
                                ncurses_input_update();
                        }

                        tmp = array_make(history[history_index], "\015", 0, 0, 0);

			array_free(lines);
			lines = xmalloc((array_count(tmp) + 2) * sizeof(char *));

			for (i = 0; i < array_count(tmp); i++) {
				lines[i] = xmalloc(LINE_MAXLEN);
				xstrcpy(lines[i], tmp[i]);
			}

			array_free(tmp);
			lines_adjust();
		} else {
			if (input_size != 1) {
		                input_size = 1;
		                ncurses_input_update();
		        }
			xstrcpy(line, history[history_index]);
	                line_adjust();
		}
        }
}

static void binding_next_only_history(const char *arg)
{
        if (history_index > 0) {
                if (history_index == 0)
                        history[0] = xstrdup(line);
                history_index--;
                if (xstrchr(history[history_index], '\015')) {
                        char **tmp;
                        int i;

                        if (input_size == 1) {
                                input_size = 5;
                                ncurses_input_update();
			}

                        tmp = array_make(history[history_index], "\015", 0, 0, 0);

                        array_free(lines);
                        lines = xmalloc((array_count(tmp) + 2) * sizeof(char *));

                        for (i = 0; i < array_count(tmp); i++) {
                                lines[i] = xmalloc(LINE_MAXLEN);
                                xstrcpy(lines[i], tmp[i]);
                        }

                        array_free(tmp);
                        lines_adjust();
                } else {
                        if (input_size != 1) {
                                input_size = 1;
                                ncurses_input_update();
                        }
                        xstrcpy(line, history[history_index]);
                        line_adjust();
                }
        }
}


static void binding_previous_history(const char *arg)
{
	if (lines) {
		if (lines_index - lines_start == 0)
			if (lines_start)
				lines_start--;

		if (lines_index)
			lines_index--;

		lines_adjust();

		return;
	}
	
	binding_previous_only_history(NULL);				
}

static void binding_next_history(const char *arg)
{
	if (lines) {
		if (lines_index - line_start == 4)
			if (lines_index < array_count(lines) - 1)
				lines_start++;

		if (lines_index < array_count(lines) - 1)
			lines_index++;

		lines_adjust();

		return;
	}

	binding_next_only_history(NULL);
}

static void binding_backward_page(const char *arg)
{
	ncurses_current->start -= window_current->height / 2;
	if (ncurses_current->start < 0)
		ncurses_current->start = 0;
	ncurses_redraw(window_current);
	ncurses_commit();
}

static void binding_forward_page(const char *arg)
{
	ncurses_current->start += window_current->height / 2;

	if (ncurses_current->start > ncurses_current->lines_count - window_current->height + ncurses_current->overflow)
		ncurses_current->start = ncurses_current->lines_count - window_current->height + ncurses_current->overflow;

	if (ncurses_current->start < 0)
		ncurses_current->start = 0;

	if (ncurses_current->start == ncurses_current->lines_count - window_current->height + ncurses_current->overflow) {
		window_current->more = 0;
		update_statusbar(0);
	}

	ncurses_redraw(window_current);
	ncurses_commit();
}



static void binding_backward_contacts_line(const char *arg)
{
	window_t *w = window_find("__contacts");

        if (!w)
        	return;

        contacts_index--;
        if (contacts_index < 0)
                contacts_index = 0;

	ncurses_contacts_update(NULL);
	ncurses_redraw(w);
	ncurses_commit();
}

static void binding_forward_contacts_line(const char *arg)
{
        ncurses_window_t *n;
        window_t *w = window_find("__contacts");
        int contacts_count = 0, all = 0, count = 0;

        if (!w)
                return;

        n = w->private;

        if (config_contacts_groups) {
                char **groups = array_make(config_contacts_groups, ", ", 0, 1, 0);
                count = array_count(groups);
                array_free(groups);
        }

        if (contacts_group_index > count + 1)
                all = 2;
        else if (contacts_group_index > count)
                all = 1;

        switch (all) {
                case 1:
                {
                        list_t l;
                        for (l = sessions; sessions && l; l = l->next) {
                                session_t *s = l->data;

                                if (!s || !s->userlist)
                                        continue;

                                contacts_count += list_count(s->userlist);
                        }
                        break;
                }
                case 2:
                        contacts_count = list_count(metacontacts);
                        break;
                default:
                        contacts_count = list_count(session_current->userlist);
                        break;
        }

        contacts_index++;

        if (contacts_index  > contacts_count - w->height + n->overflow + CONTACTS_MAX_HEADERS)
                contacts_index = contacts_count - window_current->height + n->overflow + CONTACTS_MAX_HEADERS;
        if (contacts_index < 0)
                contacts_index = 0;

        ncurses_contacts_update(NULL);
	ncurses_redraw(w);
	ncurses_commit();
}


static void binding_backward_contacts_page(const char *arg)
{
        ncurses_window_t *n;
        window_t *w = window_find("__contacts");
        int contacts_count;

        if (!w)
                return;

        n = w->private;
        contacts_count = list_count(session_current->userlist);

        contacts_index -= w->height / 2;

        if (contacts_index < 0)
                contacts_index = 0;

        ncurses_contacts_update(NULL);
	ncurses_redraw(w);
	ncurses_commit();
}

static void binding_forward_contacts_page(const char *arg)
{
        ncurses_window_t *n;
        window_t *w = window_find("__contacts");
        int contacts_count = 0, all = 0, count = 0;

        if (!w)
                return;

        n = w->private;

	if (config_contacts_groups) {
		char **groups = array_make(config_contacts_groups, ", ", 0, 1, 0);
		count = array_count(groups);
		array_free(groups);
	}

        if (contacts_group_index > count + 1) 
	        all = 2;
        else if (contacts_group_index > count) 
                all = 1;

	switch (all) {
		case 1: 
		{
			list_t l;
			for (l = sessions; sessions && l; l = l->next) {
				session_t *s = l->data;
				
				if (!s || !s->userlist)
					continue;

				contacts_count += list_count(s->userlist);
			}
			break;
		}
		case 2: 
			contacts_count = list_count(metacontacts);
			break;
		default:
                        contacts_count = list_count(session_current->userlist);
                        break;
	}

        contacts_index += w->height / 2;

        if (contacts_index  > contacts_count - w->height + n->overflow + CONTACTS_MAX_HEADERS)
                contacts_index = contacts_count - window_current->height + n->overflow + CONTACTS_MAX_HEADERS;
        if (contacts_index < 0)
                contacts_index = 0;

        ncurses_contacts_update(NULL);
	ncurses_redraw(w);
	ncurses_commit();
}

static void binding_ignore_query(const char *arg)
{
	char *tmp;
	
	if (!window_current->target)
		return;
	
	tmp = saprintf("/ignore %s", window_current->target);
	command_exec(window_current->target, window_current->session, tmp, 0);
	xfree(tmp);
}

static void binding_quick_list_wrapper(const char *arg)
{
	binding_quick_list(0, 0);
}

static void binding_toggle_contacts_wrapper(const char *arg)
{
	static int last_contacts = -1;

	if (!config_contacts) {
		if ((config_contacts = last_contacts) == -1)
			config_contacts = 2;
	} else {
		last_contacts = config_contacts;
		config_contacts = 0;
	}

	ncurses_contacts_changed("contacts");
}

static void binding_next_contacts_group(const char *arg)
{
	contacts_group_index++;
	ncurses_contacts_update(NULL);
	ncurses_resize();
	ncurses_commit();
}

static void binding_ui_ncurses_debug_toggle(const char *arg)
{
	if (ncurses_debug++ > 3)
		ncurses_debug = 0;

	update_statusbar(1);
}

static void binding_cycle_sessions(const char *arg)
{
	if (window_current->id == 0 || !window_current->target)
		window_session_cycle(window_current);
	else {
		print("session_cannot_change");
		return;
	}
	
	ncurses_contacts_update(NULL);
	update_statusbar(1);
}

/*
 * binding_parse()
 *
 * analizuje dan± akcjê i wype³nia pola struct binding odpowiedzialne
 * za dzia³anie.
 *
 *  - b - wska¼nik wype³nianej skruktury,
 *  - action - akcja,
 */
static void binding_parse(struct binding *b, const char *action)
{
	char **args;

	if (!b || !action)
		return;

	b->action = xstrdup(action);

	args = array_make(action, " \t", 1, 1, 1);

	if (!args[0]) {
		array_free(args);
		return;
	}
	
#define __action(x,y) \
	if (!xstrcmp(args[0], x)) { \
		b->function = y; \
		b->arg = xstrdup(args[1]); \
	} 

	__action("backward-word", binding_backward_word);
	__action("forward-word", binding_forward_word);
	__action("kill-word", binding_kill_word);
	__action("toggle-input", binding_toggle_input);
	__action("cancel-input", binding_cancel_input);
	__action("backward-delete-char", binding_backward_delete_char);
	__action("beginning-of-line", binding_beginning_of_line);
	__action("end-of-line", binding_end_of_line);
	__action("delete-char", binding_delete_char);
	__action("backward-page", binding_backward_page);
	__action("forward-page", binding_forward_page);
	__action("kill-line", binding_kill_line);
	__action("yank", binding_yank);
	__action("accept-line", binding_accept_line);
	__action("line-discard", binding_line_discard);
	__action("quoted-insert", binding_quoted_insert);
	__action("word-rubout", binding_word_rubout);
	__action("backward-char", binding_backward_char);
	__action("forward-char", binding_forward_char);
	__action("previous-history", binding_previous_history);
	__action("previous-only-history", binding_previous_only_history);
	__action("next-history", binding_next_history);
	__action("next-only-history", binding_next_only_history);
	__action("complete", binding_complete);
	__action("quick-list", binding_quick_list_wrapper);
	__action("toggle-contacts", binding_toggle_contacts_wrapper);
	__action("next-contacts-group", binding_next_contacts_group);
	__action("ignore-query", binding_ignore_query);
	__action("ui-ncurses-debug-toggle", binding_ui_ncurses_debug_toggle);
	__action("cycle-sessions", binding_cycle_sessions);
	__action("forward-contacts-page", binding_forward_contacts_page);
	__action("backward-contacts-page", binding_backward_contacts_page);
	__action("forward-contacts-line", binding_forward_contacts_line);
	__action("backward-contacts-line", binding_backward_contacts_line);


#undef __action

	array_free(args);
}

/*
 * binding_key()
 *
 * analizuje nazwê klawisza i wpisuje akcjê do odpowiedniej mapy.
 *
 * 0/-1.
 */
int binding_key(struct binding *b, const char *key, int add)
{
	/* debug("Key: %s\n", key); */
	if (!xstrncasecmp(key, "Alt-", 4)) {
		unsigned char ch;

		if (!xstrcasecmp(key + 4, "Enter")) {
			b->key = xstrdup("Alt-Enter");
			if (add)
				ncurses_binding_map_meta[13] = list_add(&bindings, b, sizeof(struct binding));
			return 0;
		}

		if (!xstrcasecmp(key + 4, "Backspace")) {
			b->key = xstrdup("Alt-Backspace");
			if (add) {
				ncurses_binding_map_meta[KEY_BACKSPACE] = list_add(&bindings, b, sizeof(struct binding));
				ncurses_binding_map_meta[127] = ncurses_binding_map_meta[KEY_BACKSPACE];
			}
			return 0;
		}

		if (xstrlen(key) != 5)
			return -1;
	
		ch = xtoupper(key[4]);

		b->key = saprintf("Alt-%c", ch);

		if (add) {
			ncurses_binding_map_meta[ch] = list_add(&bindings, b, sizeof(struct binding));
			if (xisalpha(ch))
				ncurses_binding_map_meta[xtolower(ch)] = ncurses_binding_map_meta[ch];
		}

		return 0;
	}

	if (!xstrncasecmp(key, "Ctrl-", 5)) {
		unsigned char ch;
		
//		if (xstrlen(key) != 6)
//			return -1;
#define __key(x, y, z) \
        if (!xstrcasecmp(key + 5, x)) { \
                b->key = xstrdup(key); \
                if (add) { \
                        ncurses_binding_map[y] = list_add(&bindings, b, sizeof(struct binding)); \
                        if (z) \
                                ncurses_binding_map[z] = ncurses_binding_map[y]; \
                } \
                return 0; \
        }

        __key("Enter", KEY_CTRL_ENTER, 0);
        __key("Escape", KEY_CTRL_ESCAPE, 0);
        __key("Home", KEY_CTRL_HOME, 0);
        __key("End", KEY_CTRL_END, 0);
        __key("Delete", KEY_CTRL_DC, 0);
        __key("Backspace", KEY_CTRL_BACKSPACE, 0);
        __key("Tab", KEY_CTRL_TAB, 0);
        __key("Left", KEY_CTRL_LEFT, 0);
        __key("Right", KEY_CTRL_RIGHT, 0);
        __key("Up", KEY_CTRL_UP, 0);
        __key("Down", KEY_CTRL_DOWN, 0);
        __key("PageUp", KEY_CTRL_PPAGE, 0);
        __key("PageDown", KEY_CTRL_NPAGE, 0);

#undef __key
	
		ch = xtoupper(key[5]);
		b->key = saprintf("Ctrl-%c", ch);

		if (add) {
                        if (xisalpha(ch))
				ncurses_binding_map[ch - 64] = list_add(&bindings, b, sizeof(struct binding));
			else
				return -1;
		}
		
		return 0;
	}

	if (xtoupper(key[0]) == 'F' && atoi(key + 1)) {
		int f = atoi(key + 1);

		if (f < 1 || f > 24)
			return -1;

		b->key = saprintf("F%d", f);
		
		if (add)
			ncurses_binding_map[KEY_F(f)] = list_add(&bindings, b, sizeof(struct binding));
		
		return 0;
	}

#define __key(x, y, z) \
	if (!xstrcasecmp(key, x)) { \
		b->key = xstrdup(x); \
		if (add) { \
			ncurses_binding_map[y] = list_add(&bindings, b, sizeof(struct binding)); \
			if (z) \
				ncurses_binding_map[z] = ncurses_binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", 13, 0);
	__key("Escape", 27, 0);
	__key("Home", KEY_HOME, KEY_FIND);
	__key("End", KEY_END, KEY_SELECT);
	__key("Delete", KEY_DC, 0);
	__key("Backspace", KEY_BACKSPACE, 127);
	__key("Tab", 9, 0);
	__key("Left", KEY_LEFT, 0);
	__key("Right", KEY_RIGHT, 0);
	__key("Up", KEY_UP, 0);
	__key("Down", KEY_DOWN, 0);
	__key("PageUp", KEY_PPAGE, 0);
	__key("PageDown", KEY_NPAGE, 0);

#undef __key

	return -1;
}

/*
 * ncurses_binding_add()
 *
 * przypisuje danemu klawiszowi akcjê.
 *
 *  - key - opis klawisza,
 *  - action - akcja,
 *  - internal - czy to wewnêtrzna akcja interfejsu?
 *  - quiet - czy byæ cicho i nie wy¶wietlaæ niczego?
 */
void ncurses_binding_add(const char *key, const char *action, int internal, int quiet)
{
	struct binding b, *c = NULL;
	list_t l;
	
	if (!key || !action)
		return;

	memset(&b, 0, sizeof(b));

	b.internal = internal;
	
	for (l = bindings; l; l = l->next) {
		struct binding *d = l->data;

		if (!xstrcasecmp(key, d->key)) {
			if (d->internal) {
				c = d;
				break;
			}
			printq("bind_seq_exist", d->key);
			return;
		}
	}

	binding_parse(&b, action);

	if (internal) {
		b.default_action = xstrdup(b.action);
		b.default_function = b.function;
		b.default_arg = xstrdup(b.arg);
	}

	if (binding_key(&b, key, (c) ? 0 : 1)) {
		printq("bind_seq_incorrect", key);
		xfree(b.action);
		xfree(b.arg);
		xfree(b.default_action);
		xfree(b.default_arg);
		xfree(b.key);
	} else {
		printq("bind_seq_add", b.key);

		if (c) {
			xfree(c->action);
			c->action = b.action;
			xfree(c->arg);
			c->arg = b.arg;
			c->function = b.function;
			xfree(b.default_action);
			xfree(b.default_arg);
			xfree(b.key);
			c->internal = 0;
		}

		if (!in_autoexec)
			config_changed = 1;
	}
}

/*
 * ncurses_binding_delete()
 *
 * usuwa akcjê z danego klawisza.
 */
void ncurses_binding_delete(const char *key, int quiet)
{
	list_t l;

	if (!key)
		return;

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;
		int i;

		if (!b->key || xstrcasecmp(key, b->key))
			continue;

		if (b->internal) {
			printq("bind_seq_incorrect", key);
			return;
		}

		xfree(b->action);
		xfree(b->arg);
		
		if (b->default_action) {
			b->action = xstrdup(b->default_action);
			b->arg = xstrdup(b->default_arg);
			b->function = b->default_function;
			b->internal = 1;
		} else {
			xfree(b->key);
			for (i = 0; i < KEY_MAX + 1; i++) {
				if (ncurses_binding_map[i] == b)
					ncurses_binding_map[i] = NULL;
				if (ncurses_binding_map_meta[i] == b)
					ncurses_binding_map_meta[i] = NULL;

			}

			list_remove(&bindings, b, 1);
		}

		config_changed = 1;

		printq("bind_seq_remove", key);
		
		return;
	}

	printq("bind_seq_incorrect", key);
}

/*
 * ncurses_binding_default()
 *
 * ustawia lub przywraca domy¶lne ustawienia przypisanych klawiszy.
 */
void ncurses_binding_default()
{
	ncurses_binding_add("Alt-`", "/window switch 0", 1, 1);
	ncurses_binding_add("Alt-1", "/window switch 1", 1, 1);
	ncurses_binding_add("Alt-2", "/window switch 2", 1, 1);
	ncurses_binding_add("Alt-3", "/window switch 3", 1, 1);
	ncurses_binding_add("Alt-4", "/window switch 4", 1, 1);
	ncurses_binding_add("Alt-5", "/window switch 5", 1, 1);
	ncurses_binding_add("Alt-6", "/window switch 6", 1, 1);
	ncurses_binding_add("Alt-7", "/window switch 7", 1, 1);
	ncurses_binding_add("Alt-8", "/window switch 8", 1, 1);
	ncurses_binding_add("Alt-9", "/window switch 9", 1, 1);
	ncurses_binding_add("Alt-0", "/window switch 10", 1, 1);
	ncurses_binding_add("Alt-Q", "/window switch 11", 1, 1);
	ncurses_binding_add("Alt-W", "/window switch 12", 1, 1);
	ncurses_binding_add("Alt-E", "/window switch 13", 1, 1);
	ncurses_binding_add("Alt-R", "/window switch 14", 1, 1);
	ncurses_binding_add("Alt-T", "/window switch 15", 1, 1);
	ncurses_binding_add("Alt-Y", "/window switch 16", 1, 1);
	ncurses_binding_add("Alt-U", "/window switch 17", 1, 1);
	ncurses_binding_add("Alt-I", "/window switch 18", 1, 1);
	ncurses_binding_add("Alt-O", "/window switch 19", 1, 1);
	ncurses_binding_add("Alt-P", "/window switch 20", 1, 1);
	ncurses_binding_add("Alt-K", "/window kill", 1, 1);
	ncurses_binding_add("Alt-N", "/window new", 1, 1);
	ncurses_binding_add("Alt-A", "/window active", 1, 1);
	ncurses_binding_add("Alt-G", "ignore-query", 1, 1);
	ncurses_binding_add("Alt-B", "backward-word", 1, 1);
	ncurses_binding_add("Alt-F", "forward-word", 1, 1);
	ncurses_binding_add("Alt-D", "kill-word", 1, 1);
	ncurses_binding_add("Alt-Enter", "toggle-input", 1, 1);
	ncurses_binding_add("Escape", "cancel-input", 1, 1);
	ncurses_binding_add("Ctrl-N", "/window next", 1, 1);
	ncurses_binding_add("Ctrl-P", "/window prev", 1, 1);
	ncurses_binding_add("Backspace", "backward-delete-char", 1, 1);
	ncurses_binding_add("Ctrl-H", "backward-delete-char", 1, 1);
	ncurses_binding_add("Ctrl-A", "beginning-of-line", 1, 1);
	ncurses_binding_add("Home", "beginning-of-line", 1, 1);
	ncurses_binding_add("Ctrl-D", "delete-char", 1, 1);
	ncurses_binding_add("Delete", "delete-char", 1, 1);
	ncurses_binding_add("Ctrl-E", "end-of-line", 1, 1);
	ncurses_binding_add("End", "end-of-line", 1, 1);
	ncurses_binding_add("Ctrl-K", "kill-line", 1, 1);
	ncurses_binding_add("Ctrl-Y", "yank", 1, 1);
	ncurses_binding_add("Enter", "accept-line", 1, 1);
	ncurses_binding_add("Ctrl-M", "accept-line", 1, 1);
	ncurses_binding_add("Ctrl-U", "line-discard", 1, 1);
	ncurses_binding_add("Ctrl-V", "quoted-insert", 1, 1);
	ncurses_binding_add("Ctrl-W", "word-rubout", 1, 1);
	ncurses_binding_add("Alt-Backspace", "word-rubout", 1, 1);
	ncurses_binding_add("Ctrl-L", "/window refresh", 1, 1);
	ncurses_binding_add("Tab", "complete", 1, 1);
	ncurses_binding_add("Right", "forward-char", 1, 1);
	ncurses_binding_add("Left", "backward-char", 1, 1);
	ncurses_binding_add("Up", "previous-history", 1, 1);
	ncurses_binding_add("Down", "next-history", 1, 1);
	ncurses_binding_add("PageUp", "backward-page", 1, 1);
	ncurses_binding_add("Ctrl-F", "backward-page", 1, 1);
	ncurses_binding_add("PageDown", "forward-page", 1, 1);
	ncurses_binding_add("Ctrl-G", "forward-page", 1, 1);
	ncurses_binding_add("Ctrl-X", "cycle-sessions", 1, 1);
	ncurses_binding_add("F1", "/help", 1, 1);
	ncurses_binding_add("F2", "quick-list", 1, 1);
	ncurses_binding_add("F3", "toggle-contacts", 1, 1);
	ncurses_binding_add("F4", "next-contacts-group", 1, 1);
	ncurses_binding_add("F12", "/window switch 0", 1, 1);
	ncurses_binding_add("F11", "ui-ncurses-debug-toggle", 1, 1);
	ncurses_binding_add("Ctrl-Down", "forward-contacts-page", 1, 1);
	ncurses_binding_add("Ctrl-Up", "backward-contacts-page", 1, 1);
}

void ncurses_binding_init()
{
	memset(ncurses_binding_map, 0, sizeof(ncurses_binding_map));
	memset(ncurses_binding_map_meta, 0, sizeof(ncurses_binding_map_meta));

	ncurses_binding_default();
	ncurses_binding_complete = binding_complete;
}

void ncurses_binding_destroy()
{

}

