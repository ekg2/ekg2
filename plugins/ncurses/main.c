/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *		  2010 S�awomir Nizio <poczta-sn@gazeta.pl>
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

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/windows.h>
#include <ekg/vars.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#include "ecurses.h"
#include "bindings.h"
#include "old.h"
#include "contacts.h"
#include "mouse.h"

static int ncurses_theme_init();
PLUGIN_DEFINE(ncurses, PLUGIN_UI, ncurses_theme_init);
/* vars */
int config_aspell;
char *config_aspell_lang;
int config_backlog_size;
int config_backlog_scroll_half_page;
int config_display_transparent;
int config_enter_scrolls;
int config_header_size;
int config_margin_size;
int config_mark_on_window_change = 0;
int config_kill_irc_window = 1;
int config_statusbar_size;
int config_lastlog_size;
int config_lastlog_lock;
int config_text_bottomalign	= 0;
int config_typing_interval	= 1;
int config_typing_timeout	= 10;
int config_typing_timeout_empty = 5;

static int config_traditional_clear	= 1;

int ncurses_initialized;
int ncurses_plugin_destroyed;

const char *ncurses_settitle_formats[3] = { NULL, "\e]0;%s%s%s\a", "\e_%s%s%s\e\\" };
static int ncurses_settitle = 0;

QUERY(ncurses_password_input); /* old.c */
void ncurses_window_gone(window_t *w);

/**
 * ncurses_beep()
 *
 * Handler for: <i>UI_BEEP</i><br>
 * do curses beep()
 *
 * @todo Check result of beep()
 * @todo What about curses flash() ? :>
 *
 * @return -1 [We don't want to hear other beeps]
 */

static QUERY(ncurses_beep) {
	beep();
	return -1;
}

static int dd_contacts(const char *name)
{
	return (config_contacts);
}

/**
 * ncurses_statusbar_timer()
 *
 * Timer, executed every second.
 * It call update_statusbar(1)
 *
 * @sa update_statusbar()
 *
 * @return 0	[permanent timer]
 */

static TIMER(ncurses_statusbar_timer) {
	if (type) return 0;
	update_statusbar(1);
	return 0;
}

static QUERY(ncurses_statusbar_query)
{
	update_statusbar(1);
	return 0;
}

/**
 * ncurses_ui_is_initialized()
 *
 * Handler for: <i>UI_IS_INITIALIZED</i><br>
 * Set @a tmp to ncurses_initialized [0/1]<br>
 *
 * @note <i>UI_IS_INITIALIZED</i> is used to check if we can display debug info by emiting <i>UI_PRINT_WINDOW</i> or not.
 *		It also used by other UI-PLUGINS to check if another UI-plugin is in use. [Becasuse we have only one priv_data struct in window_t]
 *
 * @param ap 1st param: <i>(int) </i><b>tmp</b> - place to put ncurses_initialized variable.
 * @param data NULL
 *
 * @return -1 If ncurses is initialized, else 0
 */

static QUERY(ncurses_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);

	if ((*tmp = ncurses_initialized))	return -1;
	else					return 0;
}

/*
 * ncurses_ui_window_switch()
 *
 * Handler for: <i>UI_WINDOW_SWITCH</i><br>
 *
 * Buggy?
 *
 */

static QUERY(ncurses_ui_window_switch) {
	window_t *w	= *(va_arg(ap, window_t **));
	window_t *wc;
	char *p;

	ncurses_window_t *n = w->priv_data;

	if (config_mark_on_window_change)
		command_exec(NULL, NULL, "/mark -1", 1);

	if ((wc = window_find_sa(NULL, "__contacts", 1))) {
		/* XXX, na pewno nie chcemy zapisywac polozenia userlisty podczas zmiany okna? */
		ncurses_contacts_update(wc, 0);
	}

	if (n->redraw)
		ncurses_redraw(w);

	touchwin(n->window);

	update_statusbar(0);
	ncurses_redraw_input(0);	/* redraw prompt... */
	ncurses_commit();

	if (w->act & 2) { /* enable <composing/> on incoming chat message receival */
		w->in_active = 1;
		if (!w->out_active) /* send <active/>, as we showed interest in chat */
			ncurses_window_gone(w);
	}

	p = w->alias ? w->alias : (w->target ? w->target : NULL);
	if (ncurses_settitle)
		printf(ncurses_settitle_formats[ncurses_settitle], p ? p : "", p ? " - " : "", "EKG2");

	return 0;
}

