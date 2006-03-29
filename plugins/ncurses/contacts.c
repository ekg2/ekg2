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

#include <ekg/commands.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/metacontacts.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/windows.h>

#include "old.h"
#include "mouse.h"
#include "contacts.h"

int contacts_index = 0;
int contacts_group_index = 0;

static int contacts_margin = 1;
static int contacts_edge = WF_RIGHT;
static int contacts_frame = WF_LEFT;
static int contacts_descr = 0;
static int contacts_wrap = 0;
#define CONTACTS_ORDER_DEFAULT "chopvoluavawxadninnoerr"
static char contacts_order[100] = CONTACTS_ORDER_DEFAULT;
static int contacts_nosort = 0;

list_t sorted_all_cache = NULL;

/*
 * we need this structure because we have to add it to the list 
 * maybe stupid way, but at the moment i couldn't find better (del)
 */
typedef struct {
	char *line;
	const char *name;
        const char *status;
	const char *descr;
} contact_t;

void ncurses_backward_contacts_line(int arg)
{
	window_t *w = window_find("__contacts");

        if (!w)
        	return;

	contacts_index -= arg;

        if (contacts_index < 0)
                contacts_index = 0;

	ncurses_contacts_update(NULL);
	ncurses_redraw(w);
	ncurses_commit();
}

void ncurses_forward_contacts_line(int arg)
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
				int j;

                                if (!s || !s->userlist)
                                        continue;
				
				for (j = 0; j < xstrlen(contacts_order); j += 2) {
					list_t li;

					for (li = s->userlist; li; li = li->next) {
		                        	userlist_t *u = li->data;
                        			char *short_status;

			                        if (!u || !u->status || !u->nickname || !u->status || xstrlen(u->status) < 2)
			                                continue;

			                        if (!contacts_nosort && xstrncmp(u->status, contacts_order + j, 2))
			                                continue;

			                        short_status = xstrndup(u->status, 2);

			                        if (contacts_nosort && !xstrstr(contacts_order, short_status)) {
			                                xfree(short_status);
			                                continue;
			                        }
				
                                		contacts_count++;
						xfree(short_status);
					}

                                        if (contacts_nosort)
                                                break;
				}
                        }
			
			if (window_current->userlist) {
                                int j;

                                for (j = 0; j < xstrlen(contacts_order); j += 2) {
                                        list_t li;

                                        for (li = window_current->userlist; li; li = li->next) {
                                                userlist_t *u = li->data;
                                                char *short_status;

                                                if (!u || !u->status || !u->nickname || !u->status || xstrlen(u->status) < 2)
                                                        continue;

                                                if (!contacts_nosort && xstrncmp(u->status, contacts_order + j, 2))
                                                        continue;

                                                short_status = xstrndup(u->status, 2);

                                                if (contacts_nosort && !xstrstr(contacts_order, short_status)) {
                                                        xfree(short_status);
                                                        continue;
                                                }

                                                contacts_count++;
                                                xfree(short_status);
                                        }

                                        if (contacts_nosort)
                                                break;
                                }

			}
                        break;
                }
                case 2:
		{
                        if (metacontacts) {
                                int j;

                                for (j = 0; j < xstrlen(contacts_order); j += 2) {
                                        list_t li;

                                        for (li = metacontacts; li; li = li->next) {
		                                metacontact_t *m = li->data;
                		                metacontact_item_t *i = metacontact_find_prio(m);
                                		userlist_t *u = (i) ? userlist_find_n(i->s_uid, i->name) : NULL;
                                                char *short_status;

                                                if (!u || !u->status || !u->nickname || !u->status || xstrlen(u->status) < 2)
                                                        continue;

                                                if (!contacts_nosort && xstrncmp(u->status, contacts_order + j, 2))
                                                        continue;

                                                short_status = xstrndup(u->status, 2);

                                                if (contacts_nosort && !xstrstr(contacts_order, short_status)) {
                                                        xfree(short_status);
                                                        continue;
                                                }

                                                contacts_count++;
                                                xfree(short_status);
                                        }

                                        if (contacts_nosort)
                                                break;
                                }

                        }
                        break;
		}
                default:
		{
			list_t current_list = window_current->userlist;

again:
                        if (current_list) {
                                int j;

                                for (j = 0; j < xstrlen(contacts_order); j += 2) {
                                        list_t li;

                                        for (li = current_list; li; li = li->next) {
                                                userlist_t *u = li->data;
                                                char *short_status;

                                                if (!u || !u->status || !u->nickname || !u->status || xstrlen(u->status) < 2)
                                                        continue;

                                                if (!contacts_nosort && xstrncmp(u->status, contacts_order + j, 2))
                                                        continue;

                                                short_status = xstrndup(u->status, 2);

                                                if (contacts_nosort && !xstrstr(contacts_order, short_status)) {
                                                        xfree(short_status);
                                                        continue;
                                                }

                                                contacts_count++;
                                                xfree(short_status);
                                        }

					if (contacts_nosort)
						break;
                                }

                        }
			if (current_list != session_current->userlist) {
				current_list = session_current->userlist;
				goto again;
			}
                        break;
		}
        }

	contacts_index += arg;

        if (contacts_index  > contacts_count - w->height + n->overflow + CONTACTS_MAX_HEADERS)
                contacts_index = contacts_count - window_current->height + n->overflow + CONTACTS_MAX_HEADERS;
        if (contacts_index < 0)
                contacts_index = 0;

        ncurses_contacts_update(NULL);
	ncurses_redraw(w);
	ncurses_commit();
}


