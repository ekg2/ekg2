#include "ekg2-config.h"

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#include <readline.h>
#include "ui-readline.h"

static int readline_theme_init();
PLUGIN_DEFINE(readline, PLUGIN_UI, readline_theme_init);

int config_ctrld_quits = 1;

/*
 * sigint_handler() //XXX może wywalać 
 *
 * obsługuje wciśnięcie Ctrl-C.
 */
static void sigint_handler()
{
	rl_delete_text(0, rl_end);
	rl_point = rl_end = 0;
	putchar('\n');
	rl_forced_update_display();
}

/*
 * sigcont_handler() //XXX może wywalać
 *
 * osługuje powrót z tła poleceniem ,,fg'', żeby odświeżyć ekran.
 */
static void sigcont_handler()
{
	rl_forced_update_display();
}

/*
 * sigwinch_handler()
 *
 * obsługuje zmianę rozmiaru okna.
 */
#ifdef SIGWINCH
static void sigwinch_handler()
{
	ui_need_refresh = 1;
}
#endif

static int readline_theme_init() {
#ifndef NO_DEFAULT_THEME
	/* prompty dla ui-readline */
	format_add("readline_prompt", "% ", 1);
	format_add("readline_prompt_away", "/ ", 1);
	format_add("readline_prompt_invisible", ". ", 1);
	format_add("readline_prompt_query", "%1> ", 1);
	format_add("readline_prompt_win", "%1%% ", 1);
	format_add("readline_prompt_away_win", "%1/ ", 1);
	format_add("readline_prompt_invisible_win", "%1. ", 1);
	format_add("readline_prompt_query_win", "%2:%1> ", 1);
	format_add("readline_prompt_win_act", "%1 (act/%2)%% ", 1);
	format_add("readline_prompt_away_win_act", "%1 (act/%2)/ ", 1);
	format_add("readline_prompt_invisible_win_act", "%1 (act/%2). ", 1);
	format_add("readline_prompt_query_win_act", "%2:%1 (act/%3)> ", 1);
	format_add("readline_more", _("-- Press Enter to continue or Ctrl-D to break --"), 1);
#endif
	return 0;
}

static QUERY(readline_ui_window_new) { /* window_add() */
	window_t *w = *(va_arg(ap, window_t **));
	w->private = xmalloc(sizeof(readline_window_t));
	return 0;
}

static QUERY(readline_ui_window_kill) { /* window_free */
	window_t *w = *(va_arg(ap, window_t **));
	readline_window_t *r = w->private;
	int i;

	for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
		xfree(r->line[i]);
		r->line[i] = NULL;
	}
	xfree(r);
	w->private = NULL;
	return 0;
}

static QUERY(readline_ui_window_refresh) {

	return 0;
}

static QUERY(readline_ui_window_switch) { /* window_switch */
	window_t *w = *(va_arg(ap, window_t **));
	window_current = w;
	w->act = 0;
	window_refresh();
#ifdef HAVE_RL_SET_PROMPT
	rl_set_prompt((char *) current_prompt());
#else
	rl_expand_prompt((char *) current_prompt());
#endif
	rl_initialize();
	return 0;
}

