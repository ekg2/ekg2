/* $Id$ */

/*
 *  (C) Copyright 2001-2004 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Leszek Krupiñski <leafnode@wafel.com>
 *                          Adam Mikuta <adammikuta@poczta.onet.pl>
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
#include "win32.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include "debug.h"
#include "dynstuff.h"
#include "stuff.h"
#include "themes.h"
#include "vars.h"
#include "xmalloc.h"
#include "plugins.h"

#include "queries.h"

static list_t *variables_lock = NULL;
list_t variables = NULL;
char *console_charset;

/*
 * dd_*()
 *
 * funkcje informuj±ce, czy dana grupa zmiennych ma zostaæ wy¶wietlona.
 * równie dobrze mo¿na by³o przekazaæ wska¼nik do zmiennej, która musi
 * byæ ró¿na od zera, ale dziêki funkcjom nie trzeba bêdzie mieszaæ w 
 * przysz³o¶ci.
 */
static int dd_sound(const char *name)
{
	return (config_sound_app != NULL);
}

static int dd_color(const char *name)
{
	return (config_display_color);
}

static int dd_beep(const char *name)
{
	return (config_beep);
}


/*
 * variable_init()
 *
 * inicjuje listê zmiennych.
 */
void variable_init()
{
	variables_lock = &variables; /* keep it sorted */

	variable_add(NULL, ("auto_save"), VAR_INT, 1, &config_auto_save, changed_auto_save, NULL, NULL);
	variable_add(NULL, ("auto_user_add"), VAR_BOOL, 1, &config_auto_user_add, NULL, NULL, NULL);
	variable_add(NULL, ("away_reason"), VAR_STR, 1, &config_away_reason, NULL, NULL, NULL);
	variable_add(NULL, ("back_reason"), VAR_STR, 1, &config_back_reason, NULL, NULL, NULL);
	variable_add(NULL, ("beep"), VAR_BOOL, 1, &config_beep, NULL, NULL, NULL);
	variable_add(NULL, ("beep_chat"), VAR_BOOL, 1, &config_beep_chat, NULL, NULL, dd_beep);
	variable_add(NULL, ("beep_msg"), VAR_BOOL, 1, &config_beep_msg, NULL, NULL, dd_beep);
	variable_add(NULL, ("beep_notify"), VAR_BOOL, 1, &config_beep_notify, NULL, NULL, dd_beep);
	variable_add(NULL, ("completion_char"), VAR_STR, 1, &config_completion_char, NULL, NULL, NULL);
	variable_add(NULL, ("completion_notify"), VAR_MAP, 1, &config_completion_notify, NULL, variable_map(4, 0, 0, "none", 1, 2, "add", 2, 1, "addremove", 4, 0, "away"), NULL);
		/* XXX, warn here. user should change only console_charset if it's really nesessary... we should make user know about his terminal
		 * 	encoding... and give some tip how to correct this... it's just temporary
		 */
	variable_add(NULL, ("console_charset"), VAR_STR, 1, &config_console_charset, NULL, NULL, NULL);
	variable_add(NULL, ("dcc_dir"), VAR_STR, 1, &config_dcc_dir, NULL, NULL, NULL); 
	variable_add(NULL, ("debug"), VAR_BOOL, 1, &config_debug, NULL, NULL, NULL);
/*	variable_add(NULL, ("default_protocol"), VAR_STR, 1, &config_default_protocol, NULL, NULL, NULL); */
	variable_add(NULL, ("default_status_window"), VAR_BOOL, 1, &config_default_status_window, NULL, NULL, NULL);
	variable_add(NULL, ("display_ack"), VAR_INT, 1, &config_display_ack, NULL, variable_map(4, 0, 0, "none", 1, 0, "all", 2, 0, "delivered", 3, 0, "queued"), NULL);
        variable_add(NULL, ("display_blinking"), VAR_BOOL, 1, &config_display_blinking, changed_display_blinking, NULL, NULL);
	variable_add(NULL, ("display_color"), VAR_INT, 1, &config_display_color, NULL, NULL, NULL);
	variable_add(NULL, ("display_color_map"), VAR_STR, 1, &config_display_color_map, NULL, NULL, dd_color);
	variable_add(NULL, ("display_crap"),  VAR_BOOL, 1, &config_display_crap, NULL, NULL, NULL);
	variable_add(NULL, ("display_day_changed"), VAR_BOOL, 1, &config_display_day_changed, NULL, NULL , NULL);
	variable_add(NULL, ("display_notify"), VAR_MAP, 1, &config_display_notify, NULL, variable_map(4, 0, 0, "none", 1, 2, "all", 2, 1, "significant", 4, 0, "unknown_too"), NULL);
	variable_add(NULL, ("display_pl_chars"), VAR_BOOL, 1, &config_display_pl_chars, NULL, NULL, NULL);
	variable_add(NULL, ("display_sent"), VAR_BOOL, 1, &config_display_sent, NULL, NULL, NULL);
	variable_add(NULL, ("display_welcome"), VAR_BOOL, 1, &config_display_welcome, NULL, NULL, NULL);
	variable_add(NULL, ("emoticons"), VAR_BOOL, 1, &config_emoticons, NULL, NULL, NULL);
	variable_add(NULL, ("events_delay"), VAR_INT, 1, &config_events_delay, NULL, NULL, NULL);
	variable_add(NULL, ("keep_reason"), VAR_INT, 1, &config_keep_reason, NULL, NULL, NULL);
	variable_add(NULL, ("last"), VAR_MAP, 1, &config_last, NULL, variable_map(4, 0, 0, "none", 1, 2, "all", 2, 1, "separate", 4, 0, "sent"), NULL);
	variable_add(NULL, ("last_size"), VAR_INT, 1, &config_last_size, NULL, NULL, NULL);
	variable_add(NULL, ("lastlog_display_all"), VAR_INT, 1, &config_lastlog_display_all, NULL, variable_map(3, 
			0, 0, "current window",
			1, 2, "current window + configured",
			2, 1, "all windows + configured"), NULL);
	variable_add(NULL, ("lastlog_matchcase"), VAR_BOOL, 1, &config_lastlog_case, NULL, NULL, NULL);
	variable_add(NULL, ("lastlog_noitems"), VAR_BOOL, 1, &config_lastlog_noitems, NULL, NULL, NULL);
	variable_add(NULL, ("make_window"), VAR_MAP, 1, &config_make_window, NULL, variable_map(4, 0, 0, "none", 1, 2, "usefree", 2, 1, "always", 4, 0, "chatonly"), NULL);
	variable_add(NULL, ("mesg"), VAR_INT, 1, &config_mesg, changed_mesg, variable_map(3, 0, 0, "no", 1, 2, "yes", 2, 1, "default"), NULL);
	variable_add(NULL, ("query_commands"), VAR_BOOL, 1, &config_query_commands, NULL, NULL, NULL);
	variable_add(NULL, ("quit_reason"), VAR_STR, 1, &config_quit_reason, NULL, NULL, NULL);
	variable_add(NULL, ("reason_limit"), VAR_BOOL, 1, &config_reason_limit, NULL, NULL, NULL);
	variable_add(NULL, ("save_password"), VAR_BOOL, 1, &config_save_password, NULL, NULL, NULL);
	variable_add(NULL, ("save_quit"), VAR_INT, 1, &config_save_quit, NULL, NULL, NULL);
	variable_add(NULL, ("session_default"), VAR_STR, 2, &config_session_default, NULL, NULL, NULL);
	variable_add(NULL, ("sessions_save"), VAR_BOOL, 1, &config_sessions_save, NULL, NULL, NULL);
	variable_add(NULL, ("slash_messages"), VAR_BOOL, 1, &config_slash_messages, NULL, NULL, NULL);
	variable_add(NULL, ("sort_windows"), VAR_BOOL, 1, &config_sort_windows, NULL, NULL, NULL);
	variable_add(NULL, ("sound_app"), VAR_STR, 1, &config_sound_app, NULL, NULL, NULL);
	variable_add(NULL, ("sound_chat_file"), VAR_FILE, 1, &config_sound_chat_file, NULL, NULL, dd_sound);
	variable_add(NULL, ("sound_mail_file"), VAR_FILE, 1, &config_sound_mail_file, NULL, NULL, dd_sound);
	variable_add(NULL, ("sound_msg_file"), VAR_FILE, 1, &config_sound_msg_file, NULL, NULL, dd_sound);
	variable_add(NULL, ("sound_notify_file"), VAR_FILE, 1, &config_sound_notify_file, NULL, NULL, dd_sound);
	variable_add(NULL, ("sound_sysmsg_file"), VAR_FILE, 1, &config_sound_sysmsg_file, NULL, NULL, dd_sound);
	variable_add(NULL, ("speech_app"), VAR_STR, 1, &config_speech_app, NULL, NULL, NULL);
	variable_add(NULL, ("subject_prefix"), VAR_STR, 1, &config_subject_prefix, NULL, NULL, NULL);
	variable_add(NULL, ("tab_command"), VAR_STR, 1, &config_tab_command, NULL, NULL, NULL);
	variable_add(NULL, ("theme"), VAR_THEME, 1, &config_theme, changed_theme, NULL, NULL);
	variable_add(NULL, ("time_deviation"), VAR_INT, 1, &config_time_deviation, NULL, NULL, NULL);
	variable_add(NULL, ("timestamp"), VAR_STR, 1, &config_timestamp, NULL, NULL, NULL);	/* ? */
	variable_add(NULL, ("timestamp_show"), VAR_BOOL, 1, &config_timestamp_show, NULL, NULL, NULL);
	variable_add(NULL, ("window_session_allow"), VAR_INT, 1, &config_window_session_allow, NULL, variable_map(4, 0, 0, "deny", 1, 6, "uid-capable", 2, 5, "any", 4, 3, "switch-to-status"), NULL);
	variable_add(NULL, ("windows_layout"), VAR_STR, 2, &config_windows_layout, NULL, NULL, NULL);
	variable_add(NULL, ("windows_save"), VAR_BOOL, 1, &config_windows_save, NULL, NULL, NULL);

	variables_lock = NULL;
}

