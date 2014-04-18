/* Integrity checks for ekg2 API
 * (c) 2011 Michał Górny
 */

#include "ekg2.h"

#include <stdio.h>

void add_recode_tests(void);
void add_static_aborts_tests(void);

PLUGIN_DEFINE(check, PLUGIN_UI, NULL);

static void simple_errprint(const gchar *out) {
	fputs(out, stderr);
}

EXPORT int check_plugin_init(int prio) {
	int argc = 1;
	char *argv[] = { "ekg2", NULL };
	char **argvp = argv;

	g_set_print_handler(simple_errprint);
	g_set_printerr_handler(simple_errprint);
	g_log_set_default_handler(g_log_default_handler, NULL);

	g_test_init(&argc, &argvp, NULL);

	add_recode_tests();
	add_static_aborts_tests();

	g_test_run();
	ekg_exit();
	g_assert_not_reached();
}

static int check_plugin_destroy(void) {
	return 0;
}
