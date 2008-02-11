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

#define GTK_DISABLE_DEPRECATED

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkliststore.h>
#include <gdk/gdkkeysyms.h>

#include <ekg/userlist.h>
#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "main.h"
#include "palette.h"
#include "userlistgui.h"

enum {
	USERLIST_PIXMAP = 0,
	USERLIST_NICKNAME,
	USERLIST_DESCRIPTION,
	USERLIST_USER,
	USERLIST_COLOR,

	USERLIST_COLS
};


#define show_descr_in_userlist_config 1

#if 0
#include "fe-gtk.h"


#include "../common/xchat.h"
#include "../common/util.h"
#include "../common/userlist.h"
#include "../common/modes.h"
#include "../common/notify.h"
#include "../common/xchatc.h"
#include "gtkutil.h"
#include "palette.h"
#include "maingui.h"
#include "menu.h"
#include "userlistgui.h"

#endif

/* extern */
const char *gtk_session_target(session_t *sess);

void fe_userlist_numbers(window_t *sess) {
	if (sess == window_current || !gtk_private_ui(sess)->is_tab) {
#if 0
		char tbuf[256];
		if (sess->total) {
			snprintf(tbuf, sizeof(tbuf), _("%d ops, %d total"), sess->ops,
				 sess->total);
			tbuf[sizeof(tbuf) - 1] = 0;
			gtk_label_set_text(GTK_LABEL(sess->gui->namelistinfo), tbuf);
		} else {
			gtk_label_set_text(GTK_LABEL(sess->gui->namelistinfo), NULL);
		}

		if (sess->type == SESS_CHANNEL && prefs.gui_tweaks & 1)
			fe_set_title(sess);
#endif
		gtk_label_set_text(GTK_LABEL(gtk_private_ui(sess)->namelistinfo), gtk_session_target(sess->session));
	}
}

#if 0

static void scroll_to_iter(GtkTreeIter * iter, GtkTreeView * treeview, GtkTreeModel * model)
{
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);
	if (path) {
		gtk_tree_view_scroll_to_cell(treeview, path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}
}

/* select a row in the userlist by nick-name */

void userlist_select(session *sess, char *name)
{
	GtkTreeIter iter;
	GtkTreeView *treeview = GTK_TREE_VIEW(sess->gui->user_tree);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
	struct User *row_user;

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, 3, &row_user, -1);
			if (sess->server->p_cmp(row_user->nick, name) == 0) {
				if (gtk_tree_selection_iter_is_selected(selection, &iter))
					gtk_tree_selection_unselect_iter(selection, &iter);
				else
					gtk_tree_selection_select_iter(selection, &iter);

				/* and make sure it's visible */
				scroll_to_iter(&iter, treeview, model);
				return;
			}
		}
		while (gtk_tree_model_iter_next(model, &iter));
	}
}

#endif

char **userlist_selection_list(GtkWidget *widget, int *num_ret) {
	GtkTreeIter iter;
	GtkTreeView *treeview = (GtkTreeView *) widget;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	int i, num_sel;
	char **nicks;

	*num_ret = 0;
	/* first, count the number of selections */
	num_sel = 0;
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			if (gtk_tree_selection_iter_is_selected(selection, &iter))
				num_sel++;
		}
		while (gtk_tree_model_iter_next(model, &iter));
	}

	if (num_sel < 1)
		return NULL;

	nicks = xmalloc(sizeof(char *) * (num_sel + 1));

	i = 0;
	gtk_tree_model_get_iter_first(model, &iter);
	do {
		if (gtk_tree_selection_iter_is_selected(selection, &iter)) {
			gtk_tree_model_get(model, &iter, 1, &nicks[i], -1);
			i++;
			nicks[i] = NULL;
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	*num_ret = i;
	return nicks;
}

#if 0

void fe_userlist_set_selected(struct session *sess)
{
	GtkListStore *store = sess->res->user_model;
	GtkTreeSelection *selection =
		gtk_tree_view_get_selection(GTK_TREE_VIEW(sess->gui->user_tree));
	GtkTreeIter iter;
	struct User *user;

	/* if it's not front-most tab it doesn't own the GtkTreeView! */
	if (store != (GtkListStore *) gtk_tree_view_get_model(GTK_TREE_VIEW(sess->gui->user_tree)))
		return;

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 3, &user, -1);

			if (gtk_tree_selection_iter_is_selected(selection, &iter))
				user->selected = 1;
			else
				user->selected = 0;

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
	}
}

static GtkTreeIter *find_row(GtkTreeView * treeview, GtkTreeModel * model, struct User *user,
			     int *selected)
{
	static GtkTreeIter iter;
	struct User *row_user;

	*selected = FALSE;
	if (gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter, 3, &row_user, -1);
			if (row_user == user) {
				if (gtk_tree_view_get_model(treeview) == model) {
					if (gtk_tree_selection_iter_is_selected
					    (gtk_tree_view_get_selection(treeview), &iter))
						*selected = TRUE;
				}
				return &iter;
			}
		}
		while (gtk_tree_model_iter_next(model, &iter));
	}

	return NULL;
}

