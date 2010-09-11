/* $Id$ */

/*
 *  (C) Copyright 2002-2004 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Wojtek Bojdo³ <wojboj@htcon.pl>
 *			    Pawe³ Maziarz <drg@infomex.pl>
 *			    Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include "ekg2-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ekg/bindings.h>
#include <ekg/stuff.h>
#include <ekg/metacontacts.h>
#include <ekg/xmalloc.h>
#include <ekg/debug.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include "ecurses.h"
#include "bindings.h"
#include "completion.h"
#include "old.h"
#include "contacts.h"

extern int ncurses_typing_mod;
extern window_t *ncurses_typing_win;

struct binding *ncurses_binding_map[KEY_MAX + 1];	/* mapa klawiszy */
struct binding *ncurses_binding_map_meta[KEY_MAX + 1];	/* j.w. z altem */

void *ncurses_binding_complete = NULL;
void *ncurses_binding_accept_line = NULL;

int bindings_added_max = 0;

#define line ncurses_line
#define lines ncurses_lines

static const void *BINDING_HISTORY_NOEXEC = (void*) -1;

extern int ncurses_noecho;	/* in old.c */
extern CHAR_T *ncurses_passbuf;	/* in old.c */

static void add_to_history() {
	if (history[0] != line)
		xfree(history[0]);

	history[0] = lines ? wcs_array_join(lines, TEXT("\015")) : xwcsdup(line);

	xfree(history[HISTORY_MAX - 1]);
	memmove(&history[1], &history[0], sizeof(history) - sizeof(history[0]));

	history[0] = line;
	history_index = 0;
}



static BINDING_FUNCTION(binding_backward_word)
{
	while (line_index > 0 && line[line_index - 1] == ' ')
		line_index--;
	while (line_index > 0 && line[line_index - 1] != ' ')
		line_index--;
}

static BINDING_FUNCTION(binding_forward_word) {
	size_t linelen = xwcslen(line);
	while (line_index < linelen && line[line_index] == ' ')
		line_index++;
	while (line_index < linelen && line[line_index] != ' ')
		line_index++;
}

static BINDING_FUNCTION(binding_kill_word)
{
	CHAR_T *p = line + line_index;
	int eaten = 0;

	while (*p && *p == ' ') {
		p++;
		eaten++;
	}

	while (*p && *p != ' ') {
		p++;
		eaten++;
	}

	memmove(line + line_index, line + line_index + eaten, sizeof(CHAR_T) * (xwcslen(line) - line_index - eaten + 1));
}

static BINDING_FUNCTION(binding_toggle_input)
{
	if (input_size == 1) {
		input_size = MULTILINE_INPUT_SIZE;
		ncurses_input_update(line_index);
	} else {
		string_t s = string_init((""));
		char *p, *tmp;
		int i;
	
		for (i = 0; lines[i]; i++) {
			char *tmp;

			string_append(s, (tmp = wcs_to_normal(lines[i])));	free_utf(tmp);
			if (lines[i + 1])
				string_append(s, ("\r\n"));
		}

		tmp = string_free(s, 0);

		add_to_history();

		input_size = 1;
		ncurses_input_update(0);

		for (p=tmp; *p && isspace(*p); p++);
                if (*p || config_send_white_lines)
			command_exec(window_current->target, window_current->session, tmp, 0);

		if (!tmp[0] || tmp[0] == '/' || !window_current->target)
			ncurses_typing_mod		= 1;
		else {
			ncurses_typing_win		= NULL;
			window_current->out_active	= 1;
		}

		curs_set(1);
		xfree(tmp);
	}
}

static BINDING_FUNCTION(binding_cancel_input)
{
	if (input_size != 1) {
		input_size = 1;
		ncurses_input_update(0);
		ncurses_typing_mod = 1;
	}
}

