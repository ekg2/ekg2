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
#include "internal.h"

#include <string.h>

/* WEXITSTATUS for FreeBSD */
#include <sys/wait.h>

/*
 * Common API
 */

static GSList *children = NULL;
static GSList *timers = NULL;

struct ekg_source {
	guint id;
	GSource *source;
	plugin_t *plugin;
	gchar *name;

	union {
		GChildWatchFunc as_child;
		GSourceFunc as_timer;
		int (*as_old_timer)(int, void*);
		gpointer as_void;
	} handler;

	gpointer priv_data;
	GDestroyNotify destr;

	union {
		struct {
			GPid pid;
			gboolean terminated;
		} as_child;

		struct {
			GTimeVal lasttime;
			guint64 interval;
			/* persist arg is deprecated, and mostly unused
			 * however, /at uses it, and so does xmsg plugin
			 * the former needs fixing, the latter will probably be removed */
			gboolean persist;
		} as_timer;
	} details;
};

static ekg_source_t source_new(plugin_t *plugin, const gchar *name_format, gpointer data, GDestroyNotify destr, va_list args) {
	struct ekg_source *s = g_slice_new(struct ekg_source);

	s->plugin = plugin;
	/* XXX: temporary */
	s->name = args ? g_strdup_vprintf(name_format, args) : g_strdup(name_format);
	s->priv_data = data;
	s->destr = destr;
	
	return s;
}

static void source_set_id(struct ekg_source *s, guint id) {
	s->id = id;
	if (!s->name)
		s->name = g_strdup_printf("_%d", s->id);
	s->source = g_main_context_find_source_by_id(NULL, s->id);
	g_assert(s->source);
}

static void source_free(struct ekg_source *s) {
	g_free(s->name);
	g_slice_free(struct ekg_source, s);
}

/**
 * ekg_source_remove()
 *
 * Remove a particular source (which can be ekg_child_t, ekg_timer_t...).
 *
 * @param s - the source identifier.
 */
void ekg_source_remove(ekg_source_t s) {
	g_source_remove(s->id);
}

/**
 * ekg_source_remove_by_handler()
 *
 * Remove source(s) using a particular handler (and optionally matching
 * the name).
 *
 * @param handler - handler function.
 * @param name - expected source name or NULL if any.
 *
 * @return TRUE if any source found, FALSE otherwise.
 *
 * @note This function doesn't do any source type checks. We assume
 * that (due to handler prototype differences) a particular handler is
 * used only with a single source type.
 */

gboolean ekg_source_remove_by_handler(gpointer handler, const gchar *name) {
	gboolean ret = FALSE;

	void source_remove_by_h(gpointer data, gpointer user_data) {
		struct ekg_source *s = data;

		if (s->handler.as_void == handler) {
			if (!name || G_UNLIKELY(!strcasecmp(s->name, name))) {
				ekg_source_remove(s);
				ret = TRUE;
			}
		}
	}

	g_slist_foreach(timers, source_remove_by_h, NULL);
	if (G_UNLIKELY(!ret))
		g_slist_foreach(children, source_remove_by_h, NULL);
	return ret;
}

/**
 * ekg_source_remove_by_data()
 *
 * Remove source(s) using a particular private data (and optionally
 * matching the name).
 *
 * @param priv_data - private data pointer.
 * @param name - expected source name or NULL if any.
 *
 * @return TRUE if any source found, FALSE otherwise.
 *
 * @note This function doesn't do any source type checks. We assume
 * that either one doesn't reuse the same private data with different
 * source types or expects to remove all of them at once.
 */
gboolean ekg_source_remove_by_data(gpointer priv_data, const gchar *name) {
	gboolean ret = FALSE;

	void source_remove_by_d(gpointer data, gpointer user_data) {
		struct ekg_source *s = data;

		if (s->priv_data == priv_data) {
			if (!name || G_UNLIKELY(!strcasecmp(s->name, name))) {
				ekg_source_remove(s);
				ret = TRUE;
			}
		}
	}

	g_slist_foreach(children, source_remove_by_d, NULL);
	g_slist_foreach(timers, source_remove_by_d, NULL);
	return ret;
}

