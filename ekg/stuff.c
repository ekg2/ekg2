/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#include <sys/types.h>
#include <sys/stat.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#endif

#define __USE_BSD
#include <sys/time.h>

#ifndef NO_POSIX_SYSTEM
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#ifndef NO_POSIX_SYSTEM
#include <sched.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "debug.h"
#include "commands.h"
#include "dynstuff.h"
#include "protocol.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

#ifndef PATH_MAX
# ifdef MAX_PATH
#  define PATH_MAX MAX_PATH
# else
#  define PATH_MAX _POSIX_PATH_MAX
# endif
#endif

list_t children = NULL;
list_t aliases = NULL;
list_t autofinds = NULL;
list_t bindings = NULL;
list_t timers = NULL;
list_t conferences = NULL;
list_t newconferences = NULL;
list_t buffers = NULL;
list_t searches = NULL;

list_t bindings_added;
int old_stderr;
char *config_subject_prefix;

int in_autoexec = 0;
int config_auto_save = 0;
int config_auto_user_add = 0;
time_t last_save = 0;
int config_display_color = 1;
int config_beep = 1;
int config_beep_msg = 1;
int config_beep_chat = 1;
int config_beep_notify = 1;
char *config_console_charset;
int config_display_blinking = 1;
int config_display_pl_chars = 1;
int config_events_delay = 3;
char *config_sound_msg_file = NULL;
char *config_sound_chat_file = NULL;
char *config_sound_notify_file = NULL;
char *config_sound_sysmsg_file = NULL;
char *config_sound_mail_file = NULL;
char *config_sound_app = NULL;
int config_use_unicode;
int config_changed = 0;
int config_display_ack = 3;
int config_completion_notify = 1;
char *config_completion_char = NULL;
time_t ekg_started = 0;
int config_display_notify = 1;
int config_display_unknown = 0;
char *config_theme = NULL;
int config_default_status_window = 0;
char *home_dir = NULL;
char *config_quit_reason = NULL;
char *config_away_reason = NULL;
char *config_back_reason = NULL;
int config_query_commands = 0;
int config_slash_messages = 0;
int quit_message_send = 0;
int batch_mode = 0;
char *batch_line = NULL;
int config_make_window = 2;
char *config_tab_command = NULL;
int config_save_password = 1;
int config_save_quit = 1;
char *config_timestamp = NULL;
int config_timestamp_show = 1;
int config_display_sent = 1;
int config_sort_windows = 0;
int config_keep_reason = 1;
char *config_audio_device = NULL;
char *config_speech_app = NULL;
int config_time_deviation = 300;
int config_mesg = MESG_DEFAULT;
int config_display_welcome = 1;
char *config_display_color_map = NULL;
char *config_session_default = NULL;
int config_sessions_save = 0;
int config_windows_save = 0;
char *config_windows_layout = NULL;
char *config_profile = NULL;
int config_reason_limit = 1;
int config_debug = 1;

char *last_search_first_name = NULL;
char *last_search_last_name = NULL;
char *last_search_nickname = NULL;
char *last_search_uid = 0;

int reason_changed = 0;

/* 
 * windows_save()
 *
 * saves windows to the variable 
 */
void windows_save()
{
	list_t l;

	if (config_windows_save) {
		string_t s = string_init(NULL);
		int maxid = 0, i;
		
		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (!w->floating && w->id > maxid)
				maxid = w->id;
		}

		for (i = 1; i <= maxid; i++) {
			const char *target = "-";
			const char *session_name = NULL;
			
			for (l = windows; l; l = l->next) {
				window_t *w = l->data;

				if (w->id == i) {
					target = w->target;
					if (w->session)
						session_name = w->session->uid;
					break;
				}
			}
		
			if (session_name && target) {
				string_append(s, session_name);
				string_append_c(s, '/');
			}

			if (target) {
				string_append_c(s, '\"');
				string_append(s, target);
				string_append_c(s, '\"');
			}

			if (i < maxid)
				string_append_c(s, '|');
		}

		for (l = windows; l; l = l->next) {
			window_t *w = l->data;

			if (w->floating && (!w->target || xstrncmp(w->target, "__", 2))) {
				char *tmp = saprintf("|*%d,%d,%d,%d,%d,%s", w->left, w->top, w->width, w->height, w->frames, w->target);
				string_append(s, tmp);
				xfree(tmp);
			}
		}
		xfree(config_windows_layout);
		config_windows_layout = string_free(s, 0);
	}
}

/*
 * alias_add()
 *
 * dopisuje alias do listy aliasów.
 *
 *  - string - linia w formacie 'alias cmd',
 *  - quiet - czy wypluwaæ mesgi na stdout,
 *  - append - czy dodajemy kolejn± komendê?
 *
 * 0/-1
 */
int alias_add(const char *string, int quiet, int append)
{
	char *cmd;
	list_t l;
	struct alias *a;
	char **params = NULL;
	char *array;

	if (!string || !(cmd = xstrchr(string, ' ')))
		return -1;

	*cmd++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!xstrcasecmp(string, j->name)) {
			if (!append) {
				wcs_printq("aliases_exist", string);
				return -1;
			} else {
				list_t l;

				list_add(&j->commands, cmd, xstrlen(cmd) + 1);
				
				/* przy wielu komendach trudno dope³niaæ, bo wg. której? */
				for (l = commands; l; l = l->next) {
					command_t *c = l->data;

					if (!xstrcasecmp(c->name, j->name)) {
						xfree(c->params);
						c->params = array_make(("?"), (" "), 0, 1, 1);
						break;
					}
				}
			
				wcs_printq("aliases_append", string);

				return 0;
			}
		}
	}

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;
		char *tmp = ((*cmd == '/') ? cmd + 1 : cmd);

		if (!xstrcasecmp(string, c->name) && !(c->flags & COMMAND_ISALIAS)) {
			wcs_printq("aliases_command", string);
			return -1;
		}

		if (!xstrcasecmp(tmp, c->name))
			params = c->params;
	}
	a = xmalloc(sizeof(struct alias));
	a->name = xstrdup(string);
	a->commands = NULL;
	list_add(&(a->commands), cmd, (xstrlen(cmd) + 1) * sizeof(char));
	list_add(&aliases, a, 0);

	array = (params) ? array_join(params, (" ")) : xstrdup(("?"));
	command_add(NULL, a->name, array, cmd_alias_exec, COMMAND_ISALIAS, NULL);
	xfree(array);
	
	wcs_printq("aliases_add", a->name, (""));

	return 0;
}

/*
 * alias_remove()
 *
 * usuwa alias z listy aliasów.
 *
 *  - name - alias lub NULL,
 *  - quiet.
 *
 * 0/-1
 */
int alias_remove(const char *name, int quiet)
{
	list_t l;
	int removed = 0;

	for (l = aliases; l; ) {
		struct alias *a = l->data;

		l = l->next;

		if (!name || !xstrcasecmp(a->name, name)) {
			if (name)
				wcs_printq("aliases_del", name);
			command_remove(NULL, a->name);
			xfree(a->name);
			list_destroy(a->commands, 1);
			list_remove(&aliases, a, 1);
			removed = 1;
		}
	}

	if (!removed) {
		if (name)
			wcs_printq("aliases_noexist", name);
		else
			wcs_printq("aliases_list_empty");

		return -1;
	}

	if (removed && !name)
		wcs_printq("aliases_del_all");

	return 0;
}

