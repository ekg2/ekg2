/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz (deli@rzepaknet.us>
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
#include <gmodule.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "objects.h"

GSList *plugins = NULL;
/* XXX: not freed anywhere yet */
gchar *rel_plugin_dir = NULL;

static gint plugin_register_compare(gconstpointer a, gconstpointer b) {
	const plugin_t *data1 = (const plugin_t *) a;
	const plugin_t *data2 = (const plugin_t *) b;

	return data2->prio - data1->prio;
}

static void plugins_add(plugin_t *pl) {
	plugins = g_slist_insert_sorted(plugins, pl, plugin_register_compare);
}

void plugins_unlink(plugin_t *pl) {
	plugins = g_slist_remove(plugins, pl);
}

list_t watches = NULL;

query_t* queries[QUERIES_BUCKETS];

query_def_t* registered_queries;
int registered_queries_count = 0;

LIST_FREE_ITEM(query_free_data, query_t *) {
	xfree(data->name);
}

DYNSTUFF_LIST_DECLARE(queries_list, query_t, query_free_data,
	static __DYNSTUFF_ADD,
	static __DYNSTUFF_REMOVE_SAFE,
	__DYNSTUFF_DESTROY)

void ekg2_dlinit(const gchar *argv0) {
#ifdef SHARED_LIBS
	if (g_module_supported()) {
		/* Set relative plugin path based on executable location */
		gchar *progpath = g_find_program_in_path(argv0);

		if (progpath) {
			rel_plugin_dir = g_path_get_dirname(progpath);
			g_free(progpath);
		}
	}
#	ifndef STATIC_LIBS
	else {
		g_printerr("Dynamic module loading unsupported, and no static plugins.\n"
				"Please recompile with --enable-static.\n");
		abort();
	}
#	endif
#endif
}

#ifdef SHARED_LIBS
/**
 * ekg2_dlclose()
 *
 * Close handler to dynamic loaded library.
 *
 * @param plugin - Handler to loaded library.
 *
 * @return	0 on success, else fail.
 */

int ekg2_dlclose(GModule *plugin) {
	return (g_module_close(plugin) != TRUE);
}

/**
 * ekg2_dlopen()
 *
 * Load dynamic library file from @a name
 *
 * @todo Think more about flags for dlopen() [was: RTLD_LAZY | RTLD_GLOBAL]
 *
 * @param name - Full path of library to load.
 *
 * @return Pointer to the loaded library, or NULL if fail.
 */

static GModule *ekg2_dlopen(const char *name) {
	/* RTLD_LAZY is bad flag, because code can SEGV on executing undefined symbols...
	 *	it's better to fail earlier than later with SIGSEGV
	 *
	 * RTLD_GLOBAL is bad flag also, because we have no need to export symbols to another plugns
	 *	we should do it by queries... Yeah, I know it was used for example in perl && irc plugin.
	 *	But we cannot do it. Because if we load irc before sim plugin. Than we'll have unresolved symbols
	 *	even if we load sim plugin later.
	 */
	/*
	 * RTLD_GLOBAL is required by perl and python plugins...
	 *	need investigation. [XXX]
	 */
	GModule *tmp = g_module_open(name, 0);

	if (!tmp) {
		char *errstr = ekg_recode_from_locale(g_module_error());
		debug_warn("[plugin] could not be loaded: %s %s\n", name, errstr);
		g_free(errstr);
	} else {
		debug_ok("[plugin] loaded: %s\n", name);
	}
	return tmp;
}

/**
 * ekg2_dlsym()
 *
 * Get symbol with @a name from loaded dynamic library.
 *
 * @param plugin	- Pointer to the loaded library.
 * @param name		- Name of symbol to lookup.
 *
 * @return Address of symbol or NULL if error occur.
 */

static void *ekg2_dlsym(GModule *plugin, char *name) {
	void *tmp;

	if (!g_module_symbol(plugin, name, &tmp)) {
		debug_error("[plugin] plugin: %x symbol: %s error: %s\n", plugin, name, g_module_error());
		return NULL;
	}

	return tmp;
}
#endif

/*
 * plugin_load()
 *
 * ³aduje wtyczkê o podanej nazwie.
 * 
 * 0/-1
 */
int plugin_load(const char *name, int prio, int quiet)
{
#ifdef SHARED_LIBS
	const gchar *env_ekg_plugins_path = NULL;
	char *init = NULL;
	gchar *lib;
	gchar *libname;
	GModule *plugin = NULL;
#endif

	plugin_t *pl;
	int (*plugin_init)() = NULL;

	g_assert(name);
	if (plugin_find(name)) {
		printq("plugin_already_loaded", name); 
		return -1;
	}

#ifdef SHARED_LIBS
	libname = g_strdup_printf("%s.la", name);
	if ((env_ekg_plugins_path = g_getenv("EKG_PLUGINS_PATH"))) {
		lib = g_build_filename(env_ekg_plugins_path, libname, NULL);
		plugin = ekg2_dlopen(lib);
		g_free(lib);

		if (!plugin) {
			lib = g_build_filename(env_ekg_plugins_path, name, libname, NULL);
			plugin = ekg2_dlopen(lib);
			g_free(lib);
		}
	}

	/* The following lets ekg2 load plugins when it is run directly from
	 * the source tree, without installation. This can be beneficial when
	 * developing the program, or for less knowlegeable users, who don't
	 * know how to or cannot for some other reason use installation prefix
	 * to install in their home directory. It might be also useful
	 * for win32-style installs.
	 */
	if (!plugin && rel_plugin_dir) {
		lib = g_build_filename(rel_plugin_dir, "plugins", name, libname, NULL);
		plugin = ekg2_dlopen(lib);
		g_free(lib);
	}

	if (!plugin) {
		lib = g_build_filename(PLUGINDIR, libname, NULL);
		plugin = ekg2_dlopen(lib);
		g_free(lib);
	}

	g_free(libname);
	/* prefer shared plugins */
	if (plugin) {
		init = g_strdup_printf("%s_plugin_init", name);
		plugin_init = ekg2_dlsym(plugin, init);
		g_free(init);
	}
#endif /* SHARED_LIBS */

#ifdef STATIC_LIBS
	/* if no shared plugin, fallback to the static one */
	if (!plugin_init) {
		STATIC_PLUGIN_DECLS
		STATIC_PLUGIN_CALLS

		if (plugin_init)
			debug_ok("[plugin] statically compiled in: %s\n", name);
	}
#endif

	if (!plugin_init) {
#ifdef SHARED_LIBS
		if (plugin) {
			printq("plugin_incorrect", name);
			ekg2_dlclose(plugin);
		} else
#endif
		printq("plugin_doesnt_exist", name);
		return -1;
	}

	if (plugin_init(prio) == -1) {
		printq("plugin_not_initialized", name);
#ifdef SHARED_LIBS
		if (plugin)
			ekg2_dlclose(plugin);
#endif
		return -1;
	}

	if ((pl = plugin_find(name))) {
#ifdef SHARED_LIBS
		pl->dl = plugin;
#else
		pl->dl = NULL;
#endif
	} else {
		debug_error("plugin_load() plugin_find(%s) not found.\n", name);
		/* It's FATAL */
	}

	query_emit(pl, "set-vars-default");

	printq("plugin_loaded", name);

	if (!in_autoexec) {
		in_autoexec = 1;
		config_read(name);
		if (pl->pclass == PLUGIN_PROTOCOL)
			session_read(name);

		if (pl)
			query_emit(pl, "config-postinit");

		in_autoexec = 0;
		config_changed = 1;
	}
	return 0;
}

/**
 * plugin_find()
 *
 * Find plugin by name
 *
 * @param name - name of plugin_t
 *
 * @return plugin_t with given name, or NULL if not found.
 */

plugin_t *plugin_find(const char *name)
{
	GSList *pl;

	for (pl = plugins; pl; pl = pl->next) {
		plugin_t *p = pl->data;
		if (!xstrcmp(p->name, name))
			return p;
	}

	return NULL;
}

/**
 * plugin_find_uid()
 *
 * Find <i>PLUGIN_PROTOCOL</i> plugin which can handle @a uid
 * 
 * @todo used only by session_add() in session.c move it there?
 *
 * @sa valid_plugin_uid() - For function to check if given plugin can handle given uid
 *
 * @return If such plugin was founded return it, or NULL if not found.
 */

plugin_t *plugin_find_uid(const char *uid) {
	GSList *pl;

	for (pl = plugins; pl; pl = pl->next) {
		plugin_t *p = pl->data;
		if (p && p->pclass == PLUGIN_PROTOCOL && p->name && valid_plugin_uid(p, uid))
			return p;
	}

	return NULL;
}

/*
 * plugin_unload()
 *
 * usuwa z pamiêci dan± wtyczkê, lub je¶li wtyczka jest wkompilowana na
 * sta³e, deaktywuje j±.
 *
 * 0/-1
 */
int plugin_unload(plugin_t *p)
{
	char *name; 
	list_t l;

	if (!p)
		return -1;

	if (config_expert_mode == 0 && p->pclass == PLUGIN_UI) {
		GSList *pl;

		int unloadable = 0;
		for (pl = plugins; pl; pl = pl->next) {
			const plugin_t *plug = pl->data;
			if (plug->pclass == PLUGIN_UI && plug != p) 
				unloadable = 1;
		}
		if (!unloadable) {
			print("plugin_unload_ui", p->name);
			return -1;
		}
	}

#ifdef WATCHES_FIXME
	/* XXX: why not simply destroy them right now? */

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->plugin == p && (w->removed == 1 || w->removed == -1)) {
			print("generic_error", "XXX cannot remove this plugin when there some watches active");
			return -1;
		}
	}
	/* XXX, to samo dla timerow */
