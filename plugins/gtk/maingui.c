/* X-Chat
 * Copyright (C) 1998-2005 Peter Zelezny.
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

#define GTK_DISABLE_DEPRECATED

#include <ekg2-config.h>
#define USE_XLIB

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <gtk/gtkarrow.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkbbox.h>
#include <gtk/gtkvscrollbar.h>


#include <ekg/plugins.h>
#include <ekg/windows.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

#include "main.h"
#include "xtext.h"
#include "gtkutil.h"
#include "palette.h"
#include "menu.h"
#include "chanview.h"
#include "bindings.h"
#include "userlistgui.h"

#include "maingui.h"
#include "userlistgui.h"

#if 0

#include "../common/xchat.h"
#include "../common/fe.h"
#include "../common/server.h"
#include "../common/xchatc.h"
#include "../common/outbound.h"
#include "../common/inbound.h"
#include "../common/plugin.h"
#include "../common/modes.h"
#include "../common/url.h"
#include "fe-gtk.h"
#include "banlist.h"
#include "joind.h"
#include "maingui.h"
#include "pixmaps.h"
#include "plugin-tray.h"

#endif

#define GUI_SPACING (3)
#define GUI_BORDER (0)
#define SCROLLBAR_SPACING (2)

enum {
	POS_INVALID = 0,
	POS_TOPLEFT = 1,
	POS_BOTTOMLEFT = 2,
	POS_TOPRIGHT = 3,
	POS_BOTTOMRIGHT = 4,
	POS_TOP = 5,		/* for tabs only */
	POS_BOTTOM = 6,
	POS_HIDDEN = 7
};

/* two different types of tabs */
#define TAG_IRC 0		/* server, channel, dialog */
#define TAG_UTIL 1		/* dcc, notify, chanlist */

static void mg_link_irctab(window_t *sess, int focus);
static void mg_create_entry(window_t *sess, GtkWidget *box);

static gtk_window_ui_t static_mg_gui;
static gtk_window_ui_t *mg_gui = NULL;	/* the shared irc tab */

GtkWidget *parent_window = NULL;	/* the master window */
GtkStyle *input_style;

static chan *active_tab = NULL;	/* active tab */

static PangoAttrList *away_list;
static PangoAttrList *newdata_list;
static PangoAttrList *nickseen_list;
static PangoAttrList *newmsg_list;
static PangoAttrList *plain_list = NULL;

#define NO_SESSION "no session"

const char *gtk_session_target(session_t *sess) {
	if (!sess)			return NO_SESSION;
	if (sess->alias)		return sess->alias;

	return sess->uid;
}

const char *gtk_window_target(window_t *window) {
	if (!window)			return "";

	if (window->target)		return window->target;
	else if (window->id == 1)	return "__status";
	else if (window->id == 0)	return "__debug";
        else                            return "";
}


static PangoAttrList *mg_attr_list_create(GdkColor *col, int size) {
	PangoAttribute *attr;
	PangoAttrList *list;

	list = pango_attr_list_new();

	if (col) {
		attr = pango_attr_foreground_new(col->red, col->green, col->blue);
		attr->start_index = 0;
		attr->end_index = 0xffff;
		pango_attr_list_insert(list, attr);
	}

	if (size > 0) {
		attr = pango_attr_scale_new(size == 1 ? PANGO_SCALE_SMALL : PANGO_SCALE_X_SMALL);
		attr->start_index = 0;
		attr->end_index = 0xffff;
		pango_attr_list_insert(list, attr);
	}

	return list;
}

static void mg_create_tab_colors(void) {
	if (plain_list) {
		pango_attr_list_unref(plain_list);
		pango_attr_list_unref(newmsg_list);
		pango_attr_list_unref(newdata_list);
		pango_attr_list_unref(nickseen_list);
		pango_attr_list_unref(away_list);
	}

	plain_list = mg_attr_list_create(NULL, tab_small_config);
	newdata_list = mg_attr_list_create(&colors[COL_NEW_DATA], tab_small_config);
	nickseen_list = mg_attr_list_create(&colors[COL_HILIGHT], tab_small_config);
	newmsg_list = mg_attr_list_create(&colors[COL_NEW_MSG], tab_small_config);
	away_list = mg_attr_list_create(&colors[COL_AWAY], FALSE);
}

#ifdef USE_XLIB
#include <gdk/gdkx.h>

static void set_window_urgency(GtkWidget *win, gboolean set) {
	XWMHints *hints;

	hints = XGetWMHints(GDK_WINDOW_XDISPLAY(win->window), GDK_WINDOW_XWINDOW(win->window));
	if (set)
		hints->flags |= XUrgencyHint;
	else
		hints->flags &= ~XUrgencyHint;
	XSetWMHints(GDK_WINDOW_XDISPLAY(win->window), GDK_WINDOW_XWINDOW(win->window), hints);
	XFree(hints);
}

static void flash_window(GtkWidget *win) {
	set_window_urgency(win, TRUE);
}

static void unflash_window(GtkWidget *win) {
	set_window_urgency(win, FALSE);
}

#endif

int fe_gui_info(window_t *sess, int info_type) {	/* code from fe-gtk.c */
	switch (info_type) {
		case 0:	/* window status */
			if (!GTK_WIDGET_VISIBLE(GTK_WINDOW(gtk_private_ui(sess)->window)))
				return 2;	/* hidden (iconified or systray) */

#warning "GTK issue."
	/* 2.4.0 -> gtk_window_is_active(GTK_WINDOW(gtk_private_ui(sess)->window))
	 * 2.2.0 -> GTK_WINDOW(gtk_private_ui(sess)->window)->is_active)
	 *
	 * 		return 1
	 */
		return 0;		/* normal (no keyboard focus or behind a window) */
	}

	return -1;
}

/* flash the taskbar button */

void fe_flash_window(window_t *sess) {
#if defined(WIN32) || defined(USE_XLIB)
	if (fe_gui_info(sess, 0) != 1)	/* only do it if not focused */
		flash_window(gtk_private_ui(sess)->window);
#endif
}

/* set a tab plain, red, light-red, or blue */

void fe_set_tab_color(window_t *sess, int col) {
	if (!gtk_private_ui(sess)->is_tab)
		return;

	if (sess == window_current || sess->id == 0)
		col = 0;	/* XXX */
	
//    col value, what todo                                            values                                                  comment.
//      0: chan_set_color(sess->tab, plain_list);           [new_data = NULL, msg_said = NULL, nick_said = NULL]    /* no particular color (theme default) */
//      1: chan_set_color(sess->tab, newdata_list);         [new_data = TRUE, msg_said = NULL, nick_said = NULL]    /* new data has been displayed (dark red) */
//      2: chan_set_color(sess->tab, newmsg_list);          [new_data = NULL, msg_said = TRUE, nick_said = NULL]    /* new message arrived in channel (light red) */
//      3: chan_set_color(sess->tab, nickseen_list) ;       [new_data = NULL, msg_said = NULL, nick_said = TRUE]    /* your nick has been seen (blue) */    

	if (col == 0) chan_set_color(gtk_private(sess)->tab, plain_list);
	if (col == 1) chan_set_color(gtk_private(sess)->tab, newdata_list);
	if (col == 2) chan_set_color(gtk_private(sess)->tab, newmsg_list);
}

#if 0

static void mg_set_myself_away(gtk_window_ui_t *gui, gboolean away) {
	gtk_label_set_attributes(GTK_LABEL(GTK_BIN(gui->nick_label)->child),
				 away ? away_list : NULL);
}

/* change the little icon to the left of your nickname */

void mg_set_access_icon(gtk_window_ui_t *gui, GdkPixbuf *pix, gboolean away) {
	if (gui->op_xpm) {
		gtk_widget_destroy(gui->op_xpm);
		gui->op_xpm = 0;
	}

	if (pix) {
		gui->op_xpm = gtk_image_new_from_pixbuf(pix);
		gtk_box_pack_start(GTK_BOX(gui->nick_box), gui->op_xpm, 0, 0, 0);
		gtk_widget_show(gui->op_xpm);
	}

	mg_set_myself_away(gui, away);
}

#endif

static gboolean mg_inputbox_focus(GtkWidget *widget, GdkEventFocus *event, gtk_window_ui_t *gui) {
	window_t *w;

	if (gui->is_tab)
		return FALSE;

	for (w = windows; w; w = w->next) {
		if (gtk_private(w)->gui == gui) {
#warning "window_switch() XXX"
			window_switch(w->id);
			return FALSE;
		}

	}

	printf("mg_inputbox_focus() internal error!\n");

	return FALSE;
}

void mg_inputbox_cb(GtkWidget *igad, gtk_window_ui_t *gui) {
	static int ignore = FALSE;
	window_t *sess = NULL;
	char *cmd;

	if (ignore)
		return;

	cmd = GTK_ENTRY(igad)->text;
	if (cmd[0] == '\0')
		return;

	cmd = xstrdup(cmd);

	/* avoid recursive loop */
	ignore = TRUE;
	gtk_entry_set_text(GTK_ENTRY(igad), "");
	ignore = FALSE;

	/* where did this event come from? */
	if (gui->is_tab) {
		sess = window_current;
	} else {
		window_t *w;

		for (w = windows; w; w = w->next) {
			if (gtk_private_ui(w) == gui) {
				sess = w;
				break;
			}

		}
		if (!sess)
			printf("FATAL, not found.\n");
	}

	if (sess) {
		command_exec(sess->target, sess->session, cmd, 0);

		if (config_history_savedups || xstrcmp(cmd, gtk_history[1])) {
			gtk_history[0] = cmd;
			xfree(gtk_history[HISTORY_MAX - 1]);

			memmove(&gtk_history[1], &gtk_history[0], sizeof(gtk_history) - sizeof(gtk_history[0]));

			gtk_history_index = 0;
			gtk_history[0] = NULL;
		} else
			xfree(cmd);

		return;
	}

	xfree(cmd);
}