/*
 * alias_free()
 *
 * zwalnia pamiêæ zajêt± przez aliasy.
 */
void alias_free()
{
	list_t l;

	if (!aliases)
		return;

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;
		
		xfree(a->name);
		list_destroy(a->commands, 1);
	}

	list_destroy(aliases, 1);
	aliases = NULL;
}

/*
 * binding_list()
 *
 * wy¶wietla listê przypisanych komend.
 */
void binding_list(int quiet, const char *name, int all) 
{
	list_t l;
	int found = 0;

	if (!bindings)
		wcs_printq("bind_seq_list_empty");

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		if (name) {
			if (xstrcasestr(b->key, name)) {
				wcs_printq("bind_seq_list", b->key, b->action);
				found = 1;
			}
			continue;
		}

		if (!b->internal || (all && b->internal)) 
			wcs_printq("bind_seq_list", b->key, b->action);
	}

	if (name && !found) {
		for (l = bindings; l; l = l->next) {
			struct binding *b = l->data;

			if (xstrcasestr(b->action, name))
				wcs_printq("bind_seq_list", b->key, b->action);
		}
	}
}

/*
 * binding_free()
 *
 * zwalnia pamiêæ po li¶cie przypisanych klawiszy.
 */
void binding_free() 
{
	list_t l;

	if (!bindings)
		return;

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		xfree(b->key);
		xfree(b->action);
		xfree(b->arg);
		xfree(b->default_action);
		xfree(b->default_arg);
	}

	list_destroy(bindings, 1);
	bindings = NULL;

        for (l = bindings_added; l; l = l->next) {
                binding_added_t *b = l->data;

                xfree(b->sequence);
        }

	list_destroy(bindings_added, 1);
	bindings_added = NULL;
}

/*
 * buffer_add()
 *
 * dodaje linijkê do danego typu bufora. je¶li max_lines > 0
 * to pilnuje, aby w buforze by³o maksymalnie tyle linii.
 *
 *  - type,
 *  - line,
 *  - max_lines.
 *
 * 0/-1
 */
int buffer_add(int type, const char *target, const char *line, int max_lines)
{
	struct buffer *b;

	if (max_lines) {
		list_t l = buffers;
		int bcount = buffer_count(type);
		
		while (bcount >= max_lines && l) {
			b = l->data;
			l = l->next;

			if (b->type != type) continue;

			xfree(b->line);
			xfree(b->target);
			list_remove(&buffers, b, 1);
			bcount--;
		}
	}
	b = xmalloc(sizeof(struct buffer));
	b->ts	= time(NULL);
	b->type = type;
	b->target = xstrdup(target);
	b->line = xstrdup(line);

	return ((list_add(&buffers, b, 0) ? 0 : -1));
}

int buffer_add_str(int type, const char *target, const char *str, int max_lines) {
	struct buffer *b;
	char *sep;

	time_t ts = 0;

	if (!(sep = xstrchr(str, ' '))) {
		debug("buffer_add_str() parsing str: %s failed\n", str);
		return -1;
	}
	ts = atoi(str);

	if (max_lines) {
		list_t l = buffers;
		int bcount = buffer_count(type);
		
		while (bcount >= max_lines && l) {
			b = l->data;
			l = l->next;
			if (b->type != type) continue;

			xfree(b->line);
			xfree(b->target);
			list_remove(&buffers, b, 1);
			bcount--;
		}
	}

	b	= xmalloc(sizeof(struct buffer));
	b->ts		= ts;
	b->type		= type;
	b->target	= xstrdup(target);
	b->line		= xstrdup(sep+1);

	return ((list_add(&buffers, b, 0) ? 0 : -1));
}

/* 
 * buffer_flush()
 *
 * zwraca zaalokowany ³ancuch zawieraj±cy wszystkie linie
 * z bufora danego typu.
 *
 *  - type,
 *  - target - dla kogo by³ bufor? NULL, je¶li olewamy.
 */
char *buffer_flush(int type, const char *target)
{
	string_t str = string_init(NULL);
	list_t l;

	for (l = buffers; l; ) {
		struct buffer *b = l->data;

		l = l->next;

		if (type != b->type)
			continue;

		if (target && b->target && xstrcmp(target, b->target))
			continue;

		string_append(str, itoa(b->ts));
		string_append_c(str, ' ');
		string_append(str, b->line);
		string_append_c(str, '\n');

		xfree(b->line);
		xfree(b->target);
		list_remove(&buffers, b, 1);
	}

	return string_free(str, 0);
}

/*
 * buffer_count()
 *
 * zwraca liczbê linii w buforze danego typu.
 */
int buffer_count(int type)
{
	list_t l;
	int count = 0;

	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		if (b->type == type)
			count++;
	}	

	return count;
}

/*
 * buffer_tail()
 *
 * zwraca najstarszy element buforowej kolejki, który
 * nale¿y zwolniæ. usuwa go z kolejki. zwraca NULL,
 * gdy kolejka jest pusta.
 */
char *buffer_tail(int type)
{
	char *str = NULL;
	list_t l;

	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		if (type != b->type)
			continue;
		
		str = b->line;
		b->line = NULL;

		xfree(b->target);
		list_remove(&buffers, b, 1);

		break;
	}

	return str;
}

/*
 * buffer_free()
 * 
 * zwalnia pamiêæ po buforach.
 */
void buffer_free()
{
	list_t l;

	if (!buffers)
		return;

	for (l = buffers; l; l = l->next) {
		struct buffer *b = l->data;

		xfree(b->line);
		xfree(b->target);
	}

	list_destroy(buffers, 1);
	buffers = NULL;
}

/* 
 * changed_var_default()
 * 
 * function called when session var ,,default''
 * is changed
 */
void changed_var_default(session_t *s, const char *var)
{
	list_t l;

	if (!sessions)
		return;
	
	if (session_int_get(s, var) == 0)
		return; 

	for (l = sessions; l; l = l->next) {
		session_t *sp = l->data;
		session_param_t *sparam;

		if (!session_compare(s, sp))
			continue;

		sparam = session_var_find(sp, var);

		if (sparam) {
			xfree(sparam->value);
			sparam->value = xstrdup("0");
		} else {
			debug("Default variable didn't setted for the session: %s - contact with developers\n", session_name(sp));
		}
	}
}

/*
 * changed_mesg()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,mesg''.
 */
void changed_mesg(const char *var)
{
	if (config_mesg == MESG_DEFAULT)
		mesg_set(mesg_startup);
	else
		mesg_set(config_mesg);
}
	
/*
 * changed_auto_save()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,auto_save''.
 */
void changed_auto_save(const char *var)
{
	/* oszukujemy, ale takie zachowanie wydaje siê byæ
	 * bardziej ,,naturalne'' */
	last_save = time(NULL);
}

/*
 * changed_display_blinking()
 *
 * wywo³ywane po zmianie warto¶ci zmiennej ,,display_blinking''.
 */
void changed_display_blinking(const char *var)
{
	list_t sl;

	/* wy³anczamy wszystkie blinkaj±ce uid'y */
        for (sl = sessions; sl; sl = sl->next) {
		list_t l;
        	session_t *s = sl->data;
		for (l = s->userlist; l; l = l->next) {
			userlist_t *u = l->data;
			u->blink = 0;			
		}
	}
}

/*
 * changed_theme()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,theme''.
 */
