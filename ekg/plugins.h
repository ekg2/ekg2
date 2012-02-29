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

#include <glib.h>
#include <gmodule.h>

#include <sys/types.h>
#include <stdarg.h>

#include "dynstuff.h"
#include "sessions.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EKG_ABI_VER 5798 /* git rev-list master | wc -l */

#define EXPORT __attribute__ ((visibility("default"))) G_MODULE_EXPORT

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

#define PLUGIN_VAR_ADD(name, type, value, secret, notify)		{ name, value, secret, type, notify, NULL }
#define PLUGIN_VAR_ADD_MAP(name, type, value, secret, notify, map)	{ name, value, secret, type, notify, map }
#define PLUGIN_VAR_END()					{ NULL, NULL, 0, -1, NULL } 
extern int plugin_abi_version(int plugin_abi_ver, const char * plugin_name);
#define PLUGIN_CHECK_VER(name) { if (!plugin_abi_version(EKG_ABI_VER, name)) return -1; }

typedef struct {
	char *key;			/* name */
	char *value;			/* value */
	int secret;			/* should it be hidden ? */
	int type;			/* type */
	plugin_notify_func_t *notify;	/* notify */
	struct variable_map_t *map;	/* values and labels map */
} plugins_params_t;

struct protocol_plugin_priv {
	const char **protocols;		/* NULL-terminated list of supported protocols, replacing GET_PLUGIN_PROTOCOLS */
	const status_t *statuses;	/* EKG_STATUS_NULL-terminated list of supported statuses */
};

typedef struct plugin {
	char *name;
	int prio;
	plugin_class_t pclass;
	plugin_destroy_func_t destroy;
	/* lt_dlhandle */ void *dl;
	plugins_params_t *params;
	plugin_theme_init_func_t theme_init;

	const void *priv;
} plugin_t;

/* Note about plugin_t.statuses:
 *	we currently put every supported status there, including unsettable by user,
 *	we assume that user cannot set states <= EKG_STATUS_NA
 * [XXX]
 */

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
		NULL,		\
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

/* must be power of 2 ;p */
#define QUERIES_BUCKETS 64

typedef struct query_node {
        struct query_node* next;
        char *name;
        int name_hash;
        plugin_t *plugin;
        void *data;
        query_handler_func_t *handler;
        int count;
} query_t;

int query_register(const char *name, ...);
query_t *query_connect(plugin_t *plugin, const char *name, query_handler_func_t *handler, void *data);
int query_emit(plugin_t *, const char *, ...);
int query_free(query_t* g);

void queries_reconnect();

void queries_list_destroy(query_t** kk);

void registered_queries_free();

#ifndef EKG2_WIN32_NOFUNCTION
extern GSList *plugins;
extern query_t *queries[];
#endif

#ifdef __cplusplus
}
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
