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

#include <stdio.h>
#include <stdlib.h>

/* 
 * a lot of code was gathered from `very good` program - gRiv (acronym comes from GTK RivChat)
 * main author and coder of program and gui was Artur Gajda so why he is in credits..
 * i think he won't be angry of it ;> 
 */

typedef struct {
	GtkWidget *view;
	GtkTextTag *ekg2_tags[8];
	GtkTextTag *ekg2_tag_bold;
} gtk_window_t;


GtkTreeStore *list_store;		// userlista.
GtkWidget *notebook;			// zarzadzanie okienkami.
	
PLUGIN_DEFINE(gtk, PLUGIN_UI, NULL);

extern void ekg_loop();
int ui_quit;	// czy zamykamy ui..

enum {	COLUMN_STATUS = 0, 
	COLUMN_NICK,
	COLUMN_SESSION, 
	N_COLUMNS };

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
	gchar *nick, *session;
	session_t *s = session_find(session);

	gtk_tree_model_get_iter (GTK_TREE_MODEL(list_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_NICK, &nick, -1);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_SESSION, &session, -1);
//	printf("[USERLIST_CLICK] Target: %s session: %s\n", nick, session);
	
	command_exec_format((window_current->session == s) ? window_current->target : nick /* hmmm.. TODO */, s, 0, "/query \"%s\"", nick);
	return 0;
}

/* zmiana strony - zmiana okna */
gint on_switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer user_data) {
	if (in_autoexec) 
		return 0;
//	printf("[SWITCH_PAGE] page: %d\n", page_num);
	window_switch(page_num); /* !!! to niekoniecznie musi byc numer okna, XXX */
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
	GtkCellRenderer *renderer;
	
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (win), "ekg2 p0wer!");
  
	g_signal_connect (G_OBJECT (win), "delete_event", G_CALLBACK (delete_event), NULL);
	g_signal_connect (G_OBJECT (win), "destroy", G_CALLBACK (destroy), NULL);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (win), hbox);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	/* notebook */
	notebook = gtk_notebook_new ();
//	gtk_notebook_set_show_border (GTK_NOTEBOOK(notebook), FALSE);
	gtk_box_pack_start (GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	gtk_widget_set_size_request(notebook, 505, 375);
	g_signal_connect (G_OBJECT(notebook), "switch-page", G_CALLBACK(on_switch_page), NULL);
//	gtk_notebook_set_tab_pos (notebook, 4);
	
	/* lista - przwewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);
	/* lista */
	list_store = gtk_tree_store_new (N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, NULL);
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), GTK_SELECTION_MULTIPLE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "", renderer, "pixbuf", COLUMN_STATUS, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "Nick", renderer, "text", COLUMN_NICK, NULL);
//	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "Uid", renderer, "text", COLUMN_UID, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "Session", renderer, "text", COLUMN_SESSION, NULL);
	gtk_tree_view_column_set_visible( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_SESSION), FALSE);
	
	gtk_container_add (GTK_CONTAINER (sw), tree);
	g_signal_connect (G_OBJECT (tree), "row-activated", G_CALLBACK (on_list_select), NULL);
	gtk_widget_set_size_request(tree, 165, 365);
	
	/* edit */
	edit1 = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX(vbox), edit1, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (edit1),"activate",G_CALLBACK (on_enter), NULL);
//	g_signal_connect (G_OBJECT (edit1),"key-press-event",G_CALLBACK (on_key_press), NULL);

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
#if 0
	/* statusbar */
	status_bar = gtk_statusbar_new ();
	gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(status_bar), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), status_bar, FALSE, FALSE, 0);
#endif
	gtk_widget_grab_focus(edit1);
	gtk_widget_show_all (win);

	return 0;
}

