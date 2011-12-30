/*  (C) Copyright 2011 Marcin Owsiany <marcin@owsiany.pl>
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

#include "abort.h"
#include <stddef.h>


#define NUM_ABORT_HANDLERS 10

static struct {
	abort_handler handler;
	plugin_t *plugin;
} abort_handlers[NUM_ABORT_HANDLERS];

int ekg2_register_abort_handler(abort_handler handler, plugin_t *plugin)
{
	size_t i;
	for (i = 0; i < NUM_ABORT_HANDLERS; i++) {
		if (! abort_handlers[i].handler) {
			abort_handlers[i].handler = handler;
			abort_handlers[i].plugin = plugin;
			return 1;
		}
	}
	return 0;
}

void ekg2_run_all_abort_handlers(void)
{
	size_t i;
	for (i = 0; i < NUM_ABORT_HANDLERS; i++) {
		if (abort_handlers[i].handler) {
			(*abort_handlers[i].handler)();
		}
	}
}

int ekg2_unregister_abort_handlers_for_plugin(plugin_t *plugin)
{
	int unregistered_handlers_count = 0;
	size_t i;
	for (i = 0; i < NUM_ABORT_HANDLERS; i++) {
		if (abort_handlers[i].handler && abort_handlers[i].plugin == plugin) {
			abort_handlers[i].handler = NULL;
			unregistered_handlers_count++;
		}
	}
	return unregistered_handlers_count;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
