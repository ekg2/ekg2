/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EKG_NCURSES_BINDINGS_H
#define __EKG_NCURSES_BINDINGS_H

#include "ekg2-config.h"

#include "ecurses.h"

#include <ekg/stuff.h>

#define KEY_CTRL_ENTER 350
#define KEY_CTRL_ESCAPE 351
#define KEY_CTRL_HOME 352
#define KEY_CTRL_END 353
#define KEY_CTRL_DC 354
#define KEY_CTRL_BACKSPACE 355
#define KEY_CTRL_TAB 356


struct binding *ncurses_binding_map[KEY_MAX + 1];
struct binding *ncurses_binding_map_meta[KEY_MAX + 1];

void *ncurses_binding_complete;

void ncurses_binding_init();
void ncurses_binding_destroy();

void ncurses_binding_add(const char *key, const char *action, int internal, int quiet);
void ncurses_binding_delete(const char *key, int quiet);
void ncurses_binding_default();
void ncurses_binding_set(int quiet, const char *key, const char *sequence);

int bindings_added_max;
#endif /* __EKG_NCURSES_BINDINGS_H */
