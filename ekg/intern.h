/*
 *  Internal functions
 *
 *  (C) Copyright 2011 EKG2 team
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

#ifndef __EKG_INTERN_H
#define __EKG_INTERN_H

/* commands.c */

G_GNUC_INTERNAL
TIMER(timer_handle_at);
G_GNUC_INTERNAL
TIMER(timer_handle_command);

/* legacyconfig.c */

G_GNUC_INTERNAL
void config_upgrade(void);

/* sources.c */

G_GNUC_INTERNAL
void sources_destroy(void);
G_GNUC_INTERNAL
gint timer_remove_user(gint (*handler)(gint, gpointer));
G_GNUC_INTERNAL
gint ekg_children_print(gint quiet);
G_GNUC_INTERNAL
COMMAND(cmd_debug_timers);
G_GNUC_INTERNAL
COMMAND(cmd_at);
G_GNUC_INTERNAL
COMMAND(cmd_timer);

#endif
