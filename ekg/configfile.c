/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#include "ekg2-config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "commands.h"
#include "dynstuff.h"
#include "events.h"
#include "metacontacts.h"
#include "stuff.h"
#include "vars.h"
#include "xmalloc.h"
#include "plugins.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

/* 
 * config_read_later()
 * 
 * reads data after plugins initialization 
 * 
 * 0/-1
 */
int config_read_later(const char *filename)
{
        char *buf, *foo;
        FILE *f;
        int i = 0, good_file = 0, ret = 1;
        struct stat st;

        if (!filename && !(filename = prepare_path("config", 0)))
                return -1;

        if (!(f = fopen(filename, "r")))
                return -1;

        if (stat(filename, &st) || !S_ISREG(st.st_mode)) {
                if (S_ISDIR(st.st_mode))
                        errno = EISDIR;
                else
                        errno = EINVAL;
                fclose(f);
                return -1;
        }

	if (!in_autoexec) {
 	       query_emit(NULL, "binding-default");
	}

        while ((buf = read_file(f))) {
                i++;

                if (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' && buf[1] == '/')) {
                        xfree(buf);
                        continue;
                }

                if (!(foo = xstrchr(buf, ' '))) {
                        xfree(buf);
                        continue;
                }

                *foo++ = 0;


		if (!xstrcasecmp(buf, "bind")) {
                        char **pms = array_make(foo, " \t", 2, 1, 0);

                        if (array_count(pms) == 2) {
				char *tmp;

				tmp = saprintf("/bind --add %s %s",  pms[0], pms[1]);
	                        ret = command_exec(NULL, NULL, tmp, 1);
				
				xfree(tmp);
                        }

                        array_free(pms);
                }
                if (!ret)
                        good_file = 1;

                if (!good_file && i > 100) {
                        xfree(buf);
                        break;
                }

                xfree(buf);
        }

        fclose(f);
	return (good_file) ? 0 : -1;
}

/*
 * config_read()
 *
 * czyta z pliku ~/.gg/config lub podanego konfiguracjê.
 *
 *  - filename,
 *
 * 0/-1
 */
int config_read(const char *filename)
{
	char *buf, *foo;
	FILE *f;
	int i = 0, good_file = 0, ret = 1, home = ((filename) ? 0 : 1);
	struct stat st;

	if (!filename && !(filename = prepare_path("config", 0)))
		return -1;

	if (!(f = fopen(filename, "r")))
		return -1;

	if (stat(filename, &st) || !S_ISREG(st.st_mode)) {
		if (S_ISDIR(st.st_mode))
			errno = EISDIR;
		else
			errno = EINVAL;
		fclose(f);
		return -1;
	}

	if (!in_autoexec) {
		alias_free();
		timer_remove_user(-1);
		event_free();
		variable_set_default();
		query_emit(NULL, "set-vars-default");
		debug("  flushed previous config\n");
	} 

	while ((buf = read_file(f))) {
		i++;

		if (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' && buf[1] == '/')) {
			xfree(buf);
			continue;
		}

		if (!(foo = xstrchr(buf, ' '))) {
			xfree(buf);
			continue;
		}

		*foo++ = 0;

		if (!xstrcasecmp(buf, "set")) {
			char *bar;

			if (!(bar = xstrchr(foo, ' ')))
				ret = variable_set(foo, NULL, 1);
			else {
				*bar++ = 0;
				ret = variable_set(foo, bar, 1);
			}

			if (ret)
				debug("  unknown variable %s\n", foo);
		
		} else if (!xstrcasecmp(buf, "alias")) {
			debug("  alias %s\n", foo);
			ret = alias_add(foo, 1, 1);
		
		} else if (!xstrcasecmp(buf, "on")) {
                        char **pms = array_make(foo, " \t", 4, 1, 0);

                        if (array_count(pms) == 4) {
				debug("  on %s %s %s\n", pms[0], pms[1], pms[2]);
                                ret = event_add(pms[0], atoi(pms[1]), pms[2], pms[3], 1);
			}

			array_free(pms);

		} else if (!xstrcasecmp(buf, "bind")) {
			continue;
		} else if (!xstrcasecmp(buf, "at")) {
			char **p = array_make(foo, " \t", 2, 1, 0);

			if (array_count(p) == 2) {
				char *name = NULL, *tmp;

				debug("  at %s %s\n", p[0], p[1]);

				if (xstrcmp(p[0], "(null)"))
					name = p[0];

				tmp = saprintf("/at -a %s %s", ((name) ? name : ""), p[1]);
				ret = command_exec(NULL, NULL, tmp, 1);
				xfree(tmp);
			}

			array_free(p);
		} else if (!xstrcasecmp(buf, "timer")) {
			char **p = array_make(foo, " \t", 3, 1, 0);
			char *tmp = NULL, *period_str = NULL, *name = NULL;
			time_t period;

			if (array_count(p) == 3) {
				debug("  timer %s %s %s\n", p[0], p[1], p[2]);

				if (xstrcmp(p[0], "(null)"))
					name = p[0];

				if (!strncmp(p[1], "*/", 2)) {
					period = atoi(p[1] + 2);
					period_str = saprintf("*/%ld", (long) period);
				} else {
					period = atoi(p[1]) - time(NULL);
					period_str = saprintf("%ld", (long) period);
				}
		
				if (period > 0) {
					tmp = saprintf("/timer --add %s %s %s", (name) ? name : "", period_str, p[2]);
					ret = command_exec(NULL, NULL, tmp, 1);
					xfree(tmp);
				}

				xfree(period_str);
			}
				array_free(p);
		} else if (!xstrcasecmp(buf, "plugin")) {
			plugin_load(foo, 1);
                } else {
			ret = variable_set(buf, foo, 1);

			if (ret)
				debug("  unknown variable %s\n", buf);
		}

		if (!ret)
			good_file = 1;

		if (!good_file && i > 100) {
			xfree(buf);
			break;
		}

		xfree(buf);
	}
	
	fclose(f);

	if (!good_file && !home && !in_autoexec) {
		config_read(NULL);
		errno = EINVAL;
		return -2;
	}
	
	return 0;
}