void ekg_gtk_window_new(window_t *w) {
	GtkWidget *sw, *view;
	GtkTextBuffer *buffer;
	GtkTextTagTable *table;
	char *name = window_target(w);
	int i;
	
	gtk_window_t *n = xmalloc(sizeof(gtk_window_t));
	w->private = n;

	printf("WINDOW_NEW(): [%d,%s]\n", w->id, name);

	/* tekst - przewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), sw,  gtk_label_new (name), w->id);
	/* tekst */
	view = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (view), GTK_WRAP_WORD);

	gtk_container_add (GTK_CONTAINER (sw), view);
	
	/* atrybutu tekstu */
	{
		GtkTextTag *tmp = NULL;
		int i = 0;
	#define ekg2_create_tag(x) \
		tmp = gtk_text_tag_new("FG_" #x); \
		g_object_set(tmp, "foreground", #x, NULL); \
		n->ekg2_tags[i++] = tmp;
	
		ekg2_create_tag(BLACK);	ekg2_create_tag(RED); ekg2_create_tag(GREEN);
		ekg2_create_tag(YELLOW);ekg2_create_tag(BLUE); ekg2_create_tag(MAGENTA);
		ekg2_create_tag(CYAN);	ekg2_create_tag(WHITE);
	
		n->ekg2_tag_bold = tmp = gtk_text_tag_new("BOLD");
		g_object_set(tmp, "weight", PANGO_WEIGHT_BOLD, NULL);

		tmp = gtk_text_tag_new("ITALICS");
		g_object_set(tmp, "style", PANGO_STYLE_ITALIC, NULL);
	
//		gtk_text_buffer_create_tag(buffer, "FG_NAVY", "foreground", "navy", NULL);
//		gtk_text_buffer_create_tag(buffer, "FG_DARKGREEN", "foreground", "darkgreen", NULL);
//		gtk_text_buffer_create_tag(buffer, "FG_LIGHTGREEN", "foreground", "green", NULL);
	}

	table = gtk_text_buffer_get_tag_table (buffer);
	for (i=0; i < 8; i++) { /* glowne kolorki */
		gtk_text_tag_table_add(table, n->ekg2_tags[i] );
	}
	gtk_text_tag_table_add(table, n->ekg2_tag_bold);

	
	n->view = view;
	gtk_widget_show_all (notebook);
}

/* TODO, zrobic ladniejsze.. */
void gtk_contacts_add(session_t *s, userlist_t *u, GtkTreeIter *iter) 
{
	GtkTreeIter child_iter;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;
	int isparent = (s && !u && iter);
	
	GtkTreeIter *tmp = (isparent) ? iter : &child_iter; /* jesli to jest parent - nie ma pointera do userlist_t - sesja, nazwa, cokolwiek... to wtedy zapisuje itera w iter */
	
	char *status_filename = saprintf("%s/plugins/gtk/%s.png", DATADIR, (u) ? u->status : s->status);

	if (!s && !u) {
		xfree(status_filename);
		// INTERNAL ERROR.
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (status_filename, &error);

	printf("CONTACTS_ADD() filename=%s; pixbuf=%x iter=%x;\n", status_filename, (int) pixbuf, (int) iter);
	gtk_tree_store_append (list_store, tmp,	(!isparent) ? iter : NULL);
	
	gtk_tree_store_set (list_store, tmp,
			COLUMN_STATUS, pixbuf, 
			COLUMN_NICK, (isparent) ? (s->alias ? s->alias : s->uid) :	/* sesja  - parent  */
				     (u->nickname ? u->nickname : u->uid),		/* useria - dziecko */
			COLUMN_SESSION, (s) ? s->uid : "??",
			-1);

	xfree(status_filename);
	return;
}

void gtk_contacts_update(window_t *w) {
	list_t l;
	
 	gtk_tree_store_clear(list_store);

	if (!sessions) 
		return;
	for (l=sessions; l; l = l->next) {
		GtkTreeIter iter; 
		session_t *s = l->data;
		list_t l;
/* if( s == session_current) continue; ? */
		gtk_contacts_add(s, NULL, &iter);
		for (l=s->userlist; l; l = l->next)
			gtk_contacts_add(s, l->data, &iter);
	}
	if (window_current /* always point to smth ? */ && window_current->userlist) {
		for (l=window_current->userlist; l; l = l->next)
			gtk_contacts_add(window_current->session, l->data, NULL);
	}

	if (session_current) {
		for (l=session_current->userlist; l; l = l->next)
			gtk_contacts_add(session_current, l->data, NULL);
	}
}

void gtk_process_str(window_t *w, GtkTextBuffer *buffer, char *str, short int *attr) {
	GtkTextIter iter;
	int i;
	gtk_window_t *n = w->private;
/* i know ze tak nie moze wygladac, zrobione po prostu aby dzialalo. */
	for (i=0; i < xstrlen(str); i++) {
		GtkTextTag *tags[2] = {NULL, NULL};
		short att = attr[i];

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);

		if (!(att & 128))	tags[0] = n->ekg2_tags[att & 7];
		if (att & 64)		tags[1] = n->ekg2_tag_bold;

		gtk_text_buffer_insert_with_tags(buffer, &iter, str+i, 1, 
				tags[0] ? tags[0] : tags[1], 
				tags[0] ? tags[1] : NULL,
				NULL);
	}
}

QUERY(gtk_ui_window_print) {
	window_t *w     = *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));
	gtk_window_t *n = w->private;

	GtkTextBuffer *buffer;
	GtkTextMark* mark;
	GtkTextIter iter;

	if (!n)
		return 1;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (n->view));

	if (config_timestamp && config_timestamp_show && xstrcmp(config_timestamp, "")) {
		char *tmp = format_string(config_timestamp);
		char *ts  = saprintf("%s ", timestamp(tmp));

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
		gtk_text_buffer_insert_with_tags(buffer, &iter, ts, -1, n->ekg2_tags[0], n->ekg2_tag_bold, NULL);
		
		xfree(tmp);
		xfree(ts);
	}
	gtk_process_str(w, buffer, line->str, line->attr);

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
	gtk_text_buffer_insert_with_tags(buffer, &iter, "\n", -1, NULL);

	/* scroll to end */
	gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);
	mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, 1);
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(n->view), mark, 0.0, 0, 0.0, 1.0);
	gtk_text_buffer_delete_mark (buffer, mark);

	return 0;
}

