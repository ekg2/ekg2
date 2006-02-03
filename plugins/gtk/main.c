/*
 *  (C) Copyright 2004 Artur Gajda
 *  		  2004, 2006 Jakub 'darkjames' Zawadzki <darkjames@darkjames.ath.cx>
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

#include <ekg2-config.h>

#include <gtk/gtk.h>

#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/sessions.h>
#include <ekg/userlist.h>
#include <ekg/windows.h>

/* 
 * a lot of code was gathered from `very good` program - gRiv (acronym comes from GTK RivChat)
 * main author and coder of program and gui was Artur Gajda so why he is in credits..
 * i think he won't be angry of it ;> 
 */

GtkTextTag *ekg2_tags[8];
GtkTextTag *ekg2_tag_bold;

GtkWidget *view;
GtkTreeStore *list_store;		// userlista.
	
PLUGIN_DEFINE(gtk, PLUGIN_UI, NULL);

extern void ekg_loop();
int ui_quit;	// czy zamykamy ui..

enum {	COLUMN_NICK, N_COLUMNS };

/* sprawdzenie czy mozemy zamknac okno */
gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
// TRUE - zostawiamy okno.
	return FALSE;
}

/* niszczecie okienka */
void destroy(GtkWidget *widget, gpointer data) {
	gtk_main_quit ();
	ui_quit = 1;
}

/* <ENTER> editboxa */
gint on_enter(GtkWidget *widget, gpointer data) {
	const gchar *txt;
	txt = gtk_entry_get_text(GTK_ENTRY(widget));

	command_exec(window_current->target, window_current->session, txt, 0);

	gtk_entry_set_text(GTK_ENTRY(widget), "");
	return 0;
}
/* klikniecie rowa userlisty */ 
gint on_list_select(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, gpointer user_data) {
	GtkTreeIter iter;
	gchar *nick;

	gtk_tree_model_get_iter (GTK_TREE_MODEL(list_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_NICK, &nick, -1);
/* TODO, open query.. but first we need to implement windows !!! ;p */
	printf("Selected: %s\n", nick);
	return 0;
}

int gtk_loop() {
	ekg_loop();
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	return (ui_quit == 0);
}

int gtk_create() {
	GtkWidget *win, *edit1, *tree, *status_bar;
	GtkWidget *vbox, *hbox, *sw;
	GtkTextBuffer *buffer;
	GtkTextTagTable *table;
	GtkCellRenderer *renderer;
	int i;
	
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (win), "ekg2 p0wer!");
  
	g_signal_connect (G_OBJECT (win), "delete_event", G_CALLBACK (delete_event), NULL);
	g_signal_connect (G_OBJECT (win), "destroy", G_CALLBACK (destroy), NULL);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (win), hbox);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	/* lista - przwewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (hbox), sw, TRUE, TRUE, 0);
	/* lista */
	list_store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), GTK_SELECTION_MULTIPLE);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "Nick", renderer, "text", COLUMN_NICK, NULL);
//	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "Uid", renderer, "text", COLUMN_UID, NULL);
	gtk_container_add (GTK_CONTAINER (sw), tree);
	g_signal_connect (G_OBJECT (tree), "row-activated", G_CALLBACK (on_list_select), NULL);
	gtk_widget_set_size_request(tree, 165, 365);
	
#if 0
	/* popup menu */
	menu = gtk_menu_new ();
	mi_priv = gtk_menu_item_new_with_label ("Query");
	mi_info = gtk_menu_item_new_with_label ("Info");
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi_priv);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi_info);	
	g_signal_connect_swapped (G_OBJECT(mi_priv),"activate",G_CALLBACK (on_mi_priv), NULL);
	g_signal_connect_swapped (G_OBJECT(mi_info),"activate",G_CALLBACK (on_mi_info), NULL);
	
	g_signal_connect_swapped (tree, "button_press_event",G_CALLBACK (popup_handler), menu);
#endif
	/* tekst - przewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	/* tekst */
	view = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (view), GTK_WRAP_WORD);

	gtk_widget_set_size_request(view, 495, 365);
	gtk_container_add (GTK_CONTAINER (sw), view);

	/* atrybutu tekstu */
	table = gtk_text_buffer_get_tag_table (buffer);
	for (i=0; i < 8; i++) {
		gtk_text_tag_table_add(table, ekg2_tags[i]); /* glowne kolorki */
	}
	gtk_text_tag_table_add(table, ekg2_tag_bold);

	/* edit1 */
	edit1 = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX(vbox), edit1, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (edit1),"activate",G_CALLBACK (on_enter), NULL);
//	g_signal_connect (G_OBJECT (edit1),"key-press-event",G_CALLBACK (on_key_press), NULL);

	/* statusbar */
	status_bar = gtk_statusbar_new ();
	gtk_box_pack_start (GTK_BOX (vbox), status_bar, TRUE, TRUE, 0);

	gtk_widget_grab_focus(edit1);
	gtk_widget_show_all (win);

	return 0;
}


void ekg_gtk_window_new(window_t *w) {
	char *name = window_target(w);
// TODO! to co ? robimy tak jak w xchacie? tryb detach / attach i jak cos to sie pojawia w tabach jak nie to w osobnych oknach?
// 	ok, let's try. 

	printf("WINDOW_NEW(): [%d,%s]\n", w->id, name);
}

void gtk_contacts_update(window_t *w) {
	/* na razie zeby bylo nothing more... wyswietla ladnie kolorujac (*TODO* ;p) liste userow z aktualnej sesji */
	list_t l;
	
 	gtk_tree_store_clear(list_store);

	if (!session_current)
		return;

	for (l=session_current->userlist; l; l = l->next) {
		GtkTreeIter liter;
		userlist_t *u = l->data;
		gtk_tree_store_append (list_store, &liter, NULL);
		gtk_tree_store_set (list_store, &liter, COLUMN_NICK, u->nickname ? u->nickname : u->uid, -1);
	}
}