/*
 * config_write_variable()
 *
 * zapisuje jedn± zmienn± do pliku konfiguracyjnego.
 *
 *  - f - otwarty plik konfiguracji,
 *  - v - wpis zmiennej,
 */
static void config_write_variable(FILE *f, variable_t *v)
{
	if (!f || !v)
		return;

	switch (v->type) {
		case VAR_DIR:
		case VAR_THEME:
		case VAR_FILE:
		case VAR_STR:
			if (*(char**)(v->ptr))
				fprintf(f, "%s %s\n", v->name, *(char**)(v->ptr));
			break;

		case VAR_FOREIGN:
			if (v->ptr)
				fprintf(f, "%s %s\n", v->name, (char*) v->ptr);
			break;
			
		default:
			fprintf(f, "%s %d\n", v->name, *(int*)(v->ptr));
	}
}

/*
 * config_write_main()
 *
 * w³a¶ciwa funkcja zapisuj±ca konfiguracjê do podanego pliku.
 *
 *  - f - plik, do którego piszemy
 */
static void config_write_main(FILE *f)
{
	list_t l;

	if (!f)
		return;

	for (l = variables; l; l = l->next)
		config_write_variable(f, l->data);

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;
		list_t m;

		for (m = a->commands; m; m = m->next)
			fprintf(f, "alias %s %s\n", a->name, (char*) m->data);
	}

        for (l = events; l; l = l->next) {
                event_t *e = l->data;

                fprintf(f, "on %s %d %s %s\n", e->name, e->prio, e->target, e->action);
        }

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		if (b->internal)
			continue;

		fprintf(f, "bind %s %s\n", b->key, b->action);
	}

	for (l = timers; l; l = l->next) {
		struct timer *t = l->data;
		const char *name = NULL;

		if (t->function != timer_handle_command)
			continue;

		/* nie ma sensu zapisywaæ */
		if (!t->persist && t->ends.tv_sec - time(NULL) < 5)
			continue;

		/* posortuje, je¶li nie ma nazwy */
		if (t->name && !xisdigit(t->name[0]))
			name = t->name;
		else
			name = "(null)";

		if (t->at) {
			char buf[100];
			time_t foo = (time_t) t->ends.tv_sec;
			struct tm *tt = localtime(&foo);

			strftime(buf, sizeof(buf), "%G%m%d%H%M.%S", tt);

			if (t->persist)
				fprintf(f, "at %s %s/%s %s\n", name, buf, itoa(t->period), (char*)(t->data));
			else
				fprintf(f, "at %s %s %s\n", name, buf, (char*)(t->data));
		} else {
			char *foo;

			if (t->persist)
				foo = saprintf("*/%s", itoa(t->period));
			else
				foo = saprintf("%s", itoa(t->ends.tv_sec));

			fprintf(f, "timer %s %s %s\n", name, foo, (char*)(t->data));

			xfree(foo);
		}
	}

        for (l = plugins; l; l = l->next) {
                plugin_t *p = l->data;
                if (p && p->name) fprintf(f, "plugin %s\n", p->name);
        }
}