static QUERY(ncurses_ui_window_print)
{
	window_t *w	= *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));

	ncurses_window_t *n;
	int bottom = 0, prev_count, count = 0;

	if (!(n = w->priv_data)) { 
		/* BUGFIX, cause @ ui-window-print handler (not ncurses plugin one, ncurses plugin one is called last cause of 0 prio)
		 *	plugin may call print_window() 
		 */
		ncurses_window_new(w);	
		if (!(n = w->priv_data)) {
			debug("ncurses_ui_window_print() IInd CC still not w->priv_data, quitting...\n");
			return -1;
		}
	}

	prev_count = n->lines_count;

	if (n->start == n->lines_count - w->height || (n->start == 0 && n->lines_count <= w->height))
		bottom = 1;
	
	count = ncurses_backlog_add(w, line);

	if (n->overflow) {
		n->overflow -= count;

		if (n->overflow < 0) {
			bottom = 1;
			n->overflow = 0;
		}
	}

	if (bottom)
		n->start = n->lines_count - w->height;
	else {
		if (n->backlog_size == config_backlog_size)
			n->start -= count - (n->lines_count - prev_count);
	}

	if (n->start < 0)
		n->start = 0;

	if (n->start < n->lines_count - w->height)
		w->more = 1;

	if (!w->floating) {
		ncurses_redraw(w);
		if (w->lock == 0) // && w == window_current) it should be tested
			ncurses_commit();
	}
	
	return 0;
}

static QUERY(ncurses_ui_window_new)
{
	window_t **w = va_arg(ap, window_t **);

	ncurses_window_new(*w);

	return 0;
}

static QUERY(ncurses_ui_window_kill)
{
	window_t **w = va_arg(ap, window_t **);

	ncurses_window_kill(*w);

	return 0;
}

static QUERY(ncurses_ui_window_act_changed)
{
	update_statusbar(1);

	return 0;
}

static QUERY(ncurses_ui_window_target_changed)
{
	window_t *w = *(va_arg(ap, window_t **));
	ncurses_window_t *n = w->priv_data;
	char *tmp, *p;

	xfree(n->prompt);

	p = w->alias ? w->alias : (w->target ? w->target : NULL);
	tmp = format_string(format_find((p) ? "ncurses_prompt_query" : "ncurses_prompt_none"), p);
	n->prompt = tmp; 
	n->prompt_len = xstrlen(tmp);

	ncurses_update_real_prompt(n);

	update_statusbar(1);

	return 0;
}

static QUERY(ncurses_ui_window_refresh)
{
	ncurses_refresh();
	ncurses_commit();

	return 0;
}

static QUERY(ncurses_ui_refresh)
{
/* XXX, code from ncurses_ui_window_refresh() */
	ncurses_refresh();
	ncurses_commit();
/* XXX, code from ekg1 /window refresh */
/*	window_floating_update(0); */		/* done by ncurses_refresh() */
	wrefresh(curscr);

/* XXX, research */
	return 0;
}

static QUERY(ncurses_ui_window_clear)
{
	window_t **w = va_arg(ap, window_t **);

	ncurses_clear(*w, !config_traditional_clear);
	ncurses_commit();

	return 0;
}

/*
 * ncurses_all_contacts_changed()
 *
 * wywo�ywane przy zmianach userlisty powoduj�cych konieczno��
 * podkasowania sorted_all_cache (zmiany w metakontaktach 
 * i ncurses:contacts_metacontacts_swallow)
 */

