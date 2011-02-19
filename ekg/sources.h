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

/* Common API */
typedef struct ekg_source *ekg_source_t;

/* Child watches */
typedef ekg_source_t ekg_child_t;

ekg_child_t ekg_child_add(plugin_t *plugin, GPid pid, const gchar *name_format, GChildWatchFunc handler, gpointer data, GDestroyNotify destr, ...) G_GNUC_PRINTF(3, 7) G_GNUC_MALLOC;

/* Timers */
typedef ekg_source_t ekg_timer_t;

/* XXX: fuuu, macros */
#define TIMER(x) gint x(gint type, gpointer data)
#define TIMER_SESSION(x) gint x(gint type, session_t *s)

ekg_timer_t timer_add(plugin_t *plugin, const gchar *name, guint period, gboolean persist, gint (*function)(gint, gpointer), gpointer data);
ekg_timer_t timer_add_ms(plugin_t *plugin, const gchar *name, guint period, gboolean persist, gint (*function)(gint, gpointer), gpointer data);
ekg_timer_t timer_add_session(session_t *session, const gchar *name, guint period, gboolean persist, gint (*function)(gint, session_t *));
ekg_timer_t timer_find_session(session_t *session, const gchar *name);
gint timer_remove(plugin_t *plugin, const gchar *name);
gint timer_remove_session(session_t *session, const gchar *name);
void timers_remove(ekg_timer_t t);
void timers_destroy();

#endif
