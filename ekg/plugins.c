/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 * 		  2004 Piotr Kupisiewicz (deli@rzepaknet.us>
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
#include "win32.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef NO_POSIX_SYSTEM
#  include <dlfcn.h>
#else 
#  include <winbase.h>
#endif

#include "configfile.h"
#include "commands.h"
#include "debug.h"
#include "dynstuff.h"
#include "objects.h"
#include "plugins.h"
#include "userlist.h"
#include "stuff.h"
#include "vars.h"
#include "themes.h"
#include "xmalloc.h"


#define __DECLARE_QUERIES_STUFF
#include "queries.h"

#if !defined(va_copy) && defined(__va_copy)
#define va_copy(DST,SRC) __va_copy(DST,SRC)
#endif

list_t plugins = NULL;
list_t queries = NULL;
list_t watches = NULL;

#ifdef EKG2_WIN32_HELPERS
# define WIN32_REQUEST_HELPER
# include "win32_helper.h"
#endif

int ekg2_dlinit() {
#ifdef EKG2_WIN32_HELPERS
	INIT_HELPER_FUNC(&win32_helper);

	int i;
	for (i = 0; i < (sizeof(win32_helper) / sizeof(void *)); i++) {
		void **cur = & ((void **) &win32_helper)[i];
		if (!*cur) {
			*cur = (void *) &win32_stub_function;
			printf("Making evil thing on element: %d\n", i);
		}
	}
#endif

	return 0;
/*	return lt_dlinit() */
}

/* it only support posix dlclose() but maybe in future... */
int ekg2_dlclose(void *plugin) {
#ifndef NO_POSIX_SYSTEM
	return dlclose(plugin);
#else
	return FreeLibrary(plugin);
#endif
/*	return lt_dlclose(plugin); */
}

/* it only support posix dlopen() but maybe in future... */
static void *ekg2_dlopen(char *name) {
	void *tmp = NULL;
#ifdef NO_POSIX_SYSTEM
	tmp = LoadLibraryA(name);
#else
	tmp = dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
#endif
	if(!tmp) debug_error("[plugin] could not be loaded: %s %s\n", name, dlerror());
	else debug_function("[plugin] loaded: %s\n", name);
/*	if (!tmp && !in_autoexec) debug("[plugin] Error loading plugin %s: %s\n", name, dlerror()); */
/*	return lt_dlopen(lib); */
	return tmp;
}

/* it only support posix dlsym() but maybe in future... */
static void *ekg2_dlsym(void *plugin, char *name) {
#ifndef NO_POSIX_SYSTEM
	return dlsym(plugin, name);
#else
	return GetProcAddress(plugin, name);
#endif
/*	return lt_dlsym( (lt_dlhandle) plugin, init); */
}

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
	char *lib = NULL;
	char *env_ekg_plugins_path = NULL;
	char *init = NULL;
#endif

	void *plugin = NULL;
	int (*plugin_init)() = NULL;
	list_t l;

	if (!name)
		return -1;

	if (plugin_find(name)) {
		printq("plugin_already_loaded", name); 
		return -1;
	}
#ifdef SHARED_LIBS
#ifndef NO_POSIX_SYSTEM
        if ((env_ekg_plugins_path = getenv("EKG_PLUGINS_PATH"))) {
                lib = saprintf("%s/%s.so", env_ekg_plugins_path, name);
                plugin = ekg2_dlopen(lib);
                if (!plugin) {
                        xfree(lib);
                        lib = saprintf("%s/%s/.libs/%s.so", env_ekg_plugins_path, name, name);
                        plugin = ekg2_dlopen(lib);
                }
        }

        if (!plugin) {
                xfree(lib);
                lib = saprintf("plugins/%s/.libs/%s.so", name, name);
                plugin = ekg2_dlopen(lib);
        }

        if (!plugin) {
                xfree(lib);
                lib = saprintf("../plugins/%s/.libs/%s.so", name, name);
                plugin = ekg2_dlopen(lib);
        }

	if (!plugin) {
		xfree(lib);
		lib = saprintf("%s/%s.so", PLUGINDIR, name);
		plugin = ekg2_dlopen(lib);
	}
#else	/* NO_POSIX_SYSTEM */
	if (!plugin) {
		xfree(lib);
		lib = saprintf("c:\\ekg2\\plugins\\%s.dll", name);
		plugin = ekg2_dlopen(lib);
	}
#endif /* SHARED_LIBS */
	if (!plugin) {
		printq("plugin_doesnt_exist", name);
		xfree(lib);
		return -1;
	}

	xfree(lib);
#endif

#ifdef STATIC_LIBS
/* first let's try to load static plugin... */
	extern int jabber_plugin_init(int prio);
	extern int irc_plugin_init(int prio);
	extern int gtk_plugin_init(int prio);

	debug("searching for name: %s in STATICLIBS: %s\n", name, STATIC_LIBS);

	if (!xstrcmp(name, "jabber")) plugin_init = &jabber_plugin_init;
	if (!xstrcmp(name, "irc")) plugin_init = &irc_plugin_init;
	if (!xstrcmp(name, "gtk")) plugin_init = &gtk_plugin_init;
//	if (!xstrcmp(name, "miranda")) plugin_init = &miranda_plugin_init;
#endif

#ifdef SHARED_LIBS
	if (!plugin_init) {
# ifdef EKG2_WIN32_HELPERS
		void (*plugin_preinit)(void *);
		char *preinit = saprintf("win32_plugin_init");
		if (!(plugin_preinit = ekg2_dlsym(plugin, preinit))) {
			debug("NO_POSIX_SYSTEM, PLUGIN:%s NOT COMPILATED WITH EKG2_WIN32_SHARED_LIB?!\n", name);
			wcs_printq("plugin_incorrect", name);
			xfree(preinit);
			return -1;
		}
		xfree(preinit);
		plugin_preinit(&win32_helper);
# endif
/* than if we don't have static plugin... let's try to load it dynamicly */
		init = saprintf("%s_plugin_init", name);

		if (!(plugin_init = ekg2_dlsym(plugin, init))) {
			wcs_printq("plugin_incorrect", name);
			ekg2_dlclose(plugin);
			xfree(init);
			return -1;
		}
		xfree(init);
	}
#endif
	if (!plugin_init) {
		printq("plugin_doesnt_exist", name);
		return -1;
	}

	if (plugin_init(prio) == -1) {
		wcs_printq("plugin_not_initialized", name);
		ekg2_dlclose(plugin);
		return -1;
	}

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (!xstrcasecmp(p->name, name)) {
			p->dl = plugin;
			break;
		}
	}

	wcs_printq("plugin_loaded", name);

	if (!in_autoexec) {
		char *tmp = saprintf("config-%s", name);
		char *tmp2= saprintf("sessions-%s", name);

	/* XXX, in_autoexec, hack */
		in_autoexec = 1;
		config_read(prepare_path(tmp, 0));
		session_read(prepare_path(tmp2, 0));
		in_autoexec = 0;
		xfree(tmp);
		xfree(tmp2);

		config_changed = 1;
	}
	return 0;
}

