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

#ifdef HAVE_LIBGPM
#	include <gpm.h>
#endif

#include <stdlib.h>

#include "ecurses.h"

#include <ekg/debug.h>
#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "old.h"
#include "contacts.h"
#include "mouse.h"

int mouse_initialized = 0;

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
	char *tmp;

	switch (mouse_flag) {
		case EKG_BUTTON1_CLICKED:
			tmp = "button1_clicked";
			break;
		case EKG_BUTTON2_CLICKED:
			tmp = "button2_clicked";
			break;
		case EKG_BUTTON3_CLICKED:
			tmp = "button3_clicked";
			break;
		case EKG_UNKNOWN_CLICKED:
			tmp = "unknown_clicked";
			break;
		case EKG_BUTTON1_DOUBLE_CLICKED:
			tmp = "button1_d_clicked";
			break;
		case EKG_BUTTON2_DOUBLE_CLICKED:
			tmp = "button2_d_clicked";
			break;
		case EKG_BUTTON3_DOUBLE_CLICKED:
			tmp = "button3_d_clicked";
			break;
		case EKG_UNKNOWN_DOUBLE_CLICKED:
			tmp = "unknown_d_clicked";
			break;
		case EKG_SCROLLED_UP:
			tmp = "scrolled_up";
			break;
		case EKG_SCROLLED_DOWN:
			tmp = "scrolled down";
			break;
		default:
			tmp = "nothing";
			break;
	}

	/* debug("stalo sie: %s x: %d y: %d\n", tmp, x, y); */
	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		
		if (!w)
			continue;

		if (x > w->left && x <= w->left + w->width && y > w->top && y <= w->top + w->height) {
			ncurses_window_t *n;
			if (w->id == 0) { /* if we are reporting status window it means that we clicked 
					 * on window_current and some other functions should be called */
				ncurses_main_window_mouse_handler(x - w->left, y - w->top, mouse_flag);
				break;
			} else
				n = w->private;

			/* debug("window id:%d y %d height %d\n", w->id, w->top, w->height); */
			if (n->handle_mouse)
				n->handle_mouse(x - w->left, y - w->top, mouse_flag);
			break;
		}
	}
}

#ifdef HAVE_LIBGPM
/*
 * ncurses_gpm_watch_handler()
 * 
 * handler for gpm events etc
 */
WATCHER(ncurses_gpm_watch_handler)
{
        Gpm_Event event;

        if (type)
                return;

        Gpm_GetEvent(&event);

	/* przy double click nie powinno byæ wywo³ywane single click */

	if (gpm_visiblepointer) GPM_DRAWPOINTER(&event);

	switch (event.type) {
		case GPM_MOVE:
			ncurses_mouse_move_handler(event.x, event.y);
			break;
		case GPM_DOUBLE + GPM_UP:
			{
				int mouse_state = EKG_UNKNOWN_DOUBLE_CLICKED;
			        switch (event.buttons) {
		        	        case GPM_B_LEFT:
						mouse_state = EKG_BUTTON1_DOUBLE_CLICKED;
						break;
		                	case GPM_B_RIGHT:
			                        mouse_state = EKG_BUTTON3_DOUBLE_CLICKED;
						break;
			                case GPM_B_MIDDLE:
			                        mouse_state = EKG_BUTTON2_DOUBLE_CLICKED;
						break;
		       		}
				ncurses_mouse_clicked_handler(event.x, event.y, mouse_state);
				break;
			}
                case GPM_SINGLE + GPM_UP:
                        {
                                int mouse_state = EKG_UNKNOWN_CLICKED;
                                switch (event.buttons) {
                                        case GPM_B_LEFT:
                                                mouse_state = EKG_BUTTON1_CLICKED;
                                                break;
                                        case GPM_B_RIGHT:
                                                mouse_state = EKG_BUTTON3_CLICKED;
                                                break;
                                        case GPM_B_MIDDLE:
                                                mouse_state = EKG_BUTTON2_CLICKED;
                                                break;
                                }
                                ncurses_mouse_clicked_handler(event.x, event.y, mouse_state);
                                break;
                        }
                        break;
		default:
			break;
	}
        /* debug("Event Type : %d at x=%d y=%d buttons=%d\n", event.type, event.x, event.y, event.buttons); */
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
	if (xstrcasecmp(env, "xterm") && xstrcasecmp(env, "xterm-color")) {\
                        debug("Mouse in %s terminal is not supported\n", env);\
                        goto end;\
        }\
	\
        printf("\033[?1001s");\
        printf("\033[?1000h");\
	\
	mouse_initialized = 1;
	
	char *env = getenv("TERM");
#ifdef HAVE_LIBGPM
        Gpm_Connect conn;

        conn.eventMask = ~0;
	conn.defaultMask = 0;   
	conn.minMod      = 0;
        conn.maxMod      = 0;

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
		mouse_initialized = 1;
	} else { /* xterm */
		xterm_mouse();
	}
#else
	xterm_mouse();
#endif
end:
	if (mouse_initialized)
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
	if (!mouse_initialized)
		return;

#ifdef HAVE_LIBGPM
	Gpm_Close();

	if (gpm_fd != 2) {
		watch_remove(&ncurses_plugin, gpm_fd, WATCH_READ);
	}
#endif
	timer_remove(&ncurses_plugin, "ncurses:mouse");
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