/**
 * ekg_source_remove_by_plugin()
 *
 * Remove source(s) using a particular plugin (e.g. on plugin unload),
 * and optionally bearing a name.
 *
 * @param plugin - plugin_t pointer.
 * @param name - expected source name or NULL if any.
 *
 * @return TRUE if any source found, FALSE otherwise.
 */
gboolean ekg_source_remove_by_plugin(plugin_t *plugin, const gchar *name) {
	gboolean ret = FALSE;

	void source_remove_by_p(gpointer data, gpointer user_data) {
		struct ekg_source *s = data;

		if (s->plugin == plugin) {
			if (!name || G_UNLIKELY(!strcasecmp(s->name, name))) {
				ekg_source_remove(s);
				ret = TRUE;
			}
		}
	}

	g_slist_foreach(children, source_remove_by_p, NULL);
	g_slist_foreach(timers, source_remove_by_p, NULL);
	return ret;
}

void sources_destroy(void) {
	void source_remove(gpointer data, gpointer user_data) {
		struct ekg_source *s = data;
		ekg_source_remove(s);
	}

	g_slist_foreach(children, source_remove, NULL);
	g_slist_foreach(timers, source_remove, NULL);
}

/*
 * Child watches
 */

static void child_destroy_notify(gpointer data) {
	struct ekg_source *c = data;
	children = g_slist_remove(children, data);

	if (!c->details.as_child.terminated)
#ifndef NO_POSIX_SYSTEM
		kill(c->details.as_child.pid, SIGTERM);
#else
		/* TerminateProcess / TerminateThread */;
#endif

	g_spawn_close_pid(c->details.as_child.pid);

	if (G_UNLIKELY(c->destr))
		c->destr(c->priv_data);

	source_free(c);
}

static void child_wrapper(GPid pid, gint status, gpointer data) {
	struct ekg_source *c = data;

	g_assert(pid == c->details.as_child.pid);
	g_assert(!c->details.as_child.terminated); /* avoid calling twice */
	c->details.as_child.terminated = TRUE;
	if (G_LIKELY(c->handler.as_child))
		c->handler.as_child(pid, WEXITSTATUS(status), c->priv_data);
}

/**
 * ekg_child_add()
 *
 * Add a watcher for the child process.
 *
 * @param plugin - plugin which contains handler funcs or NULL if in core.
 * @param name_format - format string for watcher name. Can be NULL, or
 *	simple string if the name is guaranteed not to contain '%'.
 * @param pid - PID of the child process.
 * @param handler - the handler func called when the process exits.
 *	The handler func will be provided with the child PID, exit status
 *	(filtered through WEXITSTATUS()) and private data.
 * @param data - the private data passed to the handler.
 * @param destr - destructor for the private data. It will be called
 *	even if the handler isn't (i.e. when the watch is removed before
 *	process exits). Can be NULL.
 * @param ... - arguments to name_format format string.
 *
 * @return An unique ekg_child_t.
 */
ekg_child_t ekg_child_add(plugin_t *plugin, const gchar *name_format, GPid pid, GChildWatchFunc handler, gpointer data, GDestroyNotify destr, ...) {
	va_list args;
	struct ekg_source *c;
	
	va_start(args, destr);
	c = source_new(plugin, name_format, data, destr, args);
	va_end(args);

	c->handler.as_child = handler;
	c->details.as_child.pid = pid;
	c->details.as_child.terminated = FALSE;
	children = g_slist_prepend(children, c);
	source_set_id(c, g_child_watch_add_full(G_PRIORITY_DEFAULT, pid, child_wrapper, c, child_destroy_notify));

	return c;
}

