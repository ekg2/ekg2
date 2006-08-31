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

#include "dynstuff.h"
#include "stuff.h"
#include "themes.h"
#include "vars.h"
#include "xmalloc.h"
#include "plugins.h"

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
static int dd_sound(const CHAR_T *name)
{
	return (config_sound_app != NULL);
}

static int dd_color(const CHAR_T *name)
{
	return (config_display_color);
}

static int dd_beep(const CHAR_T *name)
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
	variable_add(NULL, TEXT("auto_save"), VAR_INT, 1, &config_auto_save, changed_auto_save, NULL, NULL);
	variable_add(NULL, TEXT("away_reason"), VAR_STR, 1, &config_away_reason, NULL, NULL, NULL);
	variable_add(NULL, TEXT("auto_user_add"), VAR_BOOL, 1, &config_auto_user_add, NULL, NULL, NULL);
	variable_add(NULL, TEXT("back_reason"), VAR_STR, 1, &config_back_reason, NULL, NULL, NULL);
	variable_add(NULL, TEXT("beep"), VAR_BOOL, 1, &config_beep, NULL, NULL, NULL);
	variable_add(NULL, TEXT("beep_msg"), VAR_BOOL, 1, &config_beep_msg, NULL, NULL, dd_beep);
	variable_add(NULL, TEXT("beep_chat"), VAR_BOOL, 1, &config_beep_chat, NULL, NULL, dd_beep);
	variable_add(NULL, TEXT("beep_notify"), VAR_BOOL, 1, &config_beep_notify, NULL, NULL, dd_beep);
		/* XXX, warn here. user should change only console_charset if it's really nesessary... we should make user know about his terminal
		 * 	encoding... and give some tip how to correct this... it's just temporary
		 */
	variable_add(NULL, TEXT("console_charset"), VAR_STR, 1, &config_console_charset, NULL, NULL, NULL); 
	variable_add(NULL, TEXT("completion_char"), VAR_STR, 1, &config_completion_char, NULL, NULL, NULL);
	variable_add(NULL, TEXT("completion_notify"), VAR_MAP, 1, &config_completion_notify, NULL, variable_map(4, 0, 0, "none", 1, 2, "add", 2, 1, "addremove", 4, 0, "away"), NULL);
	variable_add(NULL, TEXT("debug"), VAR_BOOL, 1, &config_debug, NULL, NULL, NULL);
