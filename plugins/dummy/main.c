/* $Id$ */

/*
 *  (C) Copyright 2003 Jan Kowalski <jan.kowalski@gdzies.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <ekg/plugins.h>

PLUGIN_DEFINE(dummy, PLUGIN_GENERIC, NULL);

int dummy_plugin_init()
{
	plugin_register(&dummy_plugin);

	debug("dummy plugin registered\n");

	return 0;
}

static int dummy_plugin_destroy()
{
	plugin_unregister(&dummy_plugin);

	debug("dummy plugin unregistered\n");

	return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