static BINDING_FUNCTION(binding_backward_delete_char)
{
	if (lines && line_index == 0 && lines_index > 0 && xwcslen(lines[lines_index]) + xwcslen(lines[lines_index - 1]) < LINE_MAXLEN) {
		int i;

		line_index = xwcslen(lines[lines_index - 1]);
		xwcscat(lines[lines_index - 1], lines[lines_index]);
		
		xfree(lines[lines_index]);

		for (i = lines_index; i < array_count((char **) lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count((char **) lines) + 1) * sizeof(CHAR_T *));

		lines_index--;
		lines_adjust();
		ncurses_typing_mod = 1;
	} else if (xwcslen(line) > 0 && line_index > 0) {
		memmove(line + line_index - 1, line + line_index, (LINE_MAXLEN - line_index) * sizeof(CHAR_T));
		line[LINE_MAXLEN - 1] = 0;
		line_index--;
		ncurses_typing_mod = 1;
	}
}

static BINDING_FUNCTION(binding_window_kill)
{
	char * ptr;
	ptr = xstrstr(window_current->target, "irc:");
	if (ptr && ptr == window_current->target && (ptr[4] == '!' || ptr[4] == '#') && !config_kill_irc_window ) {
		print("cant_kill_irc_window");
		return;
	}
	command_exec(window_current->target, window_current->session, ("/window kill"), 0);
}

static BINDING_FUNCTION(binding_kill_line)
{
	line[line_index] = 0;
}

static BINDING_FUNCTION(binding_yank)
{
	if (yanked && xwcslen(yanked) + xwcslen(line) + 1 < LINE_MAXLEN) {
		memmove(line + line_index + xwcslen(yanked), line + line_index, (LINE_MAXLEN - line_index - xwcslen(yanked)) * sizeof(CHAR_T));
		memcpy(line + line_index, yanked, sizeof(CHAR_T) * xwcslen(yanked));
		line_index += xwcslen(yanked);
	}
}

static BINDING_FUNCTION(binding_delete_char)
{
	if (line_index == xwcslen(line) && lines_index < array_count((char **) lines) - 1 && xwcslen(line) + xwcslen(lines[lines_index + 1]) < LINE_MAXLEN) {
		int i;

		xwcscat(line, lines[lines_index + 1]);

		xfree(lines[lines_index + 1]);

		for (i = lines_index + 1; i < array_count((char **) lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count((char **) lines) + 1) * sizeof(CHAR_T *));

		lines_adjust();
		ncurses_typing_mod = 1;
	} else if (line_index < xwcslen(line)) {
		memmove(line + line_index, line + line_index + 1, (LINE_MAXLEN - line_index - 1) * sizeof(CHAR_T));
		line[LINE_MAXLEN - 1] =  0;
		ncurses_typing_mod = 1;
	}
}
				
static BINDING_FUNCTION(binding_accept_line)
{
	char *p, *txt;

	if (ncurses_noecho) { /* we are running ui-password-input */
		ncurses_noecho = 0;
		ncurses_passbuf = xwcsdup(line);
		line[0] = 0;
		line_adjust();
		return;
	}

	if (lines) {
		int i;

		lines = xrealloc(lines, (array_count((char **) lines) + 2) * sizeof(CHAR_T *));

		for (i = array_count((char **) lines); i > lines_index; i--)
			lines[i + 1] = lines[i];

		lines[lines_index + 1] = xmalloc(LINE_MAXLEN*sizeof(CHAR_T));
		xwcscpy(lines[lines_index + 1], line + line_index);
		line[line_index] = 0;
		
		line_index = 0;
		line_start = 0;
		lines_index++;

		lines_adjust();
	
		return;
	}
	if (arg != BINDING_HISTORY_NOEXEC) {
               txt = wcs_to_normal(line);
               for (p=txt; *p && isspace(*p); p++);
               if (*p || config_send_white_lines)
                       command_exec(window_current->target, window_current->session, txt, 0);
		free_utf(txt);
	}

	if (ncurses_plugin_destroyed)
		return;
	if (!line[0] || line[0] == '/' || !window_current->target) /* if empty or command, just mark as modified */
		ncurses_typing_mod		= 1;
	else { /* if message, assume that its' handler has already disabled <composing/> */
		ncurses_typing_win		= NULL;
		window_current->out_active	= 1; /* but also remember that it should have set <active/> chatstate */
	}

	if (xwcscmp(line, TEXT(""))) {
		if (config_history_savedups || xwcscmp(line, history[1]))
			add_to_history();
	} else {
		if (config_enter_scrolls)
			print("none", "");
	}

	history[0] = line;
	history_index = 0;
	*line = 0;
	line_adjust();
}

