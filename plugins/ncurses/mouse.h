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

#ifndef __EKG_NCURSES_MOUSE_H
#define __EKG_NCURSES_MOUSE_H


#include "ekg2-config.h"

void ncurses_enable_mouse();
void ncurses_disable_mouse();

int last_mouse_state;

#ifdef HAVE_LIBGPM
	void show_mouse_pointer();
#else
#	define show_mouse_pointer()
#endif

#define EKG_BUTTON1_CLICKED	0x0001              /* clicked once */
#define EKG_BUTTON1_DOUBLE_CLICKED 0x0002	    /* double clicked */
#define EKG_SCROLLED_UP		0x0003		    /* scrolled up */
#define EKG_SCROLLED_DOWN	0x0004		    /* scrolled down */

void ncurses_mouse_clicked_handler(int x, int y, int mouse_flag);

#endif /* __EKG_NCURSES_MOUSE_H */