/* podanie NULL jako data do ncurses_all_contacts_changed() nie spowoduje zmiany polozenia userlisty (co wcale nie znaczy ze bedzie pokazywac na stary element) */
static QUERY(ncurses_all_contacts_changed)
{
	window_t *w;

/*	ncurses_contacts_changed(data); */

	if ((w = window_find_sa(NULL, "__contacts", 1))) {
		ncurses_contacts_update(w, !data);
		ncurses_commit();
	}
	return 0;
}


static QUERY(ncurses_variable_changed)
{
	char *name = *(va_arg(ap, char**));

	if (!xstrcasecmp(name, "sort_windows") && config_sort_windows) {
		window_t *w;
		int id = 2;

		for (w = windows; w; w = w->next) {
			if (w->floating)
				continue;

			if (w->id > 1)
				w->id = id++;
		}
	} else if (!xstrcasecmp(name, "timestamp") || !xstrcasecmp(name, "timestamp_show") || !xstrcasecmp(name, "ncurses:margin_size")) {
		window_t *w;

		for (w = windows; w; w = w->next)
			ncurses_backlog_split(w, 1, 0);

		ncurses_resize();
	}

/*	ncurses_contacts_update(NULL); */
	update_statusbar(1);

	return 0;
}

static QUERY(ncurses_conference_renamed)
{
	char *oldname = *(va_arg(ap, char**));
	char *newname = *(va_arg(ap, char**));
	window_t *w;

	for (w = windows; w; w = w->next) {
		ncurses_window_t *n = w->priv_data;

		if (w->target && !xstrcasecmp(w->target, oldname)) {
			xfree(w->target);
			xfree(n->prompt);
			w->target = xstrdup(newname);
			n->prompt = format_string(format_find("ncurses_prompt_query"), newname);
			n->prompt_len = xstrlen(n->prompt);
			ncurses_update_real_prompt(n);
		}
	}

/*	ncurses_contacts_update(NULL); */
	update_statusbar(1);

	return 0;
}

/*
 * changed_aspell()
 *
 * wywo�ywane po zmianie warto�ci zmiennej ,,aspell'' lub ,,aspell_lang'' lub ,,aspell_encoding''.
 */
static void ncurses_changed_aspell(const char *var)
{
#ifdef WITH_ASPELL
	/* probujemy zainicjowac jeszcze raz aspell'a */
	if (!in_autoexec)
		ncurses_spellcheck_init();
#endif
}

static QUERY(ncurses_postinit)
{
#ifdef WITH_ASPELL
	ncurses_spellcheck_init();
#endif
	ncurses_contacts_changed(NULL);
	return 0;
}

static QUERY(ncurses_binding_set_query)
{
	char *p1 = va_arg(ap, char *);
	char *p2 = va_arg(ap, char *);
	int quiet = va_arg(ap, int);

	ncurses_binding_set(quiet, p1, p2);
	
	return 0;
}

static QUERY(ncurses_binding_adddelete_query)
{
	int add = va_arg(ap, int);
	char *p2 = va_arg(ap, char *);
	char *p3 = va_arg(ap, char *);
	int quiet = va_arg(ap, int);

	if (add)	ncurses_binding_add(p2, p3, 0, quiet);
	else		ncurses_binding_delete(p2, quiet);

/*	ncurses_contacts_update(NULL); */
	update_statusbar(1);

	return 0;
}

static QUERY(ncurses_lastlog_changed) {
	window_t *w;

	if (config_lastlog_size < 0) 
		config_lastlog_size = 0;

	if (!(w = window_find_sa(NULL, "__lastlog", 1)))
		return 0;

	ncurses_lastlog_new(w);
	ncurses_lastlog_update(w);

	ncurses_resize();
	ncurses_commit();
	return 0;
}

