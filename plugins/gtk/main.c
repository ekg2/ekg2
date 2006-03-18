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
#include <gdk/gdkkeysyms.h>

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

#define EKG2_BGCOLOR "darkgrey"
// #define EKG2_FGCOLOR "blue"

#ifdef EKG2_BGCOLOR
#define ekg2_set_color_bg_ext(widget)   gtk_widget_modify_base(widget, GTK_STATE_NORMAL, &bgcolor);
#define ekg2_set_color_bg(widget)       gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &bgcolor);
#else
#define ekg2_set_color_bg(widget)	;
#define ekg2_set_color_bg_ext(widget)	;
#endif

#ifdef EKG2_FGCOLOR
#define ekg2_set_color_fg_ext(widget)	gtk_widget_modify_text (widget, GTK_STATE_NORMAL, &fgcolor);
#define ekg2_set_color_fg(widget)	gtk_widget_modify_fg (widget, GTK_STATE_NORMAL, &fgcolor);
#else
#define ekg2_set_color_fg_ext(widget)	;
#define ekg2_set_color_fg(widget)	;
#endif

#define ekg2_set_color(widget)		{ ekg2_set_color_bg(widget)		ekg2_set_color_fg(widget) }
#define ekg2_set_color_ext(widget)	{ ekg2_set_color_bg_ext(widget)		ekg2_set_color_fg_ext(widget) }

typedef struct {
	GtkWidget *view;
	GtkWidget *sw;			// zeby okreslic.
} gtk_window_t;

GtkTreeStore *list_store;		// userlista - elementy
GtkWidget *tree;			// userlista - widget
GtkWidget *notebook;			// zarzadzanie okienkami.

GtkWidget *popupmenu;			// popup menu.

#ifdef EKG2_BGCOLOR
GdkColor bgcolor;			// background color
#endif
#ifdef EKG2_FGCOLOR
GdkColor fgcolor;			// foreground color
#endif

/* atrybuty tekstu... */
GtkTextTagTable *ekg2_table;		// tablica z tagami... 	glowne kolorki + BOLD + inne...
GtkTextTag *ekg2_tags[8];		// pomocnicze,		glowne kolorki
GtkTextTag *ekg2_tag_bold;		// pomocnicze,		BOLD

PLUGIN_DEFINE(gtk, PLUGIN_UI, NULL);

void gtk_contacts_update(window_t *w);
GtkWidget *ekg2_gtk_menu_new(GtkWidget *parentmenu, char *label, void *function, void *data);

extern void ekg_loop();
int ui_quit = -1;	/* -1: jeszcze nie wszedl do ui_loop()
			 *  0: normalny stan..
			 *  1: zamykamy ui.
			 */
int was_unicode;	/* stara wartosc use_unicode... przy wlaczaniu ustawiamy na 1. */
extern GtkWidget *gtk_session_new_window(void *ptr);	/* gtk-session.c */
extern GtkWidget *gtk_settings_window(void *ptr);	/* gtk-settings.c */

enum {	COLUMN_STATUS = 0, 
	COLUMN_NICK,
	COLUMN_UID,
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
	return TRUE;
}

/* klikniecie rowa userlisty */ 
gint on_list_select(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *arg2, gpointer user_data) {
	GtkTreeIter iter;
	gchar *nick, *session, *uid;
	session_t *s;
	const char *action = "query";

	gtk_tree_model_get_iter (GTK_TREE_MODEL(list_store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_NICK, &nick, -1);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_SESSION, &session, -1);
	gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_UID, &uid, -1);
	printf("USERLIST_ACTION (%s) Target: %s session: %s uid: %s\n", action, nick, session, uid);

	if (!(s = session_find(session))) return FALSE;
	
	if (uid) command_exec_format((window_current->session == s) ? window_current->target : nick /* hmmm.. TODO */, s, 0, "/%s %s", action, uid);
	else if (window_current->id == 0 || !window_current->target) { /* zmiana sesji... kod troche sciagniety z ncurses.. czy dobry? */
//		window_session_cycle(window_current);
		window_current->session = s;
		session_current = s;
		query_emit(NULL, "session-changed");
	} else print("session_cannot_change");
	return TRUE;
}