#endif

	name = xstrdup(p->name);

	if (p->destroy)
		p->destroy();

#ifdef SHARED_LIBS
	if (p->dl)
		ekg2_dlclose(p->dl);
#endif

	print("plugin_unloaded", name);

	xfree(name);

	if (!in_autoexec)
		config_changed = 1;

	return 0;
}

/*
 * plugin_register()
 *
 * rejestruje dan± wtyczkê.
 *
 * 0/-1
 */
int plugin_register(plugin_t *p, int prio) {
	if (prio == -254) {
		switch (p->pclass) {
			case PLUGIN_UI:
				p->prio = 0;
				break;
			case PLUGIN_LOG:
				p->prio = 5;
				break;
			case PLUGIN_SCRIPTING:
				p->prio = 10;
				break;
			case PLUGIN_PROTOCOL:
				p->prio = 15;
				break;
			default:
				p->prio = 20;
				break;
		}
	} else {
		p->prio = prio;
	}

	plugins_add(p);

	return 0;
}

/*
 * plugin_unregister()
 *
 * od³±cza wtyczkê.
 *
 * 0/-1
 */
int plugin_unregister(plugin_t *p)
{
	/* XXX eXtreme HACK warning
	 * (mp) na razie jest tak.  docelowo: wyladowywac pluginy tylko z
	 * glownego programu (queriesami?)
	 * to cos segfaultowalo (wczesniej czy pozniej), jesli bylo wywolane z
	 * ncurses.  niestety, problem pozostaje dla innych pluginow i takiego
	 * np. rc. sie zrobi nast razem */

	/* j/w If any plugin has backtrace here, and we try to remove it from memory.
	 * ekg2 do SEGV.
	 */

	session_t *s;
	query_t **kk;
	GSList *vl, *cl;
	list_t l;

	g_assert(p);

/* XXX think about sequence of unloading....: currently: watches, timers, sessions, queries, variables, commands */

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->plugin == p)
			watch_free(w);
	}

	ekg_source_remove_by_plugin(p, NULL);

	for (s = sessions; s; ) {
		session_t *next = s->next;

		if (s->plugin == p)
			session_remove(s->uid);
		s = next;
	}

	for (kk = queries; kk < &queries[QUERIES_BUCKETS]; ++kk) {
		query_t *g;

		for (g = *kk; g; ) {
			query_t *next = g->next;
			if (g->plugin == p)
				queries_list_remove(kk, g);
			g = next;
		}
	}

	for (vl = variables; vl;) {
		variable_t *v = vl->data;

		vl = vl->next;
		if (v->plugin == p)
			variables_remove(v);
	}

	for (cl = commands; cl;) {
		command_t *c = cl->data;

		cl = cl->next;
		if (c->plugin == p)
			commands_remove(c);
	}

	plugins_unlink(p);

	return 0;
}