static QUERY(ncurses_ui_window_lastlog) {
	window_t *w;
	ncurses_window_t *n;

	int lock_old = config_lastlog_lock;
	int retval;

	if (!(w = window_find_sa(NULL, "__lastlog", 1)))
		w = window_new("__lastlog", NULL, 1001);

	n = w->priv_data;

	if (!n || !n->handle_redraw) {
		debug_error("ncurses_ui_window_lastlog() BAD __lastlog wnd?\n");
		return -1;
	}

	config_lastlog_lock = 0;
	if (!(retval = n->handle_redraw(w)) && !config_lastlog_noitems) {	/* if we don't want __backlog wnd when no items founded.. */
		/* destroy __backlog */
		window_kill(w);
		config_lastlog_lock = lock_old;
/* XXX bugnotes, when killing visible w->floating window we should do: implement in window_kill() */
		ncurses_resize();
		ncurses_commit();
		return 0;
	}

	n->start = n->lines_count - w->height + n->overflow;
	config_lastlog_lock = 1;
	ncurses_redraw(w);
	config_lastlog_lock = lock_old;
	return retval;
}

static QUERY(ncurses_setvar_default)
{
	config_contacts_size = 9;	  /* szeroko�� okna kontakt�w */
	config_contacts = 2;		  /* czy ma by� okno kontakt�w */
	config_contacts_edge = 2;
	config_contacts_frame = 1;
	config_contacts_margin = 1;
	config_contacts_wrap = 0;
	config_contacts_descr = 0;
	config_contacts_orderbystate = 1;

	config_lastlog_size = 10;	  /* szerokosc/dlugosc okna kontaktow */
	config_lastlog_lock = 1;	  /* czy blokujemy lastloga.. zeby nam nie zmienialo sie w czasie zmiany okna, *wolne* */

	xfree(config_contacts_order);
	xfree(config_contacts_groups);

	config_contacts_order = NULL;
	config_contacts_groups = NULL;	  /* grupy listy kontakt�w */
	config_contacts_groups_all_sessions = 0;    /* all sessions ? */
	config_contacts_metacontacts_swallow = 1;

	config_backlog_size = 1000;	    /* maksymalny rozmiar backloga */
	config_backlog_scroll_half_page = 1;	    /* tryb przewijania: p� ekranu lub ekran bez jednej linii */
	config_display_transparent = 1;     /* czy chcemy przezroczyste t�o? */
	config_kill_irc_window = 1;	    /* czy zamyka� kana�y ircowe przez alt-k? */
	config_statusbar_size = 1;
	config_header_size = 0;
	config_enter_scrolls = 0;
	config_margin_size = 15;
	config_mark_on_window_change = 0;
#ifdef WITH_ASPELL
	xfree(config_aspell_lang);

	config_aspell_lang = xstrdup("pl");
#endif
	return 0;
}

/*
 * ncurses_display_transparent_changed ()
 *
 * called when var display_transparent is changed 
 */
static void ncurses_display_transparent_changed(const char *var)
{
	int background;
	if (in_autoexec && config_display_transparent) return;	/* stuff already inited @ ncurses_init() */

	if (config_display_transparent) {
		background = COLOR_DEFAULT;
		use_default_colors();
	} else {
		background = COLOR_BLACK;
		assume_default_colors(COLOR_WHITE, COLOR_BLACK);
	}
	init_pair(7, COLOR_BLACK, background); 
	init_pair(1, COLOR_RED, background);
	init_pair(2, COLOR_GREEN, background);
	init_pair(3, COLOR_YELLOW, background);
	init_pair(4, COLOR_BLUE, background);
	init_pair(5, COLOR_MAGENTA, background);
	init_pair(6, COLOR_CYAN, background);

	endwin();
	refresh();
	/* it will call what's needed */
	header_statusbar_resize(NULL);

	changed_backlog_size("backlog_size");
}