/*
 * config_write()
 *
 * zapisuje aktualn± konfiguracjê do pliku ~/.gg/config lub podanego.
 *
 * 0/-1
 */
int config_write(const char *filename)
{
	FILE *f;

	if (!filename && !(filename = prepare_path("config", 1)))
		return -1;
	
	if (!(f = fopen(filename, "w")))
		return -1;
	
	fchmod(fileno(f), 0600);

	config_write_main(f);

	query_emit(NULL, "config-write", &f, NULL);

	fclose(f);
	
	return 0;
}

/*
 * config_write_partly()
 *
 * zapisuje podane zmienne, nie zmieniaj±c reszty konfiguracji.
 *  
 *  - vars - tablica z nazwami zmiennych do zapisania.
 * 
 * 0/-1
 */
int config_write_partly(char **vars)
{
	const char *filename;
	char *newfn, *line;
	FILE *fi, *fo;
	int *wrote, i;

	if (!vars)
		return -1;

	if (!(filename = prepare_path("config", 1)))
		return -1;
	
	if (!(fi = fopen(filename, "r")))
		return -1;

	newfn = saprintf("%s.%d.%ld", filename, (int) getpid(), (long) time(NULL));

	if (!(fo = fopen(newfn, "w"))) {
		xfree(newfn);
		fclose(fi);
		return -1;
	}
	
	wrote = xcalloc(array_count(vars) + 1, sizeof(int));
	
	fchmod(fileno(fo), 0600);

	while ((line = read_file(fi))) {
		char *tmp;

		if (line[0] == '#' || line[0] == ';' || (line[0] == '/' && line[1] == '/'))
			goto pass;

		if (!xstrchr(line, ' '))
			goto pass;

		if (!xstrncasecmp(line, "alias ", 6))
			goto pass;

		if (!xstrncasecmp(line, "on ", 3))
			goto pass;

		if (!xstrncasecmp(line, "bind ", 5))
			goto pass;

		tmp = line;

		if (!xstrncasecmp(tmp, "set ", 4))
			tmp += 4;
		
		for (i = 0; vars[i]; i++) {
			int len = xstrlen(vars[i]);

			if (xstrlen(tmp) < len + 1)
				continue;

			if (xstrncasecmp(tmp, vars[i], len) || tmp[len] != ' ')
				continue;
			
			config_write_variable(fo, variable_find(vars[i]));

			wrote[i] = 1;
			
			xfree(line);
			line = NULL;
			
			break;
		}

		if (!line)
			continue;

pass:
		fprintf(fo, "%s\n", line);
		xfree(line);
	}

	for (i = 0; vars[i]; i++) {
		if (wrote[i])
			continue;

		config_write_variable(fo, variable_find(vars[i]));
	}

	xfree(wrote);
	
	fclose(fi);
	fclose(fo);
	
	rename(newfn, filename);

	xfree(newfn);

	return 0;
}

/*
 * config_write_crash()
 *
 * funkcja zapisuj±ca awaryjnie konfiguracjê. nie powinna alokowaæ ¿adnej
 * pamiêci.
 */
void config_write_crash()
{
	char name[32];
	FILE *f;

	chdir(config_dir);

	snprintf(name, sizeof(name), "config.%d", (int) getpid());
	if (!(f = fopen(name, "w")))
		return;

	chmod(name, 0400);
	
	config_write_main(f);

	fflush(f);

	query_emit(NULL, "config-write", &f, NULL);
	
	fclose(f);
}

/*
 * debug_write_crash()
 *
 * zapisuje ostatnie linie z debug.
 */
void debug_write_crash()
{
	char name[32];
	FILE *f;
	list_t l;

	chdir(config_dir);

	snprintf(name, sizeof(name), "debug.%d", (int) getpid());
	if (!(f = fopen(name, "w")))
		return;

	chmod(name, 0400);
	
	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		if (b->type == BUFFER_DEBUG)
			fprintf(f, "%s\n", b->line);
	}
	
	fclose(f);
}