/*
 * Timers
 */

static void timer_wrapper_destroy_notify(gpointer data) {
	struct ekg_source *t = data;

	t->handler.as_old_timer(1, t->priv_data);

	timers = g_slist_remove(timers, data);
	source_free(t);
}

static gboolean timer_wrapper_old(gpointer data) {
	struct ekg_source *t = data;

	g_source_get_current_time(t->source, &(t->details.as_timer.lasttime));
	return !(t->handler.as_old_timer(0, t->priv_data) == -1 || !t->details.as_timer.persist);
}

ekg_timer_t timer_add_ms(plugin_t *plugin, const gchar *name, guint period, gboolean persist, gint (*function)(gint, gpointer), gpointer data) {
	struct ekg_source *t = source_new(plugin, name, data, NULL, NULL);

	t->handler.as_old_timer = function;
	t->details.as_timer.interval = period;
	t->details.as_timer.persist = persist;
	timers = g_slist_prepend(timers, t);

	source_set_id(t, g_timeout_add_full(G_PRIORITY_DEFAULT, period, timer_wrapper_old, t, timer_wrapper_destroy_notify));
	g_source_get_current_time(t->source, &(t->details.as_timer.lasttime));

	return t;
}

/*
 * timer_add()
 *
 * dodaje timera.
 *
 *  - plugin - plugin obsługuj±cy timer,
 *  - name - nazwa timera w celach identyfikacji. je¶li jest równa NULL,
 *	     zostanie przyznany pierwszy numerek z brzegu.
 *  - period - za jaki czas w sekundach ma być uruchomiony,
 *  - persist - czy stały timer,
 *  - function - funkcja do wywołania po upłynięciu czasu,
 *  - data - dane przekazywane do funkcji.
 *
 * zwraca zaalokowan± struct timer lub NULL w przypadku błędu.
 */
ekg_timer_t timer_add(plugin_t *plugin, const gchar *name, guint period, gboolean persist, gint (*function)(gint, gpointer), gpointer data) {
	return timer_add_ms(plugin, name, period * 1000, persist, function, data);
}

ekg_timer_t timer_add_session(session_t *session, const gchar *name, guint period, gboolean persist, gint (*function)(gint, session_t *)) {
	g_assert(session);
	g_assert(session->plugin);

	return timer_add(session->plugin, name, period, persist, (void *) function, session);
}

static void timer_destroy_notify(gpointer data) {
	struct ekg_source *t = data;
	timers = g_slist_remove(timers, data);

	if (G_UNLIKELY(t->destr))
		t->destr(t->priv_data);

	source_free(t);
}

static gboolean timer_wrapper(gpointer data) {
	struct ekg_source *t = data;

	return t->handler.as_timer(t->priv_data);
}

/**
 * ekg_timer_add()
 *
 * Add a timer.
 *
 * @param plugin - plugin which contains handler funcs or NULL if in core.
 * @param name_format - format string for timer name. Can be NULL, or
 *	simple string if the name is guaranteed not to contain '%'.
 * @param interval - the interval between successive timer calls,
 *	in milliseconds. If it is a multiple of 1000, the timer will use
 *	glib second timeouts (more efficient); otherwise, the millisecond
 *	timeout will be used.
 * @param handler - the handler func. It will be passed the private
 *	data, and should either return TRUE or FALSE, depending on whether
 *	the timer should persist or be removed.
 * @param data - the private data passed to the handler.
 * @param destr - destructor for the private data. It will be called
 *	even if the handler is not. Can be NULL.
 * @param ... - arguments to name_format format string.
 *
 * @return An unique ekg_timer_t.
 */