void ncurses_backward_contacts_page(int arg)
{
        window_t *w = window_find("__contacts");

        if (!w)
                return;

        ncurses_backward_contacts_line(w->height / 2);
}

void ncurses_forward_contacts_page(int arg)
{
        window_t *w = window_find("__contacts");

        if (!w)
                return;

        ncurses_forward_contacts_line(w->height / 2);
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
	int j, count_all = 0;
	int all = 0; /* 1 - all, 2 - metacontacts */
	ncurses_window_t *n;
	list_t sorted_all = NULL;
	
	if (!w) {
		if (!(w = window_find("__contacts")))
			return -1;
	}

	n = w->private;
	
	ncurses_clear(w, 1);

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

        if (!session_current->userlist && !window_current->userlist && !all && contacts_group_index == 0) {
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

	if (all == 1 && !sorted_all_cache) {
		list_t l;

		for (l = sessions; l; l = l->next) {
			list_t lp;
			session_t *s = l->data;

			if (!s->userlist)
				continue;

			for (lp = s->userlist; lp; lp = lp->next) {
				userlist_t *u;
				userlist_t *up = lp->data;

				if (!up)
					continue;
				u = xmalloc(sizeof(userlist_t));
				u->uid = up->uid;
				u->nickname = up->nickname;
				u->descr = up->descr;
				u->status = up->status;
				u->private = (void *) s;
				u->blink = up->blink;
				list_add_sorted(&sorted_all, u, 0, contacts_compare);
			}
		}
	
		for (l = window_current->userlist; l; l = l->next) {
			userlist_t *up = l->data;
			userlist_t *u;

			if (!up)
				continue;
			u = xmalloc(sizeof(userlist_t));
			u->uid = up->uid;
			u->nickname = up->nickname;
			u->descr = up->descr;
			u->status = up->status;
			u->private = (void *) w->session;
			u->blink = up->blink;
			list_add_sorted(&sorted_all, u, 0, contacts_compare);
		}	
	}
	if ((all == 1 && !sorted_all_cache) || all == 2) {
		list_t l;

 		for (l = metacontacts; l; l = l->next) {
                                metacontact_t *m = l->data;
                                metacontact_item_t *i = metacontact_find_prio(m);
                                userlist_t *uu, *up = (i) ? userlist_find_n(i->s_uid, i->name) : NULL;
				userlist_t *u;
				list_t ml, sl;

				if (!m || !i || !up)
					continue;
				u = xmalloc(sizeof(userlist_t));
				u->status = up->status;
				u->descr = up->descr;
				u->nickname = m->name;
				u->private = (void *) 2;
				u->blink = up->blink;

				list_add_sorted(&sorted_all, u, 0, contacts_compare);

				/* Remove contacts contained in this metacontact. */
				if ( config_contacts_metacontacts_swallow )
					for (ml = m->metacontact_items; ml; ml = ml->next) {
						i = ml->data;
						up = (i) ? userlist_find_n(i->s_uid, i->name) : NULL;
						if (up)
						for ( sl = sorted_all ; sl ; sl = sl->next ) {
							uu = sl->data;
							if ( uu->uid && !xstrcmp(uu->uid, up->uid) )
							list_remove(&sorted_all, uu, 1);
						}
					}
		}
	} 

	if (sorted_all_cache && all != 2) 	
		sorted_all = sorted_all_cache;

	for (j = 0; j < xstrlen(contacts_order); j += 2) {
		int count = 0;
		list_t l = (!all) ? session_current->userlist : sorted_all;
		const char *footer_status = NULL;
		fstring_t *string;
		char *line;
		char tmp[100];

		if (!all && window_current->userlist)
			l = window_current->userlist;

		for (; l; l = l->next) {
			userlist_t *u = l->data;
			const char *format;
			char *short_status;

			if (!u || !u->status || !u->nickname || !u->status || xstrlen(u->status) < 2) 
				continue;
			
			if (!contacts_nosort && xstrncmp(u->status, contacts_order + j, 2))
				continue;

			short_status = xstrndup(u->status, 2);

			if (contacts_nosort && !xstrstr(contacts_order, short_status)) {
				xfree(short_status);
				continue;
			}

			xfree(short_status);
	
			if (count_all < contacts_index) {
				count_all++;
				continue;
			}
	
			if (group && (!u->private || 2!=(int)u->private)) {
				userlist_t *tmp = userlist_find(u->private ? u->private : session_current, u->uid);
				if ((group[0]=='!' && ekg_group_member(tmp, group+1)) ||
						(group[0]!='!' && !ekg_group_member(tmp, group)))
					continue;
			}
			
			if (!count) {
				snprintf(tmp, sizeof(tmp), "contacts_%s_header", u->status);
				format = format_find(tmp);
                                line = format_string(format);
                                string = fstring_new(line);
				if (xstrcmp(format, "") && count_all >= contacts_index) 
					ncurses_backlog_add(w, string);
				else
					fstring_free(string);
                                xfree(line);
				footer_status = u->status;
			}
	
			if (u->descr && contacts_descr)
				snprintf(tmp, sizeof(tmp), "contacts_%s_descr_full", u->status);
			else if (u->descr && !contacts_descr)
				snprintf(tmp, sizeof(tmp), "contacts_%s_descr", u->status);
			else
				snprintf(tmp, sizeof(tmp), "contacts_%s", u->status);
			
			if (u->blink)
				xstrcat(tmp, "_blink");
	
	                line = format_string(format_find(tmp), u->nickname, u->descr);
			string = fstring_new(line);
			if (u->private && (int) u->private == 2)
				string->private = (void *) saprintf("%s", u->nickname);
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

		if (contacts_nosort) {
			break;
		}
	}

	if (xstrcmp(footer, "")) 
		ncurses_backlog_add(w, fstring_new(format_string(footer, group)));

	if (sorted_all && !sorted_all_cache && all != 2) {
		sorted_all_cache = sorted_all;
	}

	xfree(group);

	n->redraw = 1;

	return 0;
}

/*
 * ncurses_contacts_changed()
 *
 * wywo³ywane przy zmianach rozmiaru i w³±czeniu klienta.
 */
QUERY(ncurses_contacts_changed)
{
	const char *name = data;
	window_t *w = NULL;
	if (in_autoexec)
		return 0;

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
	contacts_nosort = 0;

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

                        if (!xstrcasecmp(args[i], "nosort"))
                                contacts_nosort = 1;

			if (!xstrcasecmp(args[i], "nodescr"))
				contacts_descr = 0;

			if (!xstrncasecmp(args[i], "order=", 6))
				snprintf(contacts_order, sizeof(contacts_order), args[i] + 6);
		}

		if (contacts_margin < 0)
			contacts_margin = 0;

		array_free(args);
	}
	if ((w = window_find("__contacts"))) {
		window_kill(w, 1);
		w = NULL;
	}

	if (config_contacts && !w)
		window_new("__contacts", NULL, 1000);
	
	ncurses_contacts_update(NULL);
        ncurses_resize();
	ncurses_commit();
	return 0;
}

