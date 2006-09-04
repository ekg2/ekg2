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
#include "char.h"
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
void *ekg2_dlopen(char *name) {
	void *tmp = NULL;
#ifdef NO_POSIX_SYSTEM
	tmp = LoadLibraryA(name);
#else
	tmp = dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
#endif
/*	if (!tmp && !in_autoexec) debug("[plugin] Error loading plugin %s: %s\n", name, dlerror()); */
/*	return lt_dlopen(lib); */
	return tmp;
}

/* it only support posix dlsym() but maybe in future... */
void *ekg2_dlsym(void *plugin, char *name) {
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
int plugin_load(const CHAR_T *name, int prio, int quiet)
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
                lib = saprintf("%s/" CHARF ".so", env_ekg_plugins_path, name);
                plugin = ekg2_dlopen(lib);
                if (!plugin) {
                        xfree(lib);
                        lib = saprintf("%s/" CHARF "/.libs/" CHARF ".so", env_ekg_plugins_path, name, name);
                        plugin = ekg2_dlopen(lib);
                }
        }

        if (!plugin) {
                xfree(lib);
                lib = saprintf("plugins/" CHARF "/.libs/" CHARF ".so", name, name);
                plugin = ekg2_dlopen(lib);
        }

        if (!plugin) {
                xfree(lib);
                lib = saprintf("../plugins/" CHARF "/.libs/" CHARF ".so", name, name);
                plugin = ekg2_dlopen(lib);
        }

	if (!plugin) {
		xfree(lib);
		lib = saprintf("%s/" CHARF ".so", PLUGINDIR, name);
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
		init = saprintf(CHARF "_plugin_init", name);

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

		if (!xwcscasecmp(p->name, name)) {
			p->dl = plugin;
			break;
		}
	}

	wcs_printq("plugin_loaded", name);

	if (!in_autoexec) {
		char *tmp = saprintf("config-" CHARF, name);
		char *tmp2= saprintf("sessions-" CHARF, name);

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
plugin_t *plugin_find(const CHAR_T *name)
{
	list_t l;

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (p && !xwcscmp(p->name, name))
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
	CHAR_T *name; 

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

	name = xwcsdup(p->name);

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
			debug("[EKG_INTERNAL_ERROR] %s:%d Removed more than one watch...\n", __FILE__, __LINE__);
			goto plugin_watches_again;
		}
		ekg_watches_removed = 0;

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

int plugin_theme_reload(plugin_t *p)
{
	if (p->theme_init)
		p->theme_init();

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

query_t *query_connect(plugin_t *plugin, const CHAR_T *name, query_handler_func_t *handler, void *data)
{
	query_t *q = xmalloc(sizeof(query_t));

	q->plugin	= plugin;
	q->name		= xwcsdup(name);
	q->handler	= handler;
	q->data		= data;

	return list_add(&queries, q, 0);
}

int query_free(query_t *q) {
	if (!q) return -1;

	xfree(q->name);
	list_remove(&queries, q, 1);
	return 0;
}

int query_disconnect(plugin_t *plugin, const CHAR_T *name)
{
	list_t l;

	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if (q->plugin == plugin && q->name == name) {
			return query_free(q);
		}
	}

	return -1;
}

int query_emit(plugin_t *plugin, const CHAR_T *name, ...)
{
	static int nested = 0;
	int result = -2;
	va_list ap;
	va_list ap_plugin;
	list_t l;

	if (nested > 32) {
/*
		if (nested == 33)
			debug("too many nested queries. exiting to avoid deadlock\n");
 */
		return -1;
	}

	nested++;

	va_start(ap, name);
	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if ((!plugin || (plugin == q->plugin)) && !xwcscmp(q->name, name)) {
			int (*handler)(void *data, va_list ap) = q->handler;

			q->count++;

			result = 0;
			/*
			 * pc and amd64: va_arg remove var from va_list when you use va_arg, 
			 * so we must keep orig va_list for next plugins
			 */
			va_copy(ap_plugin, ap);

			if (handler(q->data, ap_plugin) == -1) {
				result = -1;
				goto cleanup;
			}

			va_end(ap_plugin);
		}
	}

cleanup:
	va_end(ap);

	nested--;

	return result;
}