//	variable_add(NULL, TEXT("default_protocol"), VAR_STR, 1, &config_default_protocol, NULL, NULL, NULL);
	variable_add(NULL, TEXT("default_status_window"), VAR_BOOL, 1, &config_default_status_window, NULL, NULL, NULL);
	variable_add(NULL, TEXT("display_ack"), VAR_INT, 1, &config_display_ack, NULL, variable_map(4, 0, 0, "none", 1, 0, "all", 2, 0, "delivered", 3, 0, "queued"), NULL);
        variable_add(NULL, TEXT("display_blinking"), VAR_BOOL, 1, &config_display_blinking, changed_display_blinking, NULL, NULL);
	variable_add(NULL, TEXT("display_crap"),  VAR_BOOL, 1, &config_display_crap, NULL, NULL, NULL);
	variable_add(NULL, TEXT("display_color"), VAR_INT, 1, &config_display_color, NULL, NULL, NULL);
	variable_add(NULL, TEXT("display_color_map"), VAR_STR, 1, &config_display_color_map, NULL, NULL, dd_color);
	variable_add(NULL, TEXT("display_day_changed"), VAR_BOOL, 1, &config_display_day_changed, NULL, NULL , NULL);
	variable_add(NULL, TEXT("display_notify"), VAR_INT, 1, &config_display_notify, NULL, variable_map(3, 0, 0, "none", 1, 2, "all", 2, 1, "significant"), NULL);
	variable_add(NULL, TEXT("display_pl_chars"), VAR_BOOL, 1, &config_display_pl_chars, NULL, NULL, NULL);
	variable_add(NULL, TEXT("display_sent"), VAR_BOOL, 1, &config_display_sent, NULL, NULL, NULL);
	variable_add(NULL, TEXT("display_unknown"), VAR_BOOL, 1, &config_display_unknown, NULL, NULL, NULL);
	variable_add(NULL, TEXT("display_welcome"), VAR_BOOL, 1, &config_display_welcome, NULL, NULL, NULL);
	variable_add(NULL, TEXT("emoticons"), VAR_BOOL, 1, &config_emoticons, NULL, NULL, NULL);
	variable_add(NULL, TEXT("events_delay"), VAR_INT, 1, &config_events_delay, NULL, NULL, NULL);
	variable_add(NULL, TEXT("keep_reason"), VAR_INT, 1, &config_keep_reason, NULL, NULL, NULL);
	variable_add(NULL, TEXT("last"), VAR_MAP, 1, &config_last, NULL, variable_map(4, 0, 0, "none", 1, 2, "all", 2, 1, "separate", 4, 0, "sent"), NULL);
	variable_add(NULL, TEXT("last_size"), VAR_INT, 1, &config_last_size, NULL, NULL, NULL);
	variable_add(NULL, TEXT("make_window"), VAR_INT, 1, &config_make_window, changed_display_blinking, variable_map(3, 0, 0, "none", 1, 2, "usefree", 2, 1, "always"), NULL);
	variable_add(NULL, TEXT("mesg"), VAR_INT, 1, &config_mesg, changed_mesg, variable_map(3, 0, 0, "no", 1, 2, "yes", 2, 1, "default"), NULL);
	variable_add(NULL, TEXT("reason_limit"), VAR_BOOL, 1, &config_reason_limit, NULL, NULL, NULL);
	variable_add(NULL, TEXT("quit_reason"), VAR_STR, 1, &config_quit_reason, NULL, NULL, NULL);
	variable_add(NULL, TEXT("query_commands"), VAR_BOOL, 1, &config_query_commands, NULL, NULL, NULL);
	variable_add(NULL, TEXT("save_password"), VAR_BOOL, 1, &config_save_password, NULL, NULL, NULL);
	variable_add(NULL, TEXT("save_quit"), VAR_INT, 1, &config_save_quit, NULL, NULL, NULL);
	variable_add(NULL, TEXT("session_default"), VAR_STR, 1, &config_session_default, NULL, NULL, NULL);
	variable_add(NULL, TEXT("sessions_save"), VAR_BOOL, 1, &config_sessions_save, NULL, NULL, NULL);
	variable_add(NULL, TEXT("sort_windows"), VAR_BOOL, 1, &config_sort_windows, NULL, NULL, NULL);
	variable_add(NULL, TEXT("sound_msg_file"), VAR_FILE, 1, &config_sound_msg_file, NULL, NULL, dd_sound);
	variable_add(NULL, TEXT("sound_chat_file"), VAR_FILE, 1, &config_sound_chat_file, NULL, NULL, dd_sound);
	variable_add(NULL, TEXT("sound_sysmsg_file"), VAR_FILE, 1, &config_sound_sysmsg_file, NULL, NULL, dd_sound);
	variable_add(NULL, TEXT("sound_notify_file"), VAR_FILE, 1, &config_sound_notify_file, NULL, NULL, dd_sound);
	variable_add(NULL, TEXT("sound_mail_file"), VAR_FILE, 1, &config_sound_mail_file, NULL, NULL, dd_sound);
	variable_add(NULL, TEXT("sound_app"), VAR_STR, 1, &config_sound_app, NULL, NULL, NULL);
	variable_add(NULL, TEXT("speech_app"), VAR_STR, 1, &config_speech_app, NULL, NULL, NULL);
	variable_add(NULL, TEXT("subject_prefix"), VAR_STR, 1, &config_subject_prefix, NULL, NULL, NULL);
	variable_add(NULL, TEXT("tab_command"), VAR_STR, 1, &config_tab_command, NULL, NULL, NULL);
	variable_add(NULL, TEXT("theme"), VAR_THEME, 1, &config_theme, changed_theme, NULL, NULL);
	variable_add(NULL, TEXT("time_deviation"), VAR_INT, 1, &config_time_deviation, NULL, NULL, NULL);
	variable_add(NULL, TEXT("timestamp"), VAR_STR, 1, &config_timestamp, NULL, NULL, NULL);
	variable_add(NULL, TEXT("timestamp_show"), VAR_BOOL, 1, &config_timestamp_show, NULL, NULL, NULL);