static BINDING_FUNCTION(binding_line_discard)
{
	if (ncurses_noecho) { /* we don't want to yank passwords */
		xfree(yanked);
		yanked = xwcsdup(line);
	}
	*line = 0;
	line_adjust();

	if (lines && lines_index < array_count((char **) lines) - 1) {
		int i;

		xfree(lines[lines_index]);

		for (i = lines_index; i < array_count((char **) lines); i++)
			lines[i] = lines[i + 1];

		lines = xrealloc(lines, (array_count((char **) lines) + 1) * sizeof(CHAR_T *));

		lines_adjust();
	}

}

static BINDING_FUNCTION(binding_quoted_insert)
{
/* XXX
 * naprawiæ
 */
}

static BINDING_FUNCTION(binding_word_rubout)
{
	CHAR_T *p;
	int eaten = 0;

	if (!line_index)
		return;
	
	xfree(yanked);

	p = line + line_index;
	
	if (xisspace(*(p - 1))) {
		while (p > line && xisspace(*(p - 1))) {
			p--;
			eaten++;
		}
	} else {
		while (p > line && ! xisalpha(*(p - 1)) && ! xisspace(*(p - 1))) {
			p--;
			eaten++;
		}
	}

	if (p > line) {
		while (p > line && ! xisspace(*(p - 1)) && xisalpha(*(p - 1))) {
			p--;
			eaten++;
		}
	}

	yanked = xcalloc(eaten + 1, sizeof(CHAR_T));
	xwcslcpy(yanked, p, eaten + 1);

	memmove(p, line + line_index, (xwcslen(line) - line_index + 1) * sizeof(CHAR_T));
	line_index -= eaten;
}

static BINDING_FUNCTION(binding_complete)
{
	if (!lines) {
#if USE_UNICODE
			int line_start_tmp, line_index_tmp;
			char nline[LINE_MAXLEN + 1];	/* (* MB_CUR_MAX)? No, it would be anyway truncated by completion */
			int i, j;
			int nlen;

			line_start_tmp = line_index_tmp = 0;
			for (i = 0, j = 0; line[i] && i < LINE_MAXLEN; i++) {
				char buf[MB_CUR_MAX+1];
				int tmp;
				int k;

				tmp = wctomb(buf, line[i]);

				if (tmp <= 0 || tmp >= MB_CUR_MAX) {
					debug_error("binding_complete() wctomb() failed (%d)\n", tmp);
					return;
				}

				if (j+tmp >= LINE_MAXLEN) {
					debug_error("binding_complete() buffer might be truncated, aborting\n");
					return;
				}

				if (line_start == i)
					line_start_tmp = j;
				if (line_index == i)
					line_index_tmp = j;

				for (k = 0; k < tmp && buf[k]; k++)
					nline[j++] = buf[k];
			}
			/* XXX, put into loop, wcslen()+1? */
			if (line_start == i)
				line_start_tmp = j;
			if (line_index == i)
				line_index_tmp = j;

			nline[j] = '\0';

			debug("wcs-completion WC->MB (%d,%d) => (%d,%d) [%d;%d]\n", line_start, line_index, line_start_tmp, line_index_tmp, j, i);
			ncurses_complete(&line_start_tmp, &line_index_tmp, nline);

			nlen = strlen(nline);

			line_start = line_index = 0;
			for (i = 0, j = 0; j < nlen; i++) {
				int tmp;

				tmp = mbtowc(&line[i], &nline[j], nlen-j);

				if (tmp <= 0) {
					debug_error("binding_complete() mbtowc() failed (%d)\n", tmp);
					break;	/* return; */
				}

				if (line_start_tmp == j)
					line_start = i;
				if (line_index_tmp == j)
					line_index = i;

				j += tmp;
			}

			/* XXX, put into loop, <= nlen? */
			if (line_start_tmp == j)
				line_start = i;
			if (line_index_tmp == j)
				line_index = i;

			debug("wcs-completion MB->WC (%d,%d) => (%d,%d) [%d;%d]\n", line_start_tmp, line_index_tmp, line_start, line_index, j, i);
			line[i] = '\0';
#else
			ncurses_complete(&line_start, &line_index, (char *) line);
#endif
	} else {
		int i, count = 8 - (line_index % 8);

		if (xwcslen(line) + count >= LINE_MAXLEN - 1)
			return;

		memmove(line + line_index + count, line + line_index, sizeof(CHAR_T) * (LINE_MAXLEN - line_index - count));

		for (i = line_index; i < line_index + count; i++)
			line[i] = CHAR(' ');

		line_index += count;
	}
}

