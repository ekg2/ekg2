/* $Id$ */

/*
 *  (C) Copyright 2001-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Pawe³ Maziarz <drg@o2.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
 *			    Piotr Kupisiewicz <deletek@ekg2.org>
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
#include <glib/gstdio.h>

/* for getpid() */
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "internal.h"

static char *strip_quotes(char *line) {
	size_t linelen;
	char *buf;

	if (!(linelen = xstrlen(line))) return line;

	for (buf = line; *buf == '\"'; buf++);

	while (linelen > 0 && line[linelen - 1] == '\"') {
		line[linelen - 1] = 0;
		linelen--;
	}

	return buf;
}

/* 
 * config_postread()
 *
 * initialized after config is read 
 */
void config_postread()
{
	if (config_windows_save && config_windows_layout) {
		char **targets = array_make(config_windows_layout, "|", 0, 0, 0);
		int i;

		for (i = 1; targets[i]; i++) {
			char *tmp;

			if (!xstrcmp(targets[i], "\"-\""))
				continue;

			if (xstrcmp(targets[i], "") && (tmp = xstrrchr(targets[i], '/'))) {
				char *session_name = xstrndup(targets[i], xstrlen(targets[i]) - xstrlen(tmp));
				session_t *s;

				if (!(s = session_find(session_name))) {
					xfree(session_name);
					continue;
				}

				tmp++;
				tmp = strip_spaces(tmp);
				tmp = strip_quotes(tmp);

				window_new(tmp, s, i + 1);	
	
				xfree(session_name);
			} else {
				window_new(NULL, NULL, i + 1);
			}
		}

		g_strfreev(targets);
	}

	if (config_session_default) {
		session_t *s = session_find(config_session_default);

		if (s) {
			debug("setted default session to %s\n", s->uid);
			window_session_set(window_status, s);
		} else {
			debug_warn("default session not found\n");
		}
	}
	config_upgrade();
	query_emit(NULL, "config-postinit");
}

gboolean ekg_fprintf(GIOChannel *f, const gchar *format, ...) {
	static GString *buf = NULL;
	va_list args;
	gsize out;
	GError *err = NULL;

	if (!buf)
		buf = g_string_sized_new(120);

	va_start(args, format);
	g_string_vprintf(buf, format, args);
	va_end(args);

	if (g_io_channel_write_chars(f, buf->str, buf->len, &out, &err) != G_IO_STATUS_NORMAL) {
		debug_error("ekg_fprintf() failed (wrote %d out of %d): %s\n",
				out, buf->len, err->message);
		g_error_free(err);
		return FALSE;
	}

	return TRUE;
}

static GIOChannel *config_open_real(const gchar *path, const gchar *mode) {
	GIOChannel *f;
	GError *err = NULL;
	const gchar modeline_prefix[] = "# vim:fenc=";
	const gchar *wanted_enc = console_charset;

	f = g_io_channel_new_file(path, mode, &err);
	if (!f) {
		if (err->code != G_FILE_ERROR_NOENT)
			debug_error("config_open(%s, %s) failed: %s\n", path, mode, err->message);
		g_error_free(err);
		return NULL;
	}

	if (mode[0] == 'r') {
		const gchar *buf;

		/* glib is a long runner
		 * if file is not utf8-encoded, we can end up with ILSEQ
		 * even if invalid seq is not in the first line */
		if (g_io_channel_set_encoding(f, NULL, &err) != G_IO_STATUS_NORMAL) {
			debug_error("config_open(%s, %s) failed to unset encoding: %s\n", path, mode, err->message);
			g_error_free(err);
			err = NULL;
		}

		buf = read_line(f);
		if (!buf) {
			/* Some error occured, or EOF
			 * in either case, there's no need to read that file anyway */
			g_io_channel_unref(f);
			return NULL;
		}

		/* XXX: support more modeline formats? */
		if (g_str_has_prefix(buf, modeline_prefix))
			wanted_enc = &buf[sizeof(modeline_prefix) - 1]; /* 1 for null terminator */

		if (g_io_channel_seek_position(f, 0, G_SEEK_SET, &err) != G_IO_STATUS_NORMAL) {
			if (err)
				debug_error("config_open(): rewind failed: %s\n", err->message);
			/* ok, screwed it */
			g_error_free(err);
			g_io_channel_unref(f);

			err = NULL;
			/* let's try reopening */
			f = g_io_channel_new_file(path, mode, &err);
			if (!f) {
				debug_error("config_open(): reopen failed %s\n", err->message);
				g_error_free(err);
				return NULL;
			}
		}
	}

	/* fallback to locale-encoded config */
	if (g_io_channel_set_encoding(f, wanted_enc, &err) != G_IO_STATUS_NORMAL) {
		debug_error("config_open(%s, %s) failed to set encoding: %s\n", path, mode, err->message);
		g_error_free(err);
		/* well, try the default one (utf8) anyway... */
		wanted_enc = g_io_channel_get_encoding(f);
		if (!wanted_enc) /* raw means what we use in core */
			wanted_enc = "UTF-8";
	}

	if (mode[0] == 'w') {
		g_chmod(path, 0600);
		if (!ekg_fprintf(f, "%s%s\n", modeline_prefix, wanted_enc)) {
			g_io_channel_unref(f);
			return NULL;
		}
	}

	return f;
}