#if USE_UNICODE
//	variable_add(NULL, TEXT("use_unicode"), VAR_BOOL, 1, &config_use_unicode, NULL, NULL, NULL);
#endif
	variable_add(NULL, TEXT("windows_save"), VAR_BOOL, 1, &config_windows_save, NULL, NULL, NULL);
	variable_add(NULL, TEXT("windows_layout"), VAR_STR, 2, &config_windows_layout, NULL, NULL, NULL);
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

	xfree(console_charset);

	config_timestamp = xstrdup("\\%H:\\%M:\\%S");
	config_display_color_map = xstrdup("nTgGbBrR");
	config_subject_prefix = xstrdup("## ");
#if HAVE_LANGINFO_CODESET
	console_charset = xstrdup(nl_langinfo(CODESET));
#endif

#if USE_UNICODE
	if (xstrcmp(console_charset, "UTF-8")) {
		/* XXX, warn here: ekg2 compilated with USE_UNICODE but not in unicode enviroment */
		xfree(console_charset);
		console_charset	= xstrdup("UTF-8");
	}
	config_use_unicode	= 1;
#else
	if (!xstrcmp(console_charset, "UTF-8")) {
		/* XXX, warn here. user should compile ekg2 with --enable-unicode */
		config_use_unicode = 1;
	}
#endif
	if (console_charset) 
		config_console_charset = xstrdup(console_charset);
	else
		config_console_charset = xstrdup("ISO-8859-2"); /* Default: ISO-8859-2 */
}

/*
 * variable_find()
 *
 * znajduje strukturê variable_t opisuj±c± zmienn± o podanej nazwie.
 *
 * - name.
 */
