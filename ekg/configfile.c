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

static GPtrArray *config_openfiles = NULL;
static GCancellable *config_cancellable = NULL;

/**
 * ekg_fprintf()
 *
 * Output formatted string to a GOutputStream.
 *
 * @param f - writable GOutputStream.
 * @param format - the format string.
 * 
 * @return TRUE on success, FALSE otherwise.
 *
 * @note The channel must be open for writing in blocking mode.
 */
gboolean ekg_fprintf(GOutputStream *f, const gchar *format, ...) {
	static GString *buf = NULL;
	va_list args;
	gsize out;
	GError *err = NULL;

	if (!buf)
		buf = g_string_sized_new(120);

	va_start(args, format);
	g_string_vprintf(buf, format, args);
	va_end(args);
	
	out = g_output_stream_write(f, buf->str, buf->len, NULL, &err);

	if (out < buf->len) {
		gpointer *p;

		debug_error("ekg_fprintf() failed (wrote %d out of %d): %s\n",
				out, buf->len, err ? err->message : "(no error?!)");
		g_error_free(err);

		if (config_openfiles) {
			for (p = &config_openfiles->pdata[0];
					p < &config_openfiles->pdata[config_openfiles->len];
					p++) {
				if (*p == f)
					g_cancellable_cancel(config_cancellable);
			}
		}

		return FALSE;
	}

	return TRUE;
}

static GObject *config_open_real(const gchar *path, const gchar *mode) {
	GFile *f;
	GObject *instream, *stream;
	GError *err = NULL;
	const gchar modeline_prefix[] = "# vim:fenc=";

	f = g_file_new_for_path(path);

	switch (mode[0]) {
		case 'r':
			instream = G_OBJECT(g_file_read(f, NULL, &err));
			break;
		case 'w':
			instream = G_OBJECT(g_file_replace(f, NULL, TRUE, G_FILE_CREATE_PRIVATE, NULL, &err));
			break;
		default:
			g_assert_not_reached();
	}

	if (!instream) {
		if (err->code != G_IO_ERROR_NOT_FOUND)
			debug_error("config_open(%s, %s) failed: %s\n", path, mode, err->message);
		g_error_free(err);
		g_object_unref(f);
		return NULL;
	}

	switch (mode[0]) {
		case 'r':
			stream = G_OBJECT(g_data_input_stream_new(G_INPUT_STREAM(instream)));

			{
				const gchar *wanted_enc, *buf;
				GCharsetConverter *conv;

				buf = read_line(G_DATA_INPUT_STREAM(stream));
				if (!buf) {
					/* Some error occured, or EOF
					 * in either case, there's no need to read that file anyway */
					g_object_unref(stream);
					g_object_unref(f);
					return NULL;
				}

				/* XXX: support more modeline formats? */
				if (g_str_has_prefix(buf, modeline_prefix))
					wanted_enc = &buf[sizeof(modeline_prefix) - 1]; /* 1 for null terminator */
				else
					wanted_enc = console_charset; /* fallback to locale */

				g_filter_input_stream_set_close_base_stream(G_FILTER_INPUT_STREAM(stream), FALSE);
				g_object_unref(stream);

				if (!g_seekable_can_seek(G_SEEKABLE(instream)) ||
						!g_seekable_seek(G_SEEKABLE(instream), 0, G_SEEK_SET, NULL, &err)) {

					/* ok, screwed it */
					if (err)
						debug_error("config_open(): rewind failed: %s\n", err->message);
					g_error_free(err);
					g_object_unref(instream);

					/* let's try reopening */
					err = NULL;
					instream = G_OBJECT(g_file_read(f, NULL, &err));
					if (!instream) {
						debug_error("config_open(): reopen failed %s\n", err->message);
						g_error_free(err);
						g_object_unref(f);
						return NULL;
					}
				}

				conv = g_charset_converter_new("UTF-8", wanted_enc, &err);
				if (!conv) {
					debug_error("config_open(): failed to setup recoding from %s: %s\n",
							wanted_enc, err ? err->message : "(unknown error)");
					g_error_free(err);
					stream = instream;
					/* fallback to utf8 */
				} else {
					g_charset_converter_set_use_fallback(conv, TRUE);
					stream = G_OBJECT(g_converter_input_stream_new(
								G_INPUT_STREAM(instream), G_CONVERTER(conv)));
				}
				stream = G_OBJECT(g_data_input_stream_new(G_INPUT_STREAM(stream)));
			}
			break;
		case 'w':
			stream = G_OBJECT(g_data_output_stream_new(G_OUTPUT_STREAM(instream)));
			
			/* we're always writing config in utf8 */
			if (!ekg_fprintf(G_OUTPUT_STREAM(stream), "%s%s\n", modeline_prefix, "UTF-8")) {
				g_object_unref(stream);
				g_object_unref(f);
				return NULL;
			}
			break;
		default:
			g_assert_not_reached();
	}
	g_object_unref(f);

	return stream;
}

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
 *	instance must be unrefed using g_object_unref().
 */
