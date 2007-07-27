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

#include <ekg2-config.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/queries.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "main.h"
#include "palette.h"
#include "xtext.h"
#include "bindings.h"

/* extern */
void fe_set_channel(session_t *sess);
void mg_changui_new(window_t *sess, gtk_window_t *res, int tab, int focus);
void fe_set_tab_color(window_t *sess, int col);
void fe_close_window(window_t *sess);
void mg_apply_setup(void);
void mg_change_layout(int type);
void mg_switch_page(int relative, int num);


PLUGIN_DEFINE(gtk, PLUGIN_UI, NULL);

/* config vars */
int mainwindow_width_config;
int mainwindow_height_config;
int gui_pane_left_size_config;
int gui_tweaks_config;
int tab_small_config;
int tab_pos_config;
int max_auto_indent_config;
int thin_separator_config;
int backlog_size_config;
int show_marker_config;
int tint_red_config;
int tint_green_config;
int tint_blue_config;
int transparent_config;
int wordwrap_config;
int indent_nicks_config;
int show_separator_config;

int tab_layout_config;

int gui_ulist_pos_config;
int tab_pos_config;
int contacts_config;

char *font_normal_config;

int ui_quit = 0;


/* TODO:
 *    - wrzucic zmienne do variable_add() przynajmniej te wazne..
 *    - zrobic menu
 *    - zrobic traya
 *    - zrobic userliste
 *    - zaimplementowac w xtext kolorki ekg2			[done, nie wszystkie atrybuty zrobione, ale xtext.c juz wie co to jest fstring_t
 *    									i ze ma tam atrybuty, see gtk_xtext_render_str()]
 *    - timestamp powinien byc renderowany z config_timestamp, i zamieniany na fstring_t !!!
 *    - implementowac wiecej waznych dla UI QUERY()
 *    - brakuje funkcjonalnosci detach-okienka z XChata, trzeba przejrzec ten kod i dokonczyc.
 *    - w XChacie byl jeszcze drag-drop np. plikow... jak ktos potrzebuje to wie co zrobic.
 *
 *    - zaimplementowac do konca ten dialog zamykania ekg2-gtk i dodac do niego opcje zapisu...
 *       zeby ekg2 sie na konsoli nie pytalo czy zapisac.
 */

/* BUGS:
 *    - /window move, /window kill, itd.. nie dziala
 *    - zabijanie okienek zle zrobione [czyt. w ogole nic nie dziala]
 *
 *    - sa lagi, nie wszystko w gtk sie dzieje jak user rusza myszka, klika klawiszami...
 *      niektore operacje sa robione zeby byly ,,animacja'' a select() 1s powoduje calkiem duze latencje.
 *      watch wykonywany co 0.03s bylby calkiem dobrym pomyslem... 
 *
 *    - duzo innych, roznych bledow..
 *    - jak zrobimy detach okienek, to prawdopodobnie operacje beda robione na zlych oknach, 
 *    	xchat mial current_tab, ja myslalem ze to jest to samo co window_current, wiec do pupy.
 */

static QUERY(gtk_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);

	*tmp = (ui_quit == 0);
	return 0;
}

static QUERY(ekg2_gtk_loop) {
	extern void ekg_loop();

	do {
		ekg_loop();

		while (gtk_events_pending()) {
			gtk_main_iteration();
		}
	} while (ui_quit == 0);

	return -1;
}

static WATCHER(ekg2_xorg_watcher) {
	if (type || ui_quit == 1) return -1;
/* do nothing.. successfully. it's just like readline_watch_stdin() to don't matter about select() latency... default 1s. yeah I know it's only for
 * communication between x'org server and gtk... gtk maybe want to do somethink else.. but we can provide it only by decreasing latency from 1s to for instance
 * 0.1s in select() or by creating another thread.. */
	return 0;
}

static IDLER(ekg2_xorg_idle) {
	while (gtk_events_pending()) {
		gtk_main_iteration();
	}
	return 0;
}

void ekg_gtk_window_new(window_t *w) {			/* fe_new_window() */
	int tab = TRUE;

	/* tab == new_window_in_tab */

	mg_changui_new(w, NULL, tab, 0);
}

static QUERY(gtk_ui_window_new) {			/* fe_new_window() */
	window_t *w = *(va_arg(ap, window_t **));

	ekg_gtk_window_new(w);

	return 0;
}

int gtk_ui_window_switch_lock = 0;