void fe_set_title(window_t *sess) {
	gtk_window_ui_t *n = gtk_private_ui(sess);

	if (n->is_tab && sess != window_current)
		return;

	gtk_window_set_title(GTK_WINDOW(n->window), "ekg2");
}


static gboolean mg_windowstate_cb(GtkWindow * wid, GdkEventWindowState * event, gpointer userdata) {
#if 0
	prefs.gui_win_state = 0;
	if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
		prefs.gui_win_state = 1;

	if ((event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) &&
	    (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) && (prefs.gui_tray_flags & 4)) {
		tray_toggle_visibility(TRUE);
		gtk_window_deiconify(wid);
	}
#endif
	return FALSE;
}

static gboolean mg_configure_cb(GtkWidget *wid, GdkEventConfigure * event, window_t *sess) {
#if 0
	if (sess == NULL) {	/* for the main_window */
		if (mg_gui) {
			if (prefs.mainwindow_save) {
				sess = current_sess;
				gtk_window_get_position(GTK_WINDOW(wid), &prefs.mainwindow_left,
							&prefs.mainwindow_top);
				gtk_window_get_size(GTK_WINDOW(wid), &prefs.mainwindow_width,
						    &prefs.mainwindow_height);
			}
		}
	}

	if (sess) {
		if (sess->type == SESS_DIALOG && prefs.mainwindow_save) {
			gtk_window_get_position(GTK_WINDOW(wid), &prefs.dialog_left,
						&prefs.dialog_top);
			gtk_window_get_size(GTK_WINDOW(wid), &prefs.dialog_width,
					    &prefs.dialog_height);
		}

		if (((GtkXText *) sess->gui->xtext)->transparent)
			gtk_widget_queue_draw(sess->gui->xtext);
	}
#endif
	return FALSE;
}

#if 0

/* move to a non-irc tab */

static void mg_show_generic_tab(GtkWidget *box) {
	int num;
	GtkWidget *f = NULL;

	if (current_sess && GTK_WIDGET_HAS_FOCUS(current_sess->gui->input_box))
		f = current_sess->gui->input_box;

	num = gtk_notebook_page_num(GTK_NOTEBOOK(mg_gui->note_book), box);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(mg_gui->note_book), num);
	gtk_tree_view_set_model(GTK_TREE_VIEW(mg_gui->user_tree), NULL);
	gtk_window_set_title(GTK_WINDOW(mg_gui->window),
			     g_object_get_data(G_OBJECT(box), "title"));
	gtk_widget_set_sensitive(mg_gui->menu, FALSE);

	if (f)
		gtk_widget_grab_focus(f);
}

#endif

/* a channel has been focused */

static void mg_focus(window_t *sess) {
#if 0
	if (sess->gui->is_tab)
		current_tab = sess;
	current_sess = sess;

	/* dirty trick to avoid auto-selection */
	SPELL_ENTRY_SET_EDITABLE(sess->gui->input_box, FALSE);
	gtk_widget_grab_focus(sess->gui->input_box);
	SPELL_ENTRY_SET_EDITABLE(sess->gui->input_box, TRUE);

	sess->server->front_session = sess;

	if (sess->server->server_session != NULL) {
		if (sess->server->server_session->type != SESS_SERVER)
			sess->server->server_session = sess;
	} else {
		sess->server->server_session = sess;
	}

	if (sess->new_data || sess->nick_said || sess->msg_said) {
		sess->nick_said = FALSE;
		sess->msg_said = FALSE;
		sess->new_data = FALSE;
		/* when called via mg_changui_new, is_tab might be true, but
		   sess->res->tab is still NULL. */
		if (sess->res->tab)
			fe_set_tab_color(sess, 0);
	}
#endif
}

#if 0

void mg_set_topic_tip(session *sess) {
	char *text;

	switch (sess->type) {
	case SESS_CHANNEL:
		if (sess->topic) {
			text = g_strdup_printf(_("Topic for %s is: %s"), sess->channel,
					       sess->topic);
			add_tip(sess->gui->topic_entry, text);
			g_free(text);
		} else
			add_tip(sess->gui->topic_entry, _("No topic is set"));
		break;
	default:
		if (GTK_ENTRY(sess->gui->topic_entry)->text &&
		    GTK_ENTRY(sess->gui->topic_entry)->text[0])
			add_tip(sess->gui->topic_entry, GTK_ENTRY(sess->gui->topic_entry)->text);
		else
			add_tip(sess->gui->topic_entry, NULL);
	}
}

#endif

static void mg_hide_empty_pane(GtkPaned * pane) {
	if ((pane->child1 == NULL || !GTK_WIDGET_VISIBLE(pane->child1)) &&
	    (pane->child2 == NULL || !GTK_WIDGET_VISIBLE(pane->child2))) {
		gtk_widget_hide(GTK_WIDGET(pane));
		return;
	}

	gtk_widget_show(GTK_WIDGET(pane));
}

static void mg_hide_empty_boxes(gtk_window_ui_t *gui) {
	/* hide empty vpanes - so the handle is not shown */
	mg_hide_empty_pane((GtkPaned *) gui->vpane_right);
	mg_hide_empty_pane((GtkPaned *) gui->vpane_left);
}

static void mg_userlist_showhide(window_t *sess, int show) {
	gtk_window_ui_t *gui = gtk_private_ui(sess);
	int handle_size;

	if (show) {
		gtk_widget_show(gui->user_box);
		gui->ul_hidden = 0;

		gtk_widget_style_get(GTK_WIDGET(gui->hpane_right), "handle-size", &handle_size,
				     NULL);
		gtk_paned_set_position(GTK_PANED(gui->hpane_right),
				       GTK_WIDGET(gui->hpane_right)->allocation.width -
				       (gui_pane_right_size_config + handle_size));
	} else {
		gtk_widget_hide(gui->user_box);
		gui->ul_hidden = 1;
	}

	mg_hide_empty_boxes(gui);
}

/* decide if the userlist should be shown or hidden for this tab */

void mg_decide_userlist(window_t *sess, gboolean switch_to_current) {
	/* when called from menu.c we need this */
	if (gtk_private_ui(sess) == mg_gui && switch_to_current)
		sess = window_current;

	if (!contacts_config) {
		mg_userlist_showhide(sess, FALSE);
		return;
	}

	/* xchat->ekg2 XXX, here: mg_is_userlist_and_tree_combined() stuff */

	mg_userlist_showhide(sess, TRUE);	/* show */
}


static void mg_userlist_toggle_cb(GtkWidget *button, gpointer userdata) {
	contacts_config = !contacts_config;

	mg_decide_userlist(window_current, FALSE);

	gtk_widget_grab_focus(gtk_private_ui(window_current)->input_box);
}

static idle_t *ul_tag = NULL;

/* static */ gboolean mg_populate_userlist(window_t *sess) {
	gtk_window_ui_t *gui;
	GdkPixbuf **pxs;

	if (!sess)
		sess = window_current;

#warning "mg_populate_userlist() hack, slowdown"
	fe_userlist_clear(sess);

#warning "xchat->ekg2, mg_populate_userlist() xchat here check if param is valid window_t, XXX"

	if (sess->userlist) {
		userlist_t *ul;
		
		/* XXX, irc_pixs! */
		pxs = pixs;

		for (ul = sess->userlist; ul; ul = ul->next) {
			userlist_t *u = ul;

			if (!u || !u->nickname || !u->status)
				continue;

			fe_userlist_insert(sess, u, pxs);
		}
	} else if (sess->session) {
		userlist_t *ul;
		
	/* check what network, and select pixs */
		if (sess->session->plugin == plugin_find("gg"))	pxs = gg_pixs;
		else						pxs = pixs;

		for (ul = sess->session->userlist; ul; ul = ul->next) {
			userlist_t *u = ul;

			if (!u || !u->nickname || !u->status)
				continue;

			fe_userlist_insert(sess, u, pxs);
		}
	}


//	if (is_session(sess)) 	-> if (window_find_ptr(sess)
	if (1)
	{
		gui = gtk_private_ui(sess);
#if 0
		if (sess->type == SESS_DIALOG)
			mg_set_access_icon(sess->gui, NULL, sess->server->is_away);
		else
			mg_set_access_icon(sess->gui, get_user_icon(sess->server, sess->me),
					   sess->server->is_away);
#endif
		userlist_show(sess);
		userlist_set_value(gtk_private_ui(sess)->user_tree, gtk_private(sess)->old_ul_value);
	}
	return 0;
}

static IDLER(mg_populate_userlist_idle) {
	mg_populate_userlist((window_t *) data);
	ul_tag = NULL;
	return -1;
}

/* fill the irc tab with a new channel */