ekg_timer_t ekg_timer_add(plugin_t *plugin, const gchar *name_format, guint64 interval, GSourceFunc handler, gpointer data, GDestroyNotify destr, ...) {
	va_list args;
	struct ekg_source *t;
	guint id;
	
	va_start(args, destr);
	t = source_new(plugin, name_format, data, destr, args);
	va_end(args);

	g_assert(handler);
	t->handler.as_timer = handler;
	t->details.as_timer.interval = interval;
	t->details.as_timer.persist = TRUE;

	timers = g_slist_prepend(timers, t);
	if (interval % 1000 == 0)
		id = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT, interval / 1000, timer_wrapper, t, timer_destroy_notify);
	else
		id = g_timeout_add_full(G_PRIORITY_DEFAULT, interval, timer_wrapper, t, timer_destroy_notify);

	source_set_id(t, id);
	g_source_get_current_time(t->source, &(t->details.as_timer.lasttime));

	return t;
}

/*
 * timer_remove()
 *
 * usuwa timer.
 *
 *  - plugin - plugin obsługuj±cy timer,
 *  - name - nazwa timera,
 *
 * 0/-1
 */
gint timer_remove(plugin_t *plugin, const gchar *name) {
	/* originally, timer_remove() didn't remove all timers with !name */
	g_assert(name);
	return (ekg_source_remove_by_plugin(plugin, name) ? 0 : -1);
}

ekg_timer_t timer_find_session(session_t *session, const gchar *name) {
	if (!session)
		return NULL;

	gint timer_find_session_cmp(gconstpointer li, gconstpointer ui) {
		const struct ekg_source *t = li;

		return !(t->priv_data == session && !xstrcmp(name, t->name));
	}

	return (ekg_timer_t) g_slist_find_custom(timers, NULL, timer_find_session_cmp);
}

gint timer_remove_session(session_t *session, const gchar *name) {
	gint removed = 0;

	if (!session)
		return -1;
	g_assert(session->plugin);

	void timer_remove_session_iter(gpointer data, gpointer user_data) {
		struct ekg_source *t = data;

		if (t->priv_data == session && !xstrcmp(name, t->name)) {
			ekg_source_remove(t);
			removed++;
		}
	}

	g_slist_foreach(timers, timer_remove_session_iter, NULL);
	return ((removed) ? 0 : -1);
}

/*
 * timer_remove_user()
 *
 * usuwa wszystkie timery użytkownika.
 *
 * 0/-1
 */
/* XXX: temporary API? */
G_GNUC_INTERNAL
gint timer_remove_user(gint (*handler)(gint, gpointer)) {
	g_assert(handler);
	return (ekg_source_remove_by_handler(handler, NULL) ? 0 : -1);
}

static gchar *timer_next_call(struct ekg_source *t) {
	long usec, sec, minutes = 0, hours = 0, days = 0;
	GTimeVal tv, ends;

	ends.tv_sec = t->details.as_timer.lasttime.tv_sec + (t->details.as_timer.interval / 1000);
	ends.tv_usec = t->details.as_timer.lasttime.tv_usec + ((t->details.as_timer.interval % 1000) * 1000);
	if (ends.tv_usec > 1000000) {
		ends.tv_usec -= 1000000;
		ends.tv_sec++;
	}

	g_source_get_current_time(t->source, &tv);

	if (tv.tv_sec - ends.tv_sec > 2)
		return g_strdup("?");

	if (ends.tv_usec < tv.tv_usec) {
		sec = ends.tv_sec - tv.tv_sec - 1;
		usec = (ends.tv_usec - tv.tv_usec + 1000000) / 1000;
	} else {
		sec = ends.tv_sec - tv.tv_sec;
		usec = (ends.tv_usec - tv.tv_usec) / 1000;
	}

	if (sec > 86400) {
		days = sec / 86400;
		sec -= days * 86400;
	}

	if (sec > 3600) {
		hours = sec / 3600;
		sec -= hours * 3600;
	}

	if (sec > 60) {
		minutes = sec / 60;
		sec -= minutes * 60;
	}

	if (days)
		return saprintf("%ldd %ldh %ldm %ld.%.3ld", days, hours, minutes, sec, usec);

	if (hours)
		return saprintf("%ldh %ldm %ld.%.3ld", hours, minutes, sec, usec);

	if (minutes)
		return saprintf("%ldm %ld.%.3ld", minutes, sec, usec);

	return saprintf("%ld.%.3ld", sec, usec);
}

