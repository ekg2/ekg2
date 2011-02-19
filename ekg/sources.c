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

#include "ekg2.h"
#include "intern.h"

/*
 * Common API
 */

static GSList *sources = NULL;

enum ekg_source_type {
	EKG_SOURCE_CHILD
};

struct ekg_source {
	guint id;
	plugin_t *plugin;
	gchar *name;

	union {
		GChildWatchFunc as_child;
	} handler;

	gpointer priv_data;
	GDestroyNotify destr;

	enum ekg_source_type type;
	union {
		pid_t pid;
	} details;
};

static ekg_source_t source_new(enum ekg_source_type type, plugin_t *plugin, const gchar *name_format, va_list args, gpointer data, GDestroyNotify destr) {
	struct ekg_source *s = g_slice_new(struct ekg_source);

	s->type = type;
	s->plugin = plugin;
	s->name = g_strdup_vprintf(name_format, args);
	s->priv_data = data;
	s->destr = destr;
	
	sources = g_slist_prepend(sources, s);
	return s;
}

static void source_free(gpointer data) {
	struct ekg_source *s = data;

	switch (s->type) {
		case EKG_SOURCE_CHILD:
			g_spawn_close_pid(s->details.pid);
	}

	g_free(s->name);
	g_slice_free(struct ekg_source, s);
}

static void source_destroy_notify(gpointer data) {
	struct ekg_source *s = data;
	sources = g_slist_remove(sources, data);

	if (G_UNLIKELY(s->destr))
		s->destr(s->priv_data);

	source_free(data);
}

void sources_destroy(void) {
	inline void source_remove(gpointer data, gpointer user_data) {
		struct ekg_source *s = data;

		if (s->type == EKG_SOURCE_CHILD)
#ifndef NO_POSIX_SYSTEM
			kill(s->details.pid, SIGTERM);
#else
			/* TerminateProcess / TerminateThread */;
#endif

		g_source_remove(s->id);
	}

	g_slist_foreach(sources, source_remove, NULL);
}

/*
 * Child watches
 */

static void child_wrapper(GPid pid, gint status, gpointer data) {
	struct ekg_source *c = data;

	g_assert(pid == c->details.pid);
	if (G_LIKELY(c->handler.as_child))
		c->handler.as_child(pid, WEXITSTATUS(status), c->priv_data);
}

/**
 * ekg_child_add()
 *
 * Add a watcher for the child process.
 *
 * @param plugin - plugin which contains handler funcs or NULL if in core.
 * @param pid - PID of the child process.
 * @param name_format - format string for watcher name. Can be NULL, or
 *	simple string if the name is guaranteed not to contain '%'.
 * @param handler - the handler func called when the process exits.
 *	The handler func will be provided with the child PID, exit status
 *	(filtered through WEXITSTATUS()) and private data.
 * @param data - the private data passed to the handler.
 * @param destr - destructor for the private data. It will be called
 *	even if the handler isn't (i.e. when the watch is removed before
 *	process exits). Can be NULL.
 * @param ... - arguments to name_format format string.
 *
 * @return The newly-allocated struct ekg_child pointer.
 */
ekg_child_t ekg_child_add(plugin_t *plugin, GPid pid, const gchar *name_format, GChildWatchFunc handler, gpointer data, GDestroyNotify destr, ...) {
	va_list args;
	struct ekg_source *c;
	
	va_start(args, destr);
	c = source_new(EKG_SOURCE_CHILD, plugin, name_format, args, data, destr);
	va_end(args);

	c->handler.as_child = handler;
	c->details.pid = pid;
	c->id = g_child_watch_add_full(G_PRIORITY_DEFAULT, pid, child_wrapper, c, source_destroy_notify);

	return c;
}

/*
 * Command helpers
 */

gint ekg_children_print(gint quiet) {
	gboolean found_one = FALSE;

	inline void child_print(gpointer data, gpointer user_data) {
		struct ekg_source *c = data;

		if (c->type == EKG_SOURCE_CHILD) {
			printq("process", ekg_itoa(c->details.pid), c->name ? c->name : "?");
			found_one = TRUE;
		}
	}

	g_slist_foreach(sources, child_print, NULL);

	if (!found_one) {
		printq("no_processes");
		return -1;
	}
	return 0;
}
