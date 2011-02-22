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

/* watch stuff, XXX YYY */
#include <unistd.h>
#include <errno.h>

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
		int (*as_timer)(int, void*);
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
			guint interval;
			gboolean persist;
		} as_timer;
	} details;
};

static ekg_source_t source_new(plugin_t *plugin, const gchar *name_format, va_list args, gpointer data, GDestroyNotify destr) {
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

	inline void source_remove_by_h(gpointer data, gpointer user_data) {
		struct ekg_source *s = data;

		if (s->handler.as_void == handler) {
			if (!name || G_UNLIKELY(!strcasecmp(s->name, name))) {
				ekg_source_remove(s);
				ret = TRUE;
			}
		}
	}

	g_slist_foreach(children, source_remove_by_h, NULL);
	g_slist_foreach(timers, source_remove_by_h, NULL);
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

	inline void source_remove_by_d(gpointer data, gpointer user_data) {
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

	inline void source_remove_by_p(gpointer data, gpointer user_data) {
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
	inline void source_remove(gpointer data, gpointer user_data) {
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
	c = source_new(plugin, name_format, args, data, destr);
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

	t->handler.as_timer(1, t->priv_data);

	timers = g_slist_remove(timers, data);
	source_free(t);
}

static gboolean timer_wrapper(gpointer data) {
	struct ekg_source *t = data;

	g_source_get_current_time(t->source, &(t->details.as_timer.lasttime));
	return !(t->handler.as_timer(0, t->priv_data) == -1 || !t->details.as_timer.persist);
}

ekg_timer_t timer_add_ms(plugin_t *plugin, const gchar *name, guint period, gboolean persist, gint (*function)(gint, gpointer), gpointer data) {
	struct ekg_source *t = source_new(plugin, name, NULL, data, NULL);

	t->handler.as_timer = function;
	t->details.as_timer.interval = period;
	t->details.as_timer.persist = persist;
	timers = g_slist_prepend(timers, t);

	source_set_id(t, g_timeout_add_full(G_PRIORITY_DEFAULT, period, timer_wrapper, t, timer_wrapper_destroy_notify));
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

	inline gint timer_find_session_cmp(gconstpointer li, gconstpointer ui) {
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

	inline void timer_remove_session_iter(gpointer data, gpointer user_data) {
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
	inline void child_print(gpointer data, gpointer user_data) {
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
	
	inline void timer_debug_print(gpointer data, gpointer user_data) {
		struct ekg_source *t = data;
		const gchar *plugin;
		gchar *tmp;
			
		if (t->plugin)
			plugin = t->plugin->name;
		else
			plugin = "-";

		tmp = timer_next_call(t);

		/* XXX: pointer truncated */
		snprintf(buf, sizeof(buf), "%-11s %-20s %-2d %-8u %.8x %-20s", plugin, t->name, t->details.as_timer.persist, t->details.as_timer.interval, GPOINTER_TO_UINT(t->handler.as_timer), tmp);
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

		inline void timer_print(gpointer data, gpointer user_data) {
			struct ekg_source *t = data;
			GTimeVal ends, tv;
			struct tm *at_time;
			char tmp[100], tmp2[150];
			time_t sec, minutes = 0, hours = 0, days = 0;

			if (t->handler.as_timer != timer_handle_at)
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

		inline void timer_print_list(gpointer data, gpointer user_data) {
			struct ekg_source *t = data;
			char *tmp;

			if (t->handler.as_timer != timer_handle_command)
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

/*
 * Watches
 */

/*
 * watch_find()
 *
 * zwraca obiekt watch_t o podanych parametrach.
 */
watch_t *watch_find(plugin_t *plugin, int fd, watch_type_t type) {
	list_t l;
	
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

			/* XXX: added simple plugin ignoring, make something nicer? */
		if (w && ((plugin == (void*) -1) || w->plugin == plugin) && w->fd == fd && (w->type & type))
			return w;
	}

	return NULL;
}

static LIST_FREE_ITEM(watch_free_data, watch_t *) {
	if (data->buf) {
		int (*handler)(int, int, const char *, void *) = data->handler;
		string_free(data->buf, 1);
		/* DO WE WANT TO SEND ALL  IN BUFOR TO FD ? IF IT'S WATCH_WRITE_LINE? or parse all data if it's WATCH_READ_LINE? mmh. XXX */
		if (handler)
			handler(1, data->fd, NULL, data->data);
	} else {
		int (*handler)(int, int, int, void *) = data->handler;
		if (handler)
			handler(1, data->fd, data->type, data->data);
	}

	g_io_channel_unref(data->f);
}

/*
 * watch_free()
 *
 * zwalnia pamięć po obiekcie watch_t.
 * zwraca wskaĽnik do następnego obiektu do iterowania
 * albo NULL, jak nie można skasować.
 */
void watch_free(watch_t *w) {
	if (!w)
		return;

	g_source_remove(w->id);

}

/*
 * watch_handle_line()
 *
 * obsługa deskryptorów przegl±danych WATCH_READ_LINE.
 */
static int watch_handle_line(watch_t *w)
{
	char buf[1024], *tmp;
	int ret, res = 0;
	int (*handler)(int, int, const char *, void *) = w->handler;

	g_assert(w);

#ifndef NO_POSIX_SYSTEM
	ret = read(w->fd, buf, sizeof(buf) - 1);
#else
	ret = recv(w->fd, buf, sizeof(buf) - 1, 0);
	if (ret == -1 && WSAGetLastError() == WSAENOTSOCK) {
		printf("recv() failed Error: %d, using ReadFile()", WSAGetLastError());
		res = ReadFile(w->fd, &buf, sizeof(buf)-1, &ret, NULL);
		printf(" res=%d ret=%d\n", res, ret);
	}
	res = 0;
#endif

	if (ret > 0) {
		buf[ret] = 0;
		string_append(w->buf, buf);
#ifdef NO_POSIX_SYSTEM
		printf("RECV: %s\n", buf);
#endif
	}

	if (ret == 0 || (ret == -1 && errno != EAGAIN))
		string_append_c(w->buf, '\n');

	while ((tmp = xstrchr(w->buf->str, '\n'))) {
		size_t strlen = tmp - w->buf->str;		/* get len of str from begining to \n char */
		char *line = xstrndup(w->buf->str, strlen);	/* strndup() str with len == strlen */

		/* we strndup() str with len == strlen, so we don't need to call xstrlen() */
		if (strlen > 1 && line[strlen - 1] == '\r')
			line[strlen - 1] = 0;

		if ((res = handler(0, w->fd, line, w->data)) == -1) {
			xfree(line);
			break;
		}

		string_remove(w->buf, strlen + 1);

		xfree(line);
	}

	/* je¶li koniec strumienia, lub nie jest to ci±głe przegl±danie,
	 * zwolnij pamięć i usuń z listy */
	if (res == -1 || ret == 0 || (ret == -1 && errno != EAGAIN))
		return -1; /* XXX: close(fd) was here, seemed unsafe */

	return res;
}

/* ripped from irc plugin */
static int watch_handle_write(watch_t *w) {
	int (*handler)(int, int, const char *, void *) = w->handler;
	int res = -1;
	int len = (w && w->buf) ? w->buf->len : 0;

	g_assert(w);
#ifdef FIXME_WATCHES_TRANSFER_LIMITS
	if (w->transfer_limit == -1) return 0;	/* transfer limit turned on, don't send anythink... XXX */
#endif
	if (!len) return 0;
	debug_io("[watch_handle_write] fd: %d in queue: %d bytes.... ", w->fd, len);

	if (handler) {
		res = handler(0, w->fd, w->buf->str, w->data);
	} else {
#ifdef NO_POSIX_SYSTEM
		res = send(w->fd, w->buf->str, len, 0 /* MSG_NOSIGNAL */);
#else
		res = write(w->fd, w->buf->str, len);
#endif
	}

	debug_io(" ... wrote:%d bytes (handler: 0x%x) ", res, handler);

	if (res == -1 &&
#ifdef NO_POSIX_SYSTEM
			(WSAGetLastError() != 666)
#else
			1
#endif
		) {
#ifdef NO_POSIX_SYSTEM
		debug("WSAError: %d\n", WSAGetLastError());
#else
		debug("Error: %s %d\n", strerror(errno), errno);
#endif
		return -1;
	}
	
	if (res > len) {
		/* use debug_fatal() */
		/* debug_fatal() should do:
		 *	- print this info to all open windows with RED color
		 *	- change some variable 'ekg2_need_restart' to 1.
		 *	- @ ncurses if we have ekg2_need_restart set, and if colors turned on, change from blue to red..
		 *	- and do other happy stuff.
		 *
		 * XXX, implement and use it. It should be used as ASSERT()
		 */
		
		debug_error("watch_write(): handler returned bad value, 0x%x vs 0x%x\n", res, len);
		res = len;
	} else if (res < 0) {
		debug_error("watch_write(): handler returned negative value other than -1.. XXX\n");
		res = 0;
	}

	string_remove(w->buf, res);
	debug_io("left: %d bytes\n", w->buf->len);

	return res;
}

int watch_write_data(watch_t *w, const char *buf, int len) {		/* XXX, refactory: watch_write() */
	int was_empty;

	if (!w || !buf || len <= 0)
		return -1;

	was_empty = !w->buf->len;
	string_append_raw(w->buf, buf, len);

	if (was_empty) 
		return watch_handle_write(w); /* let's try to write somethink ? */
	return 0;
}

int watch_write(watch_t *w, const char *format, ...) {			/* XXX, refactory: watch_writef() */
	char		*text;
	int		textlen;
	va_list		ap;
	int		res;

	if (!w || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);
	
	textlen = xstrlen(text); 

	debug_io("[watch]_send: %s\n", text ? textlen ? text: "[0LENGTH]":"[FAILED]");

	if (!text) 
		return -1;

	res = watch_write_data(w, text, textlen);

	xfree(text);
	return res;
}


/**
 * watch_handle()
 *
 * Handler for watches with type: <i>WATCH_READ</i> or <i>WATCH_WRITE</i><br>
 * Mark watch with w->removed = -1, to indicate that watch is in use. And it shouldn't be
 * executed again. [If watch can or even must be executed twice from ekg_loop() than you must
 * change w->removed by yourself.]<br>
 * 
 * If handler of watch return -1 or watch was removed inside function [by watch_remove() or watch_free()]. Than it'll be removed.<br>
 * ELSE Update w->started field to current time.
 *
 * @param w	- watch_t to handler
 *
 * @todo We only check for w->removed == -1, maybe instead change it to: w->removed != 0
 */

static int watch_handle(watch_t *w) {
	int (*handler)(int, int, int, void *);
	int res;

	g_assert(w);

	handler = w->handler;
		
	res = handler(0, w->fd, w->type, w->data);

	w->started = time(NULL);

	return res;
}

gboolean watch_old_wrapper(GIOChannel *f, GIOCondition cond, gpointer data) {
	watch_t *w = data;

	if (w->type != WATCH_NONE && (cond & (G_IO_IN | G_IO_OUT))) {
		int ret;
		g_assert(cond & (w->type == WATCH_WRITE ? G_IO_OUT : G_IO_IN));

		if (!w->buf)
			ret = watch_handle(w);
		else if (w->type == WATCH_READ)
			ret = watch_handle_line(w);
		else if (w->type == WATCH_WRITE)
			ret = watch_handle_write(w);

		if (ret == -1)
			return FALSE;
	}

	if (cond & (G_IO_ERR | G_IO_NVAL | G_IO_HUP)) {
		debug("watch_old_wrapper(): fd no longer valid, fd=%d, type=%d, plugin=%s\n",
				w->fd, w->type, (w->plugin) ? w->plugin->name : ("none"));
		return FALSE;
	}

	return TRUE;
}

void watch_old_destroy_notify(gpointer data) {
	watch_t *w = data;

#ifdef FIXME_WATCHES
	if (w->type == WATCH_WRITE && w->buf && !w->handler && w->plugin) {	/* XXX */
		debug_error("[INTERNAL_DEBUG] WATCH_LINE_WRITE must be removed by plugin, manually (settype to WATCH_NONE and than call watch_free()\n");
		return;
	}
#endif

	watch_free_data(w);
	list_remove_safe(&watches, w, 1);

	debug("watch_old_destroy_notify() REMOVED WATCH, oldwatch: 0x%x\n", w);
}

/**
 * watch_add()
 *
 * Create new watch_t and add it on the beginning of watches list.
 *
 * @param plugin	- plugin
 * @param fd		- fd to watch data for.
 * @param type		- type of watch.
 * @param handler	- handler of watch.
 * @param data		- data which be passed to handler.
 *
 * @return Created watch_t. if @a type is either WATCH_READ_LINE or WATCH_WRITE_LINE than also allocate memory for buffer
 */

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, watcher_handler_func_t *handler, void *data) {
	GError *err	= NULL;
	watch_t *w	= xmalloc(sizeof(watch_t));
	w->plugin	= plugin;
	w->fd		= fd;
	w->type		= type;

	if (w->type == WATCH_READ_LINE) {
		w->type = WATCH_READ;
		w->buf = string_init(NULL);
	} else if (w->type == WATCH_WRITE_LINE) {
		w->type = WATCH_WRITE;
		w->buf = string_init(NULL);
	}
	
	w->started = time(NULL);
	w->handler = handler;
	w->data    = data;

	w->f = g_io_channel_unix_new(fd);

	/* we need to disable recoding & buffering, as we use fd directly */
	g_assert(g_io_channel_set_encoding(w->f, NULL, &err) == G_IO_STATUS_NORMAL);
	g_io_channel_set_buffered(w->f, FALSE);

	w->id = g_io_add_watch_full(w->f, G_PRIORITY_DEFAULT,
			(w->type == WATCH_WRITE ? G_IO_OUT : G_IO_IN)
			| G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			watch_old_wrapper, w, watch_old_destroy_notify);

	list_add_beginning(&watches, w);
	return w;
}

/**
 * watch_add_session()
 *
 * Create new session watch_t and add it on the beginning of watches list.
 *
 * @param session	- session
 * @param fd		- fd to watch data for
 * @param type		- type of watch.
 * @param handler	- handler of watch.
 *
 * @return	If @a session is NULL, or @a session->plugin is NULL, it return NULL.<br>
 *		else created watch_t
 */

watch_t *watch_add_session(session_t *session, int fd, watch_type_t type, watcher_session_handler_func_t *handler) {
	watch_t *w;
	if (!session || !session->plugin) {
		debug_error("watch_add_session() s: 0x%x s->plugin: 0x%x\n", session, session ? session->plugin : NULL);
		return NULL;
	}
	w = watch_add(session->plugin, fd, type, (watcher_handler_func_t *) handler, session);

	w->is_session = 1;
	return w;
}

int watch_remove(plugin_t *plugin, int fd, watch_type_t type)
{
	int res = -1;
	watch_t *w;
#ifdef FIXME_WATCHES
/* XXX, here can be deadlock feel warned. */
/* DEADLOCK ACHIEVED! */
	while ((w = watch_find(plugin, fd, type))) {
		watch_free(w);
		res = 0;
	}
#endif

	return res;
}