void ekg2_gtk_userlist_menu_user(void *user_data) {
	printf("[POPUP, USERLIST, USER] action = %s\n", (char *) user_data);
}

void ekg2_gtk_userlist_menu_session(void *user_data) {
	printf("[POPUP, USERLIST, SESSION] action = %s\n", (char *) user_data);
}

void ekg2_gtk_userlist_menu_common(void *user_data) {
	printf("[POPUP, USERLIST, COMMON] action = %x\n", (int) user_data);
}

gint popup_handler(GtkWidget *widget, GdkEvent *event) {
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *event_button = (GdkEventButton *) event;
		if (event_button->button == 3) {
			gtk_widget_destroy(popupmenu);
			popupmenu = NULL;
			if (widget == tree) { /* popup menu, userlista */
				GtkTreePath *selection;

				gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event_button->x, event_button->y, &selection, NULL, NULL, NULL);
				if (!selection) {
					printf("Jak nic nie zaznaczyles (/nad niczym nie jestes) to sie nie pokaze menu o! ;p\n");
				} else {
					GtkTreeIter iter;
					gchar *nick, *session, *uid;
					session_t *s;

					gtk_tree_model_get_iter (GTK_TREE_MODEL(list_store), &iter, selection);
					printf("[debug,popup] widget tree: selection = %x iter = %x\n", (int) selection, iter);
					gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_NICK, &nick, -1);
					gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_SESSION, &session, -1);
					gtk_tree_model_get (GTK_TREE_MODEL(list_store), &iter, COLUMN_UID, &uid, -1);
/*					printf("[debug,popup] widget tree: nick = %s session = %s uid = %s\n", nick, session, uid); */

					s = session_find(session);
					popupmenu = gtk_menu_new();
/* tworzenie menu... */
					if (!uid) { /* if session ... */
						ekg2_gtk_menu_new(popupmenu, s->connected ? "Rozłącz" : "Połącz", ekg2_gtk_userlist_menu_session, s->connected ? "disconnect" : "connect" );
						// etc...
					} else { /* if user... */
						ekg2_gtk_menu_new(popupmenu, "Query", ekg2_gtk_userlist_menu_user, "query");
//						ekg2_gtk_menu_new(popupmenu, "Info",  /* on_mi_info */ NULL, NULL);
						ekg2_gtk_menu_new(popupmenu, "Usun", ekg2_gtk_userlist_menu_user, "del");
						// etc...
					}
					// common... etc..
				}
				// common... etc..
/*				ekg2_gtk_menu_new(popupmenu, "Odśwież userliste", ekg2_gtk_userlist_menu_common, (void *) 1); */

			} else if (widget == notebook) { /* popup menu, okna */
				// TODO, czek czy jestesmy nad jakims...
				popupmenu = gtk_menu_new();
				ekg2_gtk_menu_new(popupmenu, "Rozlacz okno..", NULL, NULL); /* detach / attach */
				ekg2_gtk_menu_new(popupmenu, "Przelacz na", NULL, NULL);
				ekg2_gtk_menu_new(popupmenu, "Zamknij", NULL, NULL);
			}
				
			if (popupmenu) {
				gtk_menu_popup(GTK_MENU(popupmenu), NULL, NULL, NULL, NULL, event_button->button, event_button->time);
				return TRUE;
			} else	return FALSE;
		}
	}
	return FALSE;
}

/* zmiana strony - zmiana okna */
gint on_switch_page(GtkNotebook *notebook, GtkNotebookPage *page, guint page_num, gpointer user_data) {
	if (in_autoexec) 
		return FALSE;
	if (window_current->id == page_num)
		return FALSE;
	
	window_switch(page_num); /* !!! to niekoniecznie musi byc numer okna, XXX */
	return TRUE;
}