/*
 * variable_set_default()
 *
 * ustawia pewne standardowe warto¶ci zmiennych
 * nieliczbowych.
 */
void variable_set_default()
{
	xfree(config_timestamp);
	xfree(config_display_color_map);
	xfree(config_subject_prefix);
	xfree(config_console_charset);
	xfree(config_dcc_dir);

	xfree(console_charset);

	config_slash_messages = 1;

	config_dcc_dir = NULL;

	config_timestamp = xstrdup("\\%H:\\%M:\\%S");
	config_display_color_map = xstrdup("nTgGbBrR");
	config_subject_prefix = xstrdup("## ");
#if HAVE_LANGINFO_CODESET
	console_charset = xstrdup(nl_langinfo(CODESET));
#endif

	if (console_charset) 
		config_console_charset = xstrdup(console_charset);
	else
		config_console_charset = xstrdup("ISO-8859-2"); /* Default: ISO-8859-2 */
#if USE_UNICODE
	if (!config_use_unicode && xstrcasecmp(console_charset, "UTF-8")) {
		debug("nl_langinfo(CODESET) == %s swapping config_use_unicode to 0\n", console_charset);
		config_use_unicode = 0;
	} else	config_use_unicode = 1;
#else
	config_use_unicode = 0;
	if (!xstrcasecmp(console_charset, "UTF-8")) {
		debug("Warning, nl_langinfo(CODESET) reports that you are using utf-8 encoding, but you didn't compile ekg2 with (experimental/untested) --enable-unicode\n");
		debug("\tPlease compile ekg2 with --enable-unicode or change your enviroment setting to use not utf-8 but iso-8859-1 maybe? (LC_ALL/LC_CTYPE)\n");
	}
#endif
}