void changed_theme(const char *var)
{
	if (in_autoexec)
		return;
	if (!config_theme) {
		theme_free();
		theme_init();
	} else {
		if (!theme_read(config_theme, 1)) {
			print("theme_loaded", config_theme);
		} else {
			print("error_loading_theme", strerror(errno));
			variable_set(("theme"), NULL, 0);
		}
	}
}

const char *compile_time()
{
	return __DATE__ " " __TIME__;
}

/* NEW CONFERENCE API HERE, WHEN OLD CONFERENCE API BECOME OBSOLETE CHANGE FUNCTION NAME, ETC.... */

userlist_t *newconference_member_find(newconference_t *conf, const char *uid) {
	list_t l;

	if (!conf || !uid) return NULL;

	for (l = conf->participants; l; l = l->next) {
		userlist_t *u = l->data;

		if (!xstrcasecmp(u->uid, uid))
			return u;
	}
	return NULL;
}

userlist_t *newconference_member_add(newconference_t *conf, const char *uid, const char *nick) {
	userlist_t *u;
	if (!conf || !uid) return NULL;

	if (!(u = newconference_member_find(conf, uid)))
		u = userlist_add_u(&(conf->participants), uid, nick);
	return u;
}
	/* remove userlist_t from conference. wrapper. */
int newconference_member_remove(newconference_t *conf, userlist_t *u) {
	if (!conf || !u) return -1;
	return userlist_remove_u(&(conf->participants), u);
}

newconference_t *newconference_find(session_t *s, const char *name) {
	list_t l;
	for (l = newconferences; l; l = l->next) {
		newconference_t *c = l->data;

		if ((!s || !xstrcmp(s->uid, c->session)) && !xstrcmp(name, c->name)) return c;
	}
	return NULL;
}

newconference_t *newconference_create(session_t *s, const char *name, int create_wnd) {
	newconference_t *c;
	window_t *w;

	if (!s || !name) return NULL;

	if ((c = newconference_find(s, name))) return c;

	if (!(w = window_find_s(s, name)) && create_wnd) {
		w = window_new(name, s, 0);
	}

	c		= xmalloc(sizeof(newconference_t));
	c->session	= xstrdup(s->uid);
	c->name		= xstrdup(name);
	
	return list_add(&newconferences, c, 0);
}

void newconference_destroy(newconference_t *conf, int kill_wnd) {
	window_t *w = NULL; 
	if (!conf) return;
	if (kill_wnd) w = window_find_s(session_find(conf->session), conf->name);

	xfree(conf->name);
	xfree(conf->session);
	userlist_free_u(&conf->participants);
	list_remove(&newconferences, conf, 1);

	if (w) window_kill(w, 0);
}

void newconference_free() {
	list_t l;
	for (l = newconferences; l; l = l->next) {
		newconference_t *c = l->data;

		xfree(c->session);
		xfree(c->name);
		userlist_free_u(&c->participants);
	}

	list_destroy(newconferences, 1);
	newconferences = NULL;
}

/* OLD CONFERENCE API HERE, REQUEST REWRITING/USING NEW-ONE */

/*
 * conference_add()
 *
 * dopisuje konferencje do listy konferencji.
 *
 *  - name - nazwa konferencji,
 *  - nicklist - lista nicków, grup, czegokolwiek,
 *  - quiet - czy wypluwaæ mesgi na stdout.
 *
 * zaalokowan± struct conference lub NULL w przypadku b³êdu.
 */
struct conference *conference_add(session_t *session, const char *name, const char *nicklist, int quiet)
{
	struct conference c;
	char **nicks;
	list_t l, sl;
	int i, count;
	char **p;

	if (!name || !nicklist)
		return NULL;

	if (nicklist[0] == ',' || nicklist[xstrlen(nicklist) - 1] == ',') {
		wcs_printq("invalid_params", ("chat"));
		return NULL;
	}

	memset(&c, 0, sizeof(c));

	nicks = array_make(nicklist, " ,", 0, 1, 0);

	/* grupy zamieniamy na niki */
	for (i = 0; nicks[i]; i++) {
		if (nicks[i][0] == '@') {
			char *gname = xstrdup(nicks[i] + 1);
			int first = 0;
			int nig = 0; /* nicks in group */
		
			for (sl = sessions; sl; sl = sl->next) {
				session_t *s = sl->data;
			        for (l = s->userlist; l; l = l->next) {
					userlist_t *u = l->data;
					list_t m;

					if (!u->nickname)
						continue;

					for (m = u->groups; m; m = m->next) {
						struct ekg_group *g = m->data;

						if (!xstrcasecmp(gname, g->name)) {
							if (first++)
								array_add(&nicks, xstrdup(u->nickname));
							else {
								xfree(nicks[i]);
								nicks[i] = xstrdup(u->nickname);
							}

							nig++;

							break;
						}
					}
				}
			}

			xfree(gname);

			if (!nig) {
				printq("group_empty", gname);
				printq("conferences_not_added", name);
				array_free(nicks);
				return NULL;
			}
		}
	}

	count = array_count(nicks);

	for (l = conferences; l; l = l->next) {
		struct conference *cf = l->data;
		
		if (!xstrcasecmp(name, cf->name)) {
			printq("conferences_exist", name);

			array_free(nicks);

			return NULL;
		}
	}


	for (p = nicks, i = 0; *p; p++) {
		char *uid;

		if (!xstrcmp(*p, ""))
		        continue;

		uid = get_uid(session, *p);

		if (uid)
			list_add(&(c.recipients), uid, xstrlen(uid) +1);
		i++;
	}


	array_free(nicks);

	if (i != count) {
		printq("conferences_not_added", name);
		return NULL;
	}

	printq("conferences_add", name);

	c.name = xstrdup(name);

	tabnick_add(name);

	return list_add(&conferences, &c, sizeof(c));
}

/*
 * conference_remove()
 *
 * usuwa konferencjê z listy konferencji.
 *
 *  - name - konferencja lub NULL dla wszystkich,
 *  - quiet.
 *
 * 0/-1
 */
int conference_remove(const char *name, int quiet)
{
	list_t l;
	int removed = 0;

	for (l = conferences; l; ) {
		struct conference *c = l->data;

		l = l->next;

		if (!name || !xstrcasecmp(c->name, name)) {
			if (name)
				printq("conferences_del", name);
			tabnick_remove(c->name);
			xfree(c->name);
			list_destroy(c->recipients, 1);
			list_remove(&conferences, c, 1);
			removed = 1;
		}
	}

	if (!removed) {
		if (name)
			printq("conferences_noexist", name);
		else
			wcs_printq("conferences_list_empty");
		
		return -1;
	}

	if (removed && !name)
		wcs_printq("conferences_del_all");

	return 0;
}

/*
 * conference_create()
 *
 * tworzy now± konferencjê z wygenerowan± nazw±.
 *
 *  - nicks - lista ników tak, jak dla polecenia conference.
 */
struct conference *conference_create(session_t *session, const char *nicks)
{
	struct conference *c;
	static int count = 1;
	char *name = saprintf("#conf%d", count);

	if ((c = conference_add(session, name, nicks, 0)))
		count++;

	xfree(name);

	return c;
}

/*
 * conference_find()
 *
 * znajduje i zwraca wska¼nik do konferencji lub NULL.
 *
 *  - name - nazwa konferencji.
 */
struct conference *conference_find(const char *name) 
{
	list_t l;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;

		if (!xstrcmp(c->name, name))
			return c;
	}
	
	return NULL;
}