/* nacisniecie klawisza */
gint gtk_key_press (GtkWidget *widget, GdkEventKey *event, void *data) {
	if (event->type == GDK_KEY_PRESS) { 
		/* moze wypadaloby te znaki tlumaczyc.. na cos ala ncurses-like? */
/* some state bitmasks: 
 * 	SHIFT	1	SHIFT_MASK
 * 	CTRL	4	CONTROL_MASK
 * 	L-ALT	8	MOD1_MASK
 * 	R-ALT 128	MOD2_MASK
 */

// sizeof(event->keyval) != sizeof(char) .... IMPLEMENTATION BUG (?).
		if (query_emit(NULL, "ui-keypress", &(event->keyval), NULL) == -1)
			return TRUE; /* ignore this key */

		if (event->keyval == GDK_Tab) {
			/* TODO: uzupelnianie, poczekamy na przeeniesienie kodu ncurses.. */
			gchar *complete = (gchar *) gtk_entry_get_text(GTK_ENTRY(widget));
			int pos = gtk_editable_get_position( GTK_EDITABLE(widget) );

			printf("[uzupelnianie] TODO: complete = %s pozycja = %d\n", complete, pos);
		} else if (event->keyval == GDK_Up || event->keyval == GDK_Down) {
			/* To by wypadalo juz teraz zrobic... */
		}
		/* TODO, inne, bindlista klawiszy, etc.. 
		 * moze zrobmy to tez po stronie ekg2 ? emitujemy ui-keypress w koncu... */

		/* zanim zrobimy `pelna` obsluge bindingow, handlujmy bardziej defaultowe.. just for example */
		/* TODO: przepisac obsluge bindingow? 
		 * 	BO tak mamy niefajnie... */

		if (event->keyval == GDK_F1) { command_exec(NULL, NULL, "/help", 0); return TRUE; }
		if (event->keyval == GDK_F12) { command_exec(NULL, NULL, "/window switch 0", 0); return TRUE; }
		if (event->state == GDK_CONTROL_MASK) { /* CTRL- */
			switch (event->keyval) {
				case(110): command_exec(NULL, NULL, "/window next", 0); return TRUE;
				case(112): command_exec(NULL, NULL, "/window prev", 0); return TRUE;
			}
		}
		if (event->state == GDK_MOD1_MASK) { /* LALT- */
			int gotowindow = -1;
			switch (event->keyval) {
				/* przelaczanie okienek */
				case(96): gotowindow = 0; break;
				case(48): gotowindow = 10; break;
				case(49): case(50): case(51): case(52): case(53): case(54): case(55): case(56): case(57): gotowindow = event->keyval-48; break;
				case(113): gotowindow = 11; break;
				case(119): gotowindow = 12; break;
				case(101): gotowindow = 13; break;
				case(114): gotowindow = 14; break;
				case(116): gotowindow = 15; break;
				case(121): gotowindow = 16; break;
				case(117): gotowindow = 17; break;
				case(105): gotowindow = 18; break;
				case(111): gotowindow = 19; break;
				case(112): gotowindow = 20; break;
				/* inne */
				case(110): command_exec(NULL, NULL, "/window new", 0); return TRUE;
				case(107): command_exec(NULL, NULL, "/window kill", 0); return TRUE;
				/* jeszcze inne */
				case(GDK_Return): printf("[TEMP_BIND] ALT+ENTER!!!\n"); return TRUE; /* co robimy? */
			}

			if (gotowindow != -1) { 
				printf("[window_temp_bind_switcher] gotowindow=%d\n", gotowindow);
				window_switch(gotowindow);
//				command_exec_format(NULL, NULL, 0, "/window switch %d", gotowindow);
				return TRUE;
			}
		}
		
		switch (event->keyval) {
			case (GDK_Tab): 	/* TAB-> complete */
			case (GDK_Up):		/* bufor polecen w gore */
			case (GDK_Down):	/* bufor polecen w dol */
				return TRUE;
			default:
				printf("[KEY_DEFAULT] Nacisnales: %d (%d) (widget=%x)\n", event->keyval, event->state, (int) widget);
				return FALSE;
				 
		}
	}
	return FALSE;
}

int gtk_loop() {
	ekg_loop();
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	return (ui_quit != 1);
}
void ekg2_gtk_menu_url_click(char *user_data) {
//	printf("menuitem = %x userdata = %x\n");
	printf("[POMOC->WWW->CLICK] url = %s\n", user_data);
// TODO: otworz strone.
}

void ekg2_gtk_menu_session_add(void *user_data) {
	printf("[EKG2->Sesje->Dodaj->CLICK]\n");
	gtk_session_new_window(NULL);
}