static inline gint timer_match_name(gconstpointer li, gconstpointer ui) {
	const struct ekg_source *t = li;
	const gchar *name = ui;

	return strcasecmp(t->name, name);
}

/*
 * Command helpers
 */

gint ekg_children_print(gint quiet) {
	void child_print(gpointer data, gpointer user_data) {
		struct ekg_source *c = data;

		printq("process", ekg_itoa(c->details.as_child.pid), c->name ? c->name : "?");
	}

	g_slist_foreach(children, child_print, NULL);

	if (!children) {
		printq("no_processes");
		return -1;
	}
	return 0;
}

COMMAND(cmd_debug_timers) {
/* XXX, */
	char buf[256];
	
	printq("generic_bold", ("plugin      name               pers peri     handler  next"));
	
	void timer_debug_print(gpointer data, gpointer user_data) {
		struct ekg_source *t = data;
		const gchar *plugin;
		gchar *tmp;
			
		if (t->plugin)
			plugin = t->plugin->name;
		else
			plugin = "-";

		tmp = timer_next_call(t);

		/* XXX: pointer truncated */
		snprintf(buf, sizeof(buf), "%-11s %-20s %-2d %-8" G_GINT64_MODIFIER "u %.8x %-20s", plugin, t->name, t->details.as_timer.persist, t->details.as_timer.interval, GPOINTER_TO_UINT(t->handler.as_old_timer), tmp);
		printq("generic", buf);
		g_free(tmp);
	}

	g_slist_foreach(timers, timer_debug_print, NULL);
	return 0;
}

TIMER(timer_handle_at)
{
	if (type) {
		xfree(data);
		return 0;
	}
	
	command_exec(NULL, NULL, (char *) data, 0);
	return 0;
}

