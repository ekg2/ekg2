/* $Id$ */

/*
 *  (C) Copyright 2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#ifdef HAVE_LIBGPM
#	include <gpm.h>
#endif

#include <ncurses.h>

#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "old.h"
#include "contacts.h"
#include "mouse.h"


/* 
 * show_mouse_pointer()
 * 
 * should show mouse pointer 
 */
#ifdef HAVE_LIBGPM
void show_mouse_pointer()
{
    if (gpm_visiblepointer) {
        Gpm_Event event;

	Gpm_GetSnapshot(&event);
	Gpm_DrawPointer (event.x, event.y, gpm_consolefd);
    }
}
#endif

/*
 * ncurses_mouse_timer()
 * 
 * every second we should do something
 * it's done here
 */
static void ncurses_mouse_timer(int destroy, void *data)
{
	show_mouse_pointer();
}

/*
 * ncurses_mouse_move_handler()
 * 
 * handler for move of mouse 
 */
void ncurses_mouse_move_handler(int x, int y)
{
	/* debug("%d %d | %d\n", x, y); */

	/* add function that should be done when mouse move is done */
}

/*
 * ncurses_gpm_watch_handler()
 * 
 * handler for gpm events etc
 */
#ifdef HAVE_LIBGPM
void ncurses_gpm_watch_handler(int last, int fd, int watch, void *data)
{
        Gpm_Event event;

        if (last)
                return;

        Gpm_GetEvent(&event);

	if (gpm_visiblepointer) GPM_DRAWPOINTER(&event);

	switch (GPM_BARE_EVENTS(event.type)) {
		case GPM_MOVE:
			ncurses_mouse_move_handler(event.x, event.y);
			break;
		default:
			break;
	}
//        debug("Event Type : %d at x=%d y=%d\n", event.type, event.x, event.y);
}
#endif

/*
 * ncurses_enable_mouse()
 * 
 * it should enable mouse support
 * checks if we are in console mode or in xterm
 */
void ncurses_enable_mouse()
{
	mmask_t oldmask;
#ifdef HAVE_LIBGPM
        Gpm_Connect conn;

        conn.eventMask = ~0;
	conn.defaultMask = 0;   
	conn.minMod      = 0;
        conn.maxMod      = ~0;

        if(Gpm_Open(&conn, 0) == -1) {
                debug("Cannot connect to mouse server\n");
	        mousemask(ALL_MOUSE_EVENTS, &oldmask);
		goto end;
	}
	else
		debug("Gpm at fd no %d\n", gpm_fd);

	if (gpm_fd != -2) {
	        watch_add(&ncurses_plugin, gpm_fd, WATCH_READ, 1, ncurses_gpm_watch_handler, NULL);
		gpm_visiblepointer = 1;
	} else { /* xterm */
	        mousemask(ALL_MOUSE_EVENTS, &oldmask);
	}
#else
	mousemask(ALL_MOUSE_EVENTS, &oldmask);
#endif
end:
        timer_add(&ncurses_plugin, "ncurses:mouse", 1, 1, ncurses_mouse_timer, NULL);
}

/*
 * ncurses_disable_mouse()
 * 
 * it should disable mouse and destroy everything
 * connected with it's support
 */
void ncurses_disable_mouse()
{
#ifdef HAVE_LIBGPM
	Gpm_Close();

	if (gpm_fd != 2) {
		watch_remove(&ncurses_plugin, gpm_fd, WATCH_READ);
		timer_remove(&ncurses_plugin, "ncurses:mouse");
	}
#endif
}