/*
 * conference_participant()
 *
 * sprawdza, czy dany numer jest uczestnikiem konferencji.
 *
 *  - c - konferencja,
 *  - uin - numer.
 *
 * 1 je¶li jest, 0 je¶li nie.
 */
int conference_participant(struct conference *c, const char *uid)
{
	list_t l;
	
	for (l = c->recipients; l; l = l->next) {
		char *u = l->data;

		if (!xstrcasecmp(u, uid))
			return 1;
	}

	return 0;

}

/*
 * conference_find_by_uids()
 *
 * znajduje konferencjê, do której nale¿± podane uiny. je¿eli nie znaleziono,
 * zwracany jest NULL. je¶li numerów jest wiêcej, zostan± dodane do
 * konferencji, bo najwyra¼niej kto¶ do niej do³±czy³.
 * 
 *  - from - kto jest nadawc± wiadomo¶ci,
 *  - recipients - tablica numerów nale¿±cych do konferencji,
 *  - count - ilo¶æ numerów,
 *  - quiet.
 */
struct conference *conference_find_by_uids(session_t *s, const char *from, const char **recipients, int count, int quiet) 
{
	int i;
	list_t l;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;
		int matched = 0;

		for (i = 0; i < count; i++)
			if (conference_participant(c, recipients[i]))
				matched++;

		if (conference_participant(c, from))
			matched++;

		debug_function("// conference_find_by_uids(): from=%s, rcpt count=%d, matched=%d, list_count(c->recipients)=%d\n", from, count, matched, list_count(c->recipients));

		if (matched == list_count(c->recipients) && matched <= (!xstrcasecmp(from, s->uid) ? count : count + 1)) {
			string_t new = string_init(NULL);
			int comma = 0;

			if (xstrcasecmp(from, s->uid) && !conference_participant(c, from)) {
				list_add(&c->recipients, &from, sizeof(from));

				comma++;
				string_append(new, format_user(s, from));
			} 

			for (i = 0; i < count; i++) {
				if (xstrcasecmp(recipients[i], s->uid) && !conference_participant(c, recipients[i])) {
					list_add(&c->recipients, &recipients[i], sizeof(recipients[0]));
			
					if (comma++)
						string_append(new, ", ");
					string_append(new, format_user(s, recipients[i]));
				}
			}

			if (xstrcmp(new->str, "") && !c->ignore)
				printq("conferences_joined", new->str, c->name);
			string_free(new, 1);

			debug("// conference_find_by_uins(): matching %s\n", c->name);

			return c;
		}
	}

	return NULL;
}

/*
 * conference_set_ignore()
 *
 * ustawia stan konferencji na ignorowany lub nie.
 *
 *  - name - nazwa konferencji,
 *  - flag - 1 ignorowaæ, 0 nie ignorowaæ,
 *  - quiet.
 *
 * 0/-1
 */
int conference_set_ignore(const char *name, int flag, int quiet)
{
	struct conference *c = conference_find(name);

	if (!c) {
		printq("conferences_noexist", name);
		return -1;
	}

	c->ignore = flag;
	printq((flag ? "conferences_ignore" : "conferences_unignore"), name);

	return 0;
}

/*
 * conference_rename()
 *
 * zmienia nazwê instniej±cej konferencji.
 * 
 *  - oldname - stara nazwa,
 *  - newname - nowa nazwa,
 *  - quiet.
 *
 * 0/-1
 */
int conference_rename(const char *oldname, const char *newname, int quiet)
{
	struct conference *c;
	char *tmp1, *tmp2;
	
	if (conference_find(newname)) {
		printq("conferences_exist", newname);
		return -1;
	}

	if (!(c = conference_find(oldname))) {
		printq("conference_noexist", oldname);
		return -1;
	}

	xfree(c->name);
	c->name = xstrdup(newname);
	tabnick_remove(oldname);
	tabnick_add(newname);
	
	printq("conferences_rename", oldname, newname);

	tmp1 = xstrdup(oldname);
	tmp2 = xstrdup(newname);

	query_emit(NULL, ("conference-renamed"), &tmp1, &tmp2);

	xfree(tmp1);
	xfree(tmp2);
	
	return 0;
}

/*
 * conference_free()
 *
 * zwalnia pamiêæ zajêt± przez konferencje.
 */
void conference_free()
{
	list_t l;

	if (!conferences)
		return;

	for (l = conferences; l; l = l->next) {
		struct conference *c = l->data;
		
		xfree(c->name);
		list_destroy(c->recipients, 1);
	}

	list_destroy(conferences, 1);
	conferences = NULL;
}

/*
 * help_path()
 *
 * zwraca ¶cie¿kê do pliku z pomoc± we w³a¶ciwym jêzyku lub null je¶li nie ma takiego pliku
 *
 */
char *help_path(char *name, char *plugin) {

        char *tmp;
        char *base;
        char *lang;
        FILE *fp;

        if (plugin) {
                base = saprintf(DATADIR "/plugins/%s/%s", plugin, name);
        } else {
                base = saprintf(DATADIR "/%s", name);
        }

        tmp = getenv("LANGUAGE");
        if (!tmp) {
                lang = xstrdup("en");
        } else {
                lang = xstrdup(tmp);
        }
	if (config_use_unicode) {
	        tmp = saprintf("%s-%s-utf.txt", base, lang);
		if (!(fp = fopen(tmp, "r"))) {
			xfree(tmp);
			tmp = saprintf("%s-%s.txt", base, lang);
		} else { 
			fclose(fp);
			goto end;
		}
	} else {
        	tmp = saprintf("%s-%s.txt", base, lang);
	}

        /* Temporary fallback - untill we don't have full en translation */
        fp = fopen(tmp, "r");
        if (!fp) {
		xfree(tmp);
		if (config_use_unicode) {
                	tmp = saprintf("%s-pl-utf.txt", base);
		} else {
                	tmp = saprintf("%s-pl.txt", base);
		}
        } else {
                fclose(fp);
        }
end:
        xfree(base);
        xfree(lang);
        return tmp;
}


/*
 * ekg_hash()
 *
 * liczy prosty hash z nazwy, wykorzystywany przy przeszukiwaniu list
 * zmiennych, formatów itp.
 *
 *  - name - nazwa.
 */
int ekg_hash(const char *name)
{
	int hash = 0;

	for (; *name; name++) {
		hash ^= *name;
		hash <<= 1;
	}

	return hash;
}

/*
 * mesg_set()
 *
 * w³±cza/wy³±cza/sprawdza mo¿liwo¶æ pisania do naszego terminala.
 *
 *  - what - MESG_ON, MESG_OFF lub MESG_CHECK
 * 
 * -1 je¶li b³ad, lub aktualny stan: MESG_ON/MESG_OFF
*/
int mesg_set(int what)
{
#ifndef NO_POSIX_SYSTEM
	const char *tty;
	struct stat s;

	if (!(tty = ttyname(old_stderr)) || stat(tty, &s)) {
		debug_error("mesg_set() error: %s\n", strerror(errno));
		return -1;
	}

	switch (what) {
		case MESG_OFF:
			chmod(tty, s.st_mode & ~S_IWGRP);
			break;
		case MESG_ON:
			chmod(tty, s.st_mode | S_IWGRP);
			break;
		case MESG_CHECK:
			return ((s.st_mode & S_IWGRP) ? MESG_ON : MESG_OFF);
	}
	
	return 0;
#else
	return -1;
#endif
}