/*
 * plugin_find()
 *
 * odnajduje plugin_t odpowiadaj±ce wtyczce o danej nazwie.
 */
plugin_t *plugin_find(const char *name)
{
	list_t l;

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (p && !xstrcmp(p->name, name))
			return p;
	}

	return NULL;
}

/*
 * plugin_find()
 *
 * odnajduje plugin_t odpowiadaj±cy podanemu uid'owie.
 */
plugin_t *plugin_find_uid(const char *uid)
{
        list_t l;

        for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;
                if (p && p->name && valid_plugin_uid(p, uid))
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

	if (!p)
		return -1;

	/* XXX eXtreme HACK warning
	 * (mp) na razie jest tak.  docelowo: wyladowywac pluginy tylko z
	 * glownego programu (queriesami?)
	 * to cos segfaultowalo (wczesniej czy pozniej), jesli bylo wywolane z
	 * ncurses.  niestety, problem pozostaje dla innych pluginow i takiego
	 * np. rc. sie zrobi nast razem */
	if (p->pclass == PLUGIN_PROTOCOL) {
		list_t l;

		for (l = sessions; l; ) {
			session_t *s = l->data;

			l = l->next;
		
			if (!s || !s->uid)
				continue;

			if (plugin_find_uid(s->uid) == p)
				session_remove(s->uid);
		}		
	} else if (p->pclass == PLUGIN_UI) {
		list_t l;
		int unloadable = 0;
		for (l=plugins; l; l = l->next) {
			plugin_t *plug = l->data;
			if (plug->pclass == PLUGIN_UI && plug != p) 
				unloadable = 1;
		}
		if (!unloadable) {
			print("plugin_unload_ui", p->name);
			return -1;
		}
	}

	name = xstrdup(p->name);

	if (p->destroy)
		p->destroy();

	if (p->dl) {
		ekg2_dlclose(p->dl);
	}

	wcs_print("plugin_unloaded", name);

        if (!in_autoexec)
                config_changed = 1;

	xfree(name);
	return 0;
}