static char *readline_change_string_t_back_to_char(const char *str, const short *attr) {
	int i;
	string_t asc = string_init(NULL);

	for (i = 0; i < xstrlen(str); i++) {
#define ISBOLD(x)	(x & 64)
#define ISBLINK(x)	(x & 256) 
#define ISUNDERLINE(x)	(x & 512)
#define ISREVERSE(x)	(x & 1024)
#define FGCOLOR(x)	((!(x & 128)) ? (x & 7) : -1)
#define BGCOLOR(x)	-1	/* XXX */

#define prev	attr[i-1]
#define cur	attr[i] 

		int reset = 1;

	/* attr */
		if (i && !ISBOLD(cur)  && ISBOLD(prev));		/* NOT BOLD */
		else if (i && !ISBLINK(cur) && ISBLINK(prev));		/* NOT BLINK */
		else if (i && !ISUNDERLINE(cur) && ISUNDERLINE(prev));	/* NOT UNDERLINE */
		else if (i && !ISREVERSE(cur) && ISREVERSE(prev));	/* NOT REVERSE */
		else if (i && FGCOLOR(cur) == -1 && FGCOLOR(prev) != -1);/* NO FGCOLOR */
		else if (i && BGCOLOR(cur) == -1 && BGCOLOR(prev) != -1);/* NO BGCOLOR */
		else reset = 0;
		
		if (reset) string_append(asc, ("%n"));

		if (ISBOLD(cur)	&& (!i || reset || ISBOLD(cur) != ISBOLD(prev)) && FGCOLOR(cur) == -1)
			string_append(asc, ("%T"));		/* no color + bold. */

		if (ISBLINK(cur)	&& (!i || reset || ISBLINK(cur) != ISBLINK(prev)))		string_append(asc, ("%i"));
//		if (ISUNDERLINE(cur)	&& (!i || reset || ISUNDERLINE(cur) != ISUNDERLINE(prev)));	string_append(asc, ("%"));
//		if (ISREVERSE(cur)	&& (!i || reset || ISREVERSE(cur) != ISREVERSE(prev)));		string_append(asc, ("%"));

		if (BGCOLOR(cur) != -1 && ((!i || reset || BGCOLOR(cur) != BGCOLOR(prev)))) {	/* if there's a background color... add it */
			string_append_c(asc, '%');
			switch (BGCOLOR(cur)) {
				case (0): string_append_c(asc, 'l'); break;
				case (1): string_append_c(asc, 's'); break;
				case (2): string_append_c(asc, 'h'); break;
				case (3): string_append_c(asc, 'z'); break;
				case (4): string_append_c(asc, 'e'); break;
				case (5): string_append_c(asc, 'q'); break;
				case (6): string_append_c(asc, 'd'); break;
				case (7): string_append_c(asc, 'x'); break;
			}
		}

		if (FGCOLOR(cur) != -1 && ((!i || reset || FGCOLOR(cur) != FGCOLOR(prev)) || (i && ISBOLD(prev) != ISBOLD(cur)))) {	/* if there's a foreground color... add it */
			string_append_c(asc, '%');
			switch (FGCOLOR(cur)) {
				 case (0): string_append_c(asc, ISBOLD(cur) ? 'K' : 'k'); break;
				 case (1): string_append_c(asc, ISBOLD(cur) ? 'R' : 'r'); break;
				 case (2): string_append_c(asc, ISBOLD(cur) ? 'G' : 'g'); break;
				 case (3): string_append_c(asc, ISBOLD(cur) ? 'Y' : 'y'); break;
				 case (4): string_append_c(asc, ISBOLD(cur) ? 'B' : 'b'); break;
				 case (5): string_append_c(asc, ISBOLD(cur) ? 'M' : 'm'); break; /* | fioletowy     | %m/%p  | %M/%P | %q  | */
				 case (6): string_append_c(asc, ISBOLD(cur) ? 'C' : 'c'); break;
				 case (7): string_append_c(asc, ISBOLD(cur) ? 'W' : 'w'); break;
			}
		}

	/* str */
		if (str[i] == '%' || str[i] == '\\') string_append_c(asc, '\\');	/* escape chars.. */
		string_append_c(asc, str[i]);			/* append current char */
	}
	string_append(asc, ("%n"));	/* reset */
	string_append_c(asc, '\n');		/* new line */
	return string_free(asc, 0);
}

static char *readline_ui_window_print_helper(char *str, short *attr) {
	char *ascii = readline_change_string_t_back_to_char(str, attr);
	char *colorful = format_string(ascii);

	xfree(ascii);
	return colorful;
}

static QUERY(readline_ui_window_print) {
	window_t *w = *(va_arg(ap, window_t **));
	fstring_t *l = *(va_arg(ap, fstring_t **));
	char *str = readline_ui_window_print_helper(l->str.b, l->attr);

	ui_readline_print(w, 1, str);
	xfree(str);
	return 0;
}

static QUERY(readline_variable_changed) {
	char *name = *(va_arg(ap, char**));
	if (!xstrcasecmp(name, "sort_windows") && config_sort_windows) {
		list_t l;
		int id = 1;
		for (l = windows; l; l = l->next) {
			window_t *w = l->data;
			w->id = id++;
		}
	}
	return 0;
}