/**
 * plugin_var_find()
 *
 * it looks for given variable name in given plugin
 *
 * @param	pl - plugin
 * @param	name - variable name
 *
 * returns sequence number+1 of variable if found, else 0
 */

int plugin_var_find(plugin_t *pl, const char *name) {
	int i;

	if (!pl || !pl->params)
		return 0;

	for (i = 0; (pl->params[i].key /* && pl->params[i].id != -1 */); i++) {
		if (!xstrcasecmp(pl->params[i].key, name))
			return i+1;
	}
	return 0;
}

int plugin_var_add(plugin_t *pl, const char *name, int type, const char *value, int secret, plugin_notify_func_t *notify) { return -1; }


static LIST_FREE_ITEM(registered_query_free_data, query_def_t *) {
	xfree(data->name);
}

void registered_queries_free() {
	if (!registered_queries)
	    return;

	LIST_DESTROY2(registered_queries, registered_query_free_data);

	/* this has been already done in call above */
	registered_queries = NULL;
}

static int query_register_common(const char* name, query_def_t **res) {
	query_def_t *gd;
	int found = 0, name_hash = ekg_hash(name);

	for (gd = registered_queries; gd; gd = gd->next) {
	    if (name_hash == gd->name_hash && !xstrcmp(gd->name, name)) {
			found = 1;
			break;
		}
	}
	if (found) {
		debug_error("query_register() oh noez, seems like it's already registered: [%s]\n", name);
		debug_error("I'm not sure what I should do, so I'm simply bailing out...\n");
		return -1;

	} else {
		gd            = xmalloc(sizeof(query_def_t));
		gd->name      = xstrdup(name);
		gd->name_hash = name_hash;
		registered_queries_count++;

		LIST_ADD2(&registered_queries, gd);
	}

	*res = gd;

	return 0;
}

