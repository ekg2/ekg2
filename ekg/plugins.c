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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#ifdef HAVE_DLFCN_H
#  include <dlfcn.h>
#endif

#include "commands.h"
#include "events.h"
#include "dynstuff.h"
#include "objects.h"
#include "plugins.h"
#include "stuff.h"
#include "vars.h"
#include "xmalloc.h"
#include "userlist.h"
#include "themes.h"
#include "ltdl.h"

list_t plugins = NULL;
list_t queries = NULL;

/*
 * plugin_load()
 *
 * ³aduje wtyczkê o podanej nazwie.
 * 
 * 0/-1
 */
int plugin_load(const char *name, int quiet)
{
	char *lib = NULL;
	char *env_ekg_plugins_path = NULL;
	lt_dlhandle plugin = NULL;
	char *init = NULL;
	int (*plugin_init)() = NULL;
	list_t l;

	if (!name)
		return -1;

	if (plugin_find(name)) {
		printq("plugin_already_loaded", name); 
		return -1;
	}

        if ((env_ekg_plugins_path = getenv("EKG_PLUGINS_PATH"))) {
                lib = saprintf("%s/%s.la", env_ekg_plugins_path, name);
                plugin = lt_dlopen(lib);
                if (!plugin) {
                        xfree(lib);
                        lib = saprintf("%s/%s/%s.la", env_ekg_plugins_path, name, name);
                        plugin = lt_dlopen(lib);
                }
        }
	
	if (!plugin) {
		xfree(lib);
		lib = saprintf("%s/%s.so", PLUGINDIR, name);
		plugin = lt_dlopen(lib);
	}

	if (!plugin) {
		xfree(lib);
		lib = saprintf("plugins/%s/%s.la", name, name);
		plugin = lt_dlopen(lib);
	}

	if (!plugin) {
		xfree(lib);
		lib = saprintf("../plugins/%s/%s.la", name, name);
		plugin = lt_dlopen(lib);
	}

	if (!plugin) {
		printq("plugin_doesnt_exist", name);
		xfree(lib);	
		return -1;
	}

	xfree(lib);

	init = saprintf("%s_plugin_init", name);

	if (!(plugin_init = lt_dlsym(plugin, init))) {
		printq("plugin_incorrect", name);
		lt_dlclose(plugin);
		xfree(init);
		return -1;
	}
		
	xfree(init);
	
	if (plugin_init() == -1) {
		printq("plugin_not_initialized", name);
		lt_dlclose(plugin);
		return -1;
	}

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (!xstrcasecmp(p->name, name)) {
			p->dl = plugin;
			break;
		}
	}

	printq("plugin_loaded", name);

	if (!in_autoexec)
		config_changed = 1;

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

		if (!p || !p->name || xstrcmp(p->name, name))
			continue;

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
                if (!p || !p->name || !valid_plugin_uid(p, uid))
                        continue;

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
	}

	if (p->pclass == PLUGIN_UI) {
		print("plugin_unload_ui", p->name);
		return -1;
	}

	name = xstrdup(p->name);

	if (p->destroy)
		p->destroy();

	if (p->dl) {
		lt_dlclose(p->dl);
	}

	print("plugin_unloaded", name);

        if (!in_autoexec)
                config_changed = 1;

	xfree(name);
	return 0;
}

/*
 * plugin_register()
 *
 * rejestruje dan± wtyczkê.
 *
 * 0/-1
 */