static gchar *writing_config_file = NULL;

/**
 * config_open()
 *
 * Open a configuration file, formatting the name if necessary. Choose
 * correct location and file encoding.
 *
 * @param path_format - format string for file name. Should be
 *	utf8-encoded.
 * @param mode - string mode for opening the file (r or w).
 *
 * @return Open GIOChannel or NULL if open failed. The GIOChannel
 *	instance must be closed using config_close() (especially if open
 *	for writing).
 */
GIOChannel *config_open(const gchar *path_format, const gchar *mode, ...) {
	va_list args;
	gchar *path, *lpath;
	GIOChannel *f;

	va_start(args, mode);
	path = g_strdup_vprintf(path_format, args);
	va_end(args);

	lpath = g_strdup(prepare_path(path, (mode[0] == 'w')));
	g_free(path);

	if (mode[0] == 'w') {
		g_assert(!writing_config_file);
		writing_config_file = lpath;
		lpath = g_strdup_printf("%s.tmp", lpath);
	}

	debug_function("config_open(): lpath=%s\n", lpath);
	f = config_open_real(lpath, mode);
	g_free(lpath);
	return f;
}

gboolean config_close(GIOChannel *f) {
	const gboolean writeable = !!(g_io_channel_get_flags(f) & G_IO_FLAG_IS_WRITEABLE);
	gboolean ret = TRUE;

	if (writeable) {
		GError *err = NULL;

		/* XXX: currently, we're hoping this will fail if write failed */
		if (g_io_channel_flush(f, &err) != G_IO_STATUS_NORMAL) {
			debug_error("config_close(): flush failed: %s\n",
					err ? err->message : "(reason unknown)");
			g_error_free(err);
			ret = FALSE;
		}
	}
	g_io_channel_unref(f);

	if (writeable) {
		gchar *src;

#if 0 /* re-enable when got rid of old config_open() */
		g_assert(writing_config_file);
#else
		if (!writing_config_file)
			return TRUE;
#endif

		src = g_strdup_printf("%s.tmp", writing_config_file);
		if (ret) {
			/* XXX: use GFile */
			ret = !g_rename(src, writing_config_file);
			if (!ret)
				debug_error("config_close(), failed renaming %s -> %s, config not saved.",
						src, writing_config_file);
		} else /* flush/write failed */
			g_unlink(src);

		g_free(src);
		g_free(writing_config_file);
		writing_config_file = NULL;
	}

	return ret;
}

int config_read_plugins()
{
	gchar *buf, *foo;
	GIOChannel *f;

	f = config_open("plugins", "r");
	if (!f)
		return -1;

	while ((buf = read_line(f))) {
		if (!(foo = xstrchr(buf, (' '))))
			continue;

		*foo++ = 0;

		if (!xstrcasecmp(buf, ("plugin"))) {
			char **p = array_make(foo, (" \t"), 3, 1, 0);

			if (g_strv_length(p) == 2)
				plugin_load(p[0], atoi(p[1]), 1);

			g_strfreev(p);
		}
	}
	config_close(f);

	return 0;
}

/*
 * config_read()
 *
 * czyta z pliku ~/.ekg2/config lub podanego konfiguracjê.
 *
 *  - filename,
 *
 * 0/-1
 */