GObject *config_open(const gchar *path_format, const gchar *mode, ...) {
	va_list args;
	gchar *basename, *cdir, *path, *p;
	GString *fname;
	GObject *f;
	gboolean nonalnum = FALSE;

	va_start(args, mode);
	basename = g_strdup_vprintf(path_format, args);
	va_end(args);

	fname = g_string_new(basename);
	for (p = fname->str; *p; p++) { /* filter out the name */
		if (!g_ascii_isalnum(*p) && *p != '.' && *p != '_' && *p != '-') {
			nonalnum = TRUE;
			*p = '_';
		}
	}
	if (nonalnum) {
		gchar *cksum = g_compute_checksum_for_string(G_CHECKSUM_MD5, basename, -1);
		g_string_append_c(fname, '_');
		g_string_append_len(fname, cksum, 8);
		g_free(cksum);
	}

	cdir = g_build_filename(g_get_user_config_dir(), "ekg2",
			config_profile, NULL);
	path = g_build_filename(cdir, g_string_free(fname, FALSE), NULL);

	debug_function("config_open(), path=%s\n", path);
	f = config_open_real(path, mode);

	if (G_UNLIKELY(mode[0] == 'w' && !f)) {
		if (G_LIKELY(!g_mkdir_with_parents(cdir, 0700)))
			f = config_open_real(path, mode);
	}

	g_free(path);
	g_free(cdir);

	if (G_UNLIKELY(!f && mode[0] == 'r')) /* fallback to old config */
		f = config_open_real(prepare_path(basename, 0), mode);

	g_free(basename);

	if (mode[0] == 'w') {
		if (!config_cancellable) {
			config_cancellable = g_cancellable_new();
			g_assert(!config_openfiles);
			config_openfiles = g_ptr_array_new();
		}

		if (f)
			g_ptr_array_add(config_openfiles, f);
		else
			g_cancellable_cancel(config_cancellable);
	}

	return f;
}

/**
 * config_commit()
 *
 * Close all configuration files open for writing, and commit changes
 * to them if written successfully. Otherwise, just leave old files
 * intact.
 *
 * @return TRUE if new config was saved, FALSE otherwise.
 */
gboolean config_commit(void) {
	gpointer *p;
	gboolean ret = TRUE;

	g_assert(config_cancellable);

	for (p = &config_openfiles->pdata[0];
			p < &config_openfiles->pdata[config_openfiles->len];
			p++) {
		ret &= g_output_stream_close(G_OUTPUT_STREAM(*p), config_cancellable, NULL);
		g_object_unref(*p);
	}

	g_ptr_array_free(config_openfiles, FALSE);
	g_object_unref(config_cancellable);
	config_openfiles = NULL;
	config_cancellable = NULL;

	return ret;
}

