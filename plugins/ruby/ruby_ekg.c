#include "ekg2-config.h"

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
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

/* XXX, do sprawdzenia */
static char *ruby_geterror(const char *what) {
	string_t err = string_init(what);
	VALUE exception_instance;
	VALUE message;
	VALUE class;
	ID id;

/* where? */
	string_append_format(err, "%s:%d", ruby_sourcefile, ruby_sourceline);

	if ((id = rb_frame_last_func()))
		string_append_format(err, " @%s()", rb_id2name(id));
	string_append_c(err, '\n');

	exception_instance = rb_gv_get("$!");
/* class */
	class = rb_class_path(CLASS_OF(exception_instance));
	string_append_format(err, _("Class: %s\n"), RSTRING(class)->ptr);
/* message */
	message = rb_obj_as_string(exception_instance);
	string_append_format(err, _("Message: %s\n"), RSTRING(message)->ptr);

/* backtrace */
	if(!NIL_P(ruby_errinfo)) {
		VALUE ary = rb_funcall(ruby_errinfo, rb_intern("backtrace"), 0);
		int c;

		string_append(err, "Backtrace:\n");

		for (c=0; c<RARRAY(ary)->len; c++)
			string_append_format(err, "from %s\n", RSTRING(RARRAY(ary)->ptr[c])->ptr);
	}

	return string_free(err, 0);
}

static VALUE ruby_load_wrapper(VALUE arg) {
	rb_require((const char *) arg);
	return Qnil;
}

static int ruby_load(script_t *scr) {
	int error = 0;
	
	rb_protect(ruby_load_wrapper, (VALUE) scr->path, &error);
	
	if (error) {
		char *err = ruby_geterror("ruby_load() ");
		print("script_error", err);
		xfree(err);
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
