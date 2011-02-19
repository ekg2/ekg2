/*
 *  GSource-related APIs and functions
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

#ifndef __EKG_SOURCES_H
#define __EKG_SOURCES_H

#include <glib.h>

/* Child watches */
typedef struct {
	pid_t		pid;		/* id procesu */
	char		*plugin;	/* obsługuj±cy plugin */
	char		*name;		/* nazwa, wy¶wietlana przy /exec */
	GChildWatchFunc	handler;	/* zakład pogrzebowy */
	void		*priv_data;	/* dane procesu */

	guint		id;		/* glib child_watch id */
	GDestroyNotify	destr;
} child_t;

child_t *ekg_child_add(plugin_t *plugin, GPid pid, const gchar *name_format, GChildWatchFunc handler, gpointer data, GDestroyNotify destr, ...) G_GNUC_PRINTF(3, 7) G_GNUC_MALLOC;
void children_destroy(void);

#endif