volatile int sigint_count = 0;
static void ncurses_sigint_handler(int s)
{
	if (sigint_count++ > 4) {
		ekg_exit();
	} else {
		/* this'll make some shit with ncurses_bind_set
		 * but I think someone will solve how to do it better
		 * G */
		ungetch(3);
		ncurses_watch_stdin(0, 0, 0, NULL);
		signal(SIGINT, ncurses_sigint_handler); /* odswiezenie handlera */
	}
}

static void ncurses_typing_retimer(const char *dummy) {
	timer_remove(&ncurses_plugin, "ncurses:typing");
	if (config_typing_interval > 0)
		timer_add(&ncurses_plugin, "ncurses:typing", config_typing_interval, 1, ncurses_typing, NULL);
}

static COMMAND(ncurses_cmd_dump) {
	window_t *w = NULL;
	ncurses_window_t *n;
	FILE *f;
	const char *fname = NULL, *fmode = "w";
	int i, id;

	for (i=0; params[i]; i++) {
		if (match_arg(params[i], 'a', ("append"), 2))
			fmode = "a";
		else if (match_arg(params[i], 'w', ("window"), 2)) {
			if (!params[i+1]) {
				printq("not_enough_params", name);
				return -1;
			}
			i++;
			w = window_find(params[i]);	// window target?
			if (!w && ((id = atoi(params[i])) || !xstrcmp(params[i], "0")))	// window id?
				w = window_exist(id);
			if (!w) {
				printq("window_doesnt_exist", params[i]);
				return -1;
			}
		} else if (!fname)
			fname = params[i];
		else {	// fname again?
			printq("invalid_params", name);
			return -1;
		}
	}

	if (!w)
		w = window_current;

	if (!fname)
		fname = "ekg2-dump.txt";

	if (!(f = fopen(fname, fmode)))
		return -1;

	fprintf(f, "---------- Window %s (id:%d) dump. ----------\n", window_target(w), w->id);

	n = w->priv_data;

	for (i = n->backlog_size; i; i--) {
		fstring_t *backlog = n->backlog[i-1];
		/* XXX, kolorki gdy user chce */

#ifdef USE_UNICODE
		fprintf(f, "%ld %ls\n", backlog->ts, backlog->str.w);
#else
		fprintf(f, "%ld %s\n", backlog->ts, backlog->str.b);
#endif
	}

	fclose(f);
	// XXX add print_info() about dump success???
	return 0;
}

static int ncurses_theme_init() {
#ifndef NO_DEFAULT_THEME
	/* prompty i statusy dla ui-ncurses */
	format_add("ncurses_prompt_none", "", 1);
	format_add("ncurses_prompt_query", "[%1] ", 1);
	format_add("statusbar", " %c(%w%{time}%c)%w %c(%w%{?session %{?away %G}%{?avail %Y}%{?chat %W}%{?dnd %K}%{?xa %g}%{?gone %R}"
			"%{?invisible %C}%{?notavail %r}%{session}}%{?!session ---}%c) %{?window (%wwin%c/%w%{?typing %C}%{window}}"
			"%{?query %c:%W%{query}}%{?debug %c(%Cdebug}%c)%w%{?activity  %c(%wact%c/%W}%{activity}%{?activity %c)%w}"
			"%{?mail  %c(%wmail%c/%w}%{mail}%{?mail %c)}%{?more  %c(%Gmore%c)}", 1);
	format_add("header", " %{?query %c(%{?query_away %w}%{?query_avail %W}%{?query_invisible %K}%{?query_notavail %k}"
			"%{?query_chat %W}%{?query_dnd %K}%{query_xa %g}%{?query_gone %R}%{?query_unknown %M}%{?query_error %m}%{?query_blocking %m}"
			"%{query}%{?query_descr %c/%w%{query_descr}}%c) %{?query_ip (%wip%c/%w%{query_ip}%c)} %{irctopic}}"
			"%{?!query %c(%wekg2%c/%w%{version}%c) (%w%{url}%c)}", 1);
	format_add("statusbar_act_important", "%Y", 1);
	format_add("statusbar_act_important2us", "%W", 1);
	format_add("statusbar_act", "%K", 1);
	format_add("statusbar_act_typing", "%c", 1);
	format_add("statusbar_act_important_typing", "%C", 1);
	format_add("statusbar_act_important2us_typing", "%C", 1);
	format_add("statusbar_timestamp", "%H:%M", 1);

#ifdef WITH_ASPELL
	/* aspell */
	format_add("aspell_init", "%> Please wait while initiating spellcheck...", 1);
	format_add("aspell_init_success", "%> Spellcheck initiated.", 1);
	format_add("aspell_init_error", "%! Spellcheck error: %T%1%", 1);
#endif 
#endif
	return 0;
}