static int plugin_register_compare(void *data1, void *data2)
{
        plugin_t *a = data1, *b = data2;

        return b->prio - a->prio;
}


/*
 * plugin_register()
 *
 * rejestruje dan± wtyczkê.
 *
 * 0/-1
 */
int plugin_register(plugin_t *p, int prio)
{
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

	list_add_sorted(&plugins, p, 0, plugin_register_compare);

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
	plugins_params_t **par;
	list_t l;

	if (!p)
		return -1;

	for (l = queries; l; ) {
		query_t *q = l->data;

		l = l->next;

		if (q->plugin == p)
			query_free(q);
	}


	for (l = variables; l; ) {
		variable_t *v = l->data;

		l = l->next;

		if (v && v->plugin == p) 
			variable_remove(v->plugin, v->name);
	}

	for (l = commands; l; ) {
		command_t *c = l->data;

		l = l->next;

		if (c->plugin == p)
			command_freeone(c);
	}
plugin_watches_again:
	ekg_watches_removed = 0;
	for (l = watches; l; ) {
		watch_t *w = l->data;

		l = l->next;

		if (ekg_watches_removed > 1) {
			debug_error("[EKG_INTERNAL_ERROR] %s:%d Removed more than one watch...\n", __FILE__, __LINE__);
			goto plugin_watches_again;
		}
		ekg_watches_removed = 0;

/* XXX here, to segvuje jesli na przyklad mamy dzialajacego watcha w tle.. a handler watcha zrobi cos ze trzeba watcha usunac (watch dzialajacy wiec w->remove = 1) 
 * 	a potem usunac caly plugin... watch_free() dla w->remove = 1 zadziala. a potem wraca do handlera watcha... gdzie radosnie mamy sigsegv. 
 * 	infrastuktura ekg2 ssie. w takim glibie mamy refcount dla takich rzeczy... tutaj jakies znaczniki w->removed, brak pomyslu jak zrobic zeby to dzialalo
 * 	na razie tylko komunikat. 
 *
 * 	testowac na pluginie rc. 
 *	polaczyc sie z ekg2 zrobic: /plugin -rc
 *	i wesolo debugowac....
 */
		if (w->plugin == p)
			watch_free(w);
	}

	for (l = timers; l; ) {
		struct timer *t = l->data;

		l = l->next;

		if (t->plugin == p) {
			list_remove(&timers, t, 0);
			xfree(t->name);
			xfree(t->data);
			xfree(t);
		}
	}

	if ((par = p->params)) {
		while (*par) {
			xfree((*par)->key);
			xfree((*par)->value);
			xfree((*par));
			par++;
		}
		xfree(p->params);
		p->params = NULL;
	}

	list_remove(&plugins, p, 0);

	return 0;
}

/* 
 * plugin_var_find()
 *
 * it looks for given var in given plugin
 *
 * returns pointer to this var or NULL if not found or error
 */
plugins_params_t *plugin_var_find(plugin_t *pl, const char *name)
{
	int i;
	
	if (!pl)
		return NULL;

	if (!pl->params)
		return NULL;

	for (i = 0; pl->params[i]; i++) {
		if (!xstrcasecmp(pl->params[i]->key, name))
			return pl->params[i];
	}

	return NULL;
}

