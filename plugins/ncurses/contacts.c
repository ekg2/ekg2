/* $Id$ */

/*
 *  (C) Copyright 2002-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Wojtek Bojdo³ <wojboj@htcon.pl>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *			    Piotr Kupisiewicz <deli@rzepaknet.us>
 *			    Leszek Krupiñski <leafnode@pld-linux.org>
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

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include <ekg/commands.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/metacontacts.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/windows.h>

#include "bindings.h"
#include "old.h"
#include "mouse.h"
#include "contacts.h"

int contacts_group_index = 0;

static int contacts_edge = WF_RIGHT;
static int contacts_frame = WF_LEFT;
#define CONTACTS_ORDER_DEFAULT "chavawxadninnouner"			/* if you modify it, please modify also CONTACTS_ORDER_DEFAULT_LEN */
#define CONTACTS_ORDER_DEFAULT_LEN 18					/* CONTACTS_ORDER_DEFAULT_LEN == strlen(CONTACTS_ORDER_DEFAULT) */
static char contacts_order[32] = CONTACTS_ORDER_DEFAULT;
static size_t corderlen	= CONTACTS_ORDER_DEFAULT_LEN;			/* it must be always equal xstrlen(contacts_order) XXX please note if you add somewhere code which modify contacts_order */

/* vars */
int config_contacts_size;
int config_contacts;
int config_contacts_groups_all_sessions;
int config_contacts_descr = 0;
int config_contacts_edge;
int config_contacts_frame;
int config_contacts_margin = 1;
int config_contacts_orderbystate = 1;
int config_contacts_wrap = 0;
char *config_contacts_order;
char *config_contacts_groups;
int config_contacts_metacontacts_swallow;

/* 
 * funkcja zwraca pierwsze literki status avail -> av away -> aw itd... 
 * funkcja nie sprawdza czy status jest NULL, ani czy strlen(status) > 2 
 */
static inline char *get_short_status(int status) {
	static char buf[3];
	const char *status_t = ekg_status_string(status, 0);

	buf[0] = status_t[0];
	buf[1] = status_t[1];
	buf[2] = 0; 		/* ? */
	return &buf[0];
}

/*
 * contacts_compare()
 * 
 * helps list_add_sorted() 
 */
static int contacts_compare(void *data1, void *data2)
{
	userlist_t *a = data1, *b = data2;

	if (!a || !a->nickname || !b || !b->nickname)
		return 0;

	return xstrcasecmp(a->nickname, b->nickname);
}

/*
 * userlist_dup()
 *
 * Duplicate entry, with private set to priv.
 */

