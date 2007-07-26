/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 *  port to ekg2:
 *  Copyright (C) 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkclist.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkvscrollbar.h>
#include <gdk/gdkkeysyms.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include <ekg/stuff.h>
#include <ekg/windows.h>

#include "main.h"
#include "bindings.h"
#include "completion.h"

char *gtk_history[HISTORY_MAX];
int gtk_history_index;

#define GTK_BINDING_FUNCTION(x) int x(GtkWidget *wid, GdkEventKey *evt, char *d1, window_t *sess)

/* These are cp'ed from history.c --AGL */
#define STATE_SHIFT     GDK_SHIFT_MASK
#define	STATE_ALT	GDK_MOD1_MASK
#define STATE_CTRL	GDK_CONTROL_MASK

static GTK_BINDING_FUNCTION(key_action_scroll_page) {
	int value, end;
	GtkAdjustment *adj;
	enum scroll_type { PAGE_UP, PAGE_DOWN, LINE_UP, LINE_DOWN };
	int type = PAGE_DOWN;

	if (d1) {
		if (!xstrcasecmp(d1, "up"))
			type = PAGE_UP;
		else if (!xstrcasecmp(d1, "+1"))
			type = LINE_DOWN;
		else if (!xstrcasecmp(d1, "-1"))
			type = LINE_UP;
	}

	if (!sess)
		return 0;

	adj = GTK_RANGE(gtk_private_ui(sess)->vscrollbar)->adjustment;
	end = adj->upper - adj->lower - adj->page_size;

	switch (type) {
	case LINE_UP:
		value = adj->value - 1.0;
		break;

	case LINE_DOWN:
		value = adj->value + 1.0;
		break;

	case PAGE_UP:
		value = adj->value - (adj->page_size - 1);
		break;

	default:		/* PAGE_DOWN */
		value = adj->value + (adj->page_size - 1);
		break;
	}

	if (value < 0)
		value = 0;
	if (value > end)
		value = end;

	gtk_adjustment_set_value(adj, value);

	return 0;
}

static GTK_BINDING_FUNCTION(key_action_history_up) {
	if (gtk_history_index < HISTORY_MAX && gtk_history[gtk_history_index + 1]) {
		/* for each line? */
		if (gtk_history_index == 0) {
			xfree(gtk_history[0]);
			gtk_history[0] = xstrdup((GTK_ENTRY(wid)->text));
		}

		gtk_history_index++;

		gtk_entry_set_text(GTK_ENTRY(wid), gtk_history[gtk_history_index]);
		gtk_editable_set_position(GTK_EDITABLE(wid), -1);
	}
	return 2;
}

static GTK_BINDING_FUNCTION(key_action_history_down) {
	if (gtk_history_index > 0) {
		gtk_history_index--;

		gtk_entry_set_text(GTK_ENTRY(wid), gtk_history[gtk_history_index]);
		gtk_editable_set_position(GTK_EDITABLE(wid), -1);
	}
	return 2;
}

static GTK_BINDING_FUNCTION(key_action_tab_comp) {
	char buf[COMPLETION_MAXLEN];

	const char *text;
	int cursor_pos;
	int line_start;

/* in fjuczer, use g_completion_new() ? */

	text = ((GTK_ENTRY(wid)->text));
	if (text[0] == '\0')
		return 1;

	cursor_pos = gtk_editable_get_position(GTK_EDITABLE(wid));

	if (strlcpy(buf, text, sizeof(buf)) >= sizeof(buf))
		printf("key_action_tab_comp(), strlcpy() UUUUUUUCH!\n");

/* XXX, i don't remember line_start */
	line_start = 0;		/* XXX */

	ncurses_complete(&line_start, &cursor_pos, buf);

	gtk_entry_set_text(GTK_ENTRY(wid), buf);
	gtk_editable_set_position(GTK_EDITABLE(wid), cursor_pos);

	return 2;
}

static GTK_BINDING_FUNCTION(key_action_cycle_session) {
	if (window_session_cycle(sess) == 0) {
#warning "ekg2 ncurses->gtk XXX"
/*
		ncurses_contacts_update(NULL);
		update_statusbar(1);
 */
	}

	return 2;
}

gboolean key_handle_key_press(GtkWidget *wid, GdkEventKey * evt, window_t *sess) {
	int keyval = evt->keyval;
	int mod, n;
	int was_complete = 0;
	list_t l;

	{
		sess = NULL;

		/* where did this event come from? */
		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (gtk_private_ui(w)->input_box == wid) {
				sess = w;
				if (gtk_private_ui(w)->is_tab)
					sess = window_current;
				break;
			}
		}
	}

	if (!sess) {
		printf("key_handle_key_press() FAILED (sess == NULL)\n");
		return FALSE;
	}

/*	printf("key_handle_key_press() %p [%d %d %d %s]\n", sess, evt->state, evt->keyval, evt->length, evt->string); */
	/* XXX, EMIT: KEY_PRESSED */

	mod = evt->state & (STATE_CTRL | STATE_ALT | STATE_SHIFT);

	n = -1;