/*
 * plugin_var_add()
 *
 * adds given var to the given plugin
 *
 * name - name
 * type - VAR_INT | VAR_STR
 * value - default_value
 * secret - hide when showing?
 */
int plugin_var_add(plugin_t *pl, const char *name, int type, const char *value, int secret, plugin_notify_func_t *notify)
{
	plugins_params_t *p;
	int i, count;

        p = xmalloc(sizeof(plugins_params_t));
        p->key = xstrdup(name);
	p->type = type;
	p->value = xstrdup(value);
	p->secret = secret;
        p->notify = notify;

	if (!pl->params) {
                pl->params = xmalloc(sizeof(plugins_params_t *) * 2);
                pl->params[0] = p;
                pl->params[1] = NULL;
                return 0;
        }

        for (i = 0, count = 0; pl->params[i]; i++)
                count++;

        pl->params = xrealloc(pl->params, (count + 2) * sizeof(plugins_params_t *));

        pl->params[count] = p;
        pl->params[count + 1] = NULL;

        return 0;
}

void query_external_free() {
	list_t l;

	for (l = queries_external; l; l = l->next) {
		struct query *a = l->data;

		xfree(a->name);
	}
	list_destroy(queries_external, 1);

	queries_external 	= NULL;
	queries_count		= QUERY_EXTERNAL;
}

int query_id(const char *name) {
	struct query *a = NULL;
	list_t l;
	int i;

	for (i=0; i < QUERY_EXTERNAL; i++) {
		if (!xstrcmp(query_list[i].name, name)) {
#ifdef DDEBUG
			debug_error("Use query_connect_id()/query_emit_id() for: %s\n", name);
#endif
			return query_list[i].id;
		}
	}

	for (l = queries_external; l; l = l->next) {
		a = l->data;

		if (!xstrcmp(a->name, name))
			return a->id;
	}
	debug_error("query_id() NOT FOUND[%d]: %s\n", queries_count - QUERY_EXTERNAL, __(name));

	a 	= xmalloc(sizeof(struct query));
	a->id 	= queries_count++;
	a->name	= xstrdup(name);

	list_add(&queries_external, a, 0);

	return a->id;
}

const char *query_name(const int id) {
	list_t l;

	if (id < QUERY_EXTERNAL) 
		return query_list[id].name;

	for (l = queries_external; l; l = l->next) {
		struct query* a = l->data;

		if (a->id == id) 
			return a->name;
	}

	debug_error("[%s:%d] query_name() REALLY NASTY (%d)\n", __FILE__, __LINE__, id);

	return NULL;
}

static query_t *query_connect_common(plugin_t *plugin, const int id, query_handler_func_t *handler, void *data) {
	query_t *q = xmalloc(sizeof(query_t));

	q->id		= id;
	q->plugin	= plugin;
	q->handler	= handler;
	q->data		= data;

	return list_add(&queries, q, 0);
}

#define ID_AND_QUERY_EXTERNAL	\
	"Here, we have two possibilites.\n"	\
		"\t1/ You are lazy moron, who doesn't rebuilt core but use plugin with this enum...\n"	\
		"\t2/ You are ekg2 hacker who use query_id(\"your_own_query_name\") to get new query id and use this id with "	\
			"query_connect_id()... but unfortunetly it won't work cause of 1st possibility, sorry.\n"


query_t *query_connect_id(plugin_t *plugin, const int id, query_handler_func_t *handler, void *data) {
	if (id >= QUERY_EXTERNAL) {
		debug_error("%s", ID_AND_QUERY_EXTERNAL);
		return NULL;
	}

	return query_connect_common(plugin, id, handler, data);
}

query_t *query_connect(plugin_t *plugin, const char *name, query_handler_func_t *handler, void *data) {
	return query_connect_common(plugin, query_id(name), handler, data);
}

int query_free(query_t *q) {
	if (!q) return -1;

	list_remove(&queries, q, 1);
	return 0;
}

