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

#ifndef __EKG_PLUGINS_H
#define __EKG_PLUGINS_H

#include <sys/types.h>
#include <stdarg.h>

#include "dynstuff.h"
#include "sessions.h"

#define EKG_ABI_VER 4376

#define EXPORT __attribute__ ((visibility("default")))

typedef enum {
	PLUGIN_ANY = 0,
	PLUGIN_GENERIC,
	PLUGIN_PROTOCOL,
	PLUGIN_UI,
	PLUGIN_LOG,
	PLUGIN_SCRIPTING,
	PLUGIN_AUDIO,
	PLUGIN_CODEC,
	PLUGIN_CRYPT
} plugin_class_t;

typedef int (*plugin_destroy_func_t)(void);
typedef int (*plugin_theme_init_func_t)(void);
typedef void (plugin_notify_func_t)(session_t *, const char *);

#define PLUGIN_VAR_ADD(name, type, value, secret, notify) 	{ name, value, secret, type, notify }
#define PLUGIN_VAR_END()					{ NULL, NULL, 0, -1, NULL } 
extern int plugin_abi_version(int plugin_abi_ver, const char * plugin_name);
#define PLUGIN_CHECK_VER(name) { if (!plugin_abi_version(EKG_ABI_VER, name)) return -1; }

typedef struct {
        char *key;                      /* name */
        char *value;                    /* value */
        int secret;                     /* should it be hidden ? */
	int type;			/* type */
	plugin_notify_func_t *notify;	/* notify */
} plugins_params_t;

typedef struct plugin {
	struct plugin *next;

	char *name;
	int prio;
	plugin_class_t pclass;
	plugin_destroy_func_t destroy;
	/* lt_dlhandle */ void *dl;
	plugins_params_t *params;
	plugin_theme_init_func_t theme_init;
} plugin_t;

#ifndef EKG2_WIN32_NOFUNCTION

int plugin_load(const char *name, int prio, int quiet);
int plugin_unload(plugin_t *);
int plugin_register(plugin_t *, int prio);
int plugin_unregister(plugin_t *);
plugin_t *plugin_find(const char *name);
plugin_t *plugin_find_uid(const char *uid);
int have_plugin_of_class(plugin_class_t pclass);
int plugin_var_add(plugin_t *pl, const char *name, int type, const char *value, int secret, plugin_notify_func_t *notify);
int plugin_var_find(plugin_t *pl, const char *name);

void plugins_unlink(plugin_t *pl);
#endif

#ifdef USINGANANTIQUECOMPILER
#define PLUGIN_DEFINE(x, y, z)\
	static int x##_plugin_destroy(); \
	\
	plugin_t x##_plugin = { \
		#x, \
		0, \
		y, \
		x##_plugin_destroy, \
		NULL, NULL, \
		z \
	}
#else
#define PLUGIN_DEFINE(x, y, z)\
	static int x##_plugin_destroy(); \
	\
	plugin_t x##_plugin = { \
		.name = #x, \
		.pclass = y, \
		.destroy = x##_plugin_destroy, \
		.theme_init = z \
	}
#endif /* USINGANANTIQUECOMPILER */

#define QUERY(x) int x(void *data, va_list ap)
typedef QUERY(query_handler_func_t);

typedef struct queryx {
	struct queryx *next;

	int id;
	plugin_t *plugin;
	void *data;
	query_handler_func_t *handler;
	int count;
} query_t;

#ifndef EKG2_WIN32_NOFUNCTION

query_t *query_connect(plugin_t *plugin, const char *name, query_handler_func_t *handler, void *data);
query_t *query_connect_id(plugin_t *plugin, const int id, query_handler_func_t *handler, void *data);
int query_free(query_t *q);
void query_external_free();

int query_emit_id(plugin_t *, const int, ...);
int query_emit_id_ro(plugin_t *plugin, const int id, ...);
int query_emit(plugin_t *, const char *, ...);
void queries_reconnect();

const char *query_name(const int id);
const struct query_def *query_struct(const int id);

#endif

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
	plugin_t *plugin;	/* wtyczka obs³uguj±ca deskryptor */
	void *handler;		/* funkcja wywo³ywana je¶li s± dane itp. */
	void *data;		/* dane przekazywane powy¿szym funkcjom. */
	string_t buf;		/* bufor na liniê */
	time_t timeout;		/* timeout */
	time_t started;		/* kiedy zaczêto obserwowaæ */
	int removed;		/* wywo³ano ju¿ watch_remove() */

	int transfer_limit;	/* XXX, requested by GiM to limit data transmitted to ircd server... currently only to send all data
					done by serveral calls of watch_write() in one packet... by setting it to -1 and than changing it back to 0
					if we really want to send packet in that function we ought to do by calling watch_handle_write() 
						[PLEASE NOTE, THAT YOU CANNOT DO watch_write().. cause it will check if there is somethink in write buffor...
						and if it is, it won't call watch_handle_write()] 
					or it will be 
					executed in next ekg_loop() loop.
				*/
	int is_session;		/* if set, this watch belongs to session specified in data */
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

#define IDLER(x) int x(void *data)

typedef IDLER(idle_handler_func_t);

typedef struct idle {
	struct idle *next;

	plugin_t *plugin;
	idle_handler_func_t *handler;
	void *data;
} idle_t;

	/* to be used with mainloop-based idlers as 'data'
	 * then handler could use its' value as current loop time */
extern struct timeval ekg_tv;

#ifndef EKG2_WIN32_NOFUNCTION
idle_t *idle_add(plugin_t *plugin, idle_handler_func_t *handler, void *data);
void idle_handle(idle_t *i);
#endif

int ekg2_dlinit();

#endif

#ifndef EKG2_WIN32_NOFUNCTION
extern plugin_t *plugins;
extern list_t watches;
extern idle_t *idles;
extern query_t *queries[];
#endif

#endif /* __EKG_PLUGINS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