int plugin_register(plugin_t *p)
{
	list_add(&plugins, p, 0);

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
	list_t l;

	if (!p)
		return -1;

	for (l = queries; l; ) {
		query_t *q = l->data;

		l = l->next;

		if (q->plugin == p)
			query_disconnect(q->plugin, q->name);
	}


	for (l = variables; l; ) {
		variable_t *v = l->data;

		l = l->next;

		if (!v)
			continue;

		if (v->plugin == p) 
			variable_remove(v->plugin, v->name);
	}

	for (l = commands; l; ) {
		command_t *c = l->data;

		l = l->next;

		if (c->plugin == p)
			command_remove(c->plugin, c->name);
	}

        for (l = watches; l; ) {
                watch_t *w = l->data;

                l = l->next;

                if (w->plugin == p)
                        watch_free(w);
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

int query_connect(plugin_t *plugin, const char *name, void *handler, void *data)
{
	query_t q;

	memset(&q, 0, sizeof(q));
	q.plugin = plugin;
	q.name = xstrdup(name);
	q.handler = handler;
	q.data = data;
	
	return (list_add(&queries, &q, sizeof(q)) != NULL);
}

int query_disconnect(plugin_t *plugin, const char *name)
{
	list_t l;

	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if (q->plugin == plugin && q->name == name) {
			xfree(q->name);
			list_remove(&queries, q, 1);
			return 0;
		}
	}

	return -1;
}

int query_emit(plugin_t *plugin, const char *name, ...)
{
	static int nested = 0;
	int result = -1;
	va_list ap;
	list_t l;

	if (nested > 32) {
//		if (nested == 33)
//			debug("too many nested queries. exiting to avoid deadlock\n");
		return -1;
	}

	nested++;

	va_start(ap, name);

	for (l = queries; l; l = l->next) {
		query_t *q = l->data;

		if (!q->name)
			continue;

		if (!xstrcmp(q->name, name) && (!plugin || (plugin == q->plugin))) {
			int (*handler)(void *data, va_list ap) = q->handler;

			q->count++;
			
			result = 0;

			if (handler(q->data, ap) == -1) {
				result = -1;
				goto cleanup;
			}
		}
	}

cleanup:
	va_end(ap);

	nested--;

	return result;
}

query_t *query_find(const char *name)
{
        list_t l;

        for (l = queries; l; l = l->next) {
                query_t *q = l->data;

                if (xstrcasecmp(q->name, name))
                        continue;
                else
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

	memset(w, 0, sizeof(watch_t));
	w->plugin = plugin;
	w->fd = fd;
	w->type = type;

	if (w->type == WATCH_READ_LINE) {
		w->type = WATCH_READ;
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
		if (w->plugin == plugin && w->fd == fd && w->type == type && !w->removed)
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
	void (*handler)(int, int, int, void *);

	if (!w)
		return;

	w->removed = 1;
		
	if (w->buf)
		string_free(w->buf, 1);

	handler = w->handler;
	handler(1, w->fd, w->type, w->data);

	list_remove(&watches, w, 1);
}

/*
 * watch_handle_line()
 *
 * obs³uga deskryptorów przegl±danych WATCH_READ_LINE.
 */
void watch_handle_line(watch_t *w)
{
	char buf[1024], *tmp;
	int ret;
	void (*handler)(int, int, const char *, void *) = w->handler;

	if (!w->persist)
		list_remove(&watches, w, 0);

	ret = read(w->fd, buf, sizeof(buf) - 1);

	if (ret > 0) {
		buf[ret] = 0;
		string_append(w->buf, buf);
	}

	if (ret == 0 || (ret == -1 && errno != EAGAIN))
		string_append_c(w->buf, '\n');

	while ((tmp = xstrchr(w->buf->str, '\n'))) {
		int index = tmp - w->buf->str;
		char *line = xstrmid(w->buf->str, 0, index);
		string_t new;
			
		if (xstrlen(line) > 1 && line[xstrlen(line) - 1] == '\r')
			line[xstrlen(line) - 1] = 0;

		handler(0, w->fd, line, w->data);
					
		new = string_init(w->buf->str + index + 1);
		string_free(w->buf, 1);
		w->buf = new;
		xfree(line);
	}

	/* je¶li koniec strumienia, lub nie jest to ci±g³e przegl±danie,
	 * zwolnij pamiêæ i usuñ z listy */
	if (!w->persist || ret == 0 || (ret == -1 && errno != EAGAIN)) {
		handler(1, w->fd, NULL, w->data);
		string_free(w->buf, 1);
		close(w->fd);

		if (w->persist)
			list_remove(&watches, w, 1);
	}
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
	void (*handler)(int, int, int, void *) = w->handler;

	if (!w->persist) {
		list_remove(&watches, w, 0);
		handler(0, w->fd, w->type, w->data);
		handler(1, w->fd, w->type, w->data);
		xfree(w);
	} else {
		handler(0, w->fd, w->type, w->data);
		w->started = time(NULL);
	}
}

watch_t *watch_add(plugin_t *plugin, int fd, watch_type_t type, int persist, void *handler, void *data)
{
	watch_t *w = watch_new(plugin, fd, type);

	watch_persist_set(w, persist);
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

PROPERTY_INT(watch, persist, int);
PROPERTY_INT(watch, timeout, time_t);
PROPERTY_DATA(watch);
PROPERTY_MISC(watch, handler, watch_handler_func_t, NULL);



/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
