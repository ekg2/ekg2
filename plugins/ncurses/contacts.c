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

#include "config.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <ekg/commands.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/windows.h>

#include "old.h"

int config_contacts_size = 9;		/* szeroko¶æ okna kontaktów */
int config_contacts = 2;		/* czy ma byæ okno kontaktów */
char *config_contacts_options = NULL;	/* opcje listy kontaktów */
char *config_contacts_groups = NULL;	/* grupy listy kontaktów */

int contacts_group_index = 0;
int contacts_index = 0;

static int contacts_margin = 1;
static int contacts_edge = WF_RIGHT;
static int contacts_frame = WF_LEFT;
static int contacts_descr = 0;
static int contacts_wrap = 0;
#define CONTACTS_ORDER_DEFAULT "opvoluavawdnxainno"
static char contacts_order[100] = CONTACTS_ORDER_DEFAULT;

/*
 * contacts_update()
 *
 * uaktualnia okno listy kontaktów.
 */
int contacts_update(window_t *w)
{
	const char *header = NULL, *footer = NULL;
	char *group = NULL;
	int j;
	ncurses_window_t *n;
	

        if (!window_current->session || !session_current || !session_current->userlist) 
                return -1;
	
	if (!w) {
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *v = l->data;
			
			if (v->target && !strcmp(v->target, "__contacts")) {
				w = v;
				break;
			}
		}

		if (!w)
			return -1;
	}

	n = w->private;
	
	ncurses_clear(w, 1);

	if (config_contacts_groups) {
		char **groups = array_make(config_contacts_groups, ", ", 0, 1, 0);
		if (contacts_group_index > array_count(groups))
			contacts_group_index = 0;

		if (contacts_group_index > 0) {
			group = groups[contacts_group_index - 1];
			if (*group == '@')
				group++;
			group = xstrdup(group);
			header = format_find("contacts_header_group");
			footer = format_find("contacts_footer_group");
		}

		array_free(groups);
	}

	if (!header || !footer) {
		header = format_find("contacts_header");
		footer = format_find("contacts_footer");
	}
	
	if (strcmp(header, "")) 
		ncurses_backlog_add(w, fstring_new(format_string(header, group)));

	for (j = 0; j < strlen(contacts_order); j += 2) {
		int count = 0;
		list_t l;
		const char *footer_status = NULL;
		char tmp[100];


		for (l = session_current->userlist, count = 0; l; l = l->next) {
			userlist_t *u = l->data;
			const char *format;
			char *line;

			if (!u->status || !u->nickname || strncmp(u->status, contacts_order + j, 2))
				continue;

			if (group && !group_member(u, group))
				continue;

			if (!count) {
				snprintf(tmp, sizeof(tmp), "contacts_%s_header", u->status);
				format = format_find(tmp);
				if (strcmp(format, ""))
					ncurses_backlog_add(w, fstring_new(format_string(format)));
				footer_status = u->status;
			}

			if (u->descr && contacts_descr)
				snprintf(tmp, sizeof(tmp), "contacts_%s_descr_full", u->status);
			else if (u->descr && !contacts_descr)
				snprintf(tmp, sizeof(tmp), "contacts_%s_descr", u->status);
				
			else
				snprintf(tmp, sizeof(tmp), "contacts_%s", u->status);
			
			if(u->blink)
				strcat(tmp, "_blink");

			line = format_string(format_find(tmp), u->nickname, u->descr);
			if(count >= contacts_index)	
				ncurses_backlog_add(w, fstring_new(line));
			xfree(line);

			count++;
		}

		if (count) {
			const char *format;

			snprintf(tmp, sizeof(tmp), "contacts_%s_footer", footer_status);
			format = format_find(tmp);
		
			if (strcmp(format, ""))
				ncurses_backlog_add(w, fstring_new(format_string(format)));
		}
	}

	if (strcmp(footer, "")) 
		ncurses_backlog_add(w, fstring_new(format_string(footer, group)));

	xfree(group);

	n->redraw = 1;

	return 0;
}

/*
 * contacts_changed()
 *
 * wywo³ywane przy zmianach rozmiaru i w³±czeniu klienta.
 */
