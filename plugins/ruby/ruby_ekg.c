#include "ekg2-config.h"

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/scripts.h>

#undef __
#undef _

#include <ruby.h>

static int ruby_initialize();
static int ruby_finalize_wrapper();
static int ruby_load(script_t *scr);
static int ruby_unload(script_t *scr);
static int ruby_bind_free(script_t *scr, void *data, /* niby to jest ale kiedys nie bedzie.. nie uzywac */ int type, void *private, ...);
static int ruby_query(script_t *scr, script_query_t *scr_que, void *args[]);
static int ruby_commands(script_t *scr, script_command_t *comm, char **params);
static int ruby_timers(script_t *scr, script_timer_t *time, int type);
static int ruby_variable_changed(script_t *scr, script_var_t *scr_var, char *what);
static int ruby_watches(script_t *scr, script_watch_t *scr_wat, int type, int fd, int watch);

PLUGIN_DEFINE(ruby, PLUGIN_SCRIPTING, NULL);

scriptlang_t ruby_lang = { /* SCRIPT_DEFINE(ruby, ".rb"); */
	name: "ruby",
	plugin: &ruby_plugin,
	ext: ".rb",
	init: ruby_initialize,
	deinit: ruby_finalize_wrapper,
	script_load: ruby_load,
	script_unload: ruby_unload,
	script_free_bind: ruby_bind_free,
	script_handler_query : ruby_query,
	script_handler_command: ruby_commands,
	script_handler_timer : ruby_timers,
	script_handler_var : ruby_variable_changed,
	script_handler_watch : ruby_watches
};


static int ruby_finalize_wrapper() {
	ruby_finalize();
	return 0;
}

static int ruby_initialize() {
	ruby_init();
	ruby_init_loadpath();
	ruby_script("ekg2");

	return 0;
}

VALUE ruby_load_wrapper(VALUE arg) {
	rb_require((const char *) arg);
	return Qnil;
}

static int ruby_load(script_t *scr) {
	int error = 0;
	
	rb_protect(ruby_load_wrapper, scr->path, &error);
	
	if (error) {
		/* XXX */
		debug_error("ruby_load() error: %s\n", "dupa");
		return -1;
	}

	return 1;
}

static int ruby_unload(script_t *scr) {

	return 0;
}

static int ruby_bind_free(script_t *scr, void *data, /* niby to jest ale kiedys nie bedzie.. nie uzywac */ int type, void *private, ...) {

	return 0;
}

static int ruby_query(script_t *scr, script_query_t *scr_que, void *args[]) {
	return 0;
}

static int ruby_commands(script_t *scr, script_command_t *comm, char **params) {

	return 0;
}

/* IF WATCH_READ_LINE int type == char *line */
static int ruby_watches(script_t *scr, script_watch_t *scr_wat, int type, int fd, int watch) {

	return 0;
}

static int ruby_variable_changed(script_t *scr, script_var_t *scr_var, char *what) {

	return 0;
}

static int ruby_timers(script_t *scr, script_timer_t *time, int type) {

	return 0;
}

EXPORT int ruby_plugin_init(int prio) {
	plugin_register(&ruby_plugin, prio);
	scriptlang_register(&ruby_lang);
	return 0;
}

static int ruby_plugin_destroy() {
	scriptlang_unregister(&ruby_lang);
	plugin_unregister(&ruby_plugin);
	return 0;
}