COMMAND(cmd_at)
{
	if (match_arg(params[0], 'a', ("add"), 2)) {
		const char *p, *a_name = NULL;
		char *a_command;
		time_t period = 0, freq = 0;
		struct ekg_source *t;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!strncmp(params[2], "*/", 2) || xisdigit(params[2][0])) {
			a_name = params[1];

			if (!xstrcmp(a_name, "(null)")) {
				printq("invalid_params", name);
				return -1;
			}

			if (g_slist_find_custom(timers, a_name, timer_match_name)) {
				printq("at_exist", a_name);
				return -1;
			}

			p = params[2];
		} else
			p = params[1];

		{
			struct tm *lt;
			time_t now = time(NULL);
			char *tmp, *freq_str = NULL, *foo = xstrdup(p);
			int wrong = 0;

			lt = localtime(&now);
			lt->tm_isdst = -1;

			/* częstotliwo¶ć */
			if ((tmp = xstrchr(foo, '/'))) {
				*tmp = 0;
				freq_str = ++tmp;
			}

			/* wyci±gamy sekundy, je¶li s± i obcinamy */
			if ((tmp = xstrchr(foo, '.')) && !(wrong = (xstrlen(tmp) != 3))) {
				sscanf(tmp + 1, "%2d", &lt->tm_sec);
				tmp[0] = 0;
			} else
				lt->tm_sec = 0;

			/* pozb±dĽmy się dwukropka */
			if ((tmp = xstrchr(foo, ':')) && !(wrong = (xstrlen(tmp) != 3))) {
				tmp[0] = tmp[1];
				tmp[1] = tmp[2];
				tmp[2] = 0;
			}

			/* jedziemy ... */
			if (!wrong) {
				switch (xstrlen(foo)) {
					int ret;

					case 12:
						ret = sscanf(foo, "%4d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 5)
							wrong = 1;
						lt->tm_year -= 1900;
						lt->tm_mon -= 1;
						break;
					case 10:
						ret = sscanf(foo, "%2d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 5)
							wrong = 1;
						lt->tm_year += 100;
						lt->tm_mon -= 1;
						break;
					case 8:
						ret = sscanf(foo, "%2d%2d%2d%2d", &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 4)
							wrong = 1;
						lt->tm_mon -= 1;
						break;
					case 6:
						ret = sscanf(foo, "%2d%2d%2d", &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 3)
							wrong = 1;
						break;	
					case 4:
						ret = sscanf(foo, "%2d%2d", &lt->tm_hour, &lt->tm_min);
						if (ret != 2)
							wrong = 1;
						break;
					default:
						wrong = 1;
				}
			}

			/* nie ma błędów ? */
			if (wrong || lt->tm_hour > 23 || lt->tm_min > 59 || lt->tm_sec > 59 || lt->tm_mday > 31 || !lt->tm_mday || lt->tm_mon > 11) {
				printq("invalid_params", name);
				xfree(foo);
				return -1;
			}

			if (freq_str) {
				for (;;) {
					time_t _period = 0;

					if (xisdigit(*freq_str))
						_period = atoi(freq_str);
					else {
						printq("invalid_params", name);
						xfree(foo);
						return -1;
					}

					freq_str += xstrlen(ekg_itoa(_period));

					if (xstrlen(freq_str)) {
						switch (xtolower(*freq_str++)) {
							case 'd':
								_period *= 86400;
								break;
							case 'h':
								_period *= 3600;
								break;
							case 'm':
								_period *= 60;
								break;
							case 's':
								break;
							default:
								printq("invalid_params", name);
								xfree(foo);
								return -1;
						}
					}

					freq += _period;
					
					if (!*freq_str)
						break;
				}
			}

			xfree(foo);

			/* plany na przeszło¶ć? */
			if ((period = mktime(lt) - now) <= 0) {
				if (freq) {
					while (period <= 0)
						period += freq;
				} else {
					printq("at_back_to_past");
					return -1;
				}
			}
		}

		if (a_name)
			a_command = xstrdup(params[3]);
		else
			a_command = g_strjoinv(" ", (char **) params + 2);

		if (!xstrcmp(strip_spaces(a_command), "")) {
			printq("not_enough_params", name);
			xfree(a_command);
			return -1;
		}

		if ((t = timer_add(NULL, a_name, period, ((freq) ? 1 : 0), timer_handle_at, xstrdup(a_command)))) {
			printq("at_added", t->name);
			if (freq) {
				guint d = t->details.as_timer.interval;
				t->details.as_timer.interval = freq * 1000;
				d -= t->details.as_timer.interval;
				t->details.as_timer.lasttime.tv_sec += (d / 1000); 
				t->details.as_timer.lasttime.tv_usec += ((d % 1000) * 1000);
			}
			if (!in_autoexec)
				config_changed = 1;
		}

		xfree(a_command);
		return 0;
	}

	if (match_arg(params[0], 'd', ("del"), 2)) {
		int del_all = 0;
		int ret = 1;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*"))
			del_all = 1;
		ret = !ekg_source_remove_by_handler(timer_handle_at,
				del_all ? NULL : params[1]);
		
		if (!ret) {
			if (del_all)
				printq("at_deleted_all");
			else
				printq("at_deleted", params[1]);
			
			config_changed = 1;
		} else {
			if (del_all)
				printq("at_empty");
			else {
				printq("at_noexist", params[1]);
				return -1;
			}
		}

		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2) || params[0][0] != '-') {
		const char *a_name = NULL;
		int count = 0;

		if (params[0] && match_arg(params[0], 'l', ("list"), 2))
			a_name = params[1];
		else if (params[0])
			a_name = params[0];

		void timer_print(gpointer data, gpointer user_data) {
			struct ekg_source *t = data;
			GTimeVal ends, tv;
			struct tm *at_time;
			char tmp[100], tmp2[150];
			time_t sec, minutes = 0, hours = 0, days = 0;

			if (t->handler.as_old_timer != timer_handle_at)
				return;
			if (a_name && xstrcasecmp(t->name, a_name))
				return;

			count++;

			g_source_get_current_time(t->source, &tv);

			ends.tv_sec = t->details.as_timer.lasttime.tv_sec + (t->details.as_timer.interval / 1000);
			ends.tv_usec = t->details.as_timer.lasttime.tv_usec + ((t->details.as_timer.interval % 1000) * 1000);
			at_time = localtime((time_t *) &ends);
			if (!strftime(tmp, sizeof(tmp), format_find("at_timestamp"), at_time) && format_exists("at_timestamp"))
				xstrcpy(tmp, "TOOLONG");

			if (t->details.as_timer.persist) {
				sec = t->details.as_timer.interval / 1000;

				if (sec > 86400) {
					days = sec / 86400;
					sec -= days * 86400;
				}

				if (sec > 3600) {
					hours = sec / 3600;
					sec -= hours * 3600;
				}
			
				if (sec > 60) {
					minutes = sec / 60;
					sec -= minutes * 60;
				}

				g_strlcpy(tmp2, "every ", sizeof(tmp2));

				if (days) {
					g_strlcat(tmp2, ekg_itoa(days), sizeof(tmp2));
					g_strlcat(tmp2, "d ", sizeof(tmp2));
				}

				if (hours) {
					g_strlcat(tmp2, ekg_itoa(hours), sizeof(tmp2));
					g_strlcat(tmp2, "h ", sizeof(tmp2));
				}

				if (minutes) {
					g_strlcat(tmp2, ekg_itoa(minutes), sizeof(tmp2));
					g_strlcat(tmp2, "m ", sizeof(tmp2));
				}

				if (sec) {
					g_strlcat(tmp2, ekg_itoa(sec), sizeof(tmp2));
					g_strlcat(tmp2, "s", sizeof(tmp2));
				}
			}

			printq("at_list", t->name, tmp, (char*)(t->priv_data), "", ((t->details.as_timer.persist) ? tmp2 : ""));
		}
		g_slist_foreach(timers, timer_print, NULL);

		if (!count) {
			if (a_name) {
				printq("at_noexist", a_name);
				return -1;
			} else
				printq("at_empty");
		}

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

TIMER(timer_handle_command)
{
	if (type) {
		xfree(data);
		return 0;
	}
	
	command_exec(NULL, NULL, (char *) data, 0);
	return 0;
}

COMMAND(cmd_timer)
{
	if (match_arg(params[0], 'a', ("add"), 2)) {
		const char *t_name = NULL, *p;
		char *t_command;
		time_t period = 0;
		struct ekg_source *t;
		int persistent = 0;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (xisdigit(params[2][0]) || !strncmp(params[2], "*/", 2)) {
			t_name = params[1];

			if (!xstrcmp(t_name, "(null)")) {
				printq("invalid_params", name);
				return -1;
			}

			if (g_slist_find_custom(timers, t_name, timer_match_name)) {
				printq("timer_exist", t_name);
				return -1;
			}

			p = params[2];
			t_command = xstrdup(params[3]);
		} else {
			p = params[1];
			t_command = g_strjoinv(" ", (char **) params + 2);
		}

		if ((persistent = !strncmp(p, "*/", 2)))
			p += 2;

		for (;;) {
			time_t _period = 0;

			if (xisdigit(*p))
				_period = atoi(p);
			else {
				printq("invalid_params", name);
				xfree(t_command);
				return -1;
			}

			p += xstrlen(ekg_itoa(_period));

			if (xstrlen(p)) {
				switch (xtolower(*p++)) {
					case 'd':
						_period *= 86400;
						break;
					case 'h':
						_period *= 3600;
						break;
					case 'm':
						_period *= 60;
						break;
					case 's':
						break;
					default:
						printq("invalid_params", name);
						xfree(t_command);
						return -1;
				}
			}

			period += _period;
			
			if (!*p)
				break;
		}

		if (!xstrcmp(strip_spaces(t_command), "")) {
			printq("not_enough_params", name);
			xfree(t_command);
			return -1;
		}

		if ((t = timer_add(NULL, t_name, period, persistent, timer_handle_command, xstrdup(t_command)))) {
			printq("timer_added", t->name);
			if (!in_autoexec)
				config_changed = 1;
		}

		xfree(t_command);
		return 0;
	}

	if (match_arg(params[0], 'd', ("del"), 2)) {
		int del_all = 0, ret;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*"))
			del_all = 1;
		ret = !ekg_source_remove_by_handler(timer_handle_command,
				del_all ? NULL : params[1]);

		if (!ret) {
			if (del_all)
				printq("timer_deleted_all");
			else
				printq("timer_deleted", params[1]);

			config_changed = 1;
		} else {
			if (del_all)
				printq("timer_empty");
			else {
				printq("timer_noexist", params[1]);
				return -1;	
			}
		}

		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2) || params[0][0] != '-') {
		const char *t_name = NULL;
		int count = 0;

		if (params[0] && match_arg(params[0], 'l', ("list"), 2))
			t_name = params[1];
		else if (params[0])
			t_name = params[0];

		void timer_print_list(gpointer data, gpointer user_data) {
			struct ekg_source *t = data;
			char *tmp;

			if (t->handler.as_old_timer != timer_handle_command)
				return;
			if (t_name && xstrcasecmp(t->name, t_name))
				return;

			count++;

			tmp = timer_next_call(t);
			printq("timer_list", t->name, tmp, (char*)(t->priv_data), "", (t->details.as_timer.persist) ? "*" : "");
			g_free(tmp);
		}
		g_slist_foreach(timers, timer_print_list, NULL);

		if (!count) {
			if (t_name) {
				printq("timer_noexist", t_name);
				return -1;
			} else
				printq("timer_empty");
		}

		return 0;
	}	

	printq("invalid_params", name);

	return -1;
}

void timers_write(FILE *f) {
	void timer_write(gpointer data, gpointer user_data) {
		struct ekg_source *t = data;
		FILE *f = user_data;

		const char *name = NULL;

		if (!t->details.as_timer.persist) /* XXX && t->ends.tv_sec - time(NULL) < 5) */
			return;

		if (t->name && t->name[0] != '_')
			name = t->name;
		else
			name = "(null)";

		if (t->handler.as_old_timer == timer_handle_at) {
			char buf[100];
			time_t foo = (time_t) t->details.as_timer.lasttime.tv_sec + (t->details.as_timer.interval / 1000);
			struct tm *tt = localtime(&foo);

			strftime(buf, sizeof(buf), "%G%m%d%H%M.%S", tt);

			if (t->details.as_timer.persist)
				fprintf(f, "at %s %s/%s %s\n", name, buf, ekg_itoa(t->details.as_timer.interval / 1000), (char*)(t->priv_data));
			else
				fprintf(f, "at %s %s %s\n", name, buf, (char*)(t->priv_data));
		} else if (t->handler.as_old_timer == timer_handle_command) {
			char *foo;

			if (t->details.as_timer.persist)
				foo = saprintf("*/%s", ekg_itoa(t->details.as_timer.interval / 1000));
			else
				foo = saprintf("%s", ekg_itoa(t->details.as_timer.lasttime.tv_sec + (t->details.as_timer.interval / 1000)));

			fprintf(f, "timer %s %s %s\n", name, foo, (char*)(t->priv_data));

			xfree(foo);
		}
	}

	g_slist_foreach(timers, timer_write, f);
}
