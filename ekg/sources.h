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

void ekg_source_remove(ekg_source_t s);
gboolean ekg_source_remove_by_handler(gpointer handler, const gchar *name);
gboolean ekg_source_remove_by_data(gpointer priv_data, const gchar *name);
gboolean ekg_source_remove_by_plugin(plugin_t *plugin, const gchar *name);

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

/* Watches */

extern list_t watches;

typedef enum {
	WATCH_NONE = 0,
	WATCH_WRITE = 1,
	WATCH_READ = 2,
	WATCH_READ_LINE = 4,
	WATCH_WRITE_LINE = 8,
} watch_type_t;

#define WATCHER(x) int x(int type, int fd, watch_type_t watch, void *data)
#define WATCHER_LINE(x) int x(int type, int fd, const char *watch, void *data)
#define WATCHER_SESSION(x) int x(int type, int fd, watch_type_t watch, session_t *s)
#define WATCHER_SESSION_LINE(x) int x(int type, int fd, const char *watch, session_t *s)

typedef WATCHER(watcher_handler_func_t);
/* typedef WATCHER_LINE(watcher_handler_line_func_t); */
typedef WATCHER_SESSION(watcher_session_handler_func_t);

typedef struct watch {
	int fd;			/* obserwowany deskryptor */
	watch_type_t type;	/* co sprawdzamy */
	plugin_t *plugin;	/* wtyczka obsługuj±ca deskryptor */
	void *handler;		/* funkcja wywoływana je¶li s± dane itp. */
	void *data;		/* dane przekazywane powyższym funkcjom. */
	string_t buf;		/* bufor na linię */
	time_t timeout;		/* timeout */
	time_t started;		/* kiedy zaczęto obserwować */

	int transfer_limit;	/* XXX, requested by GiM to limit data transmitted to ircd server... currently only to send all data
					done by serveral calls of watch_write() in one packet... by setting it to -1 and than changing it back to 0
					if we really want to send packet in that function we ought to do by calling watch_handle_write() 
						[PLEASE NOTE, THAT YOU CANNOT DO watch_write().. cause it will check if there is somethink in write buffor...
						and if it is, it won't call watch_handle_write()] 
					or it will be 
					executed in next ekg_loop() loop.
				*/
	int is_session;		/* if set, this watch belongs to session specified in data */

	guint id;
	GIOChannel *f;
} watch_t;

#ifndef EKG2_WIN32_NOFUNCTION

#ifdef __GNU__
int watch_write(watch_t *w, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
#else
int watch_write(watch_t *w, const char *format, ...);
#endif
int watch_write_data(watch_t *w, const char *buf, int len);

watch_t *watch_find(plugin_t *plugin, int fd, watch_type_t type);
void watch_free(watch_t *w);

typedef void *watch_handler_func_t;

int watch_timeout_set(watch_t *w, time_t timeout);

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, watcher_handler_func_t *handler, void *data);
#define watch_add_line(p, fd, type, handler, data) watch_add(p, fd, type, (watcher_handler_func_t *) (handler), data)
watch_t *watch_add_session(session_t *session, int fd, watch_type_t type, watcher_session_handler_func_t *handler);
#define watch_add_session_line(s, fd, type, handler) watch_add_session(s, fd, type, (watcher_session_handler_func_t *) (handler))

int watch_remove(plugin_t *plugin, int fd, watch_type_t type);

void watch_handle(watch_t *w);
void watch_handle_line(watch_t *w);
int watch_handle_write(watch_t *w);

#endif

#endif