/* static */ void mg_populate(window_t *sess) {
	gtk_window_t *res = gtk_private(sess);
	gtk_window_ui_t *gui = res->gui;

	int render = TRUE;
	guint16 vis = gui->ul_hidden;

#if 0
	switch (sess->type) {
	case SESS_DIALOG:
		/* show the dialog buttons */
		gtk_widget_show(gui->dialogbutton_box);
		/* hide the chan-mode buttons */
		gtk_widget_hide(gui->topicbutton_box);
		/* hide the userlist */
		mg_decide_userlist(sess, FALSE);
		/* shouldn't edit the topic */
		gtk_editable_set_editable(GTK_EDITABLE(gui->topic_entry), FALSE);
		break;
	case SESS_SERVER:
		if (prefs.chanmodebuttons)
			gtk_widget_show(gui->topicbutton_box);
		/* hide the dialog buttons */
		gtk_widget_hide(gui->dialogbutton_box);
		/* hide the userlist */
		mg_decide_userlist(sess, FALSE);
		/* shouldn't edit the topic */
		gtk_editable_set_editable(GTK_EDITABLE(gui->topic_entry), FALSE);
		break;
	default:
		/* hide the dialog buttons */
		gtk_widget_hide(gui->dialogbutton_box);
		if (prefs.chanmodebuttons)
			gtk_widget_show(gui->topicbutton_box);
		/* show the userlist */
		mg_decide_userlist(sess, FALSE);
		/* let the topic be editted */
		gtk_editable_set_editable(GTK_EDITABLE(gui->topic_entry), TRUE);
	}
#else
		mg_decide_userlist(sess, FALSE);
#endif
	/* move to THE irc tab */
	if (gui->is_tab)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gui->note_book), 0);

	/* xtext size change? Then don't render, wait for the expose caused
	   by showing/hidding the userlist */
	if (vis != gui->ul_hidden && gui->user_box->allocation.width > 1)
		render = FALSE;

	gtk_xtext_buffer_show(GTK_XTEXT(gui->xtext), res->buffer, render);
	if (gui->is_tab)
		gtk_widget_set_sensitive(gui->menu, TRUE);

	mg_focus(sess);
	fe_set_title(sess);

	/* this one flickers, so only change if necessary */
	if (strcmp(gtk_session_target(sess->session), gtk_button_get_label(GTK_BUTTON(gui->nick_label))))
		gtk_button_set_label(GTK_BUTTON(gui->nick_label), gtk_session_target(sess->session));

	/* this is slow, so make it a timeout event */
	if (!gui->is_tab) {
		mg_populate_userlist(sess);
	} else {
		if (ul_tag == NULL)
			ul_tag = idle_add(&gtk_plugin, mg_populate_userlist_idle, NULL);
	}
	fe_userlist_numbers(sess);

#if 0
	/* menu items */
	GTK_CHECK_MENU_ITEM(gui->menu_item[MENU_ID_AWAY])->active = sess->server->is_away;
	gtk_widget_set_sensitive(gui->menu_item[MENU_ID_AWAY], sess->server->connected);
	gtk_widget_set_sensitive(gui->menu_item[MENU_ID_JOIN], sess->server->end_of_motd);
	gtk_widget_set_sensitive(gui->menu_item[MENU_ID_DISCONNECT],
				 sess->server->connected || sess->server->recondelay_tag);

	mg_set_topic_tip(sess);

	plugin_emit_dummy_print(sess, "Focus Tab");
#endif
}

#if 0

void mg_bring_tofront_sess(session *sess) {				/* IRC tab or window */
	if (sess->gui->is_tab)
		chan_focus(sess->res->tab);
	else
		gtk_window_present(GTK_WINDOW(sess->gui->window));
}

void mg_bring_tofront(GtkWidget *vbox) {				/* non-IRC tab or window */
	chan *ch;

	ch = g_object_get_data(G_OBJECT(vbox), "ch");
	if (ch)
		chan_focus(ch);
	else
		gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(vbox)));
}

#endif

void mg_switch_page(int relative, int num) {
	if (mg_gui)
		chanview_move_focus(mg_gui->chanview, relative, num);
}

/* a toplevel IRC window was destroyed */

static void mg_topdestroy_cb(GtkWidget *win, window_t *sess) {
#warning "xchat->ekg2: mg_topdestroy_cb() BIG XXX"
	printf("mg_topdestroy_cb() XXX\n");
#if 0
/*	printf("enter mg_topdestroy. sess %p was destroyed\n", sess);*/

	/* kill the text buffer */
	gtk_xtext_buffer_free(gtk_private(sess)->buffer);
#if 0
	/* kill the user list */
	g_object_unref(G_OBJECT(sess->res->user_model));
#endif
	window_kill(sess);	/* XXX, session_free(sess) */
#endif
}

#if 0

/* cleanup an IRC tab */

static void mg_ircdestroy(session *sess) {
	GSList *list;

	/* kill the text buffer */
	gtk_xtext_buffer_free(sess->res->buffer);
	/* kill the user list */
	g_object_unref(G_OBJECT(sess->res->user_model));

	session_free(sess);	/* tell xchat.c about it */

	if (mg_gui == NULL) {
/*		puts("-> mg_gui is already NULL");*/
		return;
	}

	list = sess_list;
	while (list) {
		sess = list->data;
		if (sess->gui->is_tab) {
/*			puts("-> some tabs still remain");*/
			return;
		}
		list = list->next;
	}

/*	puts("-> no tabs left, killing main tabwindow");*/
	gtk_widget_destroy(mg_gui->window);
	active_tab = NULL;
	mg_gui = NULL;
	parent_window = NULL;
}

#endif

void mg_tab_close(window_t *sess) {
#warning "xchat->ekg2: mg_tab_close() XXX"
	if (chan_remove(gtk_private(sess)->tab, FALSE))
#if 0
		mg_ircdestroy(sess);
#else
		;
#endif
}

#if 0

static void mg_traymsg_cb(GtkCheckMenuItem * item, session *sess) {
	sess->tray = FALSE;
	if (item->active)
		sess->tray = TRUE;
}

static void mg_beepmsg_cb(GtkCheckMenuItem * item, session *sess) {
	sess->beep = FALSE;
	if (item->active)
		sess->beep = TRUE;
}

static void mg_hidejp_cb(GtkCheckMenuItem * item, session *sess) {
	sess->hide_join_part = TRUE;
	if (item->active)
		sess->hide_join_part = FALSE;
}
#endif

static void mg_menu_destroy(GtkWidget *menu, gpointer userdata) {
	gtk_widget_destroy(menu);
	g_object_unref(menu);
}

void mg_create_icon_item(char *label, char *stock, GtkWidget *menu, void *callback, void *userdata) {
	GtkWidget *item;

	item = create_icon_menu(label, stock, TRUE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show(item);
}

void mg_open_quit_dialog(gboolean minimize_button) {
	static GtkWidget *dialog = NULL;
	GtkWidget *dialog_vbox1;
	GtkWidget *table1;
	GtkWidget *image;
	GtkWidget *checkbutton1;
	GtkWidget *label;
	GtkWidget *dialog_action_area1;
	GtkWidget *button;
	char *text;

	if (dialog) {
		gtk_window_present(GTK_WINDOW(dialog));
		return;
	}

	if (!gui_quit_dialog_config) {
		ekg_exit();
		return;
	}

	if (config_save_quit == 1) {
#warning "Display question if user want to /save config"
/*
		if (config_changed) 				format_find("config_changed")
		else if (config_keep_reason && reason_changed)	format_find("quit_keep_reason");
*/
		config_save_quit = 0;
	}

#warning "xchat->ekg2 XXX"
	/* 	xchat count dcc's + connected network, and display warning about it.
	 *
	 * 		"<span weight=\"bold\" size=\"larger\">Are you sure you want to quit?</span>\n
	 * 			"You are connected to %i IRC networks."
	 * 			"Some file transfers are still active."
	 */


	dialog = gtk_dialog_new();
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Quit ekg2?"));
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent_window));
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

	dialog_vbox1 = GTK_DIALOG(dialog)->vbox;
	gtk_widget_show(dialog_vbox1);

	table1 = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table1);
	gtk_box_pack_start(GTK_BOX(dialog_vbox1), table1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 12);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 12);

	image = gtk_image_new_from_stock("gtk-dialog-warning", GTK_ICON_SIZE_DIALOG);
	gtk_widget_show(image);
	gtk_table_attach(GTK_TABLE(table1), image, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);

	checkbutton1 = gtk_check_button_new_with_mnemonic(_("Don't ask next time."));
	gtk_widget_show(checkbutton1);
	gtk_table_attach(GTK_TABLE(table1), checkbutton1, 0, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 4);

	text = saprintf("<span weight=\"bold\" size=\"larger\">%s</span>\n", _("Are you sure you want to quit?"));
	label = gtk_label_new(text);
	xfree(text);

	gtk_widget_show(label);
	gtk_table_attach(GTK_TABLE(table1), label, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	dialog_action_area1 = GTK_DIALOG(dialog)->action_area;
	gtk_widget_show(dialog_action_area1);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog_action_area1), GTK_BUTTONBOX_END);

	if (minimize_button) {
		button = gtk_button_new_with_mnemonic(_("_Minimize to Tray"));
		gtk_widget_show(button);
		gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, 1);
	}

	button = gtk_button_new_from_stock("gtk-cancel");
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_CANCEL);
	gtk_widget_grab_focus(button);

	button = gtk_button_new_from_stock("gtk-quit");
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, 0);

	gtk_widget_show(dialog);

	switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
	case 0:
#if 0
		if (GTK_TOGGLE_BUTTON(checkbutton1)->active)
			gui_quit_dialog_config = 0;
		xchat_exit();
