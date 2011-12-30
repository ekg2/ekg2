#include "ekg/abort.h"
#include <glib.h>

static plugin_t p, p2;

static int handler1_called = 0;
static int handler2_called = 0;

static void handler1(void)
{
	handler1_called++;
}

static void handler2(void)
{
	handler2_called++;
}

static void reset_counters(void)
{
	handler1_called = 0;
	handler2_called = 0;
}

void basics(void) {
	g_assert(! handler1_called);
	g_assert(! handler2_called);

	ekg2_run_all_abort_handlers();
	g_assert(! handler1_called);
	g_assert(! handler2_called);

	reset_counters();
	g_assert(ekg2_register_abort_handler(handler1, &p));
	ekg2_run_all_abort_handlers();
	g_assert_cmpint(handler1_called, ==, 1);
	g_assert(! handler2_called);
	g_assert_cmpint(ekg2_unregister_abort_handlers_for_plugin(&p), ==, 1);
}

void interleaved(void) {
	reset_counters();
	g_assert(ekg2_register_abort_handler(handler1, &p2));
	g_assert(ekg2_register_abort_handler(handler1, &p));
	g_assert(ekg2_register_abort_handler(handler2, &p));
	g_assert_cmpint(ekg2_unregister_abort_handlers_for_plugin(&p2), ==, 1);
	g_assert(ekg2_register_abort_handler(handler2, &p));
	ekg2_run_all_abort_handlers();
	g_assert_cmpint(handler1_called, ==, 1);
	g_assert_cmpint(handler2_called, ==, 2);
	g_assert_cmpint(ekg2_unregister_abort_handlers_for_plugin(&p), ==, 3);
}

void capacity(void) {
	int number_exceeding_abort_handler_capacity = 500;
	int i, capacity = -1;

	reset_counters();
	for (i = 1; i <= number_exceeding_abort_handler_capacity; i++) {
		if (! ekg2_register_abort_handler(handler1, &p)) {
			capacity = i - 1;
			break;
		}
	}
	g_assert_cmpint(capacity, >, 0);
	g_assert_cmpint(capacity, <, number_exceeding_abort_handler_capacity);
	ekg2_run_all_abort_handlers();
	g_assert_cmpint(handler1_called, ==, capacity);
	g_assert_cmpint(ekg2_unregister_abort_handlers_for_plugin(&p), ==, capacity);
}

void add_static_aborts_tests(void) {
	g_test_add_func("/abort/basics", basics);
	g_test_add_func("/abort/capacity", capacity);
	g_test_add_func("/abort/interleaved", interleaved);
}
