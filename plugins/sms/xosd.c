/* $Id$ */

/*
 *  (C) Copyright 2004 Piotr Kupisiewicz <deletek@ekg2.org>
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

#include "ekg2-config.h"

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

static int xosd_plugin_destroy();

static plugin_t xosd_plugin = {
        name: "xosd",
        pclass: PLUGIN_GENERIC,
        destroy: xosd_plugin_destroy,
        theme_init: NULL,
};

int xosd_plugin_init()
{
        plugin_register(&xosd_plugin);

	/* Here should be all query_emit() etc. */

        return 0;
}

static int xosd_plugin_destroy()
{
        plugin_unregister(&xosd_plugin);

	/* deinitialization */

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