int query_disconnect(plugin_t *plugin, const char *name)
{
	debug_error("[%s:%d] query_disconnect() THIS FUNCTION IS OBSOLETE, USE query_free() INSTEAD..\n");
	return -1;
}

static int query_emit_common(query_t *q, va_list ap) {
	static int nested = 0;
	int (*handler)(void *data, va_list ap) = q->handler;
	int result;
	va_list ap_plugin;

	nested++;

	if (nested > 32) {
/*
		if (nested == 33)
			debug("too many nested queries. exiting to avoid deadlock\n");
 */
		return -1;
	}

	q->count++;

	/*
	 * pc and amd64: va_arg remove var from va_list when you use va_arg, 
	 * so we must keep orig va_list for next plugins
	 */
	va_copy(ap_plugin, ap);
	result = handler(q->data, ap_plugin);
	va_end(ap_plugin);

	nested--;

	return result != -1 ? 0 : -1;
}

int query_emit_id(plugin_t *plugin, const int id, ...) {
	int result = -2;
	va_list ap;
	list_t l;

	if (id >= QUERY_EXTERNAL) {
		debug_error("%s", ID_AND_QUERY_EXTERNAL);
		return -2;
	}
		
	va_start(ap, id);
	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if ((!plugin || (plugin == q->plugin)) && q->id == id) {
			result = query_emit_common(q, ap);

			if (result == -1) break;
		}
	}
	va_end(ap);

	return result;
}

int query_emit(plugin_t *plugin, const char *name, ...) {
	int result = -2;
	va_list ap;
	list_t l;
	int id;

	id = query_id(name);

	va_start(ap, name);
	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if ((!plugin || (plugin == q->plugin)) && q->id == id) {
			result = query_emit_common(q, ap);

			if (result == -1) break;
		}
	}
	va_end(ap);

	return result;
}

/*
 * watch_find()
 *
 * zwraca obiekt watch_t o podanych parametrach.
 */
watch_t *watch_find(plugin_t *plugin, int fd, watch_type_t type)
{
	list_t l;
	
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;
		if (w->plugin == plugin && w->fd == fd && w->type == type && !(w->removed > 0))
			return w;
	}

	return NULL;
}

/*
 * watch_free()
 *
 * zwalnia pamiêæ po obiekcie watch_t.
 */
void watch_free(watch_t *w)
{
	if (!w)
		return;
	if (w->removed == -1) { /* watch is running.. we cannot remove it */
		w->removed = 1;
		return;
	} else if (w->removed == 2) /* watch is already removed, from other thread? */
		return;

	if (w->type == WATCH_WRITE && w->buf && !w->handler) { 
		debug_error("[INTERNAL_DEBUG] WATCH_LINE_WRITE must be removed by plugin, manually (settype to WATCH_NONE and than call watch_free()\n");
		return;
	}

	w->removed = 2;
		
	if (w->buf) {
		int (*handler)(int, int, const char *, void *) = w->handler;
		string_free(w->buf, 1);
		/* DO WE WANT TO SEND ALL  IN BUFOR TO FD ? IF IT'S WATCH_WRITE_LINE? or parse all data if it's WATCH_READ_LINE? mmh. XXX */
		if (handler)
			handler(1, w->fd, NULL, w->data);
	} else {
		int (*handler)(int, int, int, void *) = w->handler;
		if (handler)
			handler(1, w->fd, w->type, w->data);
	}
	list_remove(&watches, w, 1);
	ekg_watches_removed++;
	debug("watch_free() REMOVED WATCH, watches removed this loop: %d oldwatch: 0x%x\n", ekg_watches_removed, w);
}

/*
 * watch_handle_line()
 *
 * obs³uga deskryptorów przegl±danych WATCH_READ_LINE.
 */