#endif
		ekg_exit();
		break;
	case 1:		/* minimize to tray */
#if 0
		if (GTK_TOGGLE_BUTTON(checkbutton1)->active) {
			gui_tray_flags_config |= 1;
			/*prefs.gui_quit_dialog = 0; */
		}
		tray_toggle_visibility(TRUE);
#endif
		break;
	}

	gtk_widget_destroy(dialog);
	dialog = NULL;
}

void mg_close_sess(window_t *sess) {
	if (sess == window_status) {	/* status window? */
		mg_open_quit_dialog(FALSE);
		return;
	}
	window_kill(sess);	/* fe_close_window() */
}

static int mg_chan_remove(chan * ch) {
	/* remove the tab from chanview */
	chan_remove(ch, TRUE);
	/* any tabs left? */
	if (chanview_get_size(mg_gui->chanview) < 1) {
		/* if not, destroy the main tab window */
		gtk_widget_destroy(mg_gui->window);
#if DARK
		current_tab = NULL;
#endif
		active_tab = NULL;
		mg_gui = NULL;
		parent_window = NULL;
		return TRUE;
	}
	return FALSE;
}

/* the "X" close button has been pressed (tab-view) */

static void mg_xbutton_cb(chanview * cv, chan * ch, int tag, gpointer userdata) {
	printf("mg_xbutoon_cb(%p) [%d [TAG_IRC: %d]\n", userdata, tag, TAG_IRC);
	if (tag == TAG_IRC)	/* irc tab */
		mg_close_sess(userdata);

#warning "xchat->ekg2, removed support for generic tabs"
}


static void mg_detach_tab_cb(GtkWidget *item, chan * ch) {
	if (chan_get_tag(ch) == TAG_IRC) {	/* IRC tab */
		/* userdata is session * */
		mg_link_irctab(chan_get_userdata(ch), 1);
		return;
	}
#warning "xchat->ekg2, removed support for generic tabs"
}

static void mg_destroy_tab_cb(GtkWidget *item, chan * ch) {
	/* treat it just like the X button press */
	mg_xbutton_cb(mg_gui->chanview, ch, chan_get_tag(ch), chan_get_userdata(ch));
}

static void mg_color_insert(GtkWidget *item, gpointer userdata) {
	char *text;
	int num = GPOINTER_TO_INT(userdata);

	if (num > 99) {
		switch (num) {
		case 100:
			text = "\002";
			break;
		case 101:
			text = "\037";
			break;
		case 102:
			text = "\035";
			break;
		default:
			text = "\017";
			break;
		}
#if 0
		key_action_insert(current_sess->gui->input_box, 0, text, 0, 0);
#endif
	} else {
#if 0
		char buf[32];
		sprintf(buf, "\003%02d", num);
		key_action_insert(current_sess->gui->input_box, 0, buf, 0, 0);
#endif
	}
}

static void mg_markup_item(GtkWidget *menu, char *text, int arg) {
	GtkWidget *item;

	item = gtk_menu_item_new_with_label("");
	gtk_label_set_markup(GTK_LABEL(GTK_BIN(item)->child), text);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(mg_color_insert), GINT_TO_POINTER(arg));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);
}

GtkWidget * mg_submenu(GtkWidget *menu, char *text) {
	GtkWidget *submenu, *item;

	item = gtk_menu_item_new_with_mnemonic(text);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	gtk_widget_show(submenu);

	return submenu;
}

static void mg_create_color_menu(GtkWidget *menu, window_t *sess) {
	GtkWidget *submenu;
	GtkWidget *subsubmenu;
	char buf[256];
	int i;

	submenu = mg_submenu(menu, _("Insert Attribute or Color Code"));

	mg_markup_item(submenu, _("<b>Bold</b>"), 100);
	mg_markup_item(submenu, _("<u>Underline</u>"), 101);
	/*mg_markup_item (submenu, _("<i>Italic</i>"), 102); */
	mg_markup_item(submenu, _("Normal"), 103);

	subsubmenu = mg_submenu(submenu, _("Colors 0-7"));

	for (i = 0; i < 8; i++) {
		sprintf(buf, "<tt><sup>%02d</sup> <span background=\"#%02x%02x%02x\">"
			"   </span></tt>",
			i, colors[i].red >> 8, colors[i].green >> 8, colors[i].blue >> 8);
		mg_markup_item(subsubmenu, buf, i);
	}

	subsubmenu = mg_submenu(submenu, _("Colors 8-15"));

	for (i = 8; i < 16; i++) {
		sprintf(buf, "<tt><sup>%02d</sup> <span background=\"#%02x%02x%02x\">"
			"   </span></tt>",
			i, colors[i].red >> 8, colors[i].green >> 8, colors[i].blue >> 8);
		mg_markup_item(subsubmenu, buf, i);
	}
}

static gboolean mg_tab_contextmenu_cb(chanview * cv, chan * ch, int tag, gpointer ud, GdkEventButton * event) {
	GtkWidget *menu, *item;
	window_t *sess = ud;

	/* shift-click to close a tab */
	if ((event->state & GDK_SHIFT_MASK) && event->type == GDK_BUTTON_PRESS) {
		mg_xbutton_cb(cv, ch, tag, ud);
		return FALSE;
	}

	if (event->button != 3)
		return FALSE;

	menu = gtk_menu_new();

	if (tag == TAG_IRC) {
		char buf[256];

		const char *w_target = gtk_window_target(sess);
		char *target = g_markup_escape_text(w_target[0] ? w_target : "<none>", -1);

		const char *w_session = (sess && sess->session) ? sess->session->uid : NULL;
		char *session = w_session ? g_markup_escape_text(w_session, -1) : NULL;

		snprintf(buf, sizeof(buf), "<span foreground=\"#3344cc\"><b>%s %s</b></span>", target, session ? session : "");
		g_free(target);
		g_free(session);

		item = gtk_menu_item_new_with_label("");
		gtk_label_set_markup(GTK_LABEL(GTK_BIN(item)->child), buf);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

#if 0
		/* separator */
		item = gtk_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);

		menu_toggle_item(_("Beep on message"), menu, mg_beepmsg_cb, sess, sess->beep);
		if (prefs.gui_tray)
			menu_toggle_item(_("Blink tray on message"), menu, mg_traymsg_cb, sess,
					 sess->tray);
		if (sess->type == SESS_CHANNEL)
			menu_toggle_item(_("Show join/part messages"), menu, mg_hidejp_cb,
					 sess, !sess->hide_join_part);
#endif

	}
	/* separator */
	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	mg_create_icon_item(_("_Close Tab"), GTK_STOCK_CLOSE, menu, mg_destroy_tab_cb, ch);
	mg_create_icon_item(_("_Detach Tab"), GTK_STOCK_REDO, menu, mg_detach_tab_cb, ch);
#if 0

	if (sess && tabmenu_list)
		menu_create(menu, tabmenu_list, sess->channel, FALSE);
	menu_add_plugin_items(menu, "\x4$TAB", sess->channel);
#endif

	if (event->window)
		gtk_menu_set_screen(GTK_MENU(menu), gdk_drawable_get_screen(event->window));
	g_object_ref(menu);
	g_object_ref_sink(menu);
	g_object_unref(menu);
	g_signal_connect(G_OBJECT(menu), "selection-done", G_CALLBACK(mg_menu_destroy), NULL);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, event->time);
	return TRUE;
}

/* add a tabbed channel */

static void mg_add_chan(window_t *sess) {
	GdkPixbuf *icon = NULL;	/* pix_channel || pix_server || pix_dialog */

	gtk_private(sess)->tab = chanview_add(gtk_private_ui(sess)->chanview, (char *) gtk_window_target(sess),	/* sess->session, */
						  sess, FALSE, TAG_IRC, icon);
	if (plain_list == NULL)
		mg_create_tab_colors();
	chan_set_color(gtk_private(sess)->tab, plain_list);

	if (gtk_private(sess)->buffer == NULL) {
		gtk_private(sess)->buffer =
			gtk_xtext_buffer_new(GTK_XTEXT(gtk_private_ui(sess)->xtext));
		gtk_xtext_set_time_stamp(gtk_private(sess)->buffer, config_timestamp_show);
		gtk_private(sess)->user_model = userlist_create_model();
	}
}

#if 0

/* mg_userlist_button() do przemyslenia */
/* mg_create_userlistbuttons() */

static void mg_topic_cb(GtkWidget *entry, gpointer userdata) {
	session *sess = current_sess;
	char *text;

	if (sess->channel[0] && sess->server->connected && sess->type == SESS_CHANNEL) {
		text = GTK_ENTRY(entry)->text;
		if (text[0] == 0)
			text = NULL;
		sess->server->p_topic(sess->server, sess->channel, text);
	} else
		gtk_entry_set_text(GTK_ENTRY(entry), "");
	/* restore focus to the input widget, where the next input will most
	   likely be */
	gtk_widget_grab_focus(sess->gui->input_box);
}

#endif

static void mg_tabwindow_kill_cb(GtkWidget *win, gpointer userdata) {
#if 0
	GSList *list, *next;
	session *sess;

/*	puts("enter mg_tabwindow_kill_cb");*/
	xchat_is_quitting = TRUE;

	/* see if there's any non-tab windows left */
	list = sess_list;
	while (list) {
		sess = list->data;
		next = list->next;
		if (!sess->gui->is_tab) {
			xchat_is_quitting = FALSE;
/*			puts("-> will not exit, some toplevel windows left");*/
		} else {
			mg_ircdestroy(sess);
		}
		list = next;
	}

	current_tab = NULL;
	active_tab = NULL;
	mg_gui = NULL;
	parent_window = NULL;
#endif
}