EXPORT int ncurses_plugin_init(int prio)
{
	window_t *w;
	int is_UI = 0;
	va_list dummy;
	char *termtype = getenv("TERM");

	PLUGIN_CHECK_VER("ncurses");

	query_emit_id(NULL, UI_IS_INITIALIZED, &is_UI);

	if (is_UI) 
		return -1;
	plugin_register(&ncurses_plugin, prio);

	ncurses_setvar_default(NULL, dummy);

	query_connect_id(&ncurses_plugin, SET_VARS_DEFAULT, ncurses_setvar_default, NULL);
	query_connect_id(&ncurses_plugin, UI_BEEP, ncurses_beep, NULL);
	query_connect_id(&ncurses_plugin, UI_IS_INITIALIZED, ncurses_ui_is_initialized, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_SWITCH, ncurses_ui_window_switch, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_PRINT, ncurses_ui_window_print, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_NEW, ncurses_ui_window_new, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_KILL, ncurses_ui_window_kill, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_TARGET_CHANGED, ncurses_ui_window_target_changed, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_ACT_CHANGED, ncurses_ui_window_act_changed, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_REFRESH, ncurses_ui_window_refresh, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_CLEAR, ncurses_ui_window_clear, NULL);
	query_connect_id(&ncurses_plugin, UI_WINDOW_UPDATE_LASTLOG, ncurses_ui_window_lastlog, NULL);
	query_connect_id(&ncurses_plugin, UI_REFRESH, ncurses_ui_refresh, NULL);
	query_connect_id(&ncurses_plugin, UI_PASSWORD_INPUT, ncurses_password_input, NULL);
	query_connect_id(&ncurses_plugin, SESSION_ADDED, ncurses_statusbar_query, NULL);
	query_connect_id(&ncurses_plugin, SESSION_REMOVED, ncurses_statusbar_query, NULL);
	query_connect_id(&ncurses_plugin, SESSION_EVENT, ncurses_statusbar_query, NULL);
	query_connect_id(&ncurses_plugin, SESSION_RENAMED, ncurses_statusbar_query, NULL);
	query_connect_id(&ncurses_plugin, BINDING_SET, ncurses_binding_set_query, NULL);
	query_connect_id(&ncurses_plugin, BINDING_COMMAND, ncurses_binding_adddelete_query, NULL);
	query_connect_id(&ncurses_plugin, BINDING_DEFAULT, ncurses_binding_default, NULL);
	query_connect_id(&ncurses_plugin, VARIABLE_CHANGED, ncurses_variable_changed, NULL);
	query_connect_id(&ncurses_plugin, CONFERENCE_RENAMED, ncurses_conference_renamed, NULL);

	query_connect_id(&ncurses_plugin, CONFIG_POSTINIT, ncurses_postinit, NULL);
	query_connect_id(&ncurses_plugin, PROTOCOL_DISCONNECTING, ncurses_session_disconnect_handler, NULL);

/* redraw userlisty: */
	/* podanie czegokolwiek jako data do ncurses_all_contacts_changed() powoduje wyzerowanie n->start */

	query_connect_id(&ncurses_plugin, UI_REFRESH, ncurses_all_contacts_changed, (void *) 1);
	query_connect_id(&ncurses_plugin, USERLIST_REFRESH, ncurses_all_contacts_changed, NULL /* ? */);

	query_connect_id(&ncurses_plugin, SESSION_CHANGED, ncurses_all_contacts_changed, (void *) 1);
	query_connect_id(&ncurses_plugin, SESSION_EVENT, ncurses_all_contacts_changed, NULL);

	query_connect_id(&ncurses_plugin, METACONTACT_ADDED, ncurses_all_contacts_changed, NULL);
	query_connect_id(&ncurses_plugin, METACONTACT_REMOVED, ncurses_all_contacts_changed, NULL);
	query_connect_id(&ncurses_plugin, METACONTACT_ITEM_ADDED, ncurses_all_contacts_changed, NULL);
	query_connect_id(&ncurses_plugin, METACONTACT_ITEM_REMOVED, ncurses_all_contacts_changed, NULL);

	query_connect_id(&ncurses_plugin, USERLIST_CHANGED, ncurses_all_contacts_changed, NULL);
	query_connect_id(&ncurses_plugin, USERLIST_ADDED, ncurses_all_contacts_changed, NULL);
	query_connect_id(&ncurses_plugin, USERLIST_REMOVED, ncurses_all_contacts_changed, NULL);
	query_connect_id(&ncurses_plugin, USERLIST_RENAMED, ncurses_all_contacts_changed, NULL);

	command_add(&ncurses_plugin, ("mark"), NULL, cmd_mark, 0, "-a --all");
	command_add(&ncurses_plugin, ("dump"), NULL, ncurses_cmd_dump, 0, "-a --append -w --window");

#ifdef WITH_ASPELL
	variable_add(&ncurses_plugin, ("aspell"), VAR_BOOL, 1, &config_aspell, ncurses_changed_aspell, NULL, NULL);
	variable_add(&ncurses_plugin, ("aspell_lang"), VAR_STR, 1, &config_aspell_lang, ncurses_changed_aspell, NULL, NULL);
#endif
	variable_add(&ncurses_plugin, ("backlog_size"), VAR_INT, 1, &config_backlog_size, changed_backlog_size, NULL, NULL);
	variable_add(&ncurses_plugin, ("backlog_scroll_half_page"), VAR_BOOL, 1, &config_backlog_scroll_half_page, NULL, NULL, NULL);
	/* this isn't very nice solution, but other solutions would require _more_
	 * changes...
	 */
	variable_add(&ncurses_plugin, ("contacts"), VAR_INT, 1, &config_contacts, ncurses_contacts_changed, NULL, NULL);
	variable_add(&ncurses_plugin, ("contacts_descr"), VAR_BOOL, 1, &config_contacts_descr, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_edge"), VAR_INT, 1, &config_contacts_edge, ncurses_contacts_changed, variable_map(4, 0, 0, "left", 1, 0, "top", 2, 0, "right", 3, 0, "bottom"), dd_contacts);

	variable_add(&ncurses_plugin, ("contacts_frame"), VAR_BOOL, 1, &config_contacts_frame, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_groups"), VAR_STR, 1, &config_contacts_groups, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_groups_all_sessons"), VAR_BOOL, 1, &config_contacts_groups_all_sessions, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_margin"), VAR_INT, 1, &config_contacts_margin, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_metacontacts_swallow"), VAR_BOOL, 1, &config_contacts_metacontacts_swallow, (void (*)(const char *))ncurses_all_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_order"), VAR_STR, 1, &config_contacts_order, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_orderbystate"), VAR_BOOL, 1, &config_contacts_orderbystate, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_size"), VAR_INT, 1, &config_contacts_size, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("contacts_wrap"), VAR_BOOL, 1, &config_contacts_wrap, ncurses_contacts_changed, NULL, dd_contacts);
	variable_add(&ncurses_plugin, ("lastlog_size"), VAR_INT, 1, &config_lastlog_size, (void (*)(const char *))ncurses_lastlog_changed, NULL, NULL);
	variable_add(&ncurses_plugin, ("lastlog_lock"), VAR_BOOL, 1, &config_lastlog_lock, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, ("display_transparent"), VAR_BOOL, 1, &config_display_transparent, ncurses_display_transparent_changed, NULL, NULL);
	variable_add(&ncurses_plugin, ("enter_scrolls"), VAR_BOOL, 1, &config_enter_scrolls, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, ("header_size"), VAR_INT, 1, &config_header_size, header_statusbar_resize, NULL, NULL);
	variable_add(&ncurses_plugin, ("kill_irc_window"),  VAR_BOOL, 1, &config_kill_irc_window, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, ("margin_size"), VAR_INT, 1, &config_margin_size, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, ("mark_on_window_change"), VAR_BOOL, 1, &config_mark_on_window_change, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, ("statusbar_size"), VAR_INT, 1, &config_statusbar_size, header_statusbar_resize, NULL, NULL);
	variable_add(&ncurses_plugin, ("text_bottomalign"), VAR_INT, 1, &config_text_bottomalign, NULL,
			variable_map(3, 0, 0, "off", 1, 2, "except-floating", 2, 1, "all"), NULL);
	variable_add(&ncurses_plugin, ("traditional_clear"), VAR_BOOL, 1, &config_traditional_clear, NULL, NULL, NULL);

	variable_add(&ncurses_plugin, ("typing_interval"), VAR_INT, 1, &config_typing_interval, ncurses_typing_retimer, NULL, NULL);
	variable_add(&ncurses_plugin, ("typing_timeout"), VAR_INT, 1, &config_typing_timeout, NULL, NULL, NULL);
	variable_add(&ncurses_plugin, ("typing_timeout_empty"), VAR_INT, 1, &config_typing_timeout_empty, NULL, NULL, NULL);
	
	have_winch_pipe = 0;