/*
 * iso_to_ascii()
 *
 * usuwa polskie litery z tekstu.
 *
 *  - c.
 */
void iso_to_ascii(unsigned char *buf) {
#if USE_UNICODE
	if (config_use_unicode) return;
#endif
	if (!buf)
		return;

	while (*buf) {
		if (*buf == (unsigned char)'±') *buf = 'a';
		if (*buf == (unsigned char)'ê') *buf = 'e';
		if (*buf == (unsigned char)'æ') *buf = 'c';
		if (*buf == (unsigned char)'³') *buf = 'l';
		if (*buf == (unsigned char)'ñ') *buf = 'n';
		if (*buf == (unsigned char)'ó') *buf = 'o';
		if (*buf == (unsigned char)'¶') *buf = 's';
		if (*buf == (unsigned char)'¿') *buf = 'z';
		if (*buf == (unsigned char)'¼') *buf = 'z';

		if (*buf == (unsigned char)'¡') *buf = 'A';
		if (*buf == (unsigned char)'Ê') *buf = 'E';
		if (*buf == (unsigned char)'Æ') *buf = 'C';
		if (*buf == (unsigned char)'£') *buf = 'L';
		if (*buf == (unsigned char)'Ñ') *buf = 'N';
		if (*buf == (unsigned char)'Ó') *buf = 'O';
		if (*buf == (unsigned char)'¦') *buf = 'S';
		if (*buf == (unsigned char)'¯') *buf = 'Z';
		if (*buf == (unsigned char)'¬') *buf = 'Z';

		buf++;
	}
}

/*
 * strip_quotes()
 *
 * strips quotes from the begging and the end of 
 * given string
 */
char *strip_quotes(char *line)
{
        char *buf;
	if (!line)
		return NULL;

        for (buf = line; *buf == '\"'; buf++);

        while (line[xstrlen(line) - 1] == '\"')
                line[xstrlen(line) - 1] = 0;

        return buf;
}

/*
 * strip_spaces()
 *
 * pozbywa siê spacji na pocz±tku i koñcu ³añcucha.
 */
char *strip_spaces(char *line) {
	size_t linelen;
	char *buf;

	if (!(linelen = xstrlen(line))) return line;
	
	for (buf = line; xisspace(*buf); buf++);

	while (linelen > 0 && xisspace(line[linelen - 1])) {
		line[linelen - 1] = 0;
		linelen--;
	}
	
	return buf;
}

/*
 * play_sound()
 *
 * odtwarza dzwiêk o podanej nazwie.
 *
 * 0/-1
 */
int play_sound(const char *sound_path)
{
	char *params[2];
	int res;

	if (!config_sound_app || !sound_path) {
		errno = EINVAL;
		return -1;
	}

	params[0] = saprintf(("^%s %s"), config_sound_app, sound_path);
	params[1] = NULL;

	res = cmd_exec(("exec"), (const char **) params, NULL, NULL, 1);

	xfree(params[0]);

	return res;
}

/*
 * child_add()
 *
 * dopisuje do listy uruchomionych dzieci procesów.
 *
 *  - plugin
 *  - pid
 *  - name
 *  - handler
 *  - data
 *
 * 0/-1
 */
child_t *child_add(plugin_t *plugin, int pid, const char *name, child_handler_t handler, void *private)
{
	child_t *c = xmalloc(sizeof(child_t));

	c->plugin	= plugin;
	c->pid		= pid;
	c->name		= xstrdup(name);
	c->handler	= handler;
	c->private	= private;
	
	list_add(&children, c, 0);
	return c;
}

/*
 * prepare_path()
 *
 * zwraca pe³n± ¶cie¿kê do podanego pliku katalogu ~/.ekg2/
 *
 *  - filename - nazwa pliku,
 *  - do_mkdir - czy tworzyæ katalog ~/.ekg2 ?
 */
const char *prepare_path(const char *filename, int do_mkdir)
{
	static char path[PATH_MAX];

	if (do_mkdir) {
		if (config_profile) {
			char *cd = xstrdup(config_dir), *tmp;

			if ((tmp = xstrrchr(cd, '/')))
				*tmp = 0;
#ifndef NO_POSIX_SYSTEM
			if (mkdir(cd, 0700) && errno != EEXIST) {
#else
			if (mkdir(cd) && errno != EEXIST) {
#endif
				xfree(cd);
				return NULL;
			}

			xfree(cd);
		}
#ifndef NO_POSIX_SYSTEM
		if (mkdir(config_dir, 0700) && errno != EEXIST)
#else
		if (mkdir(config_dir) && errno != EEXIST)
#endif
			return NULL;
	}
	
	if (!filename || !*filename)
		snprintf(path, sizeof(path), "%s", config_dir);
	else
		snprintf(path, sizeof(path), "%s/%s", config_dir, filename);
	
	return path;
}

char *random_line(const char *path)
{
	int max = 0, item, tmp = 0;
	char *line;
	FILE *f;

	if (!path)
		return NULL;

	if ((f = fopen(path, "r")) == NULL)
		return NULL;
	
	while ((line = read_file(f))) {
		xfree(line);
		max++;
	}

	rewind(f);

	if (max) {
		item = rand() / (RAND_MAX / max + 1);

		while ((line = read_file(f))) {
			if (tmp == item) {
				fclose(f);
				return line;
			}
			xfree(line);
			tmp++;
		}
	}
		
	fclose(f);
	return NULL;
}

/*
 * read_file()
 *
 * czyta i zwraca linijkê tekstu z pliku, alokuj±c przy tym odpowiedni buforek.
 * usuwa znaki koñca linii.
 */
char *read_file(FILE *f)
{
	char buf[1024], *res = NULL;
	if (!f)
		return NULL;

	while (fgets(buf, sizeof(buf), f)) {
		int first = (res) ? 0 : 1;
		size_t new_size = ((res) ? xstrlen(res) : 0) + xstrlen(buf) + 1;

		res = xrealloc(res, new_size);
		if (first)
			*res = 0;
		xstrcpy(res + xstrlen(res), buf);

		if (xstrchr(buf, '\n'))
			break;
	}

	if (res && xstrlen(res) > 0 && res[xstrlen(res) - 1] == '\n')
		res[xstrlen(res) - 1] = 0;
	if (res && xstrlen(res) > 0 && res[xstrlen(res) - 1] == '\r')
		res[xstrlen(res) - 1] = 0;

	return res;
}

/*
 * timestamp()
 *
 * zwraca statyczny buforek z ³adnie sformatowanym czasem.
 *
 *  - format.
 */
const char *timestamp(const char *format)
{
	static char buf[100];
	time_t t;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);
	if (!strftime(buf, sizeof(buf), format, tm) && xstrlen(format)>0)
		xstrcpy(buf, "TOOLONG");

	return buf;
}

/*
 * unidle()
 *
 * uaktualnia licznik czasu ostatniej akcji, ¿eby przypadkiem nie zrobi³o
 * autoawaya, kiedy piszemy.
 */
void unidle()
{
	time(&last_action);
}

/*
 * on_off()
 *
 * zwraca 1 je¶li tekst znaczy w³±czyæ, 0 je¶li wy³±czyæ, -1 je¶li co innego.
 *
 *  - value.
 */
