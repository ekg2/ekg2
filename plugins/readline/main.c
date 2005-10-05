#include "ekg2-config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include <readline.h>
#include "ui-readline.h"

static int readline_theme_init();
PLUGIN_DEFINE(readline, PLUGIN_UI, readline_theme_init);

static int readline_theme_init() {
//        /* prompty dla ui-readline */
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
	return 0;
}

QUERY(readline_ui_window_new) {
/* static window_t *window_add() */
	window_t *w = *(va_arg(ap, window_t **));
	w->private = xmalloc(sizeof(readline_window_t));
	return 0;
}

QUERY(readline_ui_window_kill) { /* window_free */
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

QUERY(readline_ui_window_switch) { /* window_switch */
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

QUERY(readline_ekg2_loop) {
	ui_readline_loop();
	return 0;
}

QUERY(readline_ui_window_print) {
	window_t *w = *(va_arg(ap, window_t **));
	fstring_t *l = *(va_arg(ap, fstring_t **));

	ui_readline_print(w, 1, l->str);
	return 0;
}
QUERY(readline_variable_changed) {
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

QUERY(readline_ui_window_clear) {
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

QUERY(readline_beep) {
/* static void ui_readline_beep() */
	printf("\a");
	fflush(stdout);
	return 0;
}
	
int readline_plugin_init(int prio) {
	list_t l;
	plugin_register(&readline_plugin, prio);

	query_connect(&readline_plugin, "ui-beep", readline_beep, NULL);
	query_connect(&readline_plugin, "ui-window-new", readline_ui_window_new, NULL);
	query_connect(&readline_plugin, "ui-window-switch", readline_ui_window_switch, NULL);
	query_connect(&readline_plugin, "ui-window-kill", readline_ui_window_kill, NULL);
	query_connect(&readline_plugin, "ui-window-print", readline_ui_window_print, NULL);
	query_connect(&readline_plugin, "ui-window-clear", readline_ui_window_clear, NULL);
	query_connect(&readline_plugin, "variable-changed", readline_variable_changed, NULL);
	query_connect(&readline_plugin, "ekg2-loop", readline_ekg2_loop, NULL);

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		w->private = xmalloc(sizeof(readline_window_t));
	}
	ui_readline_init();
	
	return 0;
}


static int readline_plugin_destroy() {
	return 0;

}