static QUERY(readline_ui_window_clear) {
	int i;
	window_t *w = *(va_arg(ap, window_t **));
	readline_window_t *r = w->private;

	for (i = 0; i < MAX_LINES_PER_SCREEN; i++) {
		xfree(r->line[i]);
		r->line[i] = NULL;
	}
	window_refresh();
	return 0;
}

static QUERY(ekg2_readline_loop) {
	while(ui_readline_loop());
	return -1;
}

static QUERY(readline_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);
	*tmp = 1;
	return 0;
}

static QUERY(readline_beep) { /* ui_readline_beep() */
	printf("\a");
	fflush(stdout);
	return 0;
}

static WATCHER(readline_watch_stdin) {
	return 0;
}
	
EXPORT int readline_plugin_init(int prio) {
	char c;
	struct sigaction sa;
	list_t l;
	int is_UI = 0;

        query_emit_id(NULL, UI_IS_INITIALIZED, &is_UI);

        if (is_UI)
                return -1;

	plugin_register(&readline_plugin, prio);

	query_connect_id(&readline_plugin, UI_BEEP, readline_beep, NULL);
	query_connect_id(&readline_plugin, UI_IS_INITIALIZED, readline_ui_is_initialized, NULL);
	query_connect_id(&readline_plugin, UI_WINDOW_NEW, readline_ui_window_new, NULL);
	query_connect_id(&readline_plugin, UI_WINDOW_SWITCH, readline_ui_window_switch, NULL);
	query_connect_id(&readline_plugin, UI_WINDOW_KILL, readline_ui_window_kill, NULL);
	query_connect_id(&readline_plugin, UI_WINDOW_PRINT, readline_ui_window_print, NULL);
	query_connect_id(&readline_plugin, UI_WINDOW_REFRESH, readline_ui_window_refresh, NULL);
	query_connect_id(&readline_plugin, UI_WINDOW_CLEAR, readline_ui_window_clear, NULL);
	query_connect_id(&readline_plugin, VARIABLE_CHANGED, readline_variable_changed, NULL);
	query_connect_id(&readline_plugin, UI_LOOP, ekg2_readline_loop, NULL);

	variable_add(&readline_plugin, ("ctrld_quits"),  VAR_BOOL, 1, &config_ctrld_quits, NULL, NULL, NULL);

	watch_add(&readline_plugin, 0, WATCH_READ, readline_watch_stdin, NULL);

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		w->private = xmalloc(sizeof(readline_window_t));
	}
	
	window_refresh();
	rl_initialize();
	
	rl_getc_function = my_getc;
	rl_event_hook	 = my_loop;
	rl_readline_name = "ekg2";
	
	rl_attempted_completion_function = (CPPFunction *) my_completion;
	rl_completion_entry_function = (void*) empty_generator;

	rl_set_key("\033[[A", binding_help, emacs_standard_keymap);
	rl_set_key("\033OP", binding_help, emacs_standard_keymap);
	rl_set_key("\033[11~", binding_help, emacs_standard_keymap);
	rl_set_key("\033[M", binding_help, emacs_standard_keymap);
	rl_set_key("\033[[B", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033OQ", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033[12~", binding_quick_list, emacs_standard_keymap);
	rl_set_key("\033[N", binding_quick_list, emacs_standard_keymap);
	
	//rl_set_key("\033[24~", binding_toggle_debug, emacs_standard_keymap);

	for (c = '0'; c <= '9'; c++)
		rl_bind_key_in_map(c, bind_handler_window, emacs_meta_keymap);

	memset(&sa, 0, sizeof(sa));

	sa.sa_handler = sigint_handler;
	sigaction(SIGINT, &sa, NULL);

	sa.sa_handler = sigcont_handler;
	sigaction(SIGCONT, &sa, NULL);

#ifdef SIGWINCH
	sa.sa_handler = sigwinch_handler;
	sigaction(SIGWINCH, &sa, NULL);
#endif

	rl_get_screen_size(&screen_lines, &screen_columns);

	if (screen_lines < 1)
		screen_lines = 24;
	if (screen_columns < 1)
		screen_columns = 80;

	ui_screen_width = screen_columns;
	ui_screen_height = screen_lines;
	ui_need_refresh = 0;

	return 0;
}

static int readline_plugin_destroy() {
	plugin_unregister(&readline_plugin);
	return 0;

}