void ekg2_gtk_menu_settings(void *user_data) {
	printf("[EKG2->Ustawienia->CLICK]\n");
	gtk_settings_window(NULL);
}

void ekg2_gtk_menu_quit(GtkWidget *window) {
/* pytac? */
	gtk_widget_destroy(window);
}

void uid_set_func_text (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	gchar *nick;
	gtk_tree_model_get (model, iter, COLUMN_NICK, &nick, -1);
	g_object_set (GTK_CELL_RENDERER (cell), "text", nick, NULL);
}

GtkWidget *ekg2_gtk_menu_new(GtkWidget *parentmenu, char *label, void *function, void *data) {
	GtkWidget *menu_item = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(parentmenu), menu_item);
	gtk_widget_show(menu_item);
	if (function)
		g_signal_connect_swapped (G_OBJECT(menu_item),"activate",G_CALLBACK (function), data);
	return menu_item;
}

int gtk_create() {
	GtkWidget *win, *edit1;
	GtkWidget *vbox, *hbox, *sw;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
#ifdef EKG2_FGCOLOR
	gdk_color_parse (EKG2_FGCOLOR, &fgcolor);
#endif
#ifdef EKG2_BGCOLOR
	gdk_color_parse (EKG2_BGCOLOR, &bgcolor);
#endif
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (win), "ekg2 p0wer!");

	ekg2_set_color(win);
  
	g_signal_connect(G_OBJECT(win), "delete_event", G_CALLBACK (delete_event), NULL);
	g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK (destroy), NULL);
	g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK (gtk_key_press), NULL);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (win), hbox);
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	{ /* main menu */
		GtkWidget *menu;
		GtkWidget *menu_bar;
		GtkWidget *menu_ekg, *menu_window, *menu_help;
		GtkWidget *mi_session, *mi_settings;
		GtkWidget *mi_www;
		/*  Ekg2 menu */
		menu		= gtk_menu_new ();
		menu_ekg	= gtk_menu_item_new_with_label("Ekg2");
		mi_session	= ekg2_gtk_menu_new(menu, "Sesje", NULL, NULL);
		{ /* session submenu */
			GtkWidget *menu_sessions	= gtk_menu_new();
			ekg2_gtk_menu_new(menu_sessions, "Dodaj", ekg2_gtk_menu_session_add, NULL);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_session), menu_sessions);
		}
		mi_settings	= ekg2_gtk_menu_new(menu, "Ustawienia", ekg2_gtk_menu_settings, NULL);
		ekg2_gtk_menu_new(menu, "Zakończ", ekg2_gtk_menu_quit, win);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_ekg), menu);
		/* window menu */
		menu		= gtk_menu_new ();
		menu_window	= gtk_menu_item_new_with_label("Okna");
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_window), menu);
		/* help menu */
		menu		= gtk_menu_new ();
		menu_help	= gtk_menu_item_new_with_label("Pomoc");
		mi_www		= ekg2_gtk_menu_new(menu, "WWW", NULL, NULL);
		{ /* www submenu */
			GtkWidget *menu_www	= gtk_menu_new();
			ekg2_gtk_menu_new(menu_www, "ekg2.org", 	ekg2_gtk_menu_url_click, "http://ekg2.org");
			ekg2_gtk_menu_new(menu_www, "pl.ekg2.org",	ekg2_gtk_menu_url_click, "http://pl.ekg2.org");
			ekg2_gtk_menu_new(menu_www, "bugs.ekg2.org",	ekg2_gtk_menu_url_click, "http://bugs.ekg2.org");
			ekg2_gtk_menu_new(menu_www, "wiki.ekg2.org",	ekg2_gtk_menu_url_click, "http://ekg2.wafel.com");
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(mi_www), menu_www);
		}
		ekg2_gtk_menu_new(menu, "Autorzy", NULL, "todo");
		ekg2_gtk_menu_new(menu, "O EKG2..", NULL, "todo");
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_help), menu);
		/* itd... */
		
		menu_bar = gtk_menu_bar_new ();
		gtk_box_pack_start (GTK_BOX (vbox), menu_bar, FALSE, FALSE, 2);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_ekg);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_window);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_help);
		/* itd... */