/* yeah, i know it's awful. */
	if (keyval == GDK_Page_Up)		 	n = key_action_scroll_page(wid, evt, "up", sess);
	else if (keyval == GDK_Page_Down)		n = key_action_scroll_page(wid, evt, "down", sess);

	else if (keyval == GDK_Up)			n = key_action_history_up(wid, evt, NULL, sess);
	else if (keyval == GDK_Down)			n = key_action_history_down(wid, evt, NULL, sess);
	else if (keyval == GDK_Tab) {			n = key_action_tab_comp(wid, evt, NULL, sess); was_complete = 1; }

	else if (keyval == GDK_F12)			command_exec(sess->target, sess->session, "/window switch 0", 0);
	else if (keyval == GDK_F1)			command_exec(sess->target, sess->session, "/help", 0);

	else if (keyval == GDK_0 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 10", 0);
	else if (keyval == GDK_9 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 9", 0);
	else if (keyval == GDK_8 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 8", 0);
	else if (keyval == GDK_7 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 7", 0);
	else if (keyval == GDK_6 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 6", 0);
	else if (keyval == GDK_5 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 5", 0);
	else if (keyval == GDK_4 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 4", 0);
	else if (keyval == GDK_3 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 3", 0);
	else if (keyval == GDK_2 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 2", 0);
	else if (keyval == GDK_1 && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 1", 0);
	else if (keyval == '`' && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 0", 0);

	else if (((keyval == GDK_Q) || (keyval == GDK_q)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 11", 0);
	else if (((keyval == GDK_W) || (keyval == GDK_w)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 12", 0);
	else if (((keyval == GDK_E) || (keyval == GDK_e)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 13", 0);
	else if (((keyval == GDK_R) || (keyval == GDK_r)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 14", 0);
	else if (((keyval == GDK_T) || (keyval == GDK_t)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 15", 0);
	else if (((keyval == GDK_Y) || (keyval == GDK_y)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 16", 0);
	else if (((keyval == GDK_U) || (keyval == GDK_u)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 17", 0);
	else if (((keyval == GDK_I) || (keyval == GDK_i)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 18", 0);
	else if (((keyval == GDK_O) || (keyval == GDK_o)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 19", 0);
	else if (((keyval == GDK_P) || (keyval == GDK_p)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window switch 20", 0);

	else if (((keyval == GDK_N) || (keyval == GDK_n)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window new", 0);
	else if (((keyval == GDK_K) || (keyval == GDK_k)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window kill", 0);
	else if (((keyval == GDK_A) || (keyval == GDK_a)) && mod == STATE_ALT)	command_exec(sess->target, sess->session, "/window active", 0);

	else if (((keyval == GDK_N) || (keyval == GDK_n)) && mod == STATE_CTRL)	command_exec(sess->target, sess->session, "/window next", 0);
	else if (((keyval == GDK_P) || (keyval == GDK_p)) && mod == STATE_CTRL)	command_exec(sess->target, sess->session, "/window prev", 0);

	else if (((keyval == GDK_F) || (keyval == GDK_f)) && mod == STATE_CTRL)	n = key_action_scroll_page(wid, evt, "up", sess);
	else if (((keyval == GDK_G) || (keyval == GDK_g)) && mod == STATE_CTRL)	n = key_action_scroll_page(wid, evt, "down", sess);
	else if (((keyval == GDK_X) || (keyval == GDK_x)) && mod == STATE_CTRL) n = key_action_cycle_session(wid, evt, NULL, sess);

#if 0
	ncurses_binding_add("Alt-G", "ignore-query", 1, 1);
	ncurses_binding_add("Alt-B", "backward-word", 1, 1);
	ncurses_binding_add("Alt-F", "forward-word", 1, 1);
	ncurses_binding_add("Alt-D", "kill-word", 1, 1);
	ncurses_binding_add("Alt-Enter", "toggle-input", 1, 1);

	ncurses_binding_add("Escape", "cancel-input", 1, 1);
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
	ncurses_binding_add("Right", "forward-char", 1, 1);
	ncurses_binding_add("Left", "backward-char", 1, 1);
	ncurses_binding_add("F2", "quick-list", 1, 1);
	ncurses_binding_add("F3", "toggle-contacts", 1, 1);
	ncurses_binding_add("F4", "next-contacts-group", 1, 1);
	ncurses_binding_add("F11", "ui-ncurses-debug-toggle", 1, 1);
#endif

#if 0
	for (l = bindings; l; l = l->next) {
		if (kb->keyval == keyval && kb->mod == mod) {

			/* Run the function */
			n = key_actions[kb->action].handler(wid, evt, kb->data1, kb->data2, sess);
			switch (n) {
				case 0:
					return 1;
				case 2:
					g_signal_stop_emission_by_name(G_OBJECT(wid), "key_press_event");
					return 1;
			}
		}
	}
#endif
	if (!was_complete) {
		/* jeśli się coś zmieniło, wygeneruj dopełnienia na nowo */
		ncurses_complete_clear();

		/* w xchacie bylo tylko na GDK_space */
	}

	if (n == 2) {
		g_signal_stop_emission_by_name(G_OBJECT(wid), "key_press_event");
		return 1;
	}

	return (n == 0);
}

void gtk_binding_init() {

}

static void gtk_binding_destroy() {

}