void gtk_process_str(GtkTextBuffer *buffer, char *str, short int *attr) {
	GtkTextIter iter;
	int i;
/* i know ze tak nie moze wygladac, zrobione po prostu aby dzialalo. */
	for (i=0; i < xstrlen(str); i++) {
		GtkTextTag *tags[2] = {NULL, NULL};
		short att = attr[i];

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);

		if (!(att & 128))	tags[0] = ekg2_tags[att & 7];
		if (att & 64)		tags[1] = ekg2_tag_bold;

		gtk_text_buffer_insert_with_tags(buffer, &iter, str+i, 1, 
				tags[0] ? tags[0] : tags[1], 
				tags[0] ? tags[1] : NULL,
				NULL);
	}
}

QUERY(gtk_ui_window_print) {
	window_t *w     = *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));

	if (w->id) { /* debug only to STDOUT. */
		GtkTextBuffer *buffer;
		GtkTextMark* mark;
		GtkTextIter iter;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

		if (config_timestamp && config_timestamp_show && xstrcmp(config_timestamp, "")) {
			char *tmp = format_string(config_timestamp);
			char *ts  = saprintf("%s ", timestamp(tmp));

			gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
			gtk_text_buffer_insert_with_tags(buffer, &iter, ts, -1, ekg2_tags[0], ekg2_bold, NULL);
			
			xfree(tmp);
			xfree(ts);
		}
		

		gtk_process_str(buffer, line->str, line->attr);

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
		gtk_text_buffer_insert_with_tags(buffer, &iter, "\n", -1, NULL);

		/* scroll to end */
		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
		mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, 1);
		gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(view), mark, 0.0, 0, 0.0, 1.0);
		gtk_text_buffer_delete_mark (buffer, mark);
	}
	return 0;
}

QUERY(gtk_ui_window_new) {
	window_t *w = *(va_arg(ap, window_t **));
	ekg_gtk_window_new(w);
	return 0;
}

QUERY(ekg2_gtk_loop) {
	gtk_contacts_update(NULL);
	while (gtk_loop());
	return -1;
}

QUERY(ekg2_gtk_pending) {
	if (gtk_events_pending())
		return -1;
	return -1;
}

QUERY(gtk_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);
	*tmp = !ui_quit;

	return 0;
}

QUERY(gtk_userlist_changed) {
	char **p1 = va_arg(ap, char**);
	char **p2 = va_arg(ap, char**);
/* jak jest jakies okno z *p1 to wtedy zamieniamy nazwe na *p2 */
	gtk_contacts_update(NULL);
	return 0;
}

int gtk_tags_init() {
	GtkTextTag *tmp = NULL;
	int i = 0;
#define ekg2_create_tag(x) \
	tmp = gtk_text_tag_new("FG_" #x); \
	g_object_set(tmp, "foreground", #x, NULL); \
	ekg2_tags[i++] = tmp;
	
	ekg2_create_tag(BLACK);
	ekg2_create_tag(RED);
	ekg2_create_tag(GREEN);
	ekg2_create_tag(YELLOW);	// XXX: /brown
	ekg2_create_tag(BLUE); 
	ekg2_create_tag(MAGENTA);	// XXX ?
	ekg2_create_tag(CYAN);
	ekg2_create_tag(WHITE);
	
	ekg2_tag_bold = tmp = gtk_text_tag_new("BOLD");
	g_object_set(tmp, "weight", PANGO_WEIGHT_BOLD, NULL);

	tmp = gtk_text_tag_new("ITALICS");
	g_object_set(tmp, "style", PANGO_STYLE_ITALIC, NULL);
	
//	gtk_text_buffer_create_tag(buffer, "FG_NAVY", "foreground", "navy", NULL);
//	gtk_text_buffer_create_tag(buffer, "FG_DARKGREEN", "foreground", "darkgreen", NULL);
//	gtk_text_buffer_create_tag(buffer, "FG_LIGHTGREEN", "foreground", "green", NULL);
	return 0;
	
}

int gtk_plugin_init(int prio) {
	list_t l;
	
	plugin_register(&gtk_plugin, prio);

	query_connect(&gtk_plugin, "ui-window-new", gtk_ui_window_new, NULL);
	query_connect(&gtk_plugin, "ui-window-print", gtk_ui_window_print, NULL);
	query_connect(&gtk_plugin, "ui-is-initialized", gtk_ui_is_initialized, NULL);
/* userlist */
	query_connect(&gtk_plugin, "userlist-changed", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-added", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-removed", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-renamed", gtk_userlist_changed, NULL);
/* sesja */
#if 0
	query_connect(&gtk_plugin, "session-added", gtk_statusbar_query, NULL);
	query_connect(&gtk_plugin, "session-removed", gtk_statusbar_query, NULL);
	query_connect(&gtk_plugin, "session-changed", gtk_contacts_changed, NULL);
#endif			

/* w/g developerow na !ekg2 `haki`  ;) niech im bedzie ... ;p */
	query_connect(&gtk_plugin, "ui-loop", ekg2_gtk_loop, NULL);
	query_connect(&gtk_plugin, "ui-pending", ekg2_gtk_pending, NULL);
	
	gtk_init(0, NULL);
	gtk_tags_init();

	gtk_create();
	
	for (l = windows; l; l = l->next) {
		ekg_gtk_window_new(l->data);	
	}
	return 0;
}

static int gtk_plugin_destroy() {
	plugin_unregister(&gtk_plugin);
	return 0;
}

