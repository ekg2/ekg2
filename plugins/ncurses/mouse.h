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

#ifdef HAVE_LIBGPM
	void show_mouse_pointer();
#else
#	define show_mouse_pointer()
#endif

#endif /* __EKG_NCURSES_MOUSE_H */

