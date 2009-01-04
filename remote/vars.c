/* $Id: vars.c 4506 2008-08-26 16:48:57Z peres $ */

/*
 *  (C) Copyright 2001-2004 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Leszek Krupiñski <leafnode@wafel.com>
 *			    Adam Mikuta <adammikuta@poczta.onet.pl>
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

#include "ekg2-remote-config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include "debug.h"
#include "stuff.h"
#include "themes.h"
#include "vars.h"
#include "xmalloc.h"
#include "plugins.h"

#include "dynstuff_inline.h"
#include "queries.h"

static int variable_set(variable_t *v, const char *value);
static LIST_ADD_COMPARE(variable_add_compare, variable_t *) { return xstrcasecmp(data1->name, data2->name); }
static __DYNSTUFF_LIST_ADD_SORTED(variables, variable_t, variable_add_compare);	/* variables_add() */

variable_t *variables = NULL;

static int ekg_hash(const char *name) {
	int hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash <<= 1;
	}

	return hash;
}

EXPORTNOT void variable_init() {
	variable_add(NULL, ("completion_char"), VAR_STR, 1, &config_completion_char, NULL, NULL, NULL);
#if 0
		/* It's very, very special variable; shouldn't be used by user */
		/* XXX, warn here. user should change only console_charset if it's really nesessary... we should make user know about his terminal
		 *	encoding... and give some tip how to correct this... it's just temporary
		 */
#endif
	variable_add(NULL, ("console_charset"), VAR_STR, 1, &server_console_charset, NULL, NULL, NULL);
	variable_add(NULL, ("debug"), VAR_BOOL, 1, &config_debug, NULL, NULL, NULL);
	variable_add(NULL, ("default_status_window"), VAR_BOOL, 1, &config_default_status_window, NULL, NULL, NULL);
	variable_add(NULL, ("display_color"), VAR_INT, 1, &config_display_color, NULL, NULL, NULL);
	variable_add(NULL, ("display_crap"),  VAR_BOOL, 1, &config_display_crap, NULL, NULL, NULL);
	variable_add(NULL, ("display_pl_chars"), VAR_BOOL, 1, &config_display_pl_chars, NULL, NULL, NULL);
	variable_add(NULL, ("display_welcome"), VAR_BOOL, 1, &config_display_welcome, NULL, NULL, NULL);
	variable_add(NULL, ("history_savedups"),  VAR_BOOL, 1, &config_history_savedups, NULL, NULL, NULL);
	variable_add(NULL, ("lastlog_display_all"), VAR_INT, 1, &config_lastlog_display_all, NULL, variable_map(3, 
			0, 0, "current window",
			1, 2, "current window + configured",
			2, 1, "all windows + configured"), NULL);
	variable_add(NULL, ("lastlog_matchcase"), VAR_BOOL, 1, &config_lastlog_case, NULL, NULL, NULL);
	variable_add(NULL, ("lastlog_noitems"), VAR_BOOL, 1, &config_lastlog_noitems, NULL, NULL, NULL);
	variable_add(NULL, ("make_window"), VAR_MAP, 1, &config_make_window, NULL, variable_map(4, 0, 0, "none", 1, 2, "usefree", 2, 1, "always", 4, 0, "chatonly"), NULL);
	/* variable_add(NULL, ("mesg"), VAR_INT, 1, &config_mesg, changed_mesg, variable_map(3, 0, 0, "no", 1, 2, "yes", 2, 1, "default"), NULL); */
	variable_add(NULL, ("query_commands"), VAR_BOOL, 1, &config_query_commands, NULL, NULL, NULL);
	variable_add(NULL, ("save_quit"), VAR_INT, 1, &config_save_quit, NULL, NULL, NULL);
	variable_add(NULL, ("slash_messages"), VAR_INT, 1, &config_slash_messages, NULL, variable_map(3, 0, 0, "off", 1, 2, "moreslashes", 2, 1, "unknowncmd"), NULL);
	variable_add(NULL, ("sort_windows"), VAR_BOOL, 1, &config_sort_windows, NULL, NULL, NULL);
	variable_add(NULL, ("tab_command"), VAR_STR, 1, &config_tab_command, NULL, NULL, NULL);
	variable_add(NULL, ("timestamp"), VAR_STR, 1, &config_timestamp, NULL, NULL, NULL);
	variable_add(NULL, ("timestamp_show"), VAR_BOOL, 1, &config_timestamp_show, NULL, NULL, NULL);

	/* variable_set_default() */
	config_slash_messages = 1;
	config_timestamp = xstrdup("\\%H:\\%M:\\%S");

#if HAVE_LANGINFO_CODESET
	if (!config_console_charset)
		config_console_charset = xstrdup(nl_langinfo(CODESET));
#endif

	if (!config_console_charset) 
		config_console_charset = xstrdup("ISO-8859-2"); /* Default: ISO-8859-2 */
#if USE_UNICODE
	if (!config_use_unicode && xstrcasecmp(config_console_charset, "UTF-8")) {
		debug("nl_langinfo(CODESET) == %s swapping config_use_unicode to 0\n", config_console_charset);
		config_use_unicode = 0;
	} else	config_use_unicode = 1;
#else
	config_use_unicode = 0;
	if (!xstrcasecmp(config_console_charset, "UTF-8")) {
		debug("Warning, nl_langinfo(CODESET) reports that you are using utf-8 encoding, but you didn't compile ekg2 with (experimental/untested) --enable-unicode\n");
		debug("\tPlease compile ekg2 with --enable-unicode or change your enviroment setting to use not utf-8 but iso-8859-1 maybe? (LC_ALL/LC_CTYPE)\n");
	}
#endif
	config_use_iso = !xstrncasecmp(config_console_charset, "ISO-8859-", 9);
}

