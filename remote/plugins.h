/* $Id: plugins.h 4592 2008-09-01 19:12:07Z peres $ */

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

#define EKG_ABI_VER 4633

#define EXPORT     __attribute__ ((visibility("default")))

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

#define PLUGIN_VAR_ADD(name, type, value, secret, notify)	{ name, value, secret, type, notify }
#define PLUGIN_VAR_END()					{ NULL, NULL, 0, -1, NULL } 
extern int plugin_abi_version(int plugin_abi_ver, const char * plugin_name);
#define PLUGIN_CHECK_VER(name) { if (!plugin_abi_version(EKG_ABI_VER, name)) return -1; }

typedef struct {
	char *key;				/* ekg2-remote: OK */
	char *value;				/* ekg2-remote: OK, NULL */
	int secret;				/* ekg2-remote: OK, 0 */
	int type;				/* ekg2-remote: OK, 0 */
	plugin_notify_func_t *notify;		/* ekg2-remote: OK, NULL */
} plugins_params_t;

typedef struct plugin {
	struct plugin *next;

	char *name;				/* ekg2-remote: OK */
	int prio;				/* ekg2-remote: OK, (but not used) */
	plugin_class_t pclass;			/* ekg2-remote: OK, PLUGIN_ANY */
	plugin_destroy_func_t destroy;		/* ekg2-remote: OK, NULL */
	void *__dl;				/* ekg2-remote: OK, NULL */
	plugins_params_t *params;		/* ekg2-remote: OK */
	plugin_theme_init_func_t theme_init;	/* ekg2-remote: OK, NULL */

	const void *priv;
} plugin_t;

void plugin_load(const char *name);
void plugin_unload(plugin_t *p);
plugin_t *remote_plugin_load(const char *name, int prio);
int plugin_register(plugin_t *, int prio);
int plugin_unregister(plugin_t *);
void remote_plugins_destroy();
plugin_t *plugin_find(const char *name);

#define PLUGIN_DEFINE(x, y, z)\
	static int x##_plugin_destroy(); \
	\
	plugin_t x##_plugin = { \
		.name = #x, \
		.pclass = y, \
		.destroy = x##_plugin_destroy, \
		.theme_init = z \
	}

#define QUERY(x) int x(void *data, va_list ap)
typedef QUERY(query_handler_func_t);

typedef struct queryx {
	struct queryx *next;

	int id;
	plugin_t *plugin;
	void *data;
	query_handler_func_t *handler;
	int __count;				/* ekg2-remote: OK, 0 */
} query_t;

query_t *query_connect_id(plugin_t *plugin, const int id, query_handler_func_t *handler, void *data);
int query_emit_id(plugin_t *, const int, ...);
void queries_destroy();

typedef enum {
	WATCH_NONE = 0,
	WATCH_WRITE = 1,
	WATCH_READ = 2,
	WATCH_READ_LINE = 4,
	WATCH_WRITE_LINE = 8,
} watch_type_t;

#define WATCHER(x) int x(int type, int fd, watch_type_t watch, void *data)
#define WATCHER_LINE(x) int x(int type, int fd, const char *watch, void *data)

typedef WATCHER(watcher_handler_func_t);

typedef struct watch {
	int fd;			/* obserwowany deskryptor */
	watch_type_t type;	/* co sprawdzamy */
	plugin_t *plugin;	/* wtyczka obs³uguj±ca deskryptor */
	void *handler;		/* funkcja wywo³ywana je¶li s± dane itp. */
	void *data;		/* dane przekazywane powy¿szym funkcjom. */
	string_t buf;		/* bufor na liniê */
	time_t __timeout;	/* ekg2-remote: NONE */
	time_t __started;	/* ekg2-remote: NONE */
	int removed;		/* wywo³ano ju¿ watch_remove() */

	int transfer_limit;	/* XXX, requested by GiM to limit data transmitted to ircd server... currently only to send all data
					done by serveral calls of watch_write() in one packet... by setting it to -1 and than changing it back to 0
					if we really want to send packet in that function we ought to do by calling watch_handle_write() 
						[PLEASE NOTE, THAT YOU CANNOT DO watch_write().. cause it will check if there is somethink in write buffor...
						and if it is, it won't call watch_handle_write()] 
					or it will be 
					executed in next ekg_loop() loop.
				*/
	int __is_session;		/* if set, this watch belongs to session specified in data */
} watch_t;

int watch_write(watch_t *w, const char *buf, int len);

void watch_free(watch_t *w);

typedef void *watch_handler_func_t;

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, watcher_handler_func_t *handler, void *data);
#define watch_add_line(p, fd, type, handler, data) watch_add(p, fd, type, (watcher_handler_func_t *) (handler), data)

int watch_remove(plugin_t *plugin, int fd, watch_type_t type);

void watch_handle(watch_t *w);
void watches_destroy();

#define IDLER(x) int x(void *data)

typedef IDLER(idle_handler_func_t);

typedef struct idle {
	struct idle *next;

	plugin_t *plugin;
	idle_handler_func_t *handler;
	void *data;
} idle_t;

idle_t *idle_add(plugin_t *plugin, idle_handler_func_t *handler, void *data);
void idle_handle(idle_t *i);

extern plugin_t *plugins;
extern list_t watches;
extern idle_t *idles;

extern plugin_t *ui_plugin;

extern int ekg_watches_removed;

#endif /* __EKG_PLUGINS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