static GtkWidget *mg_changui_destroy(window_t *sess) {
	GtkWidget *ret = NULL;

	if (gtk_private_ui(sess)->is_tab) {
		/* avoid calling the "destroy" callback */
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_private_ui(sess)->window),
						     mg_tabwindow_kill_cb, 0);
		/* remove the tab from the chanview */
		if (!mg_chan_remove(gtk_private(sess)->tab))
			/* if the window still exists, restore the signal handler */
			g_signal_connect(G_OBJECT(gtk_private_ui(sess)->window), "destroy",
					 G_CALLBACK(mg_tabwindow_kill_cb), 0);
	} else {
		/* avoid calling the "destroy" callback */
		g_signal_handlers_disconnect_by_func(G_OBJECT(gtk_private_ui(sess)->window),
						     mg_topdestroy_cb, sess);
		/*gtk_widget_destroy (sess->gui->window); */
		/* don't destroy until the new one is created. Not sure why, but */
		/* it fixes: Gdk-CRITICAL **: gdk_colormap_get_screen: */
		/*           assertion `GDK_IS_COLORMAP (cmap)' failed */
		ret = gtk_private_ui(sess)->window;
		free(gtk_private_ui(sess));
		gtk_private(sess)->gui = NULL;
	}
	return ret;
}

static void mg_link_irctab(window_t *sess, int focus) {
	GtkWidget *win;

	if (gtk_private_ui(sess)->is_tab) {
		win = mg_changui_destroy(sess);
		mg_changui_new(sess, gtk_private(sess), 0, focus);
		mg_populate(sess);
#if 0
		xchat_is_quitting = FALSE;
#endif
		if (win)
			gtk_widget_destroy(win);
		return;
	}

	win = mg_changui_destroy(sess);
	mg_changui_new(sess, gtk_private(sess), 1, focus);
	/* the buffer is now attached to a different widget */
	((xtext_buffer *) gtk_private(sess)->buffer)->xtext = (GtkXText *) gtk_private_ui(sess)->xtext;
	if (win)
		gtk_widget_destroy(win);
}

void mg_detach(window_t *sess, int mode) {
	switch (mode) {
		/* detach only */
	case 1:
		if (gtk_private_ui(sess)->is_tab)
			mg_link_irctab(sess, 1);
		break;
		/* attach only */
	case 2:
		if (!gtk_private_ui(sess)->is_tab)
			mg_link_irctab(sess, 1);
		break;
		/* toggle */
	default:
		mg_link_irctab(sess, 1);
	}
}

static void mg_apply_entry_style(GtkWidget *entry) {
	gtk_widget_modify_base(entry, GTK_STATE_NORMAL, &colors[COL_BG]);
	gtk_widget_modify_text(entry, GTK_STATE_NORMAL, &colors[COL_FG]);
	gtk_widget_modify_font(entry, input_style->font_desc);
}

#if 0

static void mg_dialog_button_cb(GtkWidget *wid, char *cmd) {
	/* the longest cmd is 12, and the longest nickname is 64 */
	char buf[128];
	char *host = "";
	char *topic;

	topic = (char *)(GTK_ENTRY(gtk_private(window_current)->gui->topic_entry)->text);
	topic = strrchr(topic, '@');
	if (topic)
		host = topic + 1;

	auto_insert(buf, sizeof(buf), cmd, 0, 0, "", "", "",
		    server_get_network(current_sess->server, TRUE), host, "",
		    current_sess->channel);

	handle_command(current_sess, buf, TRUE);

	/* dirty trick to avoid auto-selection */
	SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, FALSE);
	gtk_widget_grab_focus(current_sess->gui->input_box);
	SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, TRUE);
}

static void
mg_dialog_button(GtkWidget *box, char *name, char *cmd)
{
	GtkWidget *wid;

	wid = gtk_button_new_with_label(name);
	gtk_box_pack_start(GTK_BOX(box), wid, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(mg_dialog_button_cb), cmd);
	gtk_widget_set_size_request(wid, -1, 0);
}

static void
mg_create_dialogbuttons(GtkWidget *box)
{
	struct popup *pop;
	GSList *list = dlgbutton_list;

	while (list) {
		pop = list->data;
		if (pop->cmd[0])
			mg_dialog_button(box, pop->name, pop->cmd);
		list = list->next;
	}
}

#endif

static void
mg_create_topicbar(window_t *sess, GtkWidget *box)
{
	GtkWidget *hbox, *topic, *bbox;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	gui->topic_bar = hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, 0, 0, 0);

	if (!gui->is_tab)
		gtk_private(sess)->tab = NULL;

	gui->topic_entry = topic = gtk_entry_new();
	gtk_widget_set_name(topic, "xchat-inputbox");
	gtk_container_add(GTK_CONTAINER(hbox), topic);
#if 0
	g_signal_connect(G_OBJECT(topic), "activate", G_CALLBACK(mg_topic_cb), 0);
#endif

	if (style_inputbox_config)
		mg_apply_entry_style(topic);

	gui->topicbutton_box = bbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), bbox, 0, 0, 0);

	gui->dialogbutton_box = bbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), bbox, 0, 0, 0);
#if 0
	mg_create_dialogbuttons(bbox);
#endif

	if (!paned_userlist_config)
		gtkutil_button(hbox, GTK_STOCK_GOTO_LAST, _("Show/Hide userlist"),
			       mg_userlist_toggle_cb, 0, 0);
}

/* check if a word is clickable */

static int
mg_word_check(GtkWidget *xtext, char *word, int len)
{
#warning "xchat->ekg2: mg_word_check() nice functionality XXX"
	return 0;
}

/* mouse click inside text area */

static void
mg_word_clicked(GtkWidget *xtext, char *word, GdkEventButton * even)
{
#warning "xchat->ekg2: mg_word_clicked() nice functionality XXX"
}

void
mg_update_xtext(GtkWidget *wid)
{
	GtkXText *xtext = GTK_XTEXT(wid);

	gtk_xtext_set_palette(xtext, colors);
	gtk_xtext_set_max_lines(xtext, backlog_size_config);
	gtk_xtext_set_tint(xtext, tint_red_config, tint_green_config, tint_blue_config);
//      gtk_xtext_set_background (xtext, channelwin_pix, transparent_config);
	gtk_xtext_set_wordwrap(xtext, wordwrap_config);
	gtk_xtext_set_show_marker(xtext, show_marker_config);
	gtk_xtext_set_show_separator(xtext, indent_nicks_config ? show_separator_config : 0);
	gtk_xtext_set_indent(xtext, indent_nicks_config);

	if (!gtk_xtext_set_font(xtext, font_normal_config)) {
		printf("Failed to open any font. I'm out of here!");	/* FE_MSG_WAIT | FE_MSG_ERROR */
		exit(1);
	}

	gtk_xtext_refresh(xtext, FALSE);
}

/* handle errors reported by xtext */

static void
mg_xtext_error(int type)
{
	printf("mg_xtext_error() %d\n", type);

	/* @ type == 0 "Unable to set transparent background!\n\n"
	 *              "You may be using a non-compliant window\n"
	 *              "manager that is not currently supported.\n"), FE_MSG_WARN);
	 *
	 *              config_transparent = 0; 
	 */
}

static void
mg_create_textarea(window_t *sess, GtkWidget *box)
{
	GtkWidget *inbox, *vbox, *frame;
	GtkXText *xtext;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(box), vbox);

	inbox = gtk_hbox_new(FALSE, SCROLLBAR_SPACING);
	gtk_container_add(GTK_CONTAINER(vbox), inbox);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(inbox), frame);

	gui->xtext = gtk_xtext_new(colors, TRUE);
	xtext = GTK_XTEXT(gui->xtext);
	gtk_xtext_set_max_indent(xtext, max_auto_indent_config);
	gtk_xtext_set_thin_separator(xtext, thin_separator_config);
	gtk_xtext_set_error_function(xtext, mg_xtext_error);
	gtk_xtext_set_urlcheck_function(xtext, mg_word_check);
	gtk_xtext_set_max_lines(xtext, backlog_size_config);
	gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(xtext));
	mg_update_xtext(GTK_WIDGET(xtext));

	g_signal_connect(G_OBJECT(xtext), "word_click", G_CALLBACK(mg_word_clicked), NULL);
	gui->vscrollbar = gtk_vscrollbar_new(GTK_XTEXT(xtext)->adj);
	gtk_box_pack_start(GTK_BOX(inbox), gui->vscrollbar, FALSE, TRUE, 0);

#warning "xchat->ekg2: g_signal_connect() \"drag_begin\", \"drag_drop\", \"drag_motion\", \"drag_end\", \"drag_data_received\" && gtk_drag_dest_set() do zaimplementowania"
}