int on_off(const char *value)
{
	if (!value)
		return -1;

	if (!xstrcasecmp(value, "on") || !xstrcasecmp(value, "true") || !xstrcasecmp(value, "yes") || !xstrcasecmp(value, "tak") || !xstrcmp(value, "1"))
		return 1;

	if (!xstrcasecmp(value, "off") || !xstrcasecmp(value, "false") || !xstrcasecmp(value, "no") || !xstrcasecmp(value, "nie") || !xstrcmp(value, "0"))
		return 0;

	return -1;
}

/*
 * timer_add()
 *
 * dodaje timera.
 *
 *  - plugin - plugin obs³uguj±cy timer,
 *  - name - nazwa timera w celach identyfikacji. je¶li jest równa NULL,
 *           zostanie przyznany pierwszy numerek z brzegu.
 *  - period - za jaki czas w sekundach ma byæ uruchomiony,
 *  - persist - czy sta³y timer,
 *  - function - funkcja do wywo³ania po up³yniêciu czasu,
 *  - data - dane przekazywane do funkcji.
 *
 * zwraca zaalokowan± struct timer lub NULL w przypadku b³êdu.
 */
struct timer *timer_add(plugin_t *plugin, const char *name, time_t period, int persist, int (*function)(int, void *), void *data)
{
	struct timer *t;
	struct timeval tv;
	struct timezone tz;

	/* wylosuj now± nazwê, je¶li nie mamy */
	if (!name) {
		int i;

		for (i = 1; !name; i++) {
			list_t l;
			int gotit = 0;

			for (l = timers; l; l = l->next) {
				struct timer *tt = l->data;

				if (!xstrcmp(tt->name, itoa(i))) {
					gotit = 1;
					break;
				}
			}

			if (!gotit)
				name = itoa(i);
		}
	}
	t = xmalloc(sizeof(struct timer));
	gettimeofday(&tv, &tz);
	tv.tv_sec += period;
	memcpy(&(t->ends), &tv, sizeof(tv));
	t->name = xstrdup(name);
	t->period = period;
	t->persist = persist;
	t->function = function;
	t->data = data;
	t->plugin = plugin;

	list_add(&timers, t, 0);
	return t;
}

int timer_freeone(struct timer *t) 
{
	if (!t) 
		return -1;

	t->function(1, t->data); 
	xfree(t->name);
	list_remove(&timers, t, 1);
	return 0;
}

/*
 * timer_remove()
 *
 * usuwa timer.
 *
 *  - plugin - plugin obs³uguj±cy timer,
 *  - name - nazwa timera,
 *
 * 0/-1
 */
int timer_remove(plugin_t *plugin, const char *name)
{
	list_t l;
	int removed = 0;

	for (l = timers; l; ) {
		struct timer *t = l->data;

		l = l->next;

		if (t->plugin == plugin && !xstrcasecmp(name, t->name)) {
			timer_freeone(t);
			removed++;
		}
	}

	return ((removed) ? 0 : -1);
}

/*
 * timer_handle_command()
 *
 * obs³uga timera wywo³uj±cego komendê.
 */
TIMER(timer_handle_command)
{
	if (type) {
		xfree(data);
		return 0;
	}
	
	command_exec(NULL, NULL, (char *) data, 0);
	return 0;
}

/*
 * timer_remove_user()
 *
 * usuwa wszystkie timery u¿ytkownika.
 *
 * 0/-1
 */
int timer_remove_user(int at)
{
	list_t l;
	int removed = 0;

	for (l = timers; l; ) {
		struct timer *t = l->data;

		l = l->next;

		if (t->at == at && t->function == timer_handle_command) { 
			t->function(1, t->data);
			xfree(t->name);
			xfree(t->data);
			list_remove(&timers, t, 1);
			removed = 1;
		}
	}

	return ((removed) ? 0 : -1);
}

/*
 * timer_free()
 *
 * zwalnia pamiêæ po timerach.
 */
void timer_free()
{
	list_t l;

	for (l = timers; l; l = l->next) {
		struct timer *t = l->data;
		
		xfree(t->name);
		xfree(t->data);
	}

	list_destroy(timers, 1);
	timers = NULL;
}

/* 
 * xstrmid()
 *
 * wycina fragment tekstu alokuj±c dla niego pamiêæ.
 *
 *  - str - tekst ¼ród³owy,
 *  - start - pierwszy znak,
 *  - length - d³ugo¶æ wycinanego tekstu, je¶li -1 do koñca.
 */
char *xstrmid(const char *str, int start, int length)
{
	char *res, *q;
	const char *p;

	if (!str)
		return xstrdup("");

	if (start > xstrlen(str))
		start = xstrlen(str);

	if (length == -1)
		length = xstrlen(str) - start;

	if (length < 1)
		return xstrdup("");

	if (length > xstrlen(str) - start)
		length = xstrlen(str) - start;
	
	res = xmalloc(length + 1);
	
	for (p = str + start, q = res; length; p++, q++, length--)
		*q = *p;

	*q = 0;

	return res;
}

struct color_map color_map_default[26] = {
	{ 'k', 0, 0, 0 },
	{ 'r', 168, 0, 0, },
	{ 'g', 0, 168, 0, },
	{ 'y', 168, 168, 0, },
	{ 'b', 0, 0, 168, },
	{ 'm', 168, 0, 168, },
	{ 'c', 0, 168, 168, },
	{ 'w', 168, 168, 168, },
	{ 'K', 96, 96, 96 },
	{ 'R', 255, 0, 0, },
	{ 'G', 0, 255, 0, },
	{ 'Y', 255, 255, 0, },
	{ 'B', 0, 0, 255, },
	{ 'M', 255, 0, 255, },
	{ 'C', 0, 255, 255, },
	{ 'W', 255, 255, 255, },

	/* dodatkowe mapowanie ró¿nych kolorów istniej±cych w GG */
	{ 'C', 128, 255, 255, },
	{ 'G', 128, 255, 128, },
	{ 'M', 255, 128, 255, },
	{ 'B', 128, 128, 255, },
	{ 'R', 255, 128, 128, },
	{ 'Y', 255, 255, 128, }, 
	{ 'm', 168, 128, 168, },
	{ 'c', 128, 168, 168, },
	{ 'g', 64, 168, 64, },
	{ 'm', 128, 64, 128, }
};

/*
 * color_map()
 *
 * funkcja zwracaj±ca kod koloru z domy¶lnej 16-kolorowej palety terminali
 * ansi odpadaj±cemu podanym warto¶ciom RGB.
 */
char color_map(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned long mindist = 255 * 255 * 255;
	struct color_map *map = color_map_default;
	char ch = 0;
	int i;

/*	debug("color=%.2x%.2x%.2x\n", r, g, b); */

#define __sq(x) ((x)*(x))
	for (i = 0; i < 26; i++) {
		unsigned long dist = __sq(r - map[i].r) + __sq(g - map[i].g) + __sq(b - map[i].b);

/*		debug("%d(%c)=%.2x%.2x%.2x, dist=%ld\n", i, map[i].color, map[i].r, map[i].g, map[i].b, dist); */

		if (dist < mindist) {
			ch = map[i].color;
			mindist = dist;
		}
	}
#undef __sq

/*	debug("mindist=%ld, color=%c\n", mindist, ch); */

	return ch;	
}

/*
 * sprawdza czy podany znak jest znakiem alphanumerycznym (uwzlglednia polskie znaki)
 */