void contacts_changed(const char *name)
{
	window_t *w = NULL;
	list_t l;

	if (config_contacts_size < 0)
		config_contacts_size = 0;

	if (config_contacts_size > 1000)
		config_contacts_size = 1000;
	
	contacts_margin = 1;
	contacts_edge = WF_RIGHT;
	contacts_frame = WF_LEFT;
	strcpy(contacts_order, CONTACTS_ORDER_DEFAULT);
	contacts_wrap = 0;
	contacts_descr = 0;

	if (config_contacts_options) {
		char **args = array_make(config_contacts_options, " \t,", 0, 1, 1);
		int i;

		for (i = 0; args[i]; i++) {
			if (!strcasecmp(args[i], "left")) {
				contacts_edge = WF_LEFT;
				if (contacts_frame)
					contacts_frame = WF_RIGHT;
			}

			if (!strcasecmp(args[i], "right")) {
				contacts_edge = WF_RIGHT;
				if (contacts_frame)
					contacts_frame = WF_LEFT;
			}

			if (!strcasecmp(args[i], "top")) {
				contacts_edge = WF_TOP;
				if (contacts_frame)
					contacts_frame = WF_BOTTOM;
			}

			if (!strcasecmp(args[i], "bottom")) {
				contacts_edge = WF_BOTTOM;
				if (contacts_frame)
					contacts_frame = WF_TOP;
			}

			if (!strcasecmp(args[i], "noframe"))
				contacts_frame = 0;

			if (!strcasecmp(args[i], "frame")) {
				switch (contacts_edge) {
					case WF_TOP:
						contacts_frame = WF_BOTTOM;
						break;
					case WF_BOTTOM:
						contacts_frame = WF_TOP;
						break;
					case WF_LEFT:
						contacts_frame = WF_RIGHT;
						break;
					case WF_RIGHT:
						contacts_frame = WF_LEFT;
						break;
				}
			}

			if (!strncasecmp(args[i], "margin=", 7)) {
				contacts_margin = atoi(args[i] + 7);
				if (contacts_margin > 10)
					contacts_margin = 10;
				if (contacts_margin < 0)
					contacts_margin = 0;
			}

			if (!strcasecmp(args[i], "nomargin"))
				contacts_margin = 0;

			if (!strcasecmp(args[i], "wrap"))
				contacts_wrap = 1;

			if (!strcasecmp(args[i], "nowrap"))
				contacts_wrap = 0;

			if (!strcasecmp(args[i], "descr"))
				contacts_descr = 1;

			if (!strcasecmp(args[i], "nodescr"))
				contacts_descr = 0;

			if (!strncasecmp(args[i], "order=", 6))
				snprintf(contacts_order, sizeof(contacts_order), args[i] + 6);
		}

		if (contacts_margin < 0)
			contacts_margin = 0;

		array_free(args);
	}
	
	for (l = windows; l; l = l->next) {
		window_t *v = l->data;

		if (v->target && !strcmp(v->target, "__contacts")) {
			w = v;
			break;
		}
	}

	if (w) {
		window_kill(w, 1);
		w = NULL;
	}

	if (config_contacts && !w)
		window_new("__contacts", NULL, 1000);
	
	contacts_update(NULL);
        ncurses_resize();
	ncurses_commit();
}

/*
 * contacts_new()
 *
 * dostosowuje nowoutworzone okno do listy kontaktów.
 */
void contacts_new(window_t *w)
{
	int size = config_contacts_size + contacts_margin + ((contacts_frame) ? 1 : 0);
	ncurses_window_t *n = w->private;

	switch (contacts_edge) {
		case WF_LEFT:
			w->width = size;
			n->margin_right = contacts_margin;
			break;
		case WF_RIGHT:
			w->width = size;
			n->margin_left = contacts_margin;
			break;
		case WF_TOP:
			w->height = size;
			n->margin_bottom = contacts_margin;
			break;
		case WF_BOTTOM:
			w->height = size;
			n->margin_top = contacts_margin;
			break;
	}

	w->floating = 1;
	w->edge = contacts_edge;
	w->frames = contacts_frame;
	n->handle_redraw = contacts_update;
	w->nowrap = !contacts_wrap;
	n->start = 10;
}