/*		ekg2_set_color(menu_bar); */
		gtk_widget_show (menu_bar);
	}
	
	/* notebook */
	notebook = gtk_notebook_new ();
//	gtk_notebook_set_show_border (GTK_NOTEBOOK(notebook), FALSE);
	gtk_box_pack_start (GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
	gtk_widget_set_size_request(notebook, 505, 375);
	g_signal_connect (G_OBJECT(notebook), "switch-page", G_CALLBACK(on_switch_page), NULL);
//	gtk_notebook_set_tab_pos (notebook, 4);
/*	ekg2_set_color(notebook); */
	
	/* lista - przwewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (hbox), sw, FALSE, FALSE, 0);
	/* lista */
	list_store = gtk_tree_store_new (N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), GTK_SELECTION_MULTIPLE);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "userlista", renderer, "pixbuf", COLUMN_STATUS, NULL); 

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_STATUS);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, uid_set_func_text, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_NICK, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_UID, NULL);
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_SESSION, NULL);
	gtk_tree_view_column_set_visible( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_NICK), FALSE);
	gtk_tree_view_column_set_visible( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_UID), FALSE);
	gtk_tree_view_column_set_visible( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_SESSION), FALSE);
	
	gtk_container_add (GTK_CONTAINER (sw), tree);
	g_signal_connect (G_OBJECT (tree), "row-activated", G_CALLBACK (on_list_select), NULL);
	gtk_widget_set_size_request(tree, 165, 365);
/*	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE); *//* czy wyswietlac nazwe kolumn ? *//* w column name jest nazwa sesji */
	ekg2_set_color_ext(tree);
	
	/* edit */
	edit1 = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX(vbox), edit1, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT(edit1), "activate", G_CALLBACK (on_enter), NULL);
	g_signal_connect (G_OBJECT(edit1), "key-press-event", G_CALLBACK (gtk_key_press), NULL);
	ekg2_set_color_ext(edit1);
	
	g_signal_connect_swapped (tree, "button_press_event", G_CALLBACK (popup_handler), tree); /* popup menu, userlista */
	g_signal_connect_swapped (notebook, "button_press_event", G_CALLBACK (popup_handler), notebook); /* popup menu, okna */

	gtk_widget_grab_focus(edit1);
	gtk_widget_show_all (win);

	/* atrybutu tekstu */
	{
		GtkTextTag *tmp = NULL;
		int i = 0;
		ekg2_table = gtk_text_tag_table_new();

#define ekg2_create_tag(x) \
	tmp = gtk_text_tag_new("FG_" #x); \
	g_object_set(tmp, "foreground", #x, NULL); \
	gtk_text_tag_table_add(ekg2_table, tmp); /* dodajemy do glownego TextTableTag... */\
	ekg2_tags[i++] = tmp;
	
		ekg2_create_tag(BLACK);	ekg2_create_tag(RED); ekg2_create_tag(GREEN);
		ekg2_create_tag(YELLOW);ekg2_create_tag(BLUE); ekg2_create_tag(MAGENTA);
		ekg2_create_tag(CYAN);	ekg2_create_tag(WHITE);
	
		ekg2_tag_bold = tmp = gtk_text_tag_new("BOLD");
		g_object_set(tmp, "weight", PANGO_WEIGHT_BOLD, NULL);
		gtk_text_tag_table_add(ekg2_table, tmp);		/* dodajemy do glownego TextTableTag... */

		tmp = gtk_text_tag_new("ITALICS");
		g_object_set(tmp, "style", PANGO_STYLE_ITALIC, NULL);
	
//		gtk_text_buffer_create_tag(buffer, "FG_NAVY", "foreground", "navy", NULL);
//		gtk_text_buffer_create_tag(buffer, "FG_DARKGREEN", "foreground", "darkgreen", NULL);
//		gtk_text_buffer_create_tag(buffer, "FG_LIGHTGREEN", "foreground", "green", NULL);
	}
	return 0;
}

void ekg_gtk_window_new(window_t *w) {
	GtkWidget *sw, *view;
	GtkTextBuffer *buffer;
	char *name = window_target(w);
	
	gtk_window_t *n = xmalloc(sizeof(gtk_window_t));
	w->private = n;

	printf("WINDOW_NEW(): [%d,%s]\n", w->id, name);

	/* tekst - przewijanie */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), sw, gtk_label_new (name), w->id);
	/* tekst */
	buffer = gtk_text_buffer_new(ekg2_table);
	view = gtk_text_view_new_with_buffer(buffer);
	gtk_text_view_set_editable(GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW (view), GTK_WRAP_WORD);

	gtk_container_add (GTK_CONTAINER (sw), view);
	
	n->view = view;
	n->sw	= sw;

	ekg2_set_color_ext(view);

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
/* TODO jesli sesja to session_avail, session_invisible etc...
 *      jesli user  to user_avail, user_invisible, etc... 
 *      bo teraz w sumie to nie wiadomo co do czego jest.. ;p 
 */
	char *status_filename = saprintf("%s/plugins/gtk/%s.png", DATADIR, (u) ? u->status : s->status);

	if (!s && !u) {
		xfree(status_filename);
		// INTERNAL ERROR.
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file (status_filename, &error);
	if (!pixbuf)
		printf("CONTACTS_ADD() filename=%s; pixbuf=%x iter=%x;\n", status_filename, (int) pixbuf, (int) iter);
	gtk_tree_store_append (list_store, tmp,	(!isparent) ? iter : NULL);
	
	gtk_tree_store_set (list_store, tmp,
			COLUMN_STATUS, pixbuf, 
			COLUMN_NICK, (isparent) ? (s->alias ? s->alias : s->uid) :	/* sesja  - parent  */
				     (u->nickname ? u->nickname : u->uid),		/* useria - dziecko */
			COLUMN_UID, (u) ? u->uid : NULL, 
			COLUMN_SESSION, (s) ? s->uid : NULL,
			-1);

	xfree(status_filename);
	return;
}

