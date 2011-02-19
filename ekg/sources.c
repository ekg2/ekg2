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

static GSList *children = NULL;

/*
 * Child watches
 */

static void child_free_item(gpointer data) {
	child_t *c = data;
	g_spawn_close_pid(c->pid);
	g_free(c->name);
	g_free(c->plugin);
	g_slice_free(child_t, c);
}

static void child_destroy_notify2(gpointer data) {
	child_t *c = data;
	children = g_slist_remove(children, data);

	if (G_LIKELY(!(c->plugin) || plugin_find(c->plugin))) {
		/* XXX: leak can happen if plugin was unloaded */
		if (c->destr)
			c->destr(c->priv_data);
	}

	child_free_item(data);
}

static void child_wrapper2(GPid pid, gint status, gpointer data) {
	child_t *c = data;

	/* plugin might have been unloaded */
	if (G_UNLIKELY(c->plugin && !plugin_find(c->plugin)))
		return;
	if (G_LIKELY(c->handler))
		c->handler(pid, WEXITSTATUS(status), c->priv_data);
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
 * @return The newly-allocated child_t pointer.
 */
child_t *ekg_child_add(plugin_t *plugin, GPid pid, const gchar *name_format, GChildWatchFunc handler, gpointer data, GDestroyNotify destr, ...) {
	va_list args;
	child_t *c = g_slice_new(child_t);

	c->plugin = plugin ? g_strdup(plugin->name) : NULL;
	c->pid = pid;
	c->handler = handler;
	c->priv_data = data;
	c->destr = destr;

	va_start(args, destr);
	c->name = g_strdup_vprintf(name_format, args);
	va_end(args);
	
	children = g_slist_prepend(children, c);
	c->id = g_child_watch_add_full(G_PRIORITY_DEFAULT, pid, child_wrapper2, c, child_destroy_notify2);
	return c;
}

void children_destroy(void) {
	inline void child_source_remove(gpointer data, gpointer user_data) {
		child_t *c = data;

#ifndef NO_POSIX_SYSTEM
		kill(c->pid, SIGTERM);
#else
		/* TerminateProcess / TerminateThread */
#endif

		g_source_remove(c->id);
	}

	g_slist_foreach(children, child_source_remove, NULL);
}

/*
 * Command helpers
 */

G_GNUC_INTERNAL
gint ekg_children_print(gint quiet) {
	inline void child_print(gpointer data, gpointer user_data) {
		child_t *c = data;
		print("process", ekg_itoa(c->pid), c->name ? c->name : "?");
	}

	if (!quiet)
		g_slist_foreach(children, child_print, NULL);

	if (!children) {
		printq("no_processes");
		return -1;
	}
	return 0;
}