query_t *query_find(const CHAR_T *name)
{
        list_t l;

        for (l = queries; l; l = l->next) {
                query_t *q = l->data;

                if (!xwcscasecmp(q->name, name))
                        return q;
        }

        return 0;
}

/*
 * watch_new()
 *
 * tworzy nowy obiekt typu watch_t i zwraca do niego wska¼nik.
 *
 *  - plugin - obs³uguj±cy plugin
 *  - fd - obserwowany deskryptor
 *  - type - rodzaj obserwacji watch_type_t
 */
watch_t *watch_new(plugin_t *plugin, int fd, watch_type_t type)
{
	watch_t *w = xmalloc(sizeof(watch_t));

	w->plugin = plugin;
	w->fd = fd;
	w->type = type;

	if (w->type == WATCH_READ_LINE) {
		w->type = WATCH_READ;
		w->buf = string_init(NULL);
	} else if (w->type == WATCH_WRITE_LINE) {
		w->type = WATCH_WRITE;
		w->buf = string_init(NULL);
	}
	
	w->started = time(NULL);

	list_add(&watches, w, 0);

	return w;
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
		debug("[INTERNAL_DEBUG] WATCH_LINE_WRITE must be removed by plugin, manually (settype to WATCH_NONE and than call watch_free()\n");
		return;
	}

	w->removed = 2;
		
	if (w->buf) {
		int (*handler)(int, int, const char *, void *) = w->handler;
		string_free(w->buf, 1);
		/* DO WE WANT TO SEND ALL TEXT IN BUFOR TO FD ? IF IT'S WATCH_WRITE_LINE? or parse all data if it's WATCH_READ_LINE? mmh. XXX */
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
		int index = tmp - w->buf->str;
		char *line = xstrmid(w->buf->str, 0, index);
		string_t new;
			
		if (xstrlen(line) > 1 && line[xstrlen(line) - 1] == '\r')
			line[xstrlen(line) - 1] = 0;

		if ((res = handler(0, w->fd, line, w->data)) == -1) {
			xfree(line);
			break;
		}

		new = string_init(w->buf->str + index + 1);
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
	debug("[watch_handle_write] fd: %d in queue: %d bytes.... ", w->fd, len);
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

	debug(" ... wrote:%d bytes (handler: 0x%x) ", res, handler);

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
	debug("left: %d bytes\n", w->buf->len);

	w->removed = 0;
	return res;
}

int watch_write(watch_t *w, const char *format, ...) {
	char		*text;
	int		was_empty = 0;
	int		textlen;
	va_list		ap;

	if (!w || !format)
		return -1;

	va_start(ap, format);
	text = vsaprintf(format, ap);
	va_end(ap);
	
	textlen = xstrlen(text); 

	debug("[watch]_send: %s\n", text ? textlen ? text: "[0LENGTH]":"[FAILED]");
	if (!text) return -1;
		/* we don't really need full length of string... so we check if it's NULL or first letter is NUL. */ 
	was_empty = (!w->buf->str || !(*w->buf->str));	
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

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, watcher_handler_func_t *handler, void *data)
{
	watch_t *w = watch_new(plugin, fd, type);

	watch_handler_set(w, handler);
	watch_data_set(w, data);
	
	return w;
}

int watch_remove(plugin_t *plugin, int fd, watch_type_t type)
{
	int res = -1;
	watch_t *w;

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

PROPERTY_INT(watch, timeout, time_t)
PROPERTY_DATA(watch)
PROPERTY_MISC(watch, handler, watch_handler_func_t, NULL)



/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