int query_register(const char *name, ...) {
	query_def_t *gd;
	int i, arg;
	va_list va;

	if (query_register_common(name, &gd)) {
	    return -1;
	}

	va_start(va, name);
	for (i = 0; i < QUERY_ARGS_MAX; i++) {
		arg = va_arg(va, int);
		gd->params[i] = arg;
		if (arg == QUERY_ARG_END)
			break;
	}
	va_end(va);
	return 0;
}

/*
 * alternative way for registering queries
 */
int query_register_const(const query_def_t *def) {
        query_def_t *gd;

	if (query_register_common(def->name, &gd)) {
	    return -1;
	}
	memcpy(gd->params, def->params, sizeof(def->params));

	return 0;
}


int query_free(query_t* g) {

    queries_list_remove(&queries[g->name_hash & (QUERIES_BUCKETS - 1)], g);

    return 0;
}

query_t *query_connect(plugin_t *plugin, const char *name, query_handler_func_t *handler, void *data) {
	int found = 0;
	query_def_t* gd;

	query_t *q = xmalloc(sizeof(query_t));

	q->name         = xstrdup(name);
	q->name_hash    = ekg_hash(name);
	q->plugin	= plugin;
	q->handler	= handler;
	q->data		= data;

	for (gd = registered_queries; gd; gd = gd->next) {
		if (q->name_hash == gd->name_hash && !xstrcmp(gd->name, name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		debug_error("query_connect() NOT FOUND[%d]: %s\n", registered_queries_count, __(name));

		gd            = xmalloc(sizeof(query_def_t));
		gd->name      = xstrdup(name);
		gd->name_hash = q->name_hash;
		registered_queries_count++;

		LIST_ADD2(&registered_queries, gd);
	}

	queries_list_add(&queries[q->name_hash & (QUERIES_BUCKETS - 1)], q);

	return q;
}

static int query_emit_inner(query_t *g, va_list ap) {
	static int nested = 0;
	int (*handler)(void *data, va_list ap) = g->handler;
	int result;
	va_list ap_plugin;

	if (nested >= 32) {
		return -1;
	}

	g->count++;
	/*
	 * pc and amd64: va_arg remove var from va_list when you use va_arg, 
	 * so we must keep orig va_list for next plugins
	 */
	nested++;;
	G_VA_COPY(ap_plugin, ap);
	result = handler(g->data, ap_plugin);
	va_end(ap_plugin);
	nested--;

	return result != -1 ? 0 : -1;
}

int query_emit(plugin_t *plugin, const char* name, ...) {
	int result = -2;
	va_list ap;
	query_t* g;
	int name_hash, bucket_id;

	name_hash = ekg_hash(name);
	bucket_id = name_hash & (QUERIES_BUCKETS - 1);

	va_start(ap, name);

	for (g = queries[bucket_id]; g; g = g->next) {
	    if (name_hash == g->name_hash && (!plugin || (plugin == g->plugin)) && !xstrcmp(name, g->name)) {

		result = query_emit_inner(g, ap);

		if (result == -1) {
		    break;
		}
	    }
	}

	va_end(ap);

	return result;
}

static LIST_ADD_COMPARE(query_compare, query_t *) {
	/*				any other suggestions: vvv ? */
	const int ap = (data1->plugin ? data1->plugin->prio : -666);
	const int bp = (data2->plugin ? data2->plugin->prio : -666);

	return (bp-ap);
}

/**
 * queries_reconnect()
 *
 * Reconnect (resort) all queries, e.g. after plugin prio change.
 */

void queries_reconnect() {
	size_t i;
	for (i = 0; i < QUERIES_BUCKETS; ++i) {
		LIST_RESORT2(&(queries[i]), query_compare);
	}
}

/**
 * have_plugin_of_class()
 *
 * Check if we have loaded plugin from @a pclass
 *
 * @param pclass 
 *
 * @return	1 - If such plugin was founded<br>
 *		else 0
 */

int have_plugin_of_class(plugin_class_t pclass) {
	GSList *pl;

	for (pl = plugins; pl; pl = pl->next) {
		const plugin_t *p = pl->data;
		if (p->pclass == pclass) return 1;
	}

	return 0;
}

PROPERTY_INT_SET(watch, timeout, time_t)

/*
 *  plugin_abi_version()
 *
 * @param plugin_abi_ver, plugin_name
 *
 * @return	1 - if core ABI version is the sama as plugin ABI version
 *		else 0
 */
int plugin_abi_version(int plugin_abi_ver, const char * plugin_name) {

	if (EKG_ABI_VER == plugin_abi_ver)
		return 1;

	debug_error("ABI versions mismatch.  %s_plugin ABI ver. %d,  core ABI ver. %d\n", plugin_name, plugin_abi_ver, EKG_ABI_VER);
	return 0;

}
/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
