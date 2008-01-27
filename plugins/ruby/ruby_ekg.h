#include <ekg/scripts.h>

#define RUBY_EKG_INTERNAL_ERROR rb_eArgError /* XXX */

extern scriptlang_t ruby_lang;

static script_t *ruby_find_script(VALUE self) {
	SCRIPT_FINDER(scr->lang == &ruby_lang && script_private_get(scr) == (void *) self);
}