static inline userlist_t *userlist_dup(userlist_t *up, char *uid, char *nickname, void *priv) {
	userlist_t *u = xmalloc(sizeof(userlist_t));

	u->uid		= uid;
	u->nickname	= nickname;
	u->descr	= up->descr;
	u->status	= up->status;
	u->xstate	= up->xstate;
	u->private	= priv;
	return u;
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
int ncurses_contacts_update(window_t *w, int save_pos) {
	int old_start;

	const char *header = NULL, *footer = NULL;
	char *group = NULL;
	int j;
	int all = 0; /* 1 - all, 2 - metacontacts */
	ncurses_window_t *n;
	newconference_t *c	= NULL;
	list_t sorted_all	= NULL;
	int (*comp)(void *, void *) = NULL;		/* coz userlist's list are sorted we don't need to sort it again... 
								unfortunetly if we create list from serveral userlists (for instance: session && window)
								we must resort... --- in ekg2 we do 
									list_add_sorted(...., NULL) on session userlist &&
									list_add_sorted(...., contacts_compare) on window userlist
							*/

	
	if (!w) w = window_find_sa(NULL, "__contacts", 1);
	if (!w)
		return -1;

	n = w->private;
	
	if (save_pos) 
		old_start = n->start;
	else
		old_start = 0;
	
	ncurses_clear(w, 1);

	if (!session_current)
		goto kon;

	if (config_contacts_groups) {
		char **groups = array_make(config_contacts_groups, ", ", 0, 1, 0);
		int count = array_count(groups);

		if (contacts_group_index > count + 2) {
			contacts_group_index = 0;
			all = 0;
		}

		if (contacts_group_index > count + 1) {
			all = (metacontacts) ? 2 : 0;
			goto group_cleanup;
		}

		if (contacts_group_index > count) {
			all = 1;
			goto group_cleanup;
		}

		if (contacts_group_index > 0) {
			all = config_contacts_groups_all_sessions ? 1 : 0;
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

	c = newconference_find(window_current->session, window_current->target);
	if (!session_current->userlist && !window_current->userlist && (!c || !c->participants) && !all && contacts_group_index == 0)
		goto kon;

	if (!header || !footer) {
		header = format_find("contacts_header");
		footer = format_find("contacts_footer");
	}

	if (xstrcmp(header, "")) {
		char *tmp = format_string(header, group);
		ncurses_backlog_add(w, fstring_new(tmp));
		xfree(tmp);
	}

	if (all == 1) {
		list_t l;

		for (l = sessions; l; l = l->next) {
			session_t *s = l->data;
			list_t lp;

			if (!s->userlist)
				continue;

			for (lp = s->userlist; lp; lp = lp->next) {
				userlist_t *u = lp->data;

				list_add_sorted(&sorted_all, userlist_dup(u, u->uid, u->nickname, s), 0, comp);
			}

			comp = contacts_compare;		/* turn on sorting */
		}

		for (l = c ? c->participants : window_current->userlist; l; l = l->next) {
			userlist_t *u = l->data;

			list_add_sorted(&sorted_all, userlist_dup(u, u->uid, u->nickname, w->session), 0, comp);
		}

		if (sorted_all) comp = contacts_compare;	/* like above */
	}

	if (all == 1 || all == 2) {
		list_t l;

		/* Remove contacts contained in metacontacts. */
		if (all == 1 && config_contacts_metacontacts_swallow) {
			for (l = metacontacts; l; l = l->next) {
				metacontact_t *m = l->data;
				list_t ml;

				/* metacontact_find_prio() should always success [for current API] */
/*
				if (!metacontact_find_prio(m)) 
					continue;
*/
				for (ml = m->metacontact_items; ml; ml = ml->next) {
					metacontact_item_t *i = ml->data;
					userlist_t *u;
					list_t sl;

					if (!(u = userlist_find_n(i->s_uid, i->name)))
						continue;

					for (sl = sorted_all; sl; sl = sl->next) {
						userlist_t *up = sl->data;

			/* up->uid == u->uid (?) */
						if (up->uid && !xstrcmp(up->uid, u->uid))
							list_remove(&sorted_all, up, 1);
					}
				}
			}
		}

		for (l = metacontacts; l; l = l->next) {
			metacontact_t *m = l->data;

			metacontact_item_t *i;
			userlist_t *u;

			if (!(i = metacontact_find_prio(m)))
				continue;

			if (!(u = userlist_find_n(i->s_uid, i->name)))
				continue;

			list_add_sorted(&sorted_all, userlist_dup(u, NULL, m->name, (void *) 2), 0, comp);
		}
	}

	if (!all) {
		sorted_all = session_current->userlist;

		if (c && c->participants) 
			sorted_all = c->participants;
		else if (window_current->userlist)
			sorted_all = window_current->userlist;
	}

	if (!sorted_all)
		goto after_loop;	/* it skips this loop below */

	for (j = 0; j < corderlen; /* xstrlen(contacts_order); */ j += 2) {
		const char *footer_status = NULL;
		int count = 0;
		fstring_t *string;
		char *line;
		char tmp[100];
		list_t l;

		for (l = sorted_all; l; l = l->next) {
			userlist_t *u = l->data;

			const char *status_t;
			const char *format;

			if (!u->nickname || !u->status) 
				continue;

			status_t = ekg_status_string(u->status, 0);

			if (config_contacts_orderbystate && xstrncmp(status_t, contacts_order + j, 2))
				continue;

			if (!config_contacts_orderbystate && !xstrstr(contacts_order, get_short_status(u->status)))
				continue;

			if (group && (!u->private || (void *) 2 != u->private)) {
				userlist_t *tmp = userlist_find(u->private ? u->private : session_current, u->uid);
				if ((group[0]=='!' && ekg_group_member(tmp, group+1)) ||
						(group[0]!='!' && !ekg_group_member(tmp, group)))
					continue;
			}

			if (!count) {
				snprintf(tmp, sizeof(tmp), "contacts_%s_header", status_t);
				format = format_find(tmp);
				if (xstrcmp(format, "")) {
					line = format_string(format);
					string = fstring_new(line);
					ncurses_backlog_add(w, string);
					xfree(line);
				}
				footer_status = status_t;
			}

			if (u->descr && config_contacts_descr)
				snprintf(tmp, sizeof(tmp), "contacts_%s_descr_full", status_t);
			else if (u->descr && !config_contacts_descr)
				snprintf(tmp, sizeof(tmp), "contacts_%s_descr", status_t);
			else
				snprintf(tmp, sizeof(tmp), "contacts_%s", status_t);

			if (u->xstate & EKG_XSTATE_BLINK)
				xstrcat(tmp, "_blink");
			if (u->xstate & EKG_XSTATE_TYPING)
				xstrcat(tmp, "_typing");

			line = format_string(format_find(tmp), u->nickname, u->descr);
			string = fstring_new(line);

			if (u->private == (void *) 2)
				string->private = (void *) xstrdup(u->nickname);
			else 
				string->private = (void *) saprintf("%s/%s", (u->private) ? ((session_t *) u->private)->uid : session_current->uid, u->nickname);

			ncurses_backlog_add(w, string);
			xfree(line);

			count++;
		}

		if (count) {
			const char *format;

			snprintf(tmp, sizeof(tmp), "contacts_%s_footer", footer_status);
			format = format_find(tmp);

			if (xstrcmp(format, "")) {
				line = format_string(format);
				string = fstring_new(line);
				ncurses_backlog_add(w, string);
				xfree(line);
			}
		}

		if (!config_contacts_orderbystate)
			break;
	}

after_loop:
	if (xstrcmp(footer, "")) {
		char *tmp = format_string(footer, group);
		ncurses_backlog_add(w, fstring_new(tmp));
		xfree(tmp);
	}

	if (all)
		list_destroy(sorted_all, 1);

	xfree(group);

kon:
/* restore old index */
	n->start = old_start;
	
	if (n->start > n->lines_count - w->height + n->overflow)
		n->start = n->lines_count - w->height + n->overflow;

	if (n->start < 0)
		n->start = 0;

/* redraw */
	n->redraw = 1;
	ncurses_redraw(w);

	return -1;
}

/*
 * ncurses_contacts_changed()
 *
 * wywo³ywane przy zmianach rozmiaru i w³±czeniu klienta.
 */

void ncurses_contacts_changed(const char *name) {
	window_t *w = NULL;

	if (in_autoexec)
		return;

	if (!xstrcasecmp(name, "ncurses:contacts_size"))
		config_contacts = 1;

	if (config_contacts_size < 0) 
		config_contacts_size = 0;

	if (config_contacts_size == 0)
		config_contacts = 0;

	if (config_contacts_size > 1000)
		config_contacts_size = 1000;

	if (config_contacts_margin > 10)
		config_contacts_margin = 10;

	if (config_contacts_edge > 3)
		config_contacts_edge = 2;

	contacts_edge = (1 << config_contacts_edge);
	contacts_frame = (!config_contacts_frame ? 0
			: contacts_edge & (WF_LEFT|WF_RIGHT) ? contacts_edge ^ (WF_LEFT|WF_RIGHT)
			: contacts_edge ^ (WF_TOP|WF_BOTTOM));

	if (config_contacts_order) {
		strlcpy(contacts_order, config_contacts_order, sizeof(contacts_order));
		corderlen = xstrlen(contacts_order);
	} else {
		xstrcpy(contacts_order, CONTACTS_ORDER_DEFAULT);
		corderlen = CONTACTS_ORDER_DEFAULT_LEN;	/* xstrlen(CONTACTS_ORDER_DEFAULT) eq CONTACTS_ORDER_DEFAULT_LEN */
	}

	/* XXX destroy window only if (!config_contacts) ? XXX */
	if ((w = window_find_sa(NULL, "__contacts", 1))) {
		window_kill(w);
		w = NULL;
	}

	if (config_contacts /* && !w */) {
		w = window_new("__contacts", NULL, 1000);
		ncurses_contacts_update(w, 0);
	}

	ncurses_resize();
	ncurses_commit();
}

/* 
 * ncurses_contacts_mouse_handler()
 * 
 * handler for mouse events
 */
void ncurses_contacts_mouse_handler(int x, int y, int mouse_state) 
{
	window_t *w = window_find_sa(NULL, "__contacts", 1);
	ncurses_window_t *n;

	if (mouse_state == EKG_SCROLLED_UP) {
		binding_helper_scroll(w, -5);
		return;
	} else if (mouse_state == EKG_SCROLLED_DOWN) {
		binding_helper_scroll(w, 5);
		return;
	}

	if (!w || mouse_state != EKG_BUTTON1_DOUBLE_CLICKED)
		return;

	n = w->private;

	if (!w->nowrap) {
		/* here new code, should work also with w->nowrap == 1 */
		y -= 1;		/* ??? */

		if (y < 0 || y >= n->lines_count)
			return;

		y = n->lines[y].backlog;
	} else {
		/* here old code */
		if (y > n->backlog_size)
			return;

		y = n->backlog_size - y;
	}

	if (y >= n->backlog_size) {
		/* error */
		return;
	}

	command_exec_format(NULL, NULL, 0, ("/query \"%s\""), n->backlog[y]->private);
	return;
}

static int ncurses_contacts_update_redraw(window_t *w) { return 0; } 

/*
 * ncurses_contacts_new()
 *
 * dostosowuje nowoutworzone okno do listy kontaktów.
 */
void ncurses_contacts_new(window_t *w)
{
	int size = config_contacts_size + config_contacts_margin + ((contacts_frame) ? 1 : 0);
	ncurses_window_t *n = w->private;

	switch (contacts_edge) {
		case WF_LEFT:
			w->width = size;
			n->margin_right = config_contacts_margin;
			break;
		case WF_RIGHT:
			w->width = size;
			n->margin_left = config_contacts_margin;
			break;
		case WF_TOP:
			w->height = size;
			n->margin_bottom = config_contacts_margin;
			break;
		case WF_BOTTOM:
			w->height = size;
			n->margin_top = config_contacts_margin;
			break;
	}

	w->floating = 1;
	w->edge = contacts_edge;
	w->frames = contacts_frame;
	n->handle_redraw = ncurses_contacts_update_redraw;
	n->handle_mouse = ncurses_contacts_mouse_handler;
	w->nowrap = !config_contacts_wrap;
	n->start = 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=0 noexpandtab sw=8
 */
