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
void ncurses_mouse_clicked_handler(int x, int y, int mouse_flag);

// int last_mouse_state;

#define EKG_BUTTON1_CLICKED	0x0001          
#define EKG_BUTTON2_CLICKED	0x0007
#define EKG_BUTTON3_CLICKED	0x0008
#define EKG_UNKNOWN_CLICKED	0x0006
#define EKG_BUTTON1_DOUBLE_CLICKED 0x0002
#define	EKG_BUTTON2_DOUBLE_CLICKED 0x0009
#define EKG_BUTTON3_DOUBLE_CLICKED 0x0010
#define EKG_UNKNOWN_DOUBLE_CLICKED 0x0005
#define EKG_SCROLLED_UP		0x0003
#define EKG_SCROLLED_DOWN	0x0004

extern int mouse_initialized;

#endif /* __EKG_NCURSES_MOUSE_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
