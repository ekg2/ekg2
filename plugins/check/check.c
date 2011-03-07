/* Integrity checks for ekg2 API
 * (c) 2011 Michał Górny
 */

#include "ekg2.h"

PLUGIN_DEFINE(check, PLUGIN_UI, NULL);

EXPORT int check_plugin_init(int prio) {
	int argc = 0;
	char **argv = { NULL };

	g_test_init(&argc, &argv, NULL);

	g_test_run();
	ekg_exit();
	g_assert_not_reached();
}

static int check_plugin_destroy(void) {
	return 0;
}