static void
mg_create_userlist(gtk_window_ui_t *gui, GtkWidget *box)
{
	GtkWidget *frame, *ulist, *vbox;

	vbox = gtk_vbox_new(0, 1);
	gtk_container_add(GTK_CONTAINER(box), vbox);

	frame = gtk_frame_new(NULL);
	if (!(gui_tweaks_config & 1))
		gtk_box_pack_start(GTK_BOX(vbox), frame, 0, 0, GUI_SPACING);

	gui->namelistinfo = gtk_label_new(NULL);
	gtk_container_add(GTK_CONTAINER(frame), gui->namelistinfo);

	gui->user_tree = ulist = userlist_create(vbox);
#if 0
	if (prefs.style_namelistgad) {
		gtk_widget_set_style(ulist, input_style);
		gtk_widget_modify_base(ulist, GTK_STATE_NORMAL, &colors[COL_BG]);
	}
#endif
}

static void
mg_leftpane_cb(GtkPaned * pane, GParamSpec * param, gtk_window_ui_t* gui)
{
	gui_pane_left_size_config = gtk_paned_get_position(pane);
}

static void
mg_rightpane_cb(GtkPaned * pane, GParamSpec * param, gtk_window_ui_t* gui)
{
	int handle_size;

/*	if (pane->child1 == NULL || (!GTK_WIDGET_VISIBLE (pane->child1)))
		return;
	if (pane->child2 == NULL || (!GTK_WIDGET_VISIBLE (pane->child2)))
		return;*/

	gtk_widget_style_get(GTK_WIDGET(pane), "handle-size", &handle_size, NULL);

	/* record the position from the RIGHT side */
	gui_pane_right_size_config =
		GTK_WIDGET(pane)->allocation.width - gtk_paned_get_position(pane) - handle_size;
}

static IDLER(mg_add_pane_signals) {
	gtk_window_ui_t *gui = data;
	g_signal_connect(G_OBJECT(gui->hpane_right), "notify::position",
			 G_CALLBACK(mg_rightpane_cb), gui);
	g_signal_connect(G_OBJECT(gui->hpane_left), "notify::position",
			 G_CALLBACK(mg_leftpane_cb), gui);
	return -1;
}

static void
mg_create_center(window_t *sess, gtk_window_ui_t *gui, GtkWidget *box)
{
	GtkWidget *vbox, *hbox, *book;

	/* sep between top and bottom of left side */
	gui->vpane_left = gtk_vpaned_new();

	/* sep between top and bottom of right side */
	gui->vpane_right = gtk_vpaned_new();

	/* sep between left and xtext */
	gui->hpane_left = gtk_hpaned_new();
	gtk_paned_set_position(GTK_PANED(gui->hpane_left), gui_pane_left_size_config);

	/* sep between xtext and right side */
	gui->hpane_right = gtk_hpaned_new();

	if (gui_tweaks_config & 4) {
		gtk_paned_pack2(GTK_PANED(gui->hpane_left), gui->vpane_left, FALSE, TRUE);
		gtk_paned_pack1(GTK_PANED(gui->hpane_left), gui->hpane_right, TRUE, TRUE);
	} else {
		gtk_paned_pack1(GTK_PANED(gui->hpane_left), gui->vpane_left, FALSE, TRUE);
		gtk_paned_pack2(GTK_PANED(gui->hpane_left), gui->hpane_right, TRUE, TRUE);
	}
	gtk_paned_pack2(GTK_PANED(gui->hpane_right), gui->vpane_right, FALSE, TRUE);

	gtk_container_add(GTK_CONTAINER(box), gui->hpane_left);

	gui->note_book = book = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(book), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(book), FALSE);
	gtk_paned_pack1(GTK_PANED(gui->hpane_right), book, TRUE, TRUE);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_paned_pack1(GTK_PANED(gui->vpane_right), hbox, FALSE, TRUE);
	mg_create_userlist(gui, hbox);

	gui->user_box = hbox;

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_notebook_append_page(GTK_NOTEBOOK(book), vbox, NULL);

	mg_create_topicbar(sess, vbox);
	mg_create_textarea(sess, vbox);
	mg_create_entry(sess, vbox);

	idle_add(&gtk_plugin, mg_add_pane_signals, gui);
}

static void mg_sessionclick_cb(GtkWidget *button, gpointer userdata) {
#warning "xchat->ekg2: mg_sessionclick_cb() XXX, change session using this [like ^X] implement"
	/* xchat: 
	 *      fe_get_str (_("Enter new nickname:"), current_sess->server->nick, mg_change_nick, NULL);
	 */
}

/* make sure chanview and userlist positions are sane */

static void
mg_sanitize_positions(int *cv, int *ul)
{
	if (tab_layout_config == 2) {
		/* treeview can't be on TOP or BOTTOM */
		if (*cv == POS_TOP || *cv == POS_BOTTOM)
			*cv = POS_TOPLEFT;
	}

	/* userlist can't be on TOP or BOTTOM */
	if (*ul == POS_TOP || *ul == POS_BOTTOM)
		*ul = POS_TOPRIGHT;

	/* can't have both in the same place */
	if (*cv == *ul) {
		*cv = POS_TOPRIGHT;
		if (*ul == POS_TOPRIGHT)
			*cv = POS_BOTTOMRIGHT;
	}
}

static void
mg_place_userlist_and_chanview_real(gtk_window_ui_t *gui, GtkWidget *userlist, GtkWidget *chanview)
{
	int unref_userlist = FALSE;
	int unref_chanview = FALSE;

	/* first, remove userlist/treeview from their containers */
	if (userlist && userlist->parent) {
		g_object_ref(userlist);
		gtk_container_remove(GTK_CONTAINER(userlist->parent), userlist);
		unref_userlist = TRUE;
	}

	if (chanview && chanview->parent) {
		g_object_ref(chanview);
		gtk_container_remove(GTK_CONTAINER(chanview->parent), chanview);
		unref_chanview = TRUE;
	}

	if (chanview) {
		/* incase the previous pos was POS_HIDDEN */
		gtk_widget_show(chanview);

		gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 1, 0);
		gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 2, 2);

		/* then place them back in their new positions */
		switch (tab_pos_config) {
		case POS_TOPLEFT:
			gtk_paned_pack1(GTK_PANED(gui->vpane_left), chanview, FALSE, TRUE);
			break;
		case POS_BOTTOMLEFT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_left), chanview, FALSE, TRUE);
			break;
		case POS_TOPRIGHT:
			gtk_paned_pack1(GTK_PANED(gui->vpane_right), chanview, FALSE, TRUE);
			break;
		case POS_BOTTOMRIGHT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_right), chanview, FALSE, TRUE);
			break;
		case POS_TOP:
			gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 1, GUI_SPACING - 1);
			gtk_table_attach(GTK_TABLE(gui->main_table), chanview,
					 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
			break;
		case POS_HIDDEN:
			gtk_widget_hide(chanview);
			/* always attach it to something to avoid ref_count=0 */
			if (gui_ulist_pos_config == POS_TOP)
				gtk_table_attach(GTK_TABLE(gui->main_table), chanview, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
			else
				gtk_table_attach(GTK_TABLE(gui->main_table), chanview, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
			break;
		default:	/* POS_BOTTOM */
			gtk_table_set_row_spacing(GTK_TABLE(gui->main_table), 2, 3);
			gtk_table_attach(GTK_TABLE(gui->main_table), chanview,
					 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
		}
	}

	if (userlist) {
		switch (gui_ulist_pos_config) {
		case POS_TOPLEFT:
			gtk_paned_pack1(GTK_PANED(gui->vpane_left), userlist, FALSE, TRUE);
			break;
		case POS_BOTTOMLEFT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_left), userlist, FALSE, TRUE);
			break;
		case POS_BOTTOMRIGHT:
			gtk_paned_pack2(GTK_PANED(gui->vpane_right), userlist, FALSE, TRUE);
			break;
		/* case POS_HIDDEN:
			break; */	/* Hide using the VIEW menu instead */
		default:	/* POS_TOPRIGHT */
			gtk_paned_pack1(GTK_PANED(gui->vpane_right), userlist, FALSE, TRUE);
		}
	}

	if (unref_chanview)
		g_object_unref(chanview);
	if (unref_userlist)
		g_object_unref(userlist);

	mg_hide_empty_boxes(gui);
}

static void
mg_place_userlist_and_chanview(gtk_window_ui_t *gui)
{
	GtkOrientation orientation;
	GtkWidget *chanviewbox = NULL;
	int pos;

	mg_sanitize_positions(&tab_pos_config, &gui_ulist_pos_config);

	if (gui->chanview) {
		pos = tab_pos_config;

		orientation = chanview_get_orientation(gui->chanview);
		if ((pos == POS_BOTTOM || pos == POS_TOP)
		    && orientation == GTK_ORIENTATION_VERTICAL)
			chanview_set_orientation(gui->chanview, FALSE);
		else if ((pos == POS_TOPLEFT || pos == POS_BOTTOMLEFT || pos == POS_TOPRIGHT
			  || pos == POS_BOTTOMRIGHT) && orientation == GTK_ORIENTATION_HORIZONTAL)
			chanview_set_orientation(gui->chanview, TRUE);
		chanviewbox = chanview_get_box(gui->chanview);
	}

	mg_place_userlist_and_chanview_real(gui, gui->user_box, chanviewbox);
}

void
mg_change_layout(int type)
{
	if (mg_gui) {
		/* put tabs at the bottom */
		if (type == 0 && tab_pos_config != POS_BOTTOM && tab_pos_config != POS_TOP)
			tab_pos_config = POS_BOTTOM;

		mg_place_userlist_and_chanview(mg_gui);
		chanview_set_impl(mg_gui->chanview, type);
	}
}

static void
mg_inputbox_rightclick(GtkEntry * entry, GtkWidget *menu)
{
	mg_create_color_menu(menu, NULL);
}