void watch_handle_line(watch_t *w)
{
	char buf[1024], *tmp;
	int ret, res = 0;
	int (*handler)(int, int, const char *, void *) = w->handler;

	if (w || w->removed == -1);	/* watch is running in another thread / context */

	w->removed = -1;
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
		string_t new;

		/* we strndup() str with len == strlen, so we don't need to call xstrlen() */
		if (strlen > 1 && line[strlen - 1] == '\r')
			line[strlen - 1] = 0;

		if ((res = handler(0, w->fd, line, w->data)) == -1) {
			xfree(line);
			break;
		}
	/* maybe simple memmove() insteasd of string_free() + string_init() ? */
		new = string_init(w->buf->str + strlen + 1);
		string_free(w->buf, 1);
		w->buf = new;
		xfree(line);
	}

	/* je¶li koniec strumienia, lub nie jest to ci±g³e przegl±danie,
	 * zwolnij pamiêæ i usuñ z listy */
	if (res == -1 || ret == 0 || (ret == -1 && errno != EAGAIN) || w->removed == 1) {
		int fd = w->fd;
		w->removed = 0;

		watch_free(w);
		close(fd);
		return;
	} 
	w->removed = 0;
}

/* ripped from irc plugin */
int watch_handle_write(watch_t *w) {
	int (*handler)(int, int, const char *, void *) = w->handler;
	int res = -1;
	int len = (w && w->buf) ? w->buf->len : 0;

	if (!w || w->removed == -1) return -1;	/* watch is running in another thread / context */
	if (w->transfer_limit == -1) return 0;	/* transfer limit turned on, don't send anythink... XXX */
	debug_io("[watch_handle_write] fd: %d in queue: %d bytes.... ", w->fd, len);
	if (!len) return -1;

	w->removed = -1;

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
		w->removed = 0;
		watch_free(w);
#endif
		return -1;
	} else if (res == len) {
		string_clear(w->buf);
	} else {
		memmove(w->buf->str, w->buf->str + res, len - res);
		w->buf->len -= res;
	}
	debug_io("left: %d bytes\n", w->buf->len);

	w->removed = 0;
	return res;
}

int watch_write(watch_t *w, const char *format, ...) {
	char		*text;
	int		was_empty;
	int		textlen;
	va_list		ap;

	if (!w || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);
	
	textlen = xstrlen(text); 

	debug_io("[watch]_send: %s\n", text ? textlen ? text: "[0LENGTH]":"[FAILED]");
	if (!text) return -1;

	was_empty = !w->buf->len;
	string_append_n(w->buf, text, textlen);

	xfree(text);

	if (was_empty) return watch_handle_write(w); /* let's try to write somethink ? */
	return 0;
}

/*
 * watch_handle()
 *
 * obs³uga deskryptorów typu WATCH_READ i WATCH_WRITE. je¶li wyst±pi na
 * nich jakakolwiek aktywno¶æ, wywo³ujemy dan± funkcjê. je¶li nie jest
 * to sta³e przegl±danie, usuwamy.
 */
void watch_handle(watch_t *w)
{
	int (*handler)(int, int, int, void *);
	int res;
	if (!w || w->removed == -1)	/* watch is running in another thread / context */
		return;

	w->removed = -1;
	handler = w->handler;
		
	res = handler(0, w->fd, w->type, w->data);

	if (res == -1 || w->removed == 1) {
		w->removed = 0;
		watch_free(w);
		return;
	} else {
		w->started = time(NULL);
	}
	w->removed = 0;
}

/*
 * watch_add()
 *
 * tworzy nowy obiekt typu watch_t i zwraca do niego wska¼nik.
 *
 *  - plugin - obs³uguj±cy plugin
 *  - fd - obserwowany deskryptor
 *  - type - rodzaj obserwacji watch_type_t
 *  - handler - handler
 *  - data - data przekazywana do handlera
 */
watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, watcher_handler_func_t *handler, void *data) {
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

	list_add_beginning(&watches, w, 0);

	return w;
}

int watch_remove(plugin_t *plugin, int fd, watch_type_t type)
{
	int res = -1;
	watch_t *w;
/* XXX, here can be deadlock feel warned. */
	while ((w = watch_find(plugin, fd, type))) {
		watch_free(w);
		res = 0;
	}

	return res;
}

int have_plugin_of_class(int pclass) {
	list_t l;
	for(l = plugins; l; l = l->next) {
		plugin_t *p = l->data;
		if (p->pclass == pclass) return 1;
	}
	return 0;
}

PROPERTY_INT_SET(watch, timeout, time_t)

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