/*
 * ncurses_all_contacts_changed()
 *
 * wywo³ywane przy zmianach userlisty powoduj±cych konieczno¶æ
 * podkasowania sorted_all_cache (zmiany w metakontaktach 
 * i ncurses:contacts_metacontacts_swallow)
 */
QUERY(ncurses_all_contacts_changed)
{
	list_destroy(sorted_all_cache, 1);
	sorted_all_cache = NULL;
	ncurses_contacts_changed(data, NULL);
	return 0;
}

/* 
 * ncurses_contacts_mouse_handler()
 * 
 * handler for mouse events
 */
void ncurses_contacts_mouse_handler(int x, int y, int mouse_state) 
{
        window_t *w = window_find("__contacts");
        ncurses_window_t *n;
	CHAR_T *name;

	if (mouse_state == EKG_SCROLLED_UP) {
		ncurses_backward_contacts_line(5);
		return;
	} else if (mouse_state == EKG_SCROLLED_DOWN) {
		ncurses_forward_contacts_line(5);
		return;
	}

	if (!w || mouse_state != EKG_BUTTON1_DOUBLE_CLICKED)
		return;

	n = w->private;

	if (y > n->backlog_size)
		return;

	name = n->backlog[n->backlog_size - y]->str;

	command_exec_format(NULL, NULL, 0, TEXT("/query \"%s\""), n->backlog[n->backlog_size - y ]->private);
	return;
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
	n->handle_mouse = ncurses_contacts_mouse_handler;
	w->nowrap = !contacts_wrap;
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