void gtk_contacts_update(window_t *w) {
	list_t l;
	printf("[CONTACTS_UPDATE()\n");
 	gtk_tree_store_clear(list_store);

	gtk_tree_view_column_set_title( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_STATUS), 
			session_current ? session_current->alias ? session_current->alias : session_current->uid : "" /* "brak sesji ?" */); /* zmien nazwe kolumny na nazwe aktualnej sesji */

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

void gtk_process_str(window_t *w, GtkTextBuffer *buffer, char *str, short int *attr, int istimestamp) {
	GtkTextIter iter;
	int i;
/* i know ze tak nie moze wygladac, zrobione po prostu aby dzialalo. */
	for (i=0; i < xstrlen(str); i++) {
		GtkTextTag *tags[2] = {NULL, NULL};
		short att = attr[i];

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, -1);

		if (!(att & 128))	tags[0] = ekg2_tags[att & 7];
		if (att & 64)		tags[1] = ekg2_tag_bold;

		if (istimestamp && (att & 7) == 0) tags[1] = ekg2_tag_bold;

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
		fstring_t *t = fstring_new(ts);

		gtk_process_str(w, buffer, t->str, t->attr, 1);
		
		xfree(tmp);
		xfree(ts);
		fstring_free(t);
	}
	gtk_process_str(w, buffer, line->str, line->attr, 0);

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

QUERY(gtk_statusbar_query) { /* dodanie / usuniecie sesji... */
	if (ui_quit)
		return 1;
	gtk_contacts_update(NULL);
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
	ui_quit = 0;
	was_unicode = config_use_unicode;
	config_use_unicode = 1;

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

QUERY(gtk_ui_window_clear) { /* to w przeciwienstwie od ncursesowego clear. naprawde czysci okno. wiec nie myslec ze jest takie samo behavior.. */
	GtkTextBuffer *buffer;
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;

	if (!n)
		return 1;
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW (n->view));
	gtk_text_buffer_set_text (buffer, "", -1);
	return 0;
}

QUERY(gtk_ui_window_switch) {
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;
	int realid;

	if (!n)
		return 1;
	realid = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), n->sw);

	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) == realid)
		return 1;

	printf("[UI_WINDOW_SWITCH] gtk_notebook_page_num() = %d; w->id = %d\n", realid, w->id);

	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), realid);
	gtk_contacts_update(NULL);
	return 0;
}