static void
mg_create_entry(window_t *sess, GtkWidget *box)
{
	GtkWidget *hbox, *but, *entry;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), hbox, 0, 0, 0);

	gui->nick_box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gui->nick_box, 0, 0, 0);
#if DARK
# warning "XXX?"
#endif
	gui->nick_label = but =	gtk_button_new_with_label(gtk_session_target(sess->session));
	gtk_button_set_relief(GTK_BUTTON(but), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(but, GTK_CAN_FOCUS);
	gtk_box_pack_end(GTK_BOX(gui->nick_box), but, 0, 0, 0);
	g_signal_connect(G_OBJECT(but), "clicked", G_CALLBACK(mg_sessionclick_cb), NULL);

	gui->input_box = entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(gui->input_box), 2048);
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(mg_inputbox_cb), gui);

	gtk_container_add(GTK_CONTAINER(hbox), entry);

	gtk_widget_set_name(entry, "xchat-inputbox");

	g_signal_connect(G_OBJECT(entry), "key_press_event",
			 G_CALLBACK(key_handle_key_press), NULL);

	g_signal_connect(G_OBJECT(entry), "focus_in_event", G_CALLBACK(mg_inputbox_focus), gui);
	g_signal_connect(G_OBJECT(entry), "populate_popup", G_CALLBACK(mg_inputbox_rightclick), NULL);

	gtk_widget_grab_focus(entry);

	if (style_inputbox_config)
		mg_apply_entry_style(entry);
}

static void mg_switch_tab_cb(chanview * cv, chan * ch, int tag, gpointer ud) {
	chan *old;
	window_t *sess = ud;

	old = active_tab;
	active_tab = ch;

	if (active_tab != old) {
#warning "xchat->ekg2 mg_switch_tab_cb() mg_unpopulate()"
		mg_populate(sess);

		/* it's switched by gui, let's inform ekg2 */
		if (in_autoexec == 0 && gtk_ui_window_switch_lock == 0) {
			window_switch(sess->id);
		}
	}
}

/* compare two tabs (for tab sorting function) */

static int mg_tabs_compare(window_t *a, window_t *b) {	/* it's lik: window_new_compare() */
	return (a->id - b->id);
}

static void mg_create_tabs(gtk_window_ui_t *gui) {
	gui->chanview = chanview_new(tab_layout_config, truncchans_config,
				     tab_sort_config, tab_icons_config,
				     style_namelistgad_config ? input_style : NULL);
	chanview_set_callbacks(gui->chanview, mg_switch_tab_cb, mg_xbutton_cb,
			       mg_tab_contextmenu_cb, (void *) mg_tabs_compare);
	mg_place_userlist_and_chanview(gui);
}

static gboolean
mg_tabwin_focus_cb(GtkWindow * win, GdkEventFocus * event, gpointer userdata)
{
#if 0
	current_sess = current_tab;
	if (current_sess) {
		gtk_xtext_check_marker_visibility(GTK_XTEXT(current_sess->gui->xtext));
		plugin_emit_dummy_print(current_sess, "Focus Window");
	}
#endif
#ifdef USE_XLIB
	unflash_window(GTK_WIDGET(win));
#endif
	return FALSE;
}

static gboolean
mg_topwin_focus_cb(GtkWindow * win, GdkEventFocus * event, window_t *sess)
{
#if 0
	current_sess = sess;
	if (!sess->server->server_session)
		sess->server->server_session = sess;
	gtk_xtext_check_marker_visibility(GTK_XTEXT(current_sess->gui->xtext));
#ifdef USE_XLIB
	unflash_window(GTK_WIDGET(win));
#endif
	plugin_emit_dummy_print(sess, "Focus Window");
#endif
	return FALSE;
}

static void
mg_create_menu(gtk_window_ui_t *gui, GtkWidget *table, int away_state)
{
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(table)), accel_group);
	g_object_unref(accel_group);

	gui->menu = menu_create_main(accel_group, TRUE, away_state, !gui->is_tab, gui->menu_item);
	gtk_table_attach(GTK_TABLE(table), gui->menu, 0, 3, 0, 1,
			 GTK_EXPAND | GTK_FILL, GTK_SHRINK | GTK_FILL, 0, 0);
}

static void
mg_create_irctab(window_t *sess, GtkWidget *table)
{
	GtkWidget *vbox;
	gtk_window_ui_t *gui = gtk_private_ui(sess);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 1, 2, 2, 3,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	mg_create_center(sess, gui, vbox);
}

static void
mg_create_topwindow(window_t *sess)
{
	GtkWidget *win;
	GtkWidget *table;

	win = gtkutil_window_new("ekg2", NULL, mainwindow_width_config,
				 mainwindow_height_config, 0);

	gtk_private_ui(sess)->window = win;
	gtk_container_set_border_width(GTK_CONTAINER(win), GUI_BORDER);

	g_signal_connect(G_OBJECT(win), "focus_in_event", G_CALLBACK(mg_topwin_focus_cb), sess);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(mg_topdestroy_cb), sess);
	g_signal_connect(G_OBJECT(win), "configure_event", G_CALLBACK(mg_configure_cb), sess);

	palette_alloc(win);

	table = gtk_table_new(4, 3, FALSE);
	/* spacing under the menubar */
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, GUI_SPACING);
	/* left and right borders */
	gtk_table_set_col_spacing(GTK_TABLE(table), 0, 1);
	gtk_table_set_col_spacing(GTK_TABLE(table), 1, 1);
	gtk_container_add(GTK_CONTAINER(win), table);

	mg_create_irctab(sess, table);
	/* vvvvv sess->server->is_away */
	mg_create_menu(gtk_private_ui(sess), table, 0);

	if (gtk_private(sess)->buffer == NULL) {
		gtk_private(sess)->buffer =
			gtk_xtext_buffer_new(GTK_XTEXT(gtk_private_ui(sess)->xtext));
		gtk_xtext_buffer_show(GTK_XTEXT(gtk_private_ui(sess)->xtext),
				      gtk_private(sess)->buffer, TRUE);
		gtk_xtext_set_time_stamp(gtk_private(sess)->buffer, config_timestamp_show);
		gtk_private(sess)->user_model = userlist_create_model();
	}
	userlist_show(sess);

	gtk_widget_show_all(table);

	if (hidemenu_config)
		gtk_widget_hide(gtk_private_ui(sess)->menu);

	if (!topicbar_config)
		gtk_widget_hide(gtk_private_ui(sess)->topic_bar);

	if (gui_tweaks_config & 2)
		gtk_widget_hide(gtk_private_ui(sess)->nick_box);

	mg_decide_userlist(sess, FALSE);

#if DARK
	if (sess->type == SESS_DIALOG) {
		/* hide the chan-mode buttons */
		gtk_widget_hide(sess->gui->topicbutton_box);
	} else {
		gtk_widget_hide(sess->gui->dialogbutton_box);

		if (!prefs.chanmodebuttons)
			gtk_widget_hide(sess->gui->topicbutton_box);
	}
#endif

	mg_place_userlist_and_chanview(gtk_private_ui(sess));

	gtk_widget_show(win);
}

static gboolean mg_tabwindow_de_cb(GtkWidget *widget, GdkEvent * event, gpointer user_data) {
#if 0
	if ((gui_tray_flags_config & 1) && tray_toggle_visibility(FALSE))
		return TRUE;

	/* check for remaining toplevel windows */
	list = sess_list;
	while (list) {
		sess = list->data;
		if (!sess->gui->is_tab)
			return FALSE;
		list = list->next;
	}
#endif

	mg_open_quit_dialog(TRUE);
	return TRUE;
}

static void mg_create_tabwindow(window_t *sess) {
	GtkWidget *win;
	GtkWidget *table;

	win = gtkutil_window_new("ekg2", NULL, mainwindow_width_config, mainwindow_height_config,
				 0);

	gtk_private_ui(sess)->window = win;
	gtk_window_move(GTK_WINDOW(win), mainwindow_left_config, mainwindow_top_config);

	if (gui_win_state_config)
		gtk_window_maximize(GTK_WINDOW(win));

	gtk_container_set_border_width(GTK_CONTAINER(win), GUI_BORDER);

	g_signal_connect(G_OBJECT(win), "delete_event", G_CALLBACK(mg_tabwindow_de_cb), 0);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(mg_tabwindow_kill_cb), 0);
	g_signal_connect(G_OBJECT(win), "focus_in_event", G_CALLBACK(mg_tabwin_focus_cb), NULL);
	g_signal_connect(G_OBJECT(win), "configure_event", G_CALLBACK(mg_configure_cb), NULL);
	g_signal_connect(G_OBJECT(win), "window_state_event", G_CALLBACK(mg_windowstate_cb), NULL);

	palette_alloc(win);

	gtk_private_ui(sess)->main_table = table = gtk_table_new(4, 3, FALSE);
	/* spacing under the menubar */
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, GUI_SPACING);
	/* left and right borders */
	gtk_table_set_col_spacing(GTK_TABLE(table), 0, 1);
	gtk_table_set_col_spacing(GTK_TABLE(table), 1, 1);
	gtk_container_add(GTK_CONTAINER(win), table);

	mg_create_irctab(sess, table);
	mg_create_tabs(gtk_private_ui(sess));
	/* vvvvvv sess->server->is_away */
	mg_create_menu(gtk_private_ui(sess), table, 0);

	mg_focus(sess);

	gtk_widget_show_all(table);

	if (hidemenu_config)
		gtk_widget_hide(gtk_private_ui(sess)->menu);

	mg_decide_userlist(sess, FALSE);

	if (!topicbar_config)
		gtk_widget_hide(gtk_private_ui(sess)->topic_bar);

	if (!chanmodebuttons_config)
		gtk_widget_hide(gtk_private_ui(sess)->topicbutton_box);

	if (gui_tweaks_config & 2)
		gtk_widget_hide(gtk_private_ui(sess)->nick_box);

	mg_place_userlist_and_chanview(gtk_private_ui(sess));
	gtk_widget_show(win);
}