int isalpha_pl(unsigned char c)
{
/*  gg_debug(GG_DEBUG_MISC, "c: %d\n", c); */
    if(isalpha(c)) /* normalne znaki */
        return 1;
    else if(c == 177 || c == 230 || c == 234 || c == 179 || c == 241 || c == 243 || c == 182 || c == 191 || c == 188) /* polskie literki */
        return 1;
    else if(c == 161 || c == 198 || c == 202 || c == 209 || c == 163 || c == 211 || c == 166 || c == 175 || c == 172) /* wielka litery polskie */
        return 1;
    else
        return 0;
}

/*
 * strcasestr()
 *
 * robi to samo co xstrstr() tyle ¿e bez zwracania uwagi na wielko¶æ
 * znaków.
 */
char *strcasestr(const char *haystack, const char *needle)
{
	int i, hlen = xstrlen(haystack), nlen = xstrlen(needle);

	for (i = 0; i <= hlen - nlen; i++) {
		if (!xstrncasecmp(haystack + i, needle, nlen))
			return (char*) (haystack + i);
	}

	return NULL;
}

/*
 * msg_all()
 *
 * msg to all users in session's userlist
 * it uses function to do it
 */
int msg_all(session_t *s, const char *function, const char *what)
{
	list_t l;

	if (!s->userlist)
		return -1;

	if (!function)
		return -2;

	for (l = s->userlist; l; l = l->next) {
		userlist_t *u = l->data;

		if (!u || !u->uid)
			continue;
		command_exec_format(NULL, s, 0, "%s \"%s\" %s", function, get_nickname(s, u->uid), what);
	}

	return 0;
}
/*
 * say_it()
 *
 * zajmuje siê wypowiadaniem tekstu, uwa¿aj±c na ju¿ dzia³aj±cy
 * syntezator w tle.
 *
 * 0/-1/-2. -2 w przypadku, gdy dodano do bufora.
 */
int say_it(const char *str)
{
#ifndef NO_POSIX_SYSTEM
	pid_t pid;

	if (!config_speech_app || !str || !xstrcmp(str, ("")))
		return -1;

	if (speech_pid) {
		buffer_add(BUFFER_SPEECH, NULL, str, 50);
		return -2;
	}

	if ((pid = fork()) < 0)
		return -1;

	speech_pid = pid;

	if (!pid) {
		char *tmp = saprintf("%s 2>/dev/null 1>&2", config_speech_app);
		FILE *f = popen(tmp, "w");
		int status = -1;

		xfree(tmp);

		if (f) {
			fprintf(f, "%s.", str);
			status = pclose(f);	/* dzieciak czeka na dzieciaka */
		}

		exit(status);
	}

	child_add(NULL, pid, NULL, NULL, NULL);
	return 0;
#else
	return -1;
#endif
}

void debug_ext(int level, const char *format, ...) {
	va_list ap;
	if (!config_debug) return;

	va_start(ap, format);
	ekg_debug_handler(level, format, ap);
	va_end(ap);
}

/*
 * debug()
 *
 * debugowanie dla ekg.
 */
void debug(const char *format, ...)
{
	va_list ap;

	if (!config_debug)
		return;

	va_start(ap, format);
	ekg_debug_handler(0, format, ap);
	va_end(ap);
}

static char base64_charset[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * base64_encode()
 *
 * zapisuje ci±g znaków w base64.
 *
 *  - buf - ci±g znaków.
 *
 * zaalokowany bufor.
 */
char *base64_encode(const char *buf, size_t len)
{
	char *out, *res;
	int i = 0, j = 0, k = 0;

	if (!buf) return NULL;
/*	if (!len) return NULL; */
	
	res = out = xmalloc((len / 3 + 1) * 4 + 2);

	while (j <= len) {
		switch (i % 4) {
			case 0:
				k = (buf[j] & 252) >> 2;
				break;
			case 1:
				if (j < len)
					k = ((buf[j] & 3) << 4) | ((buf[j + 1] & 240) >> 4);
				else
					k = (buf[j] & 3) << 4;

				j++;
				break;
			case 2:
				if (j < len)
					k = ((buf[j] & 15) << 2) | ((buf[j + 1] & 192) >> 6);
				else
					k = (buf[j] & 15) << 2;

				j++;
				break;
			case 3:
				k = buf[j++] & 63;
				break;
		}
		*out++ = base64_charset[k];
		i++;
	}

	if (i % 4)
		for (j = 0; j < 4 - (i % 4); j++, out++)
			*out = '=';
	
	*out = 0;
	
	return res;
}

/*
 * base64_decode()
 *
 * dekoduje ci±g znaków z base64.
 *
 *  - buf - ci±g znaków.
 *
 * zaalokowany bufor.
 */
char *base64_decode(const char *buf)
{
	char *res, *save, *foo, val;
	const char *end;
	int index = 0;
	size_t buflen;

	if (!buf || !(buflen = xstrlen(buf)))
		return NULL;

	save = res = xcalloc(1, (buflen / 4 + 1) * 3 + 2);

	end = buf + buflen - 1;

	while (*buf && buf < end) {
		if (*buf == '\r' || *buf == '\n') {
			buf++;
			continue;
		}
		if (!(foo = xstrchr(base64_charset, *buf)))
			foo = base64_charset;
		val = (int)(foo - base64_charset);
		buf++;
		switch (index) {
			case 0:
				*res |= val << 2;
				break;
			case 1:
				*res++ |= val >> 4;
				*res |= val << 4;
				break;
			case 2:
				*res++ |= val >> 2;
				*res |= val << 6;
				break;
			case 3:
				*res++ |= val;
				break;
		}
		index++;
		index %= 4;
	}
	*res = 0;
	
	return save;
}

/*
 * split_line()
 * 
 * podaje kolejn± liniê z bufora tekstowego. niszczy go bezpowrotnie, dziel±c
 * na kolejne stringi. zdarza siê, nie ma potrzeby pisania funkcji dubluj±cej
 * bufor ¿eby tylko mieæ nieruszone dane wej¶ciowe, skoro i tak nie bêd± nam
 * po¼niej potrzebne. obcina `\r\n'.
 * 
 *  - ptr - wska¼nik do zmiennej, która przechowuje aktualn± pozycjê
 *    w przemiatanym buforze
 * 
 * wska¼nik do kolejnej linii tekstu lub NULL, je¶li to ju¿ koniec bufora.
 */
char *split_line(char **ptr)
{
        char *foo, *res;

        if (!ptr || !*ptr || !xstrcmp(*ptr, ""))
                return NULL;

        res = *ptr;

        if (!(foo = xstrchr(*ptr, '\n')))
                *ptr += xstrlen(*ptr);
        else {
                *ptr = foo + 1;
                *foo = 0;
                if (xstrlen(res) > 1 && res[xstrlen(res) - 1] == '\r')
                        res[xstrlen(res) - 1] = 0;
        }

        return res;
}

/*
 * ekg_status_label()
 *
 * tworzy etykietê formatki opisuj±cej stan.
 */
const char *ekg_status_label(const char *status, const char *descr, const char *prefix)
{
	static char buf[100];

	snprintf(buf, sizeof(buf), "%s%s%s", (prefix) ? prefix : "", __(status), (descr) ? "_descr" : "");

	return buf;
}

/*
 * ekg_draw_descr()
 *
 * losuje opis dla danego stanu lub pobiera ze zmiennej, lub cokolwiek
 * innego.
 */
char *ekg_draw_descr(const char *status)
{
	const char *value;
	char file[100];
	char var[100];
	variable_t *v;	

	if (!xstrcmp(status, EKG_STATUS_NA) || !xstrcmp(status, EKG_STATUS_INVISIBLE)) {
		xstrcpy(var, ("quit_reason"));
		xstrcpy(file, "quit.reasons");
	} else if (!xstrcmp(status, EKG_STATUS_AVAIL)) {
		xstrcpy(var, ("back_reason"));
		xstrcpy(file, "back.reasons");
	} else {
		snprintf(var, sizeof(var), "%s_reason", status);
		snprintf(file, sizeof(file), "%s.reasons", status);
	}

	if (!(v = variable_find(var)) || v->type != VAR_STR)
		return NULL;

	value = *(char**)(v->ptr);

	if (!value)
		return NULL;

	if (!xstrcmp(value, "*"))
		return random_line(prepare_path(file, 0));

	return xstrdup(value);
}

/* 
 * ekg_update_status()
 *
 * updates our status, if we are on session contact list 
 * 
 */
void ekg_update_status(session_t *session)
{
	userlist_t *u;

        if ((u = userlist_find(session, session->uid))) {
                xfree(u->descr);
                u->descr = xstrdup(session->descr);

                xfree(u->status);
                if (!session_connected_get(session))
                        u->status = xstrdup(EKG_STATUS_NA);
                else
                        u->status = xstrdup(session->status);
		u->blink = 0;
        }

}

/*
 * ekg_sent_message_format()
 *
 * funkcja pomocnicza dla protoko³ów obs³uguj±cych kolorki. z podanego
 * tekstu wycina kolorki i zwraca informacje o formatowaniu tekstu bez
 * kolorków.
 */
uint32_t *ekg_sent_message_format(const char *text)
{
	uint32_t *format, attr;
	char *newtext, *q;
	const char *p, *end;
	int len;

	/* je¶li nie stwierdzono znaków kontrolnych, spadamy */
/*
	if (!xstrpbrk(text, "\x02\x03\x12\x14\x1f"))
		return NULL;
 */

	/* oblicz d³ugo¶æ tekstu bez znaczków formatuj±cych */
	for (p = text, len = 0; *p; p++) {
		if (!xstrchr(("\x02\x03\x12\x14\x1f"), *p))
			len++;
	}

	if (len == xstrlen(text))
		return NULL;
	
	newtext = xmalloc(len + 1);
	format = xmalloc(len * 4);

	end = text + xstrlen(text);

	for (p = text, q = newtext, attr = 0; p < end; ) {
		int j;
			
		if (*p == 18 || *p == 3) {	/* Ctrl-R, Ctrl-C */
			p++;

			if (xisdigit(*p)) {
				int num = atoi(p);
				
				if (num < 0 || num > 15)
					num = 0;

				p++;

				if (xisdigit(*p))
					p++;

				attr &= ~EKG_FORMAT_RGB_MASK;
				attr |= EKG_FORMAT_COLOR;
				attr |= color_map_default[num].r;
				attr |= color_map_default[num].g << 8;
				attr |= color_map_default[num].b << 16;
			} else
				attr &= ~EKG_FORMAT_COLOR;

			continue;
		}

		if (*p == 2) {		/* Ctrl-B */
			attr ^= EKG_FORMAT_BOLD;
			p++;
			continue;
		}

		if (*p == 20) {		/* Ctrl-T */
			attr ^= EKG_FORMAT_ITALIC;
			p++;
			continue;
		}

		if (*p == 31) {		/* Ctrl-_ */
			attr ^= EKG_FORMAT_UNDERLINE;
			p++;
			continue;
		}

		/* zwyk³y znak */
		*q = *p;
		for (j = (int) (q - newtext); j < len; j++)
			format[j] = attr;
		q++;
		p++;
	}

	return format;
}

/*
 * strncasecmp_pl()
 *
 * porównuje dwa ci±gi o okre¶lonej przez n d³ugo¶ci
 * dzia³a analogicznie do xstrncasecmp()
 * obs³uguje polskie znaki
 */

int strncasecmp_pl(const char *cs, const char *ct , size_t count)
{
        register signed char __res = 0;

        while (count) {
                if ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) != 0 || !*cs++)
                        break;
                count--;
        }

        return __res;
}