variable_t *variable_find(const CHAR_T *name)
{
	list_t l;
	int hash;

	if (!name)
		return NULL;

	hash = variable_hash(name);

	for (l = variables; l; l = l->next) {
		variable_t *v = l->data;
		if (v->name_hash == hash && !xwcscasecmp(v->name, name))
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

        return xwcscasecmp(a->name, b->name);
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
variable_t *variable_add(plugin_t *plugin, const CHAR_T *name, int type, int display, void *ptr, variable_notify_func_t *notify, variable_map_t *map, variable_display_func_t *dyndisplay)
{
	variable_t *v;
	int hash;
	CHAR_T *__name;
	list_t l;

	if (!name)
		return NULL;

	if (plugin)
#if USE_UNICODE
		__name = wcsprintf(TEXT("%ls:%ls"), plugin->name, name);
#else
		__name = wcsprintf("%s:%s", plugin->name, name);
#endif
	else
		__name = xwcsdup(name);

	hash = variable_hash(__name);

	for (l = variables; l; l = l->next) {
		v = l->data;
		if (v->name_hash != hash || xwcscasecmp(v->name, __name) || v->type != VAR_FOREIGN)
			continue;

		if (type == VAR_INT || type == VAR_BOOL || type == VAR_MAP) {
			*(int*)(ptr) = atoi((char*)(v->ptr));
			xfree((char*)(v->ptr));
		} else 
			*(char**)(ptr) = (char*)(v->ptr);
	
		xfree(v->name);
		v->name = xwcsdup(__name);
		v->name_hash = hash;
		v->type = type;
		v->plugin = plugin;
		v->display = display;
		v->map = map;
		v->notify = notify;
		v->dyndisplay = dyndisplay;
		v->ptr = ptr;
		
		xfree(__name);		
		return 0;
	}
	v = xmalloc(sizeof(variable_t));
	v->name = xwcsdup(__name);
	v->name_hash = variable_hash(__name);
	v->type = type;
	v->display = display;
	v->ptr = ptr;
	v->notify = notify;
	v->map = map;
	v->dyndisplay = dyndisplay;
	v->plugin = plugin;

	xfree(__name);

	return list_add_sorted(&variables, v, 0, variable_add_compare);

}

/*
 * variable_remove()
 *
 * usuwa zmienn±.
 */
int variable_remove(plugin_t *plugin, const CHAR_T *name)
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
		
		if (hash == v->name_hash && plugin == v->plugin && !xwcscasecmp(name, v->name)) {
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
int variable_set(const CHAR_T *name, const char *value, int allow_foreign)
{
	variable_t *v = variable_find(name);
	CHAR_T *tmpname;

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

	tmpname = xwcsdup(v->name);
	query_emit(NULL, TEXT("variable-changed"), &tmpname);
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
void variable_help(const CHAR_T *name)
{
	FILE *f; 
	CHAR_T *line, *type = NULL, *def = NULL, *tmp;
	char *filename;
	const CHAR_T *seeking_name;
	char *tmplang;
	wcs_string_t s;
	int found = 0;
	variable_t *v = variable_find(name);

	if (!v) {
		wcs_print("variable_not_found", name);
		return;
	}	

	if ((tmplang = getenv("LANGUAGE"))) {
		char *tmp_cutted = xstrndup(tmplang, 2);
		if (config_use_unicode) {
			filename = saprintf("vars-%s-utf.txt", tmp_cutted);
		} else {
			filename = saprintf("vars-%s.txt", tmp_cutted);
		}
		xfree(tmp_cutted);
	} else {
		filename = xstrdup("vars.txt");
	}

again:
	if (v->plugin && v->plugin->name) {
		char *tmp = saprintf(DATADIR "/plugins/"CHARF"/%s", v->plugin->name, filename);
		f = fopen(tmp, "r");
		xfree(tmp);

		if (!f) {
			if (xstrcasecmp(filename, "vars.txt")) {
				xfree(filename);
				filename = xstrdup("vars.txt");
				goto again;
			}
			wcs_print("help_set_file_not_found_plugin", v->plugin->name);
			xfree(filename);
			return;
		}

		seeking_name = xwcschr(name, ':') + 1;
	} else {
		char *tmp = saprintf(DATADIR "/%s", filename);
		f = fopen(tmp, "r");
		xfree(tmp);

                if (!f) {
                        if (xstrcasecmp(filename, "vars.txt")) {
                                xfree(filename);
                                filename = xstrdup("vars.txt");
				goto again;
                        }
                        wcs_print("help_set_file_not_found");
			xfree(filename);
                        return;
                }

		seeking_name = name;
	}

	xfree(filename);

	while ((line = wcs_read_file(f))) {
		if (!xwcscasecmp(line, seeking_name)) {
			found = 1;
			xfree(line);
			break;
		}

		xfree(line);
	}

	if (!found) {
		fclose(f);
		wcs_print("help_set_var_not_found", name);
		return;
	}

	line = wcs_read_file(f);
	
	if ((tmp = xwcsstr(line, TEXT(": "))))
		type = xwcsdup(tmp + 2);
	else
		type = xwcsdup(TEXT("?"));
	
	xfree(line);

	tmp = NULL;
	
	line = wcs_read_file(f);
	if ((tmp = xwcsstr(line, TEXT(": "))))
		def = xwcsdup(tmp + 2);
	else
		def = xwcsdup(TEXT("?"));
	xfree(line);

	wcs_print("help_set_header", name, type, def);

	xfree(type);
	xfree(def);

	if (tmp)			/* je¶li nie jest to ukryta zmienna... */
		xfree(wcs_read_file(f));	/* ... pomijamy liniê */
	s = wcs_string_init(NULL);
	while ((line = wcs_read_file(f))) {
		if (line[0] != '\t') {
			xfree(line);
			break;
		}

		if (!xwcsncmp(line, TEXT("\t- "), 3) && xwcscmp(s->str, TEXT(""))) {
			wcs_print("help_set_body", s->str);
			wcs_string_clear(s);
		}

                if (!xwcsncmp(line, TEXT("\t"), 1) && xwcslen(line) == 1) {
	                wcs_string_append(s, TEXT("\n\r"));
                        continue;
                }
	
		wcs_string_append(s, line + 1);

		if (line[xwcslen(line) - 1] != ' ')
			wcs_string_append_c(s, ' ');

		xfree(line);
	}

	if (xwcscmp(s->str, TEXT("")))
		wcs_print("help_set_body", s->str);

	wcs_string_free(s, 1);
	
	if (xstrcmp(format_find("help_set_footer"), ""))
		wcs_print("help_set_footer", name);

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