QUERY(gtk_ui_window_kill) {
	window_t *w = *(va_arg(ap, window_t **));
	gtk_window_t *n = w->private;
	int realid;
	
	if (!n) 
		return 1;
	realid = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), n->sw);
	printf("[UI_WINDOW_KILL] gtk_notebook_page_num() = %d; w->id = %d\n", realid, w->id);
	
	gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), realid);
	return 0;
}

QUERY(gtk_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);
	*tmp = !(ui_quit == 1);
	return 0;
}

QUERY(gtk_contacts_changed) {
	gtk_contacts_update(NULL);
	return 0;
}

QUERY(gtk_userlist_changed) {
	char **p1 = va_arg(ap, char**);
	char **p2 = va_arg(ap, char**);
	if (ui_quit)
		return 1;
	
/* jak jest jakies okno z *p1 to wtedy zamieniamy nazwe na *p2 */
	gtk_contacts_update(NULL);
	return 0;
}

void gtk_statusbar_timer() {
/*	gtk_contacts_update(NULL); */
}

int gtk_plugin_init(int prio) {
	const char *ekg2_another_ui = "Masz uruchomione inne ui, aktualnie nie mozesz miec uruchomionych obu na raz... Jesli chcesz zmienic ui uzyj ekg2 -F gtk\n";
	const char *ekg2_no_display = "Zmienna $DISPLAY nie jest ustawiona\nInicjalizacja gtk napewno niemozliwa...\n";
	list_t l;
        int is_UI = 0;

        query_emit(NULL, "ui-is-initialized", &is_UI);

	if (!getenv("DISPLAY")) {
/* po czyms takim for sure bedzie initowane ncurses... no ale moze to jest wlasciwe zachowanie? jatam nie wiem.
 * gorsze to ze ten komunikat nigdzie sie nie pojawi... */
		if (is_UI) debug(ekg2_no_display);
		else	   fprintf(stderr, ekg2_no_display);
		return -1;
	}
        if (is_UI) {
		debug(ekg2_another_ui);
                return -1;
	}
	plugin_register(&gtk_plugin, prio);
/* glowne eventy ui */
	query_connect(&gtk_plugin, "ui-beep", gtk_ui_beep, NULL);
	query_connect(&gtk_plugin, "ui-window-clear", gtk_ui_window_clear, NULL);
	query_connect(&gtk_plugin, "ui-window-kill", gtk_ui_window_kill, NULL);
	query_connect(&gtk_plugin, "ui-window-new", gtk_ui_window_new, NULL);
	query_connect(&gtk_plugin, "ui-window-print", gtk_ui_window_print, NULL);
	query_connect(&gtk_plugin, "ui-window-switch", gtk_ui_window_switch, NULL);
/* userlist */
	query_connect(&gtk_plugin, "userlist-changed", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-added", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-removed", gtk_userlist_changed, NULL);
	query_connect(&gtk_plugin, "userlist-renamed", gtk_userlist_changed, NULL);
/* sesja */
	query_connect(&gtk_plugin, "session-added", gtk_statusbar_query, NULL);
	query_connect(&gtk_plugin, "session-removed", gtk_statusbar_query, NULL);
	query_connect(&gtk_plugin, "session-changed", gtk_contacts_changed, NULL);

/* w/g developerow na !ekg2 `haki`  ;) niech im bedzie ... ;p */
	query_connect(&gtk_plugin, "ui-loop", ekg2_gtk_loop, NULL);
	query_connect(&gtk_plugin, "ui-pending", ekg2_gtk_pending, NULL);
/* inne */
	query_connect(&gtk_plugin, "ui-is-initialized", gtk_ui_is_initialized, NULL); /* aby __debug sie wyswietlalo */
	query_connect(&gtk_plugin, "plugin-print-version", gtk_print_version, NULL);  /* aby sie po /version wyswietlalo */

/*	timer_add(&gtk_plugin, "gtk:clock", 1, 1, gtk_statusbar_timer, NULL); */
	
	gtk_init(0, NULL);

	gtk_create();
	
	for (l = windows; l; l = l->next) {
		ekg_gtk_window_new(l->data);	
	}
	return 0;
}

static int gtk_plugin_destroy() {
	plugin_unregister(&gtk_plugin);
	config_use_unicode = was_unicode;
	return 0;
}