int strcasecmp_pl(const char *cs, const char *ct)
{
        register signed char __res = 0;

        while ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) == 0 && !*cs++) {
		if (!*cs++)
			return(0);
        }

        return __res;
}


unsigned char pl_to_normal(unsigned char ch)
{
	switch(ch) {
                case 161: /* ¡ */
			return 'A';
		case 177: /* ± */
			return 'a';
                case 198: /* Æ */
			return 'C';
		case 230: /* æ */
			return 'c';
                case 202: /* Ê */
			return 'E';
		case 234: /* ê */
			return 'e';
                case 163: /* £ */
			return 'L';
		case 179: /* ³ */
			return 'l';
                case 209: /* Ñ */
			return 'N';
		case 241: /* ñ */
			return 'n';
                case 211: /* Ó */
			return 'O';
		case 243: /* ó */
			return 'o';
                case 166: /* ¦ */
			return 'S';
		case 182: /* ¶ */
			return 's';
                case 175: /* ¯ */
		case 172: /* ¬ */
			return 'Z';
		case 191: /* ¿ */
		case 188: /* ¼ */
			return 'z';
	}
	return ch;
}

/*
 * tolower_pl()
 *
 * zamienia podany znak na ma³y je¶li to mo¿liwe
 * obs³uguje polskie znaki
 */
int tolower_pl(const unsigned char c) {
        switch(c) {
                case 161: /* ¡ */
                        return 177;
                case 198: /* Æ */
                        return 230;
                case 202: /* Ê */
                        return 234;
                case 163: /* £ */
                        return 179;
                case 209: /* Ñ */
                        return 241;
                case 211: /* Ó */
                        return 243;
                case 166: /* ¦ */
                        return 182;
                case 175: /* ¯ */
                        return 191;
                case 172: /* ¬ */
                        return 188;
                default: /* reszta */
                        return tolower(c);
        }
}


/*
 * str_tolower()
 *
 * zamienia wszystkie znaki ci±gu na ma³e
 * zwraca ci±g po zmianach (wraz z zaalokowan± pamiêci±)
 */
char *str_tolower(const char *text) {
        int i;
        char *tmp;

        tmp = xmalloc(xstrlen(text) + 1);

        for(i=0; i < xstrlen(text); i++)
                tmp[i] = tolower_pl(text[i]);
        tmp[i] = '\0';
        return tmp;
}

/*
 * saprintf()
 *
 * dzia³a jak sprintf() tylko, ¿e wyrzuca wska¼nik
 * do powsta³ego ci±gu
 */
char *saprintf(const char *format, ...)
{
        va_list ap;
        char *res;

        va_start(ap, format);
        res = vsaprintf(format, ap);
        va_end(ap);

        return res;
}

/*
 * xstrtr()
 *
 * zamienia wszystko znaki a na b w podanym ci±gu
 * nie robie jego kopi!
 */
void xstrtr(char *text, char from, char to)
{
	
	if (!text || !from || !to)
		return;

	while (*text++) 
		if (*text == from)
			*text = to;
}

/*
 * ekg_yield_cpu()
 *
 * releases cpu
 * meant to be called while busy-looping
 */
void inline ekg_yield_cpu()
{
#ifdef _POSIX_PRIORITY_SCHEDULING
	sched_yield();
#endif
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