static QUERY(gtk_ui_window_switch) {
#warning "XXX, fast implementation"
	window_t *w 	= *(va_arg(ap, window_t **));

	if (gtk_ui_window_switch_lock)
		return 0;

	gtk_ui_window_switch_lock = 1;
		mg_switch_page(FALSE, w->id);
	gtk_ui_window_switch_lock = 0;

	fe_set_tab_color(w, w->act & 3);
	return 0;
}

static QUERY(gtk_ui_window_kill) {			/* fe_session_callback() || fe_close_window() */
	window_t *w = *(va_arg(ap, window_t **));

	fe_close_window(w);
	return 0;
}


static QUERY(gtk_ui_window_print) {			/* fe_print_text() */
	window_t *w = *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));

/*	PrintTextRaw (sess->res->buffer, (unsigned char *)text, prefs.indent_nicks, stamp); */

#if 0
	size_t len = xstrlen(line->str.b);

	if (!indent_nicks_config) {
		if (config_timestamp_show) {
#warning "XXX, use config_timestamp instead"
			const char *ts = timestamp_time("%H:%M:%S", line->ts);	
			char *text;
			
			text = saprintf("%s %s", ts, line->str.b);

			gtk_xtext_append(gtk_private(w)->buffer, text, xstrlen(ts) + 1 + len);

			xfree(text);

		} else	gtk_xtext_append(gtk_private(w)->buffer, line->str.b, len);


	} else {
		#warning "XXX"
		gtk_xtext_append_indent(gtk_private(w)->buffer, 0, 0, line->str.b, len, line->ts);
	}
#endif

	gtk_xtext_append_fstring(gtk_private(w)->buffer, line);

	return 0;
}

static QUERY(gtk_ui_window_target_changed) {
	window_t *w = *(va_arg(ap, window_t **));

#warning "ncurses do much more here"
#if 0
	ncurses_window_t *n = w->private;
	char *tmp;

	xfree(n->prompt);

	tmp = format_string(format_find((w->target) ? "ncurses_prompt_query" : "ncurses_prompt_none"), w->target);
	n->prompt = tmp; 
	n->prompt_len = xstrlen(tmp);

	update_statusbar(1);
#endif
	fe_set_channel(w);
	return 0;
}

static QUERY(gtk_beep) {				/* fe_beep() */
	gdk_beep();
	return -1;
}

static QUERY(gtk_ui_window_act_changed) { 		/* fe_set_tab_color() */
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		fe_set_tab_color(w, w->act & 3);
	}

	return 0;
}

static QUERY(gtk_print_version) {
	char *ver = saprintf("GTK2 plugin for ekg2 ported from XChat gtk frontend ((C) Peter Zelezny) using gtk: %d.%d.%d.%d",
				gtk_major_version, gtk_minor_version, gtk_micro_version, gtk_binary_age);
/* 				GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION GTK_BINARY_AGE );*/
	print("generic", ver);
	xfree(ver);
	return 0;
}

static QUERY(gtk_utf_postinit) {
	/* hack */
	xfree(config_console_charset);
	config_console_charset = xstrdup("UTF-8");

	config_use_unicode = 1;
	return 0;
}

static QUERY(gtk_postinit) {
	mg_apply_setup();

	mg_switch_page(FALSE, window_current->id);

	return 0;
}

static QUERY(gtk_setvar_default) {
	mainwindow_width_config		= 640;
	mainwindow_height_config	= 400;

	gui_pane_left_size_config	= 100;
	gui_tweaks_config		= 0;
	tab_small_config		= 0;
	tab_pos_config			= 0;
	transparent_config		= 0;

	wordwrap_config			= 1;
	indent_nicks_config		= 1;
	show_separator_config		= 1;
	show_marker_config		= 1;
	thin_separator_config		= 1;

	max_auto_indent_config = 256;
	backlog_size_config = 1000;
	tint_red_config = tint_green_config = tint_blue_config = 195;

	gui_ulist_pos_config = 3;
	tab_pos_config = 6;

	tab_layout_config = 2;

	xfree(font_normal_config);
	font_normal_config = xstrdup("Monospace 9");

	contacts_config			= 2;

	return 0;
}

static void gtk_tab_layout_change(const char *var) {
	mg_change_layout(tab_layout_config);
}

static QUERY(gtk_variable_changed) {
	char *name = *(va_arg(ap, char**));

        if (!xstrcasecmp(name, "timestamp_show")) {
		mg_apply_setup();
	}
}