static BINDING_FUNCTION(binding_backward_char)
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

static BINDING_FUNCTION(binding_forward_char) {
	size_t linelen = xwcslen(line);
	if (lines) {
		if (line_index < linelen)
			line_index++;
		else {
			if (lines_index < array_count((char **) lines) - 1) {
				lines_index++;
				line_index = 0;
				line_start = 0;
			}
			lines_adjust();
		}

		return;
	}

	if (line_index < linelen)
		line_index++;
}

static BINDING_FUNCTION(binding_end_of_line)
{
	line_adjust();
}

static BINDING_FUNCTION(binding_beginning_of_line)
{
	line_index = 0;
	line_start = 0;
}

static void get_history_lines() {
	if (xwcschr(history[history_index], ('\015'))) {
		CHAR_T **tmp;
		int i, count;

		if (input_size == 1) {
			input_size = MULTILINE_INPUT_SIZE;
			ncurses_input_update(0);
		}

		tmp = wcs_array_make(history[history_index], TEXT("\015"), 0, 0, 0);
		count = array_count((char **) tmp);

		array_free((char **) lines);
		lines = xmalloc((count + 2) * sizeof(CHAR_T *));

		for (i = 0; i < count; i++) {
			lines[i] = xmalloc(LINE_MAXLEN * sizeof(CHAR_T));
			xwcscpy(lines[i], tmp[i]);
		}

		array_free((char **) tmp);

		line_index = 0;
		lines_index = 0;
		lines_adjust();
	} else {
		if (input_size != 1) {
			input_size = 1;
			ncurses_input_update(0);
		}
		xwcscpy(line, history[history_index]);
		line_adjust();
	}
}

BINDING_FUNCTION(binding_previous_only_history)
{
	if (!history[history_index + 1])
		return;

	if (history_index == 0) {
		if (lines) {
			add_to_history();

			history_index = 1;

			input_size = 1;
			ncurses_input_update(0);
		} else
			history[0] = xwcsdup(line);
	}

	history_index++;
	get_history_lines();

	if (lines) {
		lines_index = array_count((char **)lines) - 1;
		line_index = LINE_MAXLEN+1;
		lines_adjust();
	}
}

BINDING_FUNCTION(binding_next_only_history)
{
	if (history_index > 0) {
		history_index--;
		get_history_lines();
	} else /* history_index == 0 */
		binding_accept_line(BINDING_HISTORY_NOEXEC);
}


static BINDING_FUNCTION(binding_previous_history)
{
	if (lines && (lines_index || lines_start)) {
		if (lines_index - lines_start == 0 && lines_start)
			lines_start--;

		if (lines_index)
			lines_index--;

		lines_adjust();

	} else
		binding_previous_only_history(NULL);				
	ncurses_redraw_input(0);
}

static BINDING_FUNCTION(binding_next_history)
{
	int count = array_count((char **) lines);

	if (lines && (lines_index+1<count)) {
		if (lines_index - line_start == MULTILINE_INPUT_SIZE - 1)
			if (lines_index < count - 1)
				lines_start++;

		if (lines_index < count - 1)
			lines_index++;

		lines_adjust();

	} else 
		binding_next_only_history(NULL);
	ncurses_redraw_input(0);
}