void mg_apply_setup(void) {
	int done_main = FALSE;
	window_t *w;

	mg_create_tab_colors();

	for (w = windows; w; w = w->next) {
		gtk_xtext_set_time_stamp(gtk_private(w)->buffer, config_timestamp_show);
		((xtext_buffer *) gtk_private(w)->buffer)->needs_recalc = TRUE;

		if (!gtk_private_ui(w)->is_tab || !done_main)
			mg_place_userlist_and_chanview(gtk_private_ui(w));

		if (gtk_private_ui(w)->is_tab)
			done_main = TRUE;
	}
}

#if 0
static chan *
mg_add_generic_tab(char *name, char *title, void *family, GtkWidget *box)
{
	chan *ch;

	gtk_notebook_append_page(GTK_NOTEBOOK(mg_gui->note_book), box, NULL);
	gtk_widget_show(box);

	ch = chanview_add(mg_gui->chanview, name, NULL, box, TRUE, TAG_UTIL, pix_util);
	chan_set_color(ch, plain_list);
	/* FIXME: memory leak */
	g_object_set_data(G_OBJECT(box), "title", strdup(title));
	g_object_set_data(G_OBJECT(box), "ch", ch);

	if (prefs.newtabstofront)
		chan_focus(ch);

	return ch;
}
#endif

#if 0

void
fe_clear_channel(window_t *sess)
{
	char tbuf[CHANLEN + 6];
	gtk_window_ui_t *gui = gtk_private(sess);

	if (gui->is_tab) {
		if (sess->waitchannel[0]) {
			if (prefs.truncchans > 2
			    && g_utf8_strlen(sess->waitchannel, -1) > prefs.truncchans) {
				/* truncate long channel names */
				tbuf[0] = '(';
				strcpy(tbuf + 1, sess->waitchannel);
				g_utf8_offset_to_pointer(tbuf, prefs.truncchans)[0] = 0;
				strcat(tbuf, "..)");
			} else {
				sprintf(tbuf, "(%s)", sess->waitchannel);
			}
		} else
			strcpy(tbuf, _("<none>"));
		chan_rename(sess->res->tab, tbuf, prefs.truncchans);
	}

	if (!gui->is_tab || sess == current_tab) {
		gtk_entry_set_text(GTK_ENTRY(gui->topic_entry), "");

		if (gui->op_xpm) {
			gtk_widget_destroy(gui->op_xpm);
			gui->op_xpm = 0;
		}
	} else {
	}
}

void
fe_dlgbuttons_update(window_t *sess)
{
	GtkWidget *box;
	gtk_window_ui_t *gui = gtk_private(sess);

	gtk_widget_destroy(gui->dialogbutton_box);

	gui->dialogbutton_box = box = gtk_hbox_new(0, 0);
	gtk_box_pack_start(GTK_BOX(gui->topic_bar), box, 0, 0, 0);
	gtk_box_reorder_child(GTK_BOX(gui->topic_bar), box, 3);
	mg_create_dialogbuttons(box);

	gtk_widget_show_all(box);

	if (current_tab && current_tab->type != SESS_DIALOG)
		gtk_widget_hide(current_tab->gui->dialogbutton_box);
}

/* fe_set_nick() nieciekawe */

#endif

void fe_set_away(session_t * serv) {
	window_t *w;

	for (w = windows; w; w = w->next) {
		if (w->session == serv) {
#if DARK
			if (!sess->gui->is_tab || sess == current_tab) {
				GTK_CHECK_MENU_ITEM(sess->gui->menu_item[MENU_ID_AWAY])->active =
					serv->is_away;
				/* gray out my nickname */
				mg_set_myself_away(sess->gui, serv->is_away);
			}
#endif
		}
	}
}

void fe_set_channel(window_t *sess) {
	if (gtk_private(sess)->tab != NULL)
		chan_rename(gtk_private(sess)->tab, (char *) gtk_window_target(sess), truncchans_config);
}

void mg_changui_new(window_t *sess, gtk_window_t *res, int tab, int focus) {
	int first_run = FALSE;
	gtk_window_t	*gtk_window;
	gtk_window_ui_t *gui;

	if (res)
		gtk_window = res;
	else	gtk_window = xmalloc(sizeof(gtk_window_t));

#if DARK
	struct User *user = NULL;

	if (!sess->server->front_session)
		sess->server->front_session = sess;

	if (!is_channel(sess->server, sess->channel))
		user = userlist_find_global(sess->server, sess->channel);
#endif
	if (!tab) {
		gui = xmalloc(sizeof(gtk_window_ui_t));
		gui->is_tab = FALSE;

		gtk_window->gui = gui;
		sess->private = gtk_window;
		mg_create_topwindow(sess);
		fe_set_title(sess);
#if DARK
		if (user && user->hostname)
			set_topic(sess, user->hostname);
#endif
		return;
	}

	if (mg_gui == NULL) {
		first_run = TRUE;
		gui = &static_mg_gui;
		memset(gui, 0, sizeof(gtk_window_ui_t));
		gui->is_tab = TRUE;
		gtk_window->gui = gui;
		sess->private = gtk_window;
		mg_create_tabwindow(sess);
		mg_gui = gui;
		parent_window = gui->window;
	} else {
		gtk_window->gui = gui = mg_gui;
		
		sess->private = gtk_window;
		gui->is_tab = TRUE;
	}
#if 0
	if (user && user->hostname)
		set_topic(sess, user->hostname);
#endif
	mg_add_chan(sess);

	if (first_run || (newtabstofront_config == FOCUS_NEW_ONLY_ASKED && focus)
	    || newtabstofront_config == FOCUS_NEW_ALL)
		chan_focus(gtk_private(sess)->tab);
}

#if 0

GtkWidget *
mg_create_generic_tab(char *name, char *title, int force_toplevel,
		      int link_buttons,
		      void *close_callback, void *userdata,
		      int width, int height, GtkWidget **vbox_ret, void *family)
{
	GtkWidget *vbox, *win;

	if (tab_pos_config == POS_HIDDEN && prefs.windows_as_tabs)
		prefs.windows_as_tabs = 0;

	if (force_toplevel || !prefs.windows_as_tabs) {
		win = gtkutil_window_new(title, name, width, height, 3);
		vbox = gtk_vbox_new(0, 0);
		*vbox_ret = vbox;
		gtk_container_add(GTK_CONTAINER(win), vbox);
		gtk_widget_show(vbox);
		if (close_callback)
			g_signal_connect(G_OBJECT(win), "destroy",
					 G_CALLBACK(close_callback), userdata);
		return win;
	}

	vbox = gtk_vbox_new(0, 2);
	g_object_set_data(G_OBJECT(vbox), "w", GINT_TO_POINTER(width));
	g_object_set_data(G_OBJECT(vbox), "h", GINT_TO_POINTER(height));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
	*vbox_ret = vbox;

	if (close_callback)
		g_signal_connect(G_OBJECT(vbox), "destroy", G_CALLBACK(close_callback), userdata);

	mg_add_generic_tab(name, title, family, vbox);

/*	if (link_buttons)
	{
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, 0, 0, 0);
		mg_create_link_buttons (hbox, ch);
		gtk_widget_show (hbox);
	}*/

	return vbox;
}

void
mg_move_tab(window_t *sess, int delta)
{
	if (sess->gui->is_tab)
		chan_move(sess->res->tab, delta);
}

void
mg_set_title(GtkWidget *vbox, char *title)
{				/* for non-irc tab/window only */
	char *old;

	old = g_object_get_data(G_OBJECT(vbox), "title");
	if (old) {
		g_object_set_data(G_OBJECT(vbox), "title", xstrdup(title));
		free(old);
	} else {
		gtk_window_set_title(GTK_WINDOW(vbox), title);
	}
}

#endif

/* called when a session is being killed */

void fe_close_window(window_t *sess) {
	printf("fe_close_window(%p)\n", sess);

	if (gtk_private_ui(sess)->is_tab)
		mg_tab_close(sess);
	else
		gtk_widget_destroy(gtk_private_ui(sess)->window);

	if (gtk_private_ui(sess) != &static_mg_gui)
		xfree(gtk_private_ui(sess));		/* free gui, if not static */

	xfree(sess->private);				/* free window strukt */

	sess->private = NULL;
}

/* NOT COPIED:
 *
 * is_child_of()  mg_handle_drop() mg_drag_begin_cb() mg_drag_end_cb() mg_drag_drop_cb()
 * mg_drag_motion_cb() 
 * mg_dialog_dnd_drop()
 * mg_dnd_drop_file()
 */

/* mg_count_dccs() mg_count_networks() */

/* inne okienka, ,,generic'':
 *	mg_link_gentab() wywolywany z mg_detach_tab_cb() 
 *	mg_close_gen() wywolywany z mg_xbutton_cb() 
 */