int config_read_plugins()
{
	gchar *buf, *foo;
	GDataInputStream *f;

	f = G_DATA_INPUT_STREAM(config_open("plugins", "r"));
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
	g_object_unref(f);

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
	GDataInputStream *f;
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
		f = G_DATA_INPUT_STREAM(config_open("config-%s", "r", plugin_name));
	else
		f = G_DATA_INPUT_STREAM(config_open("config", "r"));

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
	
	g_object_unref(f);

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
static void config_write_variable(GOutputStream *f, variable_t *v)
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
static void config_write_plugins(GOutputStream *f)
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
static void config_write_main(GOutputStream *f)
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
 */
void config_write()
{
	GOutputStream *f;
	GSList *pl;

	/* first of all we are saving plugins */
	if (!(f = G_OUTPUT_STREAM(config_open("plugins", "w"))))
		return;
	
	config_write_plugins(f);

	/* now we are saving global variables and settings
	 * timers, bindings etc. */

	if (!(f = G_OUTPUT_STREAM(config_open("config", "w"))))
		return;

	config_write_main(f);

	/* now plugins variables */
	for (pl = plugins; pl; pl = pl->next) {
		const plugin_t *p = pl->data;
		GSList *vl;

		if (!(f = G_OUTPUT_STREAM(config_open("config-%s", "w", p->name))))
			return;

		for (vl = variables; vl; vl = vl->next) {
			variable_t *v = vl->data;
			if (p == v->plugin) {
				config_write_variable(f, v);
			}
		}	
	}
}

/*
 * config_write_partly()
 *
 * zapisuje podane zmienne, nie zmieniaj±c reszty konfiguracji.
 *  
 *  - plugin - zmienne w vars, maja byc z tego pluginu, lub NULL gdy to sa zmienne z core.
 *  - vars - tablica z nazwami zmiennych do zapisania.
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
	GDataInputStream *fi;
	GOutputStream *fo;
	int *wrote, i;

	if (!vars)
		return -1;

	if (plugin)
		fi = G_DATA_INPUT_STREAM(config_open("config-%s", "r", plugin->name));
	else
		fi = G_DATA_INPUT_STREAM(config_open("config", "r"));
	if (!fi)
		return -1;

	/* config_open() writes through temporary file,
	 * so it's sane to open the same name twice */
	if (plugin)
		fo = G_OUTPUT_STREAM(config_open("config-%s", "w", plugin->name));
	else
		fo = G_OUTPUT_STREAM(config_open("config", "w"));

	if (!fo) {
		g_object_unref(fi);
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
	
	g_object_unref(fi);
	
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
	GOutputStream *f;
	GSList *pl;

	g_chdir(config_dir);

	/* first of all we are saving plugins */
	if (!(f = G_OUTPUT_STREAM(config_open("crash-%d-plugins", "w", (int) getpid()))))
		return;

	config_write_plugins(f);

	/* then main part of config */
	if (!(f = G_OUTPUT_STREAM(config_open("crash-%d-plugin", "w", (int) getpid()))))
		return;

	config_write_main(f);

	/* now plugins variables */
	for (pl = plugins; pl; pl = pl->next) {
		const plugin_t *p = pl->data;
		GSList *vl;

		if (!(f = G_OUTPUT_STREAM(config_open("crash-%d-config-%s", "w", (int) getpid(), p->name))))
			continue;	
	
		for (vl = variables; vl; vl = vl->next) {
			variable_t *v = vl->data;
			if (p == v->plugin) {
				config_write_variable(f, v);
			}
		}
	}
}

/*
 * debug_write_crash()
 *
 * zapisuje ostatnie linie z debug.
 */
void debug_write_crash()
{
	GOutputStream *f;
	struct buffer *b;

	g_chdir(config_dir);

	if (!(f = G_OUTPUT_STREAM(config_open("crash-%d-debug", "w", (int) getpid()))))
		return;

	for (b = buffer_debug.data; b; b = b->next)
		ekg_fprintf(f, "%s\n", b->line);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