void binding_helper_scroll(window_t *w, int offset) {
	ncurses_window_t *n;

	if (!w || !(n = w->priv_data))
		return;

	if (offset < 0) {
		n->start += offset;
		if (n->start < 0)
			n->start = 0;

	} else {
		n->start += offset;

		if (n->start > n->lines_count - w->height + n->overflow)
			n->start = n->lines_count - w->height + n->overflow;

		if (n->start < 0)
			n->start = 0;

	/* old code from: binding_forward_page() need it */
		if (w == window_current) {
			if (ncurses_current->start == ncurses_current->lines_count - window_current->height + ncurses_current->overflow) {
				window_current->more = 0;
				update_statusbar(0);
			}
		}
	}

	ncurses_redraw(w);
	ncurses_commit();
}

static void binding_helper_scroll_page(window_t *w, int backward) {
	if (!w)
		return;

	if (backward)
		binding_helper_scroll(w, -(w->height / 2));
	else
		binding_helper_scroll(w, +(w->height / 2));
}

static BINDING_FUNCTION(binding_backward_page) {
	binding_helper_scroll_page(window_current, 1);
}

static BINDING_FUNCTION(binding_forward_page) {
	binding_helper_scroll_page(window_current, 0);
}

static BINDING_FUNCTION(binding_backward_lastlog_page) {
	binding_helper_scroll_page(window_find_sa(NULL, "__lastlog", 1), 1);
}

static BINDING_FUNCTION(binding_forward_lastlog_page) {
	binding_helper_scroll_page(window_find_sa(NULL, "__lastlog", 1), 0);
}

static BINDING_FUNCTION(binding_backward_contacts_page) {
	binding_helper_scroll_page(window_find_sa(NULL, "__contacts", 1), 1);
}

static BINDING_FUNCTION(binding_forward_contacts_page) {
	binding_helper_scroll_page(window_find_sa(NULL, "__contacts", 1), 0);
}

static BINDING_FUNCTION(binding_backward_contacts_line) {
	binding_helper_scroll(window_find_sa(NULL, "__contacts", 1), -1);
}

static BINDING_FUNCTION(binding_forward_contacts_line) {
	binding_helper_scroll(window_find_sa(NULL, "__contacts", 1), 1);
}

static BINDING_FUNCTION(binding_ignore_query)
{
	if (!window_current->target)
		return;
	
	command_exec_format(window_current->target, window_current->session, 0, ("/ignore \"%s\""), window_current->target);
}

static BINDING_FUNCTION(binding_quick_list_wrapper)
{
	binding_quick_list(0, 0);
}

static BINDING_FUNCTION(binding_toggle_contacts_wrapper)
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

static BINDING_FUNCTION(binding_next_contacts_group) {
	window_t *w;

	contacts_group_index++;
	
	if ((w = window_find_sa(NULL, "__contacts", 1))) {
		ncurses_contacts_update(w, 0);
/*		ncurses_resize(); */ 
		ncurses_commit();
	}
}

static BINDING_FUNCTION(binding_ui_ncurses_debug_toggle)
{
	if (++ncurses_debug > 3)
		ncurses_debug = 0;

	update_statusbar(1);
}