/*
 * variable_find()
 *
 * znajduje strukturê variable_t opisuj±c± zmienn± o podanej nazwie.
 *
 * - name.
 */
variable_t *variable_find(const char *name)
{
	list_t l;
	int hash;

	if (!name)
		return NULL;

	hash = variable_hash(name);

	for (l = variables; l; l = l->next) {
		variable_t *v = l->data;
		if (v->name_hash == hash && !xstrcasecmp(v->name, name))
			return v;
	}

	return NULL;
}

/*
 * variable_map()
 *
 * tworzy now± mapê warto¶ci. je¶li która¶ z warto¶ci powinna wy³±czyæ inne
 * (na przyk³ad w ,,log'' 1 wy³±cza 2, 2 wy³±cza 1, ale nie maj± wp³ywu na 4)
 * nale¿y dodaæ do ,,konflikt''.
 *
 *  - count - ilo¶æ,
 *  - ... - warto¶æ, konflikt, opis.
 *
 * zaalokowana tablica.
 */
variable_map_t *variable_map(int count, ...)
{
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

/*
 * variable_add_compare()
 *
 * funkcja porównuj±ca nazwy zmiennych,, by wystêpowa³y alfabetycznie w li¶cie.
 *
 *  - data1, data2 - dwa wpisy zmiennychd do porównania.
 *
 * zwraca wynik xstrcasecmp() na nazwach zmiennych.
 */
static int variable_add_compare(void *data1, void *data2)
{
        variable_t *a = data1, *b = data2;

        if (!a || !a->name || !b || !b->name)
                return 0;

        return xstrcasecmp(a->name, b->name);
}

/*
 * variable_add()
 *
 * dodaje zmienn± do listy zmiennych.
 *
 *  - plugin - opis wtyczki, która obs³uguje zmienn±,
 *  - name - nazwa,
 *  - type - typ zmiennej,
 *  - display - czy i jak ma wy¶wietlaæ,
 *  - ptr - wska¼nik do zmiennej,
 *  - notify - funkcja powiadomienia,
 *  - map - mapa warto¶ci,
 *  - dyndisplay - funkcja sprawdzaj±ca czy wy¶wietliæ zmienn±.
 *
 * zwraca 0 je¶li siê nie uda³o, w przeciwnym wypadku adres do strutury.
 */
variable_t *variable_add(plugin_t *plugin, const char *name, int type, int display, void *ptr, variable_notify_func_t *notify, variable_map_t *map, variable_display_func_t *dyndisplay)
{
	variable_t *v;
	int hash;
	char *__name;
	list_t l;

	if (!name)
		return NULL;

	if (plugin)
		__name = saprintf("%s:%s", plugin->name, name);
	else
		__name = xstrdup(name);

	hash = variable_hash(__name);

	for (l = variables; l; l = l->next) {
		v = l->data;
		if (v->name_hash != hash || xstrcasecmp(v->name, __name) || v->type != VAR_FOREIGN)
			continue;

		if (type == VAR_INT || type == VAR_BOOL || type == VAR_MAP) {
			*(int*)(ptr) = atoi((char*)(v->ptr));
			xfree((char*)(v->ptr));
		} else 
			*(char**)(ptr) = (char*)(v->ptr);

		xfree(v->name);
		v->name		= __name;
		v->name_hash	= hash;
		v->type		= type;
		v->plugin	= plugin;
		v->display	= display;
		v->map		= map;
		v->notify	= notify;
		v->dyndisplay	= dyndisplay;
		v->ptr		= ptr;

		return v;
	}
	v 	= xmalloc(sizeof(variable_t));
	v->name		= __name;
	v->name_hash 	= hash;
	v->type 	= type;
	v->display 	= display;
	v->ptr 		= ptr;
	v->notify 	= notify;
	v->map 		= map;
	v->dyndisplay 	= dyndisplay;
	v->plugin 	= plugin;

/* like commands_lock in command_add() @ commands.c */
	if (variables_lock) {
		if (*variables_lock == variables) {
			for (; *variables_lock && (variable_add_compare((*variables_lock)->data, v) < 0); variables_lock = &((*variables_lock)->next));
		} else		variables_lock = &((*variables_lock)->next);
		list_add_beginning(variables_lock, v, 0);
		return v;
	}

	return list_add_sorted(&variables, v, 0, variable_add_compare);
}

/*
 * variable_remove()
 *
 * usuwa zmienn±.
 */
int variable_remove(plugin_t *plugin, const char *name)
{
	list_t l;
	int hash;

	if (!name)
		return -1;
	hash = ekg_hash(name);

	for (l = variables; l; l = l->next) {
		variable_t *v = l->data;
		
		if (!v->name)
			continue;
		
		if (hash == v->name_hash && plugin == v->plugin && !xstrcasecmp(name, v->name)) {
			char *tmp;

			if (v->type == VAR_INT || v->type == VAR_BOOL || v->type == VAR_MAP) {
				tmp = saprintf("%d", *(int*)(v->ptr));
				v->ptr = (void*)tmp;
			} else {
				char **pointer = (char **) (v->ptr);
				tmp = xstrdup(*pointer);
				xfree(*pointer);
				v->ptr = tmp;
			}

			v->type = VAR_FOREIGN;

			return 0;
		}
	}

	return -1;
}

/*
 * variable_set()
 *
 * ustawia warto¶æ podanej zmiennej. je¶li to zmienna liczbowa lub boolowska,
 * zmienia ci±g na liczbê. w przypadku boolowskich, rozumie zwroty typu `on',
 * `off', `yes', `no' itp. je¶li dana zmienna jest bitmap±, akceptuje warto¶æ
 * w postaci listy flag oraz konstrukcje `+flaga' i `-flaga'.
 *
 *  - name - nazwa zmiennej,
 *  - value - nowa warto¶æ,
 *  - allow_foreign - czy ma pozwalaæ dopisywaæ obce zmienne.
 */
int variable_set(const char *name, const char *value, int allow_foreign)
{
	variable_t *v = variable_find(name);
	char *tmpname;

	if (!v) {
		if (allow_foreign) {
			variable_add(NULL, name, VAR_FOREIGN, 2, xstrdup(value), NULL, NULL, NULL);
		}
		return -1;
	}
	switch (v->type) {
		case VAR_INT:
		case VAR_MAP:
		{
			const char *p = value;
			int hex, tmp;

			if (!value)
				return -2;

			if (v->map && v->type == VAR_INT && !xisdigit(*p)) {
				int i;

				for (i = 0; v->map[i].label; i++)
					if (!xstrcasecmp(v->map[i].label, value))
						value = itoa(v->map[i].value);
			}

			if (v->map && v->type == VAR_MAP && !xisdigit(*p)) {
				int i, k = *(int*)(v->ptr);
				int mode = 0; /* 0 set, 1 add, 2 remove */
				char **args;

				if (*p == '+') {
					mode = 1;
					p++;
				} else if (*p == '-') {
					mode = 2;
					p++;
				}

				if (!mode)
					k = 0;

				args = array_make(p, ",", 0, 1, 0);

				for (i = 0; args[i]; i++) {
					int j, found = 0;

					for (j = 0; v->map[j].label; j++) {
						if (!xstrcasecmp(args[i], v->map[j].label)) {
							found = 1;

							if (mode == 2)
								k &= ~(v->map[j].value);
							if (mode == 1)
								k &= ~(v->map[j].conflicts);
							if (mode == 1 || !mode)
								k |= v->map[j].value;
						}
					}

					if (!found) {
						array_free(args);
						return -2;
					}
				}

				array_free(args);

				value = itoa(k);
			}

			p = value;
				
			if ((hex = !xstrncasecmp(p, "0x", 2)))
				p += 2;

			while (*p && *p != ' ') {
				if (hex && !xisxdigit(*p))
					return -2;
				
				if (!hex && !xisdigit(*p))
					return -2;
				p++;
			}

			tmp = strtol(value, NULL, 0);

			if (v->map) {
				int i;

				for (i = 0; v->map[i].label; i++) {
					if ((tmp & v->map[i].value) && (tmp & v->map[i].conflicts))
						return -2;
				}
			}

			*(int*)(v->ptr) = tmp;

			goto notify;
		}

		case VAR_BOOL:
		{
			int tmp;
		
			if (!value)
				return -2;
		
			if ((tmp = on_off(value)) == -1)
				return -2;

			*(int*)(v->ptr) = tmp;

			goto notify;
		}
		case VAR_THEME:
		case VAR_FILE:
		case VAR_DIR:
		case VAR_STR:
		{
			char **tmp = (char**)(v->ptr);
			
			xfree(*tmp);
			
			if (value) {
				if (*value == 1)
					*tmp = base64_decode(value + 1);
				else
					*tmp = xstrdup(value);
			} else
				*tmp = NULL;
	
			goto notify;
		}
	}

	return -1;

notify:
	if (v->notify)
		(v->notify)(v->name);

	tmpname = xstrdup(v->name);
	query_emit_id(NULL, VARIABLE_CHANGED, &tmpname);
	xfree(tmpname);
			
	return 0;
}

/*
 * variable_free()
 *
 * zwalnia pamiêæ u¿ywan± przez zmienne.
 */
void variable_free()
{
	list_t l;

	for (l = variables; l; l = l->next) {
		variable_t *v = l->data;

		xfree(v->name);

		switch (v->type) {
			case VAR_STR:
			case VAR_FILE:
			case VAR_THEME:
			case VAR_DIR:
			{
	                        xfree(*((char**) v->ptr));
	                        *((char**) v->ptr) = NULL;
				break;
			}
			case VAR_FOREIGN:
			{
				xfree((char*) v->ptr);
				break;
			}
			default:
				break;
		}

		if (v->map) {
			int i;

			for (i = 0; v->map[i].label; i++)
				xfree(v->map[i].label);

			xfree(v->map);
		}
	}

	list_destroy(variables, 1);
	variables = NULL;
}

/*
 * variable_help()
 *
 * it shows help about variable from file ${datadir}/ekg/vars.txt
 * or ${datadir}/ekg/plugins/{plugin_name}/vars.txt
 *
 * name - name of the variable
 */
void variable_help(const char *name)
{
	FILE *f; 
	char *line, *type = NULL, *def = NULL, *tmp;
	const char *seeking_name;
	string_t s;
	int found = 0;
	variable_t *v = variable_find(name);

	if (!v) {
		print("variable_not_found", name);
		return;
	}

	if (v->plugin && v->plugin->name) {
		char *tmp2;

		if (!(f = help_path("vars", v->plugin->name))) {
			print("help_set_file_not_found_plugin", v->plugin->name);
			return;
		}

		tmp2 = xstrchr(name, ':');
		if (tmp2)
			seeking_name = tmp2+1;
		else
			seeking_name = name;
	} else {
		if (!(f = help_path("vars", NULL))) {
			print("help_set_file_not_found");
			return;
		}
		
		seeking_name = name;
	}

	while ((line = read_file(f, 0))) {
		if (!xstrcasecmp(line, seeking_name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		fclose(f);
		print("help_set_var_not_found", name);
		return;
	}

	line = read_file(f, 0);
	
	if ((tmp = xstrstr(line, (": "))))
		type = xstrdup(tmp + 2);
	else
		type = xstrdup(("?"));
	
	line = read_file(f, 0);
	if ((tmp = xstrstr(line, (": "))))
		def = xstrdup(tmp + 2);
	else
		def = xstrdup(("?"));

	print("help_set_header", name, type, def);

	xfree(type);
	xfree(def);

	if (tmp)		/* je¶li nie jest to ukryta zmienna... */
		read_file(f, 0);	/* ... pomijamy liniê */
	s = string_init(NULL);
	while ((line = read_file(f, 0))) {
		if (line[0] != '\t')
			break;

		if (!xstrncmp(line, ("\t- "), 3) && xstrcmp(s->str, (""))) {
			print("help_set_body", s->str);
			string_clear(s);
		}

                if (!xstrncmp(line, ("\t"), 1) && xstrlen(line) == 1) {
	                string_append(s, ("\n\r"));
                        continue;
                }
	
		string_append(s, line + 1);

		if (line[xstrlen(line) - 1] != ' ')
			string_append_c(s, ' ');
	}

	if (xstrcmp(s->str, ("")))
		print("help_set_body", s->str);

	string_free(s, 1);
	
	if (xstrcmp(format_find("help_set_footer"), ""))
		print("help_set_footer", name);

	fclose(f);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
