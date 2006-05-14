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
#include "char.h"
#include "dynstuff.h"
#include "sessions.h"

extern list_t plugins;
extern list_t queries;
extern list_t watches;

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

typedef struct {
        char *key;                      /* name */
        char *value;                    /* value */
        int secret;                     /* should it be hidden ? */
	int type;			/* type */
	plugin_notify_func_t *notify;	/* notify */
} plugins_params_t;


typedef struct {
	CHAR_T *name;
	int prio;
	plugin_class_t pclass;
	plugin_destroy_func_t destroy;
	/* lt_dlhandle */ void *dl;
	plugins_params_t **params;
	plugin_theme_init_func_t theme_init;
} plugin_t;

int plugin_load(const CHAR_T *name, int prio, int quiet);
int plugin_unload(plugin_t *);
int plugin_register(plugin_t *, int prio);
int plugin_unregister(plugin_t *);
int plugin_theme_reload(plugin_t *);
plugin_t *plugin_find(const CHAR_T *name);
plugin_t *plugin_find_uid(const char *uid);
#define plugin_find_s(a) plugin_find_uid(a->uid)
int have_plugin_of_class(int);
int plugin_var_add(plugin_t *pl, const char *name, int type, const char *value, int secret, plugin_notify_func_t *notify);
plugins_params_t *plugin_var_find(plugin_t *pl, const char *name);

#ifdef USINGANANTIQUECOMPILER
#define PLUGIN_DEFINE(x, y, z)\
	static int x##_plugin_destroy(); \
	\
	plugin_t x##_plugin = { \
		TEXT(#x), \
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
		.name = TEXT(#x), \
		.pclass = y, \
		.destroy = x##_plugin_destroy, \
		.theme_init = z \
	}
#endif /* USINGANANTIQUECOMPILER */

#define QUERY(x) int x(void *data, va_list ap)
typedef QUERY(query_handler_func_t);

typedef struct {
	char *name;
	plugin_t *plugin;
	void *data;
	query_handler_func_t *handler;
	int count;
} query_t;

query_t *query_connect(plugin_t *plugin, const char *name, query_handler_func_t *handler, void *data);
int query_disconnect(plugin_t *, const char *);
query_t *query_find(const char *name);

int query_emit(plugin_t *, const char *, ...);

#define WATCHER(x) int x(int type, int fd, const char *watch, void *data)
typedef WATCHER(watcher_handler_func_t);

typedef enum {
	WATCH_NONE = 0,
	WATCH_WRITE = 1,
	WATCH_READ = 2,
	WATCH_READ_LINE = 4
} watch_type_t;

typedef struct {
	int fd;			/* obserwowany deskryptor */
	watch_type_t type;	/* co sprawdzamy */
	plugin_t *plugin;	/* wtyczka obs³uguj±ca deskryptor */
	void *handler;		/* funkcja wywo³ywana je¶li s± dane itp. */
	void *data;		/* dane przekazywane powy¿szym funkcjom. */
	int persist;		/* czy ma byæ na zawsze? */
	string_t buf;		/* bufor na liniê */
	time_t timeout;		/* timeout */
	time_t started;		/* kiedy zaczêto obserwowaæ */
	int removed;		/* wywo³ano ju¿ watch_remove() */
} watch_t;

watch_t *watch_new(plugin_t *plugin, int fd, watch_type_t type);
watch_t *watch_find(plugin_t *plugin, int fd, watch_type_t type);
void watch_free(watch_t *w);

typedef void *watch_handler_func_t;

int watch_persist_set(watch_t *w, int value);
int watch_persist_get(watch_t *w);
int watch_data_set(watch_t *w, void *priv);
void *watch_data_get(watch_t *w);
int watch_timeout_set(watch_t *w, time_t timeout);
time_t watch_timeout_get(watch_t *w);
int watch_handler_set(watch_t *w, watch_handler_func_t h);
watch_handler_func_t watch_handler_get(watch_t *w);
time_t watch_started_get(watch_t *w);

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, int persist, watcher_handler_func_t *handler, void *data);
int watch_remove(plugin_t *plugin, int fd, watch_type_t type);

void watch_handle(watch_t *w);
void watch_handle_line(watch_t *w);

int ekg2_dlinit();

#endif /* __EKG_PLUGINS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