static BINDING_FUNCTION(binding_cycle_sessions)
{
	if (window_session_cycle(window_current) == 0)
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

	args = array_make(action, (" \t"), 1, 1, 1);

	if (!args[0]) {
		array_free(args);
		return;
	}
	
#define __action(x,y) \
	if (!xstrcmp(args[0], (x))) { \
		b->function = y; \
		b->arg = xstrdup(args[1]); \
	} else	

	__action("backward-word", binding_backward_word)
	__action("forward-word", binding_forward_word)
	__action("kill-word", binding_kill_word)
	__action("toggle-input", binding_toggle_input)
	__action("cancel-input", binding_cancel_input)
	__action("backward-delete-char", binding_backward_delete_char)
	__action("beginning-of-line", binding_beginning_of_line)
	__action("end-of-line", binding_end_of_line)
	__action("delete-char", binding_delete_char)
	__action("backward-page", binding_backward_page)
	__action("forward-page", binding_forward_page)
	__action("kill-line", binding_kill_line)
	__action("window-kill", binding_window_kill)
	__action("yank", binding_yank)
	__action("accept-line", binding_accept_line)
	__action("line-discard", binding_line_discard)
	__action("quoted-insert", binding_quoted_insert)
	__action("word-rubout", binding_word_rubout)
	__action("backward-char", binding_backward_char)
	__action("forward-char", binding_forward_char)
	__action("previous-history", binding_previous_history)
	__action("previous-only-history", binding_previous_only_history)
	__action("next-history", binding_next_history)
	__action("next-only-history", binding_next_only_history)
	__action("complete", binding_complete)
	__action("quick-list", binding_quick_list_wrapper)
	__action("toggle-contacts", binding_toggle_contacts_wrapper)
	__action("next-contacts-group", binding_next_contacts_group)
	__action("ignore-query", binding_ignore_query)
	__action("ui-ncurses-debug-toggle", binding_ui_ncurses_debug_toggle)
	__action("cycle-sessions", binding_cycle_sessions)
	__action("forward-contacts-page", binding_forward_contacts_page)
	__action("backward-contacts-page", binding_backward_contacts_page)
	__action("forward-contacts-line", binding_forward_contacts_line)
	__action("backward-contacts-line", binding_backward_contacts_line)
	__action("forward-lastlog-page", binding_forward_lastlog_page)
	__action("backward-lastlog-page", binding_backward_lastlog_page)
	; /* no/unknown action */


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
static int binding_key(struct binding *b, const char *key, int add)
{
	/* debug("Key: %s\n", key); */
	if (!xstrncasecmp(key, ("Alt-"), 4)) {
		unsigned char ch;

#define __key(x, y, z) \
	if (!xstrcasecmp(key + 4, (x))) { \
		b->key = saprintf("Alt-%s", (x)); \
		if (add) { \
			ncurses_binding_map_meta[y] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map_meta[z] = ncurses_binding_map_meta[y]; \
		} \
		return 0; \
	}

	__key("Enter", 13, 0);
	__key("Backspace", KEY_BACKSPACE, 127);
	__key("Home", KEY_HOME, KEY_FIND);
	__key("End", KEY_END, KEY_SELECT);
	__key("Delete", KEY_DC, 0);
	__key("Insert", KEY_IC, 0);
	__key("Left", KEY_LEFT, 0);
	__key("Right", KEY_RIGHT, 0);
	__key("Up", KEY_UP, 0);
	__key("Down", KEY_DOWN, 0);
	__key("PageUp", KEY_PPAGE, 0);
	__key("PageDown", KEY_NPAGE, 0);

#undef __key

		if (xstrlen(key) != 5)
			return -1;
	
		ch = xtoupper(key[4]);

		b->key = saprintf(("Alt-%c"), ch);	/* XXX Alt-Ó ??? */

		if (add) {
			ncurses_binding_map_meta[ch] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
			if (xisalpha(ch))
				ncurses_binding_map_meta[xtolower(ch)] = ncurses_binding_map_meta[ch];
		}

		return 0;
	}

	if (!xstrncasecmp(key, ("Ctrl-"), 5)) {
		unsigned char ch;
		
//		if (xstrlen(key) != 6)
//			return -1;
#define __key(x, y, z) \
	if (!xstrcasecmp(key + 5, (x))) { \
		b->key = saprintf("Ctrl-%s", (x)); \
		if (add) { \
			ncurses_binding_map[y] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding))); \
			if (z) \
				ncurses_binding_map[z] = ncurses_binding_map[y]; \
		} \
		return 0; \
	}

	__key("Enter", KEY_CTRL_ENTER, 0);
	__key("Escape", KEY_CTRL_ESCAPE, 0);
	__key("Delete", KEY_CTRL_DC, 0);
	__key("Backspace", KEY_CTRL_BACKSPACE, 0);
	__key("Tab", KEY_CTRL_TAB, 0);