EXPORT int gtk_plugin_init(int prio) {
	const char ekg2_another_ui[] = "Masz uruchomione inne ui, aktualnie nie mozesz miec uruchomionych obu na raz... Jesli chcesz zmienic ui uzyj ekg2 -F gtk\n";
	const char ekg2_no_display[] = "Zmienna $DISPLAY nie jest ustawiona\nInicjalizacja gtk napewno niemozliwa...\n";

        int is_UI = 0;
	int xfd;
	list_t l;

        query_emit_id(NULL, UI_IS_INITIALIZED, &is_UI);

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

	/* fe_args() */

	if (!(gtk_init_check(0, NULL)))
		return -1;

#ifdef USE_UNICODE
	if (!config_use_unicode)
#endif
	{
		int la = in_autoexec;
		bind_textdomain_codeset("ekg2", "UTF-8");
		in_autoexec = 0;	changed_theme(("theme"));	in_autoexec = la; /* gettext + themes... */
	}

		/* ... */

	/* fe_init() */
	gtk_binding_init();
#if 0
	pixmaps_init();
	channelwin_pix	= pixmap_load_from_file(prefs.background);
	input_style	= create_input_style(gtk_style_new());
#endif
	
		/* xchat_init() */
		/* fe_main() */

	plugin_register(&gtk_plugin, prio);

	query_connect_id(&gtk_plugin, UI_IS_INITIALIZED,	gtk_ui_is_initialized, NULL); /* aby __debug sie wyswietlalo */
	query_connect_id(&gtk_plugin, SET_VARS_DEFAULT,		gtk_setvar_default, NULL);

	query_emit_id(&gtk_plugin, SET_VARS_DEFAULT);

	query_connect_id(&gtk_plugin, CONFIG_POSTINIT,		gtk_utf_postinit, NULL);
	query_connect_id(&gtk_plugin, CONFIG_POSTINIT,		gtk_postinit, NULL);

	query_connect_id(&gtk_plugin, UI_LOOP,			ekg2_gtk_loop, NULL);
	query_connect_id(&gtk_plugin, PLUGIN_PRINT_VERSION,	gtk_print_version, NULL);

	query_connect_id(&gtk_plugin, UI_BEEP,			gtk_beep, NULL);		/* fe_beep() */
	query_connect_id(&gtk_plugin, UI_WINDOW_NEW,		gtk_ui_window_new, NULL);	/* fe_new_window() */
	query_connect_id(&gtk_plugin, UI_WINDOW_PRINT,		gtk_ui_window_print, NULL);	/* fe_print_text() */
	query_connect_id(&gtk_plugin, UI_WINDOW_ACT_CHANGED,	gtk_ui_window_act_changed, NULL);/* fe_set_tab_color() */
	query_connect_id(&gtk_plugin, UI_WINDOW_KILL,		gtk_ui_window_kill, NULL);	/* fe_session_callback() */
	query_connect_id(&gtk_plugin, UI_WINDOW_SWITCH,		gtk_ui_window_switch, NULL);
	query_connect_id(&gtk_plugin, UI_WINDOW_TARGET_CHANGED, gtk_ui_window_target_changed, NULL);	/* fe_set_channel() */

	query_connect_id(&gtk_plugin, VARIABLE_CHANGED,		gtk_variable_changed, NULL);

#define gtk_backlog_change NULL
#warning "gtk_backlog_change == NULL, need research"
	variable_add(&gtk_plugin, ("backlog_size"), VAR_INT, 1, &backlog_size_config, gtk_backlog_change, NULL, NULL);
	variable_add(&gtk_plugin, ("tab_layout"), VAR_INT, 1, &tab_layout_config, gtk_tab_layout_change, NULL, NULL);	/* XXX, variable_map() 0 -> 2-> */

	xfd = XConnectionNumber(gdk_x11_get_default_xdisplay());
	printf("[HELLO ekg2-GTK] XFD: %d\n", xfd);
	if (xfd != -1)
		watch_add(&gtk_plugin, xfd, WATCH_READ, ekg2_xorg_watcher, NULL);

	idle_add(&gtk_plugin, ekg2_xorg_idle, NULL);

	for (l = windows; l; l = l->next)
		ekg_gtk_window_new(l->data);	

	memset(gtk_history, 0, sizeof(gtk_history));

	return 0;
}

static int gtk_plugin_destroy() {
	int i;

	for (i = 0; i < HISTORY_MAX; i++) {
		xfree(gtk_history[i]);
		gtk_history[i] = NULL;
	}

	plugin_unregister(&gtk_plugin);

	return 0;
}
