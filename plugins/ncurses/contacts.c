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
#include <ekg/metacontacts.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/windows.h>

#include "old.h"

int contacts_group_index = 0;

static int contacts_margin = 1;
static int contacts_edge = WF_RIGHT;
static int contacts_frame = WF_LEFT;
static int contacts_descr = 0;
static int contacts_wrap = 0;
#define CONTACTS_ORDER_DEFAULT "opvoluavawdnxainno"
static char contacts_order[100] = CONTACTS_ORDER_DEFAULT;


/*
 * we need this structure because we have to add it to the list 
 * maybe stupid way, but at the moment i couldn't find better (del)
 */
typedef struct {
	char *name;
        char *status;
	char *line;
} contact_t;

/*
 * contacts_compare()
 * 
 * helps list_add_sorted() 
 */
static int contacts_compare(void *data1, void *data2)
{
        contact_t *a = data1, *b = data2;

        if (!a || !a->status || !a->name || !b || !b->status || !b->name)
                return 0;

	if (xstrncasecmp(a->status, b->status, 2))
		return 1;
  	else 
		return xstrcasecmp(a->name, b->name);
}


/*
 * ncurses_contacts_update()
 *
 * updates contacts window 
 * 
 * it switches also groups, metacontacts, all together
 * details in documentation
 * 
 */