#endif

void userlist_set_value(GtkWidget *treeview, gfloat val)
{
	gtk_adjustment_set_value(gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(treeview)), val);
}

gfloat userlist_get_value(GtkWidget *treeview)
{
	return gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(treeview))->value;
}

static gint gtk_userlist_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer userdata) {
	GdkPixbuf *a1, *b1;

	gint sortcol = GPOINTER_TO_INT(userdata);

	if (sortcol != USERLIST_PIXMAP) {
		printf("gtk_userlist_sort_func() IE\n");
		return 0;
	}

/* XXX, sequence should match sequence in contacts_options */
	gtk_tree_model_get(model, a, USERLIST_PIXMAP, &a1, -1);
	gtk_tree_model_get(model, b, USERLIST_PIXMAP, &b1, -1);

/* yeah, i know, i'm lazy */
	if (a1 < b1)
		return -1;
	else if (a1 > b1)
		return 1;
	else	return 0;
}


void fe_userlist_insert(window_t *sess, userlist_t *u, GdkPixbuf **pixmaps)
{
	GtkTreeModel *model = gtk_private(sess)->user_model;
	GdkPixbuf *pix = NULL;	/* get_user_icon (sess->server, newuser); */
	GtkTreeIter iter;
	int do_away = TRUE;

	int sel = 0;

	if (pixmaps) {
		const char *str;
		
		switch (u->status) {
			case EKG_STATUS_NA:
				pix = pixmaps[PIXBUF_NOTAVAIL];
				break;
			case EKG_STATUS_INVISIBLE:
				pix = pixmaps[PIXBUF_INVISIBLE];
				break;
			case EKG_STATUS_XA:
				pix = pixmaps[PIXBUF_XA];
				break;
			case EKG_STATUS_DND:
				pix = pixmaps[PIXBUF_DND];
				break;
			case EKG_STATUS_AWAY:
				pix = pixmaps[PIXBUF_AWAY];
				break;
			case EKG_STATUS_AVAIL:
				pix = pixmaps[PIXBUF_AVAIL];
				break;
			case EKG_STATUS_FFC:
				pix = pixmaps[PIXBUF_FFC];
				break;
			case EKG_STATUS_ERROR:
				pix = pixmaps[PIXBUF_ERROR];
				break;
			default: /* + EKG_STATUS_UNKNOWN */
				pix = pixmaps[PIXBUF_UNKNOWN];
		}
	}

#if 0
	if (prefs.away_size_max < 1 || !prefs.away_track)
		do_away = FALSE;

#endif
	gtk_list_store_insert_with_values(GTK_LIST_STORE(model), &iter, -1,
					  USERLIST_PIXMAP, pix,
					  USERLIST_NICKNAME, u->nickname,
					  /* XXX, u->uid */
					  USERLIST_DESCRIPTION, u->descr,
					  USERLIST_USER, u,
//					  USERLIST_COLOR, /* (do_away) */ FALSE,  ? (newuser->away ? &colors[COL_AWAY] : NULL) : */ (NULL),
					  -1);

#if DARK
	/* is it me? */
	if (newuser->me && sess->gui->nick_box) {
		if (!sess->gui->is_tab || sess == current_tab)
			mg_set_access_icon(sess->gui, pix, sess->server->is_away);
	}
#if 0				/* not mine IF !! */
	if (prefs.hilitenotify && notify_isnotify(sess, newuser->nick)) {
		gtk_clist_set_foreground((GtkCList *) sess->gui->user_clist, row,
					 &colors[prefs.nu_color]);
	}
#endif

#endif
	/* is it the front-most tab? */
	if (sel && gtk_tree_view_get_model(GTK_TREE_VIEW(gtk_private_ui(sess)->user_tree)) == model) {
		gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_private_ui(sess)->user_tree)), &iter);
	}
}

void fe_userlist_clear(window_t *sess)
{
	gtk_list_store_clear(gtk_private(sess)->user_model);
}

void *userlist_create_model(void)
{
	GtkTreeSortable *sortable;

	void *liststore;

	liststore = gtk_list_store_new(USERLIST_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR);

	sortable = GTK_TREE_SORTABLE(liststore);
	
	gtk_tree_sortable_set_sort_func(sortable, USERLIST_PIXMAP, gtk_userlist_sort_func, GINT_TO_POINTER(USERLIST_PIXMAP), NULL);

/* initial sort */
	gtk_tree_sortable_set_sort_column_id(sortable, USERLIST_PIXMAP, GTK_SORT_ASCENDING);

	return liststore;
}