int config_read(const gchar *plugin_name)
{
	gchar *buf, *foo;
	GIOChannel *f;
	int err_count = 0, ret;

	if (!in_autoexec && !plugin_name) {
		aliases_destroy();
		timer_remove_user(timer_handle_command);
		timer_remove_user(timer_handle_at);
		event_free();
		variable_set_default();
		query_emit(NULL, "set-vars-default");
		query_emit(NULL, "binding-default");
		debug("  flushed previous config\n");
	} 

	/* then global and plugins variables */
	if (plugin_name)
		f = config_open("config-%s", "r", plugin_name);
	else
		f = config_open("config", "r");

	if (!f)
		return -1;

	while ((buf = read_line(f))) {
		ret = 0;

		if (buf[0] == '#' || buf[0] == ';' || (buf[0] == '/' && buf[1] == '/'))
			continue;

		if (!(foo = xstrchr(buf, ' ')))
			continue;

		*foo++ = 0;
		if (!xstrcasecmp(buf, ("set"))) {
			char *bar;

			if (!(bar = xstrchr(foo, ' ')))
				ret = variable_set(foo, NULL) < 0;
			else {
				*bar++ = 0;
				ret = variable_set(foo, bar) < 0;
			}

			if (ret)
				debug_error("  unknown variable %s\n", foo);

		} else if (!xstrcasecmp(buf, ("plugin"))) {
			char **p = array_make(foo, (" \t"), 3, 1, 0);
			if (g_strv_length(p) == 2) 
				plugin_load(p[0], atoi(p[1]), 1);
			g_strfreev(p);
		} else if (!xstrcasecmp(buf, ("bind"))) {
			char **pms = array_make(foo, (" \t"), 2, 1, 0);

			if (g_strv_length(pms) == 2) {
				ret = command_exec_format(NULL, NULL, 1, ("/bind --add %s %s"),  pms[0], pms[1]);
			}

			g_strfreev(pms);
		} else if (!xstrcasecmp(buf, ("bind-set"))) {
			char **pms = array_make(foo, (" \t"), 2, 1, 0);

			if (g_strv_length(pms) == 2) {
				query_emit(NULL, "binding-set", pms[0], pms[1], 1);
			}

			g_strfreev(pms);
		} else if (!xstrcasecmp(buf, ("alias"))) {
			debug("  alias %s\n", foo);
			ret = alias_add(foo, 1, 1);
		} else if (!xstrcasecmp(buf, ("on"))) {
			char **pms = array_make(foo, (" \t"), 4, 1, 0);

			if (g_strv_length(pms) == 4) {
				debug("  on %s %s %s\n", pms[0], pms[1], pms[2]);
				ret = event_add(pms[0], atoi(pms[1]), pms[2], pms[3], 1);
			}

			g_strfreev(pms);

		} else if (!xstrcasecmp(buf, ("bind"))) {
			continue;
		} else if (!xstrcasecmp(buf, ("at"))) {
			char **p = array_make(foo, (" \t"), 2, 1, 0);

			if (g_strv_length(p) == 2) {
				char *name = NULL;

				debug("  at %s %s\n", p[0], p[1]);

				if (xstrcmp(p[0], ("(null)")))
					name = p[0];

				ret = command_exec_format(NULL, NULL, 1, ("/at -a %s %s"), ((name) ? name : ("")), p[1]);
			}

			g_strfreev(p);
		} else if (!xstrcasecmp(buf, ("timer"))) {
			char **p = array_make(foo, (" \t"), 3, 1, 0);
			char *period_str = NULL;
			char *name = NULL;
			time_t period;

			if (g_strv_length(p) == 3) {
				debug("  timer %s %s %s\n", p[0], p[1], p[2]);

				if (xstrcmp(p[0], ("(null)")))
					name = p[0];

				if (!xstrncmp(p[1], ("*/"), 2)) {
					period = atoi(p[1] + 2);
					period_str = saprintf("*/%ld", (long) period);
				} else {
					period = atoi(p[1]) - time(NULL);
					period_str = saprintf("%ld", (long) period);
				}
		
				if (period > 0) {
					ret = command_exec_format(NULL, NULL, 1, 
						("/timer --add %s %s %s"), (name) ? name : "", period_str, p[2]);
				}

				xfree(period_str);
			}
			g_strfreev(p);
		} else {
			ret = variable_set(buf, (xstrcmp(foo, (""))) ? foo : NULL) < 0;

			if (ret)
				debug_error("  unknown variable %s\n", buf);
		}

		if (ret && (err_count++ > 100))
			break;
	}
	
	config_close(f);

	if (!plugin_name) {
		GSList *pl;

		for (pl = plugins; pl; pl = pl->next) {
			const plugin_t *p = pl->data;
			
			config_read(p->name);
		}
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
static void config_write_variable(GIOChannel *f, variable_t *v)
{
	if (!f || !v)
		return;

	switch (v->type) {
		case VAR_DIR:
		case VAR_THEME:
		case VAR_FILE:
		case VAR_STR:
			ekg_fprintf(f, "%s %s\n", v->name, (*(char**)(v->ptr)) ? *(char**)(v->ptr) : "");
			break;
		default:
			ekg_fprintf(f, "%s %d\n", v->name, *(int*)(v->ptr));
	}
}

/*
 * config_write_plugins()
 *
 * function saving plugins 
 *
 * - f - file, that we are saving to
 */
static void config_write_plugins(GIOChannel *f)
{
	GSList *pl;

	if (!f)
		return;

	for (pl = plugins; pl; pl = pl->next) {
		const plugin_t *p = pl->data;
		if (p->name) ekg_fprintf(f, "plugin %s %d\n", p->name, p->prio);
	}
}

/*
 * config_write_main()
 *
 * w³a¶ciwa funkcja zapisuj±ca konfiguracjê do podanego pliku.
 *
 *  - f - plik, do którego piszemy
 */
static void config_write_main(GIOChannel *f)
{
	if (!f)
		return;

	{
		GSList *vl;

		for (vl = variables; vl; vl = vl->next) {
			variable_t *v = vl->data;
			if (!v->plugin)
				config_write_variable(f, v);
		}
	}

	{
		alias_t *a;

		for (a = aliases; a; a = a->next) {
			list_t m;

			for (m = a->commands; m; m = m->next)
				ekg_fprintf(f, "alias %s %s\n", a->name, (char *) m->data);
		}
	}

	{
		event_t *e;

		for (e = events; e; e = e->next) {
			ekg_fprintf(f, "on %s %d %s %s\n", e->name, e->prio, e->target, e->action);
		}
	}

	{
		struct binding *b;

		for (b = bindings; b; b = b->next) {
			if (b->internal)
				continue;

			ekg_fprintf(f, "bind %s %s\n", b->key, b->action);
		}
	}

	{
		binding_added_t *d;

		for (d = bindings_added; d; d = d->next) {
			ekg_fprintf(f, "bind-set %s %s\n", d->binding->key, d->sequence);
		}
	}

	timers_write(f);
}

/*
 * config_write()
 *
 * zapisuje aktualn± konfiguracjê do pliku ~/.ekg2/config lub podanego.
 *
 * 0/-1
 */
int config_write()
{
	GIOChannel *f;
	GSList *pl;

	if (!prepare_path(NULL, 1))	/* try to create ~/.ekg2 dir */
		return -1;

	/* first of all we are saving plugins */
	if (!(f = config_open("plugins", "w")))
		return -1;
	
	config_write_plugins(f);
	config_close(f);

	/* now we are saving global variables and settings
	 * timers, bindings etc. */

	if (!(f = config_open("config", "w")))
		return -1;

	config_write_main(f);
	config_close(f);

	/* now plugins variables */
	for (pl = plugins; pl; pl = pl->next) {
		const plugin_t *p = pl->data;
		GSList *vl;

		if (!(f = config_open("config-%s", "w", p->name)))
			return -1;

		for (vl = variables; vl; vl = vl->next) {
			variable_t *v = vl->data;
			if (p == v->plugin) {
				config_write_variable(f, v);
			}
		}	

		config_close(f);
	}

	return 0;
}

/*
 * config_write_partly()
 *
 * zapisuje podane zmienne, nie zmieniaj±c reszty konfiguracji.
 *  
 *  - plugin - zmienne w vars, maja byc z tego pluginu, lub NULL gdy to sa zmienne z core.
 *  - vars - tablica z nazwami zmiennych do zapisania.
 * 
 * 0/-1
 */
/* BIG BUGNOTE:
 *	Ta funkcja jest zle zportowana z ekg1, zle napisana, wolna, etc..
 *	Powinnismy robic tak:
 *		- dla kazdej zmiennej w vars[] znalezc variable_t * jej odpowiadajace i do tablicy vars_ptr[]
 *		- dla kazdej zmiennej w vars[] policzyc dlugosc i do vars_len[]
 *	- nastepnie otworzyc "config-%s", vars_ptr[0]->plugin->name (lub "config" gdy nie plugin)
 *		- zrobic to co tutaj robimy, czyli poszukac tej zmiennej.. oraz nastepnie wszystkie inne ktore maja taki
 *			sam vars_ptr[]->plugin jak vars_ptr[0]->plugin, powtarzac dopoki sie skoncza takie.
 *	- nastepnie wziasc zmienna ktora ma inny plugin.. i j/w
 */
int config_write_partly(plugin_t *plugin, const char **vars)
{
	char *line;
	GIOChannel *fi, *fo;
	int *wrote, i;

	if (!vars)
		return -1;

	if (plugin)
		fi = config_open("config-%s", "r", plugin->name);
	else
		fi = config_open("config", "r");
	if (!fi)
		return -1;

	/* config_open() writes through temporary file,
	 * so it's sane to open the same name twice */
	if (plugin)
		fo = config_open("config-%s", "w", plugin->name);
	else
		fo = config_open("config", "w");

	if (!fo) {
		config_close(fi);
		return -1;
	}
	
	wrote = xcalloc(g_strv_length((char **) vars) + 1, sizeof(int));
	
	while ((line = read_line(fi))) {
		char *tmp;

		if (line[0] == '#' || line[0] == ';' || (line[0] == '/' && line[1] == '/'))
			goto pass;

		if (!xstrchr(line, ' '))
			goto pass;

		if (!xstrncasecmp(line, ("alias "), 6))
			goto pass;

		if (!xstrncasecmp(line, ("on "), 3))
			goto pass;

		if (!xstrncasecmp(line, ("bind "), 5))
			goto pass;

		tmp = line;

		if (!xstrncasecmp(tmp, ("set "), 4))
			tmp += 4;
		
		for (i = 0; vars[i]; i++) {
			int len;

			if (wrote[i])
				continue;
			
			len = xstrlen(vars[i]);

			if (xstrlen(tmp) < len + 1)
				continue;

			if (xstrncasecmp(tmp, vars[i], len) || tmp[len] != ' ')
				continue;
			
			config_write_variable(fo, variable_find(vars[i]));

			wrote[i] = 1;
			
			line = NULL;
			break;
		}

pass:
		if (line)
			ekg_fprintf(fo, "%s\n", line);
	}

	for (i = 0; vars[i]; i++) {
		if (wrote[i])
			continue;

		config_write_variable(fo, variable_find(vars[i]));
	}

	xfree(wrote);
	
	config_close(fi);
	config_close(fo);
	
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
	GIOChannel *f;
	GSList *pl;

	g_chdir(config_dir);

	/* first of all we are saving plugins */
	if (!(f = config_open("crash-%d-plugins", "w", (int) getpid())))
		return;

	config_write_plugins(f);

	config_close(f);

	/* then main part of config */
	if (!(f = config_open("crash-%d-plugin", "w", (int) getpid())))
		return;

	config_write_main(f);

	config_close(f);

	/* now plugins variables */
	for (pl = plugins; pl; pl = pl->next) {
		const plugin_t *p = pl->data;
		GSList *vl;

		if (!(f = config_open("crash-%d-config-%s", "w", (int) getpid(), p->name)))
			continue;	
	
		for (vl = variables; vl; vl = vl->next) {
			variable_t *v = vl->data;
			if (p == v->plugin) {
				config_write_variable(f, v);
			}
		}

		config_close(f);
	}
}

/*
 * debug_write_crash()
 *
 * zapisuje ostatnie linie z debug.
 */
void debug_write_crash()
{
	GIOChannel *f;
	struct buffer *b;

	g_chdir(config_dir);

	if (!(f = config_open("crash-%d-debug", "w", (int) getpid())))
		return;

	for (b = buffer_debug.data; b; b = b->next)
		ekg_fprintf(f, "%s\n", b->line);
	
	config_close(f);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