int ncurses_contacts_update(window_t *w)
{
	const char *header = NULL, *footer = NULL;
	char *group = NULL;
	int j, old_start;
	int all = 0; /* 1 - all, 2 - metacontacts */
	ncurses_window_t *n;
	list_t sorted = NULL;
	
	if (!w) {
		list_t l;

		for (l = windows; l; l = l->next) {
			window_t *v = l->data;
			
			if (v->target && !xstrcmp(v->target, "__contacts")) {
				w = v;
				break;
			}
		}

		if (!w)
			return -1;
	}

	n = w->private;
	
	old_start = n->start;
	ncurses_clear(w, 1);
	n->start = old_start;

        if (!session_current)
		return -1;



	if (config_contacts_groups) {
		char **groups = array_make(config_contacts_groups, ", ", 0, 1, 0);
		int count = array_count(groups);

                if (contacts_group_index > count + 2) {
                        contacts_group_index = 0;
                        all = 0;
                }

		if (contacts_group_index > count + 1) {
			all = 2;
			goto group_cleanup;
		}

		if (contacts_group_index > count) {
			all = 1;
			goto group_cleanup;
		}

		if (contacts_group_index > 0) {
			group = groups[contacts_group_index - 1];
			if (*group == '@')
				group++;
			group = xstrdup(group);
			header = format_find("contacts_header_group");
			footer = format_find("contacts_footer_group");
		}
group_cleanup:
		array_free(groups);
	} else if (contacts_group_index) {
		if (contacts_group_index > 2) {
			all = 0;
			contacts_group_index = 0;
		} else {
			all = contacts_group_index;
		}
	}

	if (all == 2) {
		header = format_find("contacts_metacontacts_header");
		footer = format_find("contacts_metacontacts_footer");
	} 

        if (!session_current->userlist && !all && contacts_group_index == 0) {
                n->redraw = 1;
                return 0;
        }


	if (!header || !footer) {
		header = format_find("contacts_header");
		footer = format_find("contacts_footer");
	}
	
	if (xstrcmp(header, "")) {
		ncurses_backlog_add(w, fstring_new(format_string(header, group)));
	}

	
	for (j = 0; j < xstrlen(contacts_order); j += 2) {
		int count = 0;
		list_t l;
		list_t lp;
		const char *footer_status = NULL;
		char tmp[100];

		for (lp = sessions, count = 0; lp && all != 2; lp = lp->next) {
			session_t *s = lp->data;
			for (l = (all) ? s->userlist : session_current->userlist; l; l = l->next) {
				userlist_t *u = l->data;
				const char *format;
				char *line;
				contact_t c;

				if (!u->status || !u->nickname || xstrncmp(u->status, contacts_order + j, 2))
					continue;
	
				if (group && !ekg_group_member(u, group))
					continue;
				
				if (!count) {
					snprintf(tmp, sizeof(tmp), "contacts_%s_header", u->status);
					format = format_find(tmp);
					if (xstrcmp(format, ""))
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
					xstrcat(tmp, "_blink");
	
				line = format_string(format_find(tmp), u->nickname, u->descr);

				memset(&c, 0, sizeof(c));
				c.status = xstrdup(u->status);
				c.line = xstrdup(line);
				c.name = xstrdup(u->nickname);
				list_add_sorted(&sorted, &c, sizeof(c), contacts_compare);

				xfree(line);
	
				count++;
			}
			if (!all)
				break;
		}
		if (all) { 
			for (l = metacontacts; l; l = l->next) {
                                metacontact_t *m = l->data;
                                const char *format;
                                char *line;
				metacontact_item_t *i = metacontact_find_prio(m);
				userlist_t *u = (i) ? userlist_find_n(i->s_uid, i->name) : NULL;
                                contact_t c;

				if (!i)
					continue;

                                if (!u->status || !u->nickname || xstrncmp(u->status, contacts_order + j, 2))
                                        continue;

                                if (!count) {
                                        snprintf(tmp, sizeof(tmp), "contacts_%s_header", u->status);
                                        format = format_find(tmp);
                                        if (xstrcmp(format, ""))
                                                ncurses_backlog_add(w, fstring_new(format_string(format)));
                                        footer_status = u->status;
                                }

                                if (u->descr && contacts_descr)
                                        snprintf(tmp, sizeof(tmp), "contacts_%s_descr_full", u->status);
                                else if (u->descr && !contacts_descr)
                                        snprintf(tmp, sizeof(tmp), "contacts_%s_descr", u->status);

                                else
                                        snprintf(tmp, sizeof(tmp), "contacts_%s", u->status);

                                line = format_string(format_find(tmp), m->name, u->descr);

                                memset(&c, 0, sizeof(c));
                                c.status = xstrdup(u->status);
                                c.line = xstrdup(line);
                                c.name = xstrdup(m->name);
                                list_add_sorted(&sorted, &c, sizeof(c), contacts_compare);

                                xfree(line);

                                count++;
			}
		} 

		if (count) {
			const char *format;

			for (; sorted; sorted = sorted->next) {
				contact_t *c = sorted->data;

				ncurses_backlog_add(w, fstring_new(c->line));

				xfree(c->line);
				xfree(c->status);
				xfree(c->name);
			}
			if (sorted)
				list_destroy(sorted, 1);

			snprintf(tmp, sizeof(tmp), "contacts_%s_footer", footer_status);
			format = format_find(tmp);
		
			if (xstrcmp(format, ""))
				ncurses_backlog_add(w, fstring_new(format_string(format)));
		}
	}

	if (xstrcmp(footer, "")) 
		ncurses_backlog_add(w, fstring_new(format_string(footer, group)));

	xfree(group);

	n->redraw = 1;

	return 0;
}

/*
 * ncurses_contacts_changed()
 *
 * wywo³ywane przy zmianach rozmiaru i w³±czeniu klienta.
 */
void ncurses_contacts_changed(const char *name)
{
	window_t *w = NULL;
	list_t l;

	if (!xstrcasecmp(name, "ncurses:contacts_size"))
		config_contacts = 1;

	if (config_contacts_size < 0) 
		config_contacts_size = 0;

        if (config_contacts_size == 0)
                config_contacts = 0;

	if (config_contacts_size > 1000)
		config_contacts_size = 1000;
	
	contacts_margin = 1;
	contacts_edge = WF_RIGHT;
	contacts_frame = WF_LEFT;
	xstrcpy(contacts_order, CONTACTS_ORDER_DEFAULT);
	contacts_wrap = 0;
	contacts_descr = 0;

	if (config_contacts_options) {
		char **args = array_make(config_contacts_options, " \t,", 0, 1, 1);
		int i;

		for (i = 0; args[i]; i++) {
			if (!xstrcasecmp(args[i], "left")) {
				contacts_edge = WF_LEFT;
				if (contacts_frame)
					contacts_frame = WF_RIGHT;
			}

			if (!xstrcasecmp(args[i], "right")) {
				contacts_edge = WF_RIGHT;
				if (contacts_frame)
					contacts_frame = WF_LEFT;
			}

			if (!xstrcasecmp(args[i], "top")) {
				contacts_edge = WF_TOP;
				if (contacts_frame)
					contacts_frame = WF_BOTTOM;
			}

			if (!xstrcasecmp(args[i], "bottom")) {
				contacts_edge = WF_BOTTOM;
				if (contacts_frame)
					contacts_frame = WF_TOP;
			}

			if (!xstrcasecmp(args[i], "noframe"))
				contacts_frame = 0;

			if (!xstrcasecmp(args[i], "frame")) {
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

			if (!xstrncasecmp(args[i], "margin=", 7)) {
				contacts_margin = atoi(args[i] + 7);
				if (contacts_margin > 10)
					contacts_margin = 10;
				if (contacts_margin < 0)
					contacts_margin = 0;
			}

			if (!xstrcasecmp(args[i], "nomargin"))
				contacts_margin = 0;

			if (!xstrcasecmp(args[i], "wrap"))
				contacts_wrap = 1;

			if (!xstrcasecmp(args[i], "nowrap"))
				contacts_wrap = 0;

			if (!xstrcasecmp(args[i], "descr"))
				contacts_descr = 1;

			if (!xstrcasecmp(args[i], "nodescr"))
				contacts_descr = 0;

			if (!xstrncasecmp(args[i], "order=", 6))
				snprintf(contacts_order, sizeof(contacts_order), args[i] + 6);
		}

		if (contacts_margin < 0)
			contacts_margin = 0;

		array_free(args);
	}
	
	for (l = windows; l; l = l->next) {
		window_t *v = l->data;

		if (v->target && !xstrcmp(v->target, "__contacts")) {
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
	
	ncurses_contacts_update(NULL);
        ncurses_resize();
	ncurses_commit();
}

/*
 * ncurses_contacts_new()
 *
 * dostosowuje nowoutworzone okno do listy kontaktów.
 */
void ncurses_contacts_new(window_t *w)
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
	n->handle_redraw = ncurses_contacts_update;
	w->nowrap = !contacts_wrap;
	n->start = 0;
}


