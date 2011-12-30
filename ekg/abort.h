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

#ifndef __EKG_ABORT_H
#define __EKG_ABORT_H

/**
 * Statically allocated handlers for fatal signal handling.
 *
 * The reason for all this is to avoid heap references in fatal signal handlers
 * (which are a big no-no), but still allow plugins to do some rudimentary
 * cleanup.
 *
 * Plugins, during initialization, register their abort handlers using
 * ekg2_register_abort_handler().
 *
 * The registered abort handlers will be called by core in cases such as
 * receiving a SIGSEGV or SIGABRT.
 *
 * Core will also unregister all handlers for a given plugin right before
 * unregistering the plugin itself, but there is no harm in the plugin
 * unregistering its own abort handlers if it knows of a more appropriate time.
 */

#include "plugins.h"
#include <glib.h>

G_BEGIN_DECLS

typedef void (*abort_handler)(void);

/**
 * Statically register the abort @a handler function for the @a plugin.
 *
 * @return 1 if successful, 0 if there was no space left.
 */
int ekg2_register_abort_handler(abort_handler handler, plugin_t *plugin);

/**
 * Run all registered abort handlers (possibly none).
 *
 * No particular order of invocation is guaranteed.
 */
void ekg2_run_all_abort_handlers(void);

/**
 * Unregister all abort handlers for @a plugin.
 *
 * @return the number of unregistered handlers.
 */
int ekg2_unregister_abort_handlers_for_plugin(plugin_t *plugin);

G_END_DECLS

#endif /* __EKG_ABORT_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