static void userlist_add_columns(GtkTreeView * treeview)
{
	GtkCellRenderer *renderer;

	/* icon column */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, NULL, renderer, "pixbuf", USERLIST_PIXMAP, NULL);

	/* nick column */
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(renderer), 1);
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, NULL, renderer, "text", USERLIST_NICKNAME, "foreground-gdk", USERLIST_COLOR, NULL);

	/* description column (?) */
	if (show_descr_in_userlist_config) {
		renderer = gtk_cell_renderer_text_new();
		gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(renderer), 1);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, NULL, renderer, "text", 2, NULL);
	}
}

static gint userlist_click_cb(GtkWidget *widget, GdkEventButton * event, gpointer userdata) {
	char **nicks;
	int i;
	GtkTreeSelection *sel;
	GtkTreePath *path;

	if (!event)
		return FALSE;

	if (!(event->state & GDK_CONTROL_MASK) && event->type == GDK_2BUTTON_PRESS /* && prefs.doubleclickuser[0] */) {
		nicks = userlist_selection_list(widget, &i);
		if (nicks) {
/*			nick_command_parse(current_sess, prefs.doubleclickuser, nicks[0], nicks[0]); */
			command_exec_format(NULL, NULL, 0, ("/query \"%s\""), nicks[0]);

			while (i) {
				i--;
				g_free(nicks[i]);
			}
			free(nicks);
		}
		return TRUE;
	}

	if (event->button == 3) {
		/* do we have a multi-selection? */
		nicks = userlist_selection_list(widget, &i);
		if (nicks && i > 1) {
			menu_nickmenu(window_current, event, nicks[0], i);
			while (i) {
				i--;
				g_free(nicks[i]);
			}
			free(nicks);
			return TRUE;
		}
		if (nicks) {
			g_free(nicks[0]);
			free(nicks);
		}

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
						  event->x, event->y, &path, 0, 0, 0)) {
			gtk_tree_selection_unselect_all(sel);
			gtk_tree_selection_select_path(sel, path);
			gtk_tree_path_free(path);
			nicks = userlist_selection_list(widget, &i);
			if (nicks) {
				menu_nickmenu(window_current, event, nicks[0], i);
				while (i) {
					i--;
					g_free(nicks[i]);
				}
				free(nicks);
			}
		} else {
			gtk_tree_selection_unselect_all(sel);
		}

		return TRUE;
	}
	return FALSE;
}

static gboolean userlist_key_cb(GtkWidget *wid, GdkEventKey * evt, gpointer userdata)
{
#if 0
	if (evt->keyval >= GDK_asterisk && evt->keyval <= GDK_z) {
		/* dirty trick to avoid auto-selection */
		SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, FALSE);
		gtk_widget_grab_focus(current_sess->gui->input_box);
		SPELL_ENTRY_SET_EDITABLE(current_sess->gui->input_box, TRUE);
		gtk_widget_event(current_sess->gui->input_box, (GdkEvent *) evt);
		return TRUE;
	}
#endif
	return FALSE;
}

GtkWidget *userlist_create(GtkWidget *box)
{
	GtkWidget *sw, *treeview;

	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
			show_descr_in_userlist_config ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER, 
			GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(box), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	treeview = gtk_tree_view_new();
	gtk_widget_set_name(treeview, "xchat-userlist");
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection
				    (GTK_TREE_VIEW(treeview)), GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(treeview), "button_press_event",
			 G_CALLBACK(userlist_click_cb), 0);
	g_signal_connect(G_OBJECT(treeview), "key_press_event", G_CALLBACK(userlist_key_cb), 0);

#warning "xchat->ekg2: drag & drop"

	userlist_add_columns(GTK_TREE_VIEW(treeview));

	gtk_container_add(GTK_CONTAINER(sw), treeview);
	gtk_widget_show(treeview);

	return treeview;
}

void userlist_show(window_t *sess)
{
	gtk_tree_view_set_model(GTK_TREE_VIEW(gtk_private_ui(sess)->user_tree), gtk_private(sess)->user_model);
}

#if 0

void fe_uselect(session *sess, char *word[], int do_clear, int scroll_to)
{
	int thisname;
	char *name;
	GtkTreeIter iter;
	GtkTreeView *treeview = GTK_TREE_VIEW(sess->gui->user_tree);
	GtkTreeModel *model = gtk_tree_view_get_model(treeview);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
	struct User *row_user;

	if (gtk_tree_model_get_iter_first(model, &iter)) {
		if (do_clear)
			gtk_tree_selection_unselect_all(selection);

		do {
			if (*word[0]) {
				gtk_tree_model_get(model, &iter, 3, &row_user, -1);
				thisname = 0;
				while (*(name = word[thisname++])) {
					if (sess->server->p_cmp(row_user->nick, name) == 0) {
						gtk_tree_selection_select_iter(selection, &iter);
						if (scroll_to)
							scroll_to_iter(&iter, treeview, model);
						break;
					}
				}
			}

		}
		while (gtk_tree_model_iter_next(model, &iter));
	}
}

#endif