variable_t *variable_find(const char *name) {
	variable_t *v;
	int hash;

	if (!name)
		return NULL;

	hash = ekg_hash(name);

	for (v = variables; v; v = v->next) {
		if (v->name_hash == hash && !strcmp(v->name, name))
			return v;
	}

	return NULL;
}

variable_map_t *variable_map(int count, ...) {
	variable_map_t *res;
	va_list ap;
	int i;

	res = xcalloc(count + 1, sizeof(variable_map_t));
	
	va_start(ap, count);

	for (i = 0; i < count; i++) {
		res[i].value = va_arg(ap, int);
		res[i].conflicts = va_arg(ap, int);
		res[i].label = xstrdup(va_arg(ap, char*));
	}
	
	va_end(ap);

	return res;
}

variable_t *variable_add(plugin_t *plugin, const char *name, int type, int display, void *ptr, variable_notify_func_t *notify, variable_map_t *map, variable_display_func_t *dyndisplay) {
	variable_t *v;
	char *__name;

	if (!name)
		return NULL;

	if (plugin)
		__name = saprintf("%s:%s", plugin->name, name);
	else
		__name = xstrdup(name);

	v	= xmalloc(sizeof(variable_t));
	v->name		= __name;
	v->name_hash	= ekg_hash(__name);
	v->type		= type;
	v->display	= display;
	v->ptr		= ptr;
	v->notify	= notify;
	v->map		= map;
	v->dyndisplay	= dyndisplay;
	v->plugin	= plugin;

	variables_add(v);
	return v;
}

/* NOTE:
 * 	XXX, troche kodu w completion, polega na typie.
 * 	wartosc mozemy olac
 */
EXPORTNOT variable_t *remote_variable_add(const char *name, const char *value) {
	variable_t *v;

	if (!name)
		return NULL;

	if ((v = variable_find(name))) {
		if (v->type == VAR_REMOTE)		/* olewamy */
			return v;

		if (variable_set(v, value) != 0)
			debug_error("variable_set(%s, %s) failed!\n", name, value);

		return v;
	}

	v	= xmalloc(sizeof(variable_t));
	v->name		= xstrdup(name);
	v->name_hash	= ekg_hash(name);
	v->type		= VAR_REMOTE;
	v->display	= 1;
	/* v->ptr	= xstrdup(value); */			/* jak przestaniemy olewac value */

	variables_add(v);
	return v;
}

static int on_off(const char *value) {
	/* ekg2-remote: in comments, old code */
	if (value[0] == '1' && value[1] == '\0')	/* !xstrcasecmp(value, "on") || !xstrcasecmp(value, "true") || !xstrcasecmp(value, "yes") || !xstrcasecmp(value, "tak") || !strcmp(value, "1") */
		return 1;
	if (value[0] == '0' && value[1] == '\0')	/* !xstrcasecmp(value, "off") || !xstrcasecmp(value, "false") || !xstrcasecmp(value, "no") || !xstrcasecmp(value, "nie") || !strcmp(value, "0") */
		return 0;

	return -1;
}

static int variable_set(variable_t *v, const char *value) {
	switch (v->type) {
		case VAR_INT:
		case VAR_MAP:
		{
			char *p;
			int tmp;

			if (!value || !(*value))
				return -2;

			tmp = strtol(value, &p, 0);

			if (*p != '\0')
				return -2;

			if (v->map) {
				int i;

				for (i = 0; v->map[i].label; i++) {
					if ((tmp & v->map[i].value) && (tmp & v->map[i].conflicts))
						return -2;
				}
			}

			*(int*)(v->ptr) = tmp;

			break;
		}

		case VAR_BOOL:
		{
			int tmp;
		
			if (!value)
				return -2;
		
			if ((tmp = on_off(value)) == -1)
				return -2;

			*(int*)(v->ptr) = tmp;

			break;
		}
		case VAR_THEME:
		case VAR_FILE:
		case VAR_DIR:
		case VAR_STR:
		{
			char **tmp = (char**)(v->ptr);
			
			xfree(*tmp);
			*tmp = xstrdup(value);

			/* ekg2-remote note:
			 * 	here was code:
			 *
			 * 	if (value && *value == 1) 
			 * 		*tmp = base64_decode(value + 1);
			 *
			 * 	but it shouldn't happen
			 */
			break;
		}
	/*
		case VAR_REMOTE:
			xfree(v->ptr);
			v->ptr = xstrdup(value);
			break;
	 */

		default:
			return -1;
	}


	if (v->notify)
		(v->notify)(v->name);

	query_emit_id(NULL, VARIABLE_CHANGED, &(v->name));
	return 0;
}

static LIST_FREE_ITEM(variable_list_freeone, variable_t *) {
	xfree(data->name);

	switch (data->type) {
		case VAR_STR:
		case VAR_FILE:
		case VAR_THEME:
		case VAR_DIR:
			xfree(*((char**) data->ptr));
			*((char**) data->ptr) = NULL;
			break;

		case VAR_REMOTE:
			xfree((char *) data->ptr);
			break;

		default:
			break;
	}

	if (data->map) {
		int i;

		for (i = 0; data->map[i].label; i++)
			xfree(data->map[i].label);

		xfree(data->map);
	}
}

EXPORTNOT __DYNSTUFF_LIST_REMOVE_ITER(variables, variable_t, variable_list_freeone);	/* variables_removei() */
EXPORTNOT __DYNSTUFF_LIST_DESTROY(variables, variable_t, variable_list_freeone);	/* variables_destroy() */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