QUERY(gtk_print_version) {
	char *ver = saprintf("GTK plugin for ekg2 comes to you with GTK version: %d.%d.%d.%d",
/*			GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION GTK_BINARY_AGE, */
			gtk_major_version, gtk_minor_version, gtk_micro_version, gtk_binary_age);
	print("generic", ver);
	xfree(ver);
	return 0;
}

QUERY(gtk_ui_window_new) {
	window_t *w = *(va_arg(ap, window_t **));
	ekg_gtk_window_new(w);
	return 0;
}

QUERY(gtk_ui_beep) {
	gdk_beep();
	return 0;
}

QUERY(ekg2_gtk_loop) {
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), window_current->id);
	gtk_contacts_update(NULL);
	while (gtk_loop());
	return -1;
}

QUERY(ekg2_gtk_pending) {
	if (gtk_events_pending())
		return -1;
	return -1;
}

QUERY(gtk_ui_window_clear) { /* to w przeciwienstwie od ncursesowego clear. naprawde czysci okno. wiec nie myslec ze jest takie samo behavtior.. */
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;

	if (!n)
		return 1;

	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (n->view));
	gtk_text_buffer_set_text (buffer, "", -1);
	return 0;
}

QUERY(gtk_ui_window_switch) {
	window_t *w = *(va_arg(ap, window_t **));

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), w->id);
	return 0;
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

int gtk_plugin_init(int prio) {
#define EKG2_NO_DISPLAY "Zmienna $DISPLAY nie jest ustawiona\nInicjalizacja gtk napewno niemozliwa..." /* const char * ? */
	list_t l;

	if (!getenv("DISPLAY")) {
		int tmp = 0;
		if ((query_emit(NULL, "ui-is-initialized", &tmp) != -2) && tmp)
			debug(EKG2_NO_DISPLAY);
		else 	fprintf(stderr, EKG2_NO_DISPLAY);
/* po czyms takim for sure bedzie initowane ncurses... no ale moze to jest wlasciwe zachowanie? jatam nie wiem.
 * gorsze to ze ten komunikat nigdzie sie nie pojawi... */
		return -1;
	}
	
	plugin_register(&gtk_plugin, prio);
/* glowne eventy ui */
	query_connect(&gtk_plugin, "ui-beep", gtk_ui_beep, NULL);
	query_connect(&gtk_plugin, "ui-window-new", gtk_ui_window_new, NULL);
	query_connect(&gtk_plugin, "ui-window-print", gtk_ui_window_print, NULL);
	query_connect(&gtk_plugin, "ui-window-clear", gtk_ui_window_clear, NULL);
	query_connect(&gtk_plugin, "ui-window-switch", gtk_ui_window_switch, NULL);
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
/* inne */
	query_connect(&gtk_plugin, "ui-is-initialized", gtk_ui_is_initialized, NULL); /* aby __debug sie wyswietlalo */
	query_connect(&gtk_plugin, "plugin-print-version", gtk_print_version, NULL);  /* aby sie po /version wyswietlalo */
	
	gtk_init(0, NULL);

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

