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

#include <stdlib.h>

#include "ecurses.h"

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
 * ncurses_mouse_clicked_handler()
 * 
 * handler for clicked of mouse
 */
void ncurses_mouse_clicked_handler(int x, int y, int mouse_flag)
{
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		
		if (!w)
			continue;

		if (x >= w->left && x < w->left + w->width && y >= w->top && y < w->top + w->height) {
			ncurses_window_t *n;
			if (w->id == 0) { /* if we are reporting status window it means that we clicked 
					 * on window_current and some other functions should be called */
				ncurses_main_window_mouse_handler(x, y, mouse_flag);
				break;
			} else
				n = w->private;

//			debug("window id:%d y %d height %d\n", w->id, w->top, w->height);
			if (n->handle_mouse)
				n->handle_mouse(x, y, mouse_flag);
			break;
		}
	}
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
        /* debug("Event Type : %d at x=%d y=%d\n", event.type, event.x, event.y); */
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
#define xterm_mouse() mouseinterval(-1);\
	if (xstrcasecmp(env, "xterm") && xstrcasecmp(env, "xterm-colour")) {\
                        debug("Mouse in %s terminal is not supported\n", env);\
                        goto end;\
        }\
	\
	mousemask(ALL_MOUSE_EVENTS, &oldmask);\
        mouseinterval(-1);
	
	mmask_t oldmask;
	char *env = getenv("TERM");
#ifdef HAVE_LIBGPM
        Gpm_Connect conn;

        conn.eventMask = ~0;
	conn.defaultMask = 0;   
	conn.minMod      = 0;
        conn.maxMod      = ~0;

	if(Gpm_Open(&conn, 0) == -1) {
                debug("Cannot connect to mouse server\n");
		xterm_mouse();
		goto end;
	}
	else
		debug("Gpm at fd no %d\n", gpm_fd);

	if (gpm_fd != -2) {
	        watch_add(&ncurses_plugin, gpm_fd, WATCH_READ, 1, ncurses_gpm_watch_handler, NULL);
		gpm_visiblepointer = 1;
	} else { /* xterm */
		xterm_mouse();
	}
#else
	xterm_mouse();
#endif
end:
        timer_add(&ncurses_plugin, "ncurses:mouse", 1, 1, ncurses_mouse_timer, NULL);
#undef xterm_mouse
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
	}
#endif
	timer_remove(&ncurses_plugin, "ncurses:mouse");
}

