#include "ekg2-config.h"

#include <ekg/themes.h>

#include <ruby.h>
#include "ruby_ekg.h"

static VALUE ruby_print(int argc, VALUE *argv, VALUE self) {
	script_t *scr = ruby_find_script(self);

	if (!scr) {
		rb_raise(RUBY_EKG_INTERNAL_ERROR, "@ handler_bind internal error");
		return Qnil;
	}
	if (argc < 1 || argc > 10) rb_raise(rb_eArgError, "print() accepts 1-10 params, but %d given", argc);

	Check_Type(argv[0], T_STRING);

	if (argc == 1)
		print("script_generic", "ruby", scr->name, RSTRING(argv[0])->ptr);
	else {
		char *args[9] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
		int i;

		for (i = 1; i < argc; i++) {
			Check_Type(argv[i], T_STRING);
			args[i-1] = RSTRING(argv[i])->ptr;
		}

		print(RSTRING(argv[0])->ptr, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
	}

	return Qnil;
}

static VALUE ruby_print_window(int argc, VALUE *argv, VALUE self) {
#if 0
	window_t *w = NULL;
	char *args[9] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	int i;

	if (type(argv[0]) == T_FIXNUM) {	/* id of window. */
		if (argc < 1 || argv > 10) {
			rb_raise(rb_eArgError, "print_window(): accepts 1-10 params, but %d given", argc);
			return Qnil;
		}

		if (!(w = window_exist(FIX2INT(argv[0])))) {
			rb_raise(rb_eIndexError, "print_window(): window_exist(%d) failed\n", FIX2INT(argv[0]));
			return Qnil;
		}

	} else if (TYPE(argv[0]) == T_STRING) {	/* target */

		if (TYPE(argv[1]) == T_STRING) 


	}

	if (TYPE(obj) == T_STRING)	query_name = RSTRING(argv[0])->ptr;
	else if (TYPE(obj) == T_FIXNUM); /* XXX ? */

	Check_Type(argv[0], T_STRING);

	for (i = 1; i < argc; i++) {
		Check_Type(argv[i], T_STRING);
		args[i-1] = RSTRING(argv[i])->ptr;
	}

	tmp = str = xstrdup(RSTRING(argv[0])->ptr);

        while ((line = split_line(&str))) {
		char *tmp = format_string(line);
		window_print(window_exist(dest), fstring_new(tmp));
		xfree(tmp);
        }
	xfree(tmp);
#endif
	return Qnil;
}

static VALUE ekg2_ruby_theme;

static VALUE ruby_format_add(int argc, VALUE *argv, VALUE self) {
	int replace = 1;
	char *name;

	if (argc != 2 && argc != 3) rb_raise(rb_eArgError, "format_add() accepts 2 or 3 params, but %d given", argc);

	Check_Type(argv[0], T_STRING);
	Check_Type(argv[1], T_STRING);

	name = RSTRING(argv[0])->ptr;

	if (argc == 3) {
		Check_Type(argv[2], T_FIXNUM);
		replace = FIX2INT(argv[2]);
	}
	
	format_add(name, RSTRING(argv[1])->ptr, replace);

	rb_iv_set(self, "@name", rb_str_new2(name));

	return ekg2_ruby_theme;
}

static VALUE ruby_format_find(int argc, VALUE *argv, VALUE self) {
	if (argc != 1) rb_raise(rb_eArgError, "format_find() accepts 1 param, but %d given", argc);

	Check_Type(argv[0], T_STRING);
	return rb_str_new2(format_find(RSTRING(argv[0])->ptr));
//	return ekg2_ruby_theme;
}

void ruby_define_theme_class(VALUE module) {
	ekg2_ruby_theme = rb_define_class_under(module, "Format", rb_cObject);

	rb_define_method(ekg2_ruby_theme, "initialize", ruby_format_add, -1);			/* format_add() */
	rb_define_singleton_method(ekg2_ruby_theme, "find", ruby_format_find, -1);		/* format_find() */

	rb_define_method(module, "format_add", ruby_format_add, -1);
	rb_define_method(module, "format_find", ruby_format_find, -1);

	rb_define_method(module, "print", ruby_print, -1);
	rb_define_method(module, "print_window", ruby_print_window, -1);
}