#ifdef SIGWINCH
	if (pipe(winch_pipe) == 0) {
		have_winch_pipe = 1;
		watch_add(&ncurses_plugin,
				winch_pipe[0],
				WATCH_READ,
				ncurses_watch_winch,	/* handler */
				NULL);			/* data */
	}
#endif
	watch_add(&ncurses_plugin, 0, WATCH_READ, ncurses_watch_stdin, NULL);
	signal(SIGINT, ncurses_sigint_handler);
	timer_add(&ncurses_plugin, "ncurses:clock", 1, 1, ncurses_statusbar_timer, NULL);

	ncurses_init();
	
	header_statusbar_resize(NULL);
	ncurses_typing_retimer(NULL);

	for (w = windows; w; w = w->next)
		ncurses_window_new(w);

	ncurses_initialized = 1;

	if (!no_mouse)
		ncurses_enable_mouse(termtype);

	if (termtype) {
		/* determine window title setting support */
		if (!xstrncasecmp(termtype, "screen", 6))
			ncurses_settitle = 2;
		else if (!xstrncasecmp(termtype, "xterm", 5) || !xstrncasecmp(termtype, "rxvt", 4) || !xstrncasecmp(termtype, "gnome", 5)
				|| ((*termtype == 'E' || *termtype == 'a' || *termtype == 'k') && !xstrcasecmp(termtype+1, "term")))
			ncurses_settitle = 1;
	}

	if (ncurses_settitle)
		printf(ncurses_settitle_formats[ncurses_settitle], "", "", "EKG2");

	return 0;
}

static int ncurses_plugin_destroy()
{
	ncurses_plugin_destroyed = 1;
	ncurses_initialized = 0;

	ncurses_disable_mouse(); 

	watch_remove(&ncurses_plugin, 0, WATCH_READ);
	if (have_winch_pipe)
		watch_remove(&ncurses_plugin, winch_pipe[0], WATCH_READ);

	timer_remove(&ncurses_plugin, "ncurses:clock");

	ncurses_deinit();

	plugin_unregister(&ncurses_plugin);

	return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