#undef __key
	
		ch = xtoupper(key[5]);
		b->key = saprintf(("Ctrl-%c"), ch);

		if (add) {
			if (xisalpha(ch))
				ncurses_binding_map[ch - 64] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
			else
				return -1;
		}
		
		return 0;
	}

	if (xtoupper(key[0]) == 'F' && atoi(key + 1)) {
		int f = atoi(key + 1);

		if (f < 1 || f > 63)
			return -1;

		b->key = saprintf(("F%d"), f);
		
		if (add)
			ncurses_binding_map[KEY_F(f)] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding)));
		
		return 0;
	}

#define __key(x, y, z) \
	if (!xstrcasecmp(key, (x))) { \
		b->key = xstrdup((x)); \
		if (add) { \
			ncurses_binding_map[y] = LIST_ADD2(&bindings, xmemdup(b, sizeof(struct binding))); \
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
	__key("Insert", KEY_IC, 0);
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
 * ncurses_binding_set()
 *
 * it sets some sequence to the given key
 */
void ncurses_binding_set(int quiet, const char *key, const char *sequence)
{
	struct binding *d;
	binding_added_t *b;
	struct binding *binding_orginal = NULL;
	char *joined = NULL;
	int count = 0;

	for (d = bindings; d; d = d->next) {
		if (!xstrcasecmp(key, d->key)) {
			binding_orginal = d;
			break;
		}
	}

	if (!binding_orginal) {
		printq("bind_doesnt_exist", key);
		return;
	}

	if (!sequence) {
		char **chars = NULL;
		char ch;
		printq("bind_press_key");
		nodelay(input, FALSE);
		while ((ch = wgetch(input)) != ERR) {
			array_add(&chars, xstrdup(itoa(ch)));
			nodelay(input, TRUE);
			count++;
		}
		joined = array_join(chars, (" "));
		array_free(chars);
	} else
		joined = xstrdup(sequence);

	for (b = bindings_added; b; b = b->next) {
		if (!xstrcasecmp(b->sequence, joined)) {
			b->binding = binding_orginal;
			xfree(joined);
			goto end;
		}
	}

	b = xmalloc(sizeof(binding_added_t));
	b->sequence = joined;
	b->binding = binding_orginal;
	LIST_ADD2(&bindings_added, b);
end:
	if (!in_autoexec)
		config_changed = 1;
	printq("bind_added");
	if (count > bindings_added_max)
		bindings_added_max = count;
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
	struct binding b, *c = NULL, *d;
	
	if (!key || !action)
		return;

	memset(&b, 0, sizeof(b));

	b.internal = internal;
	
	for (d = bindings; d; d = d->next) {
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
		b.default_action	= xstrdup(b.action);
		b.default_function	= b.function;
		b.default_arg		= xstrdup(b.arg);
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
	struct binding *b;

	if (!key)
		return;

	for (b = bindings; b; b = b->next) {
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
			b->action	= xstrdup(b->default_action);
			b->arg		= xstrdup(b->default_arg);
			b->function	= b->default_function;
			b->internal	= 1;
		} else {
			xfree(b->key);
			for (i = 0; i < KEY_MAX + 1; i++) {
				if (ncurses_binding_map[i] == b)
					ncurses_binding_map[i] = NULL;
				if (ncurses_binding_map_meta[i] == b)
					ncurses_binding_map_meta[i] = NULL;

			}

			LIST_REMOVE2(&bindings, b, NULL);
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
QUERY(ncurses_binding_default)
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
	ncurses_binding_add("Alt-K", "window-kill", 1, 1);
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
	/* ncurses_binding_add("Ctrl-Down", "forward-contacts-page", 1, 1); 
	ncurses_binding_add("Ctrl-Up", "backward-contacts-page", 1, 1); */
	return 0;
}

void ncurses_binding_init()
{
	va_list dummy;
	memset(ncurses_binding_map, 0, sizeof(ncurses_binding_map));
	memset(ncurses_binding_map_meta, 0, sizeof(ncurses_binding_map_meta));

	ncurses_binding_default(NULL, dummy);
	ncurses_binding_complete	= binding_complete;
	ncurses_binding_accept_line	= binding_accept_line;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
