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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "dynstuff.h"
#include "protocol.h"
#ifndef HAVE_STRLCAT
#  include "compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "xmalloc.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

struct gg_session *sess = NULL;
list_t children = NULL;
list_t aliases = NULL;
list_t events = NULL;
list_t bindings = NULL;
list_t timers = NULL;
list_t conferences = NULL;
list_t buffers = NULL;
list_t searches = NULL;

int in_autoexec = 0;
int in_auto_away = 0;
int config_auto_reconnect = 10;
int reconnect_timer = 0;
int config_auto_away = 600;
int config_auto_save = 0;
time_t last_save = 0;
int config_display_color = 1;
int config_beep = 1;
int config_beep_msg = 1;
int config_beep_chat = 1;
int config_beep_notify = 1;
int config_beep_mail = 1;
int config_display_blinking = 1;
int config_display_pl_chars = 1;
int config_events_delay = 3;
char *config_sound_msg_file = NULL;
char *config_sound_chat_file = NULL;
char *config_sound_sysmsg_file = NULL;
char *config_sound_notify_file = NULL;
char *config_sound_mail_file = NULL;
char *config_sound_app = NULL;
int config_last_sysmsg = 0;
int config_last_sysmsg_changed = 0;
int config_changed = 0;
int config_display_ack = 3;
int config_completion_notify = 1;
int connecting = 0;
time_t last_conn_event = 0;
time_t ekg_started = 0;
int config_display_notify = 1;
char *config_theme = NULL;
char *reg_password = NULL;
char *reg_email = NULL;
int config_dcc = 0;
char *config_dcc_ip = NULL;
char *config_dcc_dir = NULL;
char *home_dir = NULL;
char *config_quit_reason = NULL;
char *config_away_reason = NULL;
char *config_back_reason = NULL;
int config_random_reason = 0;
int config_query_commands = 0;
char *config_proxy = NULL;
char *config_server = NULL;
int quit_message_send = 0;
int registered_today = 0;
int config_protocol = 0;
int batch_mode = 0;
char *batch_line = NULL;
int config_make_window = 2;
char *config_tab_command = NULL;
int config_ctrld_quits = 1;
int config_save_password = 1;
char *config_timestamp = NULL;
int config_display_sent = 1;
int config_sort_windows = 0;
int config_keep_reason = 1;
int server_index = 0;
char *config_audio_device = NULL;
char *config_speech_app = NULL;
int config_encryption = 0;
int config_server_save = 0;
char *config_email = NULL;
int config_time_deviation = 300;
int config_mesg = MESG_DEFAULT;
int config_display_welcome = 1;
int config_auto_back = 0;
char *config_display_color_map = NULL;
int config_windows_save = 0;
char *config_windows_layout = NULL;
char *config_profile = NULL;
char *config_proxy_forwarding = NULL;
char *config_interface = NULL;
int config_reason_limit = 0;
char *config_reason_first = NULL;
char *config_dcc_limit = NULL;
int config_debug = 1;

char *last_search_first_name = NULL;
char *last_search_last_name = NULL;
char *last_search_nickname = NULL;
char *last_search_uid = 0;

int reason_changed = 0;

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
	struct alias a;
	char *params = NULL;

	if (!string || !(cmd = strchr(string, ' ')))
		return -1;

	*cmd++ = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *j = l->data;

		if (!strcasecmp(string, j->name)) {
			if (!append) {
				printq("aliases_exist", string);
				return -1;
			} else {
				list_t l;

				list_add(&j->commands, cmd, strlen(cmd) + 1);
				
				/* przy wielu komendach trudno dope³niaæ, bo wg. której? */
				for (l = commands; l; l = l->next) {
					command_t *c = l->data;

					if (!strcasecmp(c->name, j->name)) {
						xfree(c->params);
						c->params = xstrdup("?");
						break;
					}
				}
			
				printq("aliases_append", string);

				return 0;
			}
		}
	}

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;
		char *tmp = ((*cmd == '/') ? cmd + 1 : cmd);

		if (!strcasecmp(string, c->name) && !c->alias) {
			printq("aliases_command", string);
			return -1;
		}

		if (!strcasecmp(tmp, c->name))
			params = xstrdup(c->params);
	}

	a.name = xstrdup(string);
	a.commands = NULL;
	list_add(&a.commands, cmd, strlen(cmd) + 1);
	list_add(&aliases, &a, sizeof(a));

	command_add(NULL, a.name, ((params) ? params: "?"), cmd_alias_exec, 1, "", "", "");
	
	xfree(params);

	printq("aliases_add", a.name, "");

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

		if (!name || !strcasecmp(a->name, name)) {
			if (name)
				printq("aliases_del", name);
			command_remove(NULL, a->name);
			xfree(a->name);
			list_destroy(a->commands, 1);
			list_remove(&aliases, a, 1);
			removed = 1;
		}
	}

	if (!removed) {
		if (name)
			printq("aliases_noexist", name);
		else
			printq("aliases_list_empty");

		return -1;
	}

	if (removed && !name)
		printq("aliases_del_all");

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
		printq("bind_seq_list_empty");

	for (l = bindings; l; l = l->next) {
		struct binding *b = l->data;

		if (name) {
			if (strcasestr(b->key, name)) {
				printq("bind_seq_list", b->key, b->action);
				found = 1;
			}
			continue;
		}

		if (!b->internal || (all && b->internal))
			printq("bind_seq_list", b->key, b->action);
	}

	if (name && !found) {
		for (l = bindings; l; l = l->next) {
			struct binding *b = l->data;

			if (strcasestr(b->action, name))
				printq("bind_seq_list", b->key, b->action);
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
	struct buffer b;

	if (max_lines && buffer_count(type) >= max_lines) {
		struct buffer *foo = buffers->data;

		xfree(foo->line);
		list_remove(&buffers, foo, 1);
	}

	b.type = type;
	b.target = xstrdup(target);
	b.line = xstrdup(line);

	return ((list_add(&buffers, &b, sizeof(b)) ? 0 : -1));
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

		if (target && b->target && strcmp(target, b->target))
			continue;

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
		
		str = xstrdup(b->line);

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
	
#if 0
/*
 * changed_proxy()
 *
 * funkcja wywo³ywana przy zmianie warto¶ci zmiennej ,,proxy''.
 */
void changed_proxy(const char *var)
{
	char **auth, **userpass = NULL, **hostport = NULL;
	
	gg_proxy_port = 0;
	xfree(gg_proxy_host);
	gg_proxy_host = NULL;
	xfree(gg_proxy_username);
	gg_proxy_username = NULL;
	xfree(gg_proxy_password);
	gg_proxy_password = NULL;
	gg_proxy_enabled = 0;	

	if (!config_proxy)
		return;

	auth = array_make(config_proxy, "@", 0, 0, 0);

	if (!auth[0] || !strcmp(auth[0], "")) {
		array_free(auth);
		return; 
	}
	
	gg_proxy_enabled = 1;

	if (auth[0] && auth[1]) {
		userpass = array_make(auth[0], ":", 0, 0, 0);
		hostport = array_make(auth[1], ":", 0, 0, 0);
	} else
		hostport = array_make(auth[0], ":", 0, 0, 0);
	
	if (userpass && userpass[0] && userpass[1]) {
		gg_proxy_username = xstrdup(userpass[0]);
		gg_proxy_password = xstrdup(userpass[1]);
	}

	gg_proxy_host = xstrdup(hostport[0]);
	gg_proxy_port = (hostport[1]) ? atoi(hostport[1]) : 8080;

	array_free(hostport);
	array_free(userpass);
	array_free(auth);
}
#endif

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
		for(l = s->userlist; l; l = l->next) {
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
	if (!config_theme) {
		theme_free();
		theme_init();
//		ui_event("theme_init");
	} else {
		if (!theme_read(config_theme, 1)) {
			if (!in_autoexec)
				print("theme_loaded", config_theme);
		} else
			print("error_loading_theme", strerror(errno));
	}
}

#if 0
/*
 * changed_xxx_reason()
 *
 * funkcja wywo³ywana przy zmianie domy¶lnych powodów.
 */
void changed_xxx_reason(const char *var)
{
	char *tmp = NULL;

	if (!strcmp(var, "away_reason"))
		tmp = config_away_reason;
	if (!strcmp(var, "back_reason"))
		tmp = config_back_reason;
	if (!strcmp(var, "quit_reason"))
		tmp = config_quit_reason;

	if (!tmp)
		return;

	if (strlen(tmp) > GG_STATUS_DESCR_MAXSIZE)
		print("descr_too_long", itoa(strlen(tmp) - GG_STATUS_DESCR_MAXSIZE));
}
#endif

const char *compile_time()
{
	return __DATE__ " " __TIME__;
}

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
struct conference *conference_add(const char *name, const char *nicklist, int quiet)
{
	struct conference c;
	char **nicks;
	list_t l, sl;
	int i, count;

	memset(&c, 0, sizeof(c));

	if (!name || !nicklist)
		return NULL;

	if (nicklist[0] == ',' || nicklist[strlen(nicklist) - 1] == ',') {
		printq("invalid_params", "chat");
		return NULL;
	}

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
						struct group *g = m->data;

						if (!strcasecmp(gname, g->name)) {
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
		
		if (!strcasecmp(name, cf->name)) {
			printq("conferences_exist", name);

			array_free(nicks);

			return NULL;
		}
	}

#if 0
	for (p = nicks, i = 0; *p; p++) {
		uin_t uin;

		if (!strcmp(*p, ""))
		        continue;

		uin = get_uin(*p);

		list_add(&(c.recipients), &uin, sizeof(uin));
		i++;
	}
#endif

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

		if (!name || !strcasecmp(c->name, name)) {
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
			printq("conferences_list_empty");
		
		return -1;
	}

	if (removed && !name)
		printq("conferences_del_all");

	return 0;
}

/*
 * conference_create()
 *
 * tworzy now± konferencjê z wygenerowan± nazw±.
 *
 *  - nicks - lista ników tak, jak dla polecenia conference.
 */
struct conference *conference_create(const char *nicks)
{
	struct conference *c;
	static int count = 1;
	char *name = saprintf("#conf%d", count);

	if ((c = conference_add(name, nicks, 0)))
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

		if (!strcmp(c->name, name))
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

		if (!strcasecmp(u, uid))
			return 1;
	}

	return 0;

}

/*
 * conference_find_by_uins()
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
#if 0
struct conference *conference_find_by_uins(uin_t from, uin_t *recipients, int count, int quiet) 
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

		gg_debug(GG_DEBUG_MISC, "// conference_find_by_uins(): from=%d, rcpt count=%d, matched=%d, list_count(c->recipients)=%d\n", from, count, matched, list_count(c->recipients));

#if 0
		if (matched == list_count(c->recipients) && matched <= (from == config_uin ? count : count + 1)) {
			string_t new = string_init(NULL);
			int comma = 0;

			if (from != config_uin && !conference_participant(c, from)) {
				list_add(&c->recipients, &from, sizeof(from));

				comma++;
				string_append(new, format_user(from));
			}

			for (i = 0; i < count; i++) {
				if (recipients[i] != config_uin && !conference_participant(c, recipients[i])) {
					list_add(&c->recipients, &recipients[i], sizeof(recipients[0]));
			
					if (comma++)
						string_append(new, ", ");
					string_append(new, format_user(recipients[i]));
				}
			}

			if (strcmp(new->str, "") && !c->ignore)
				printq("conferences_joined", new->str, c->name);
			string_free(new, 1);

			gg_debug(GG_DEBUG_MISC, "// conference_find_by_uins(): matching %s\n", c->name);

			return c;
		}
#endif
	}

	return NULL;
}
#endif

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

//	ui_event("conference_rename", oldname, newname, NULL);
	
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
	const char *tty;
	struct stat s;

	if (!(tty = ttyname(old_stderr)) || stat(tty, &s)) {
		debug("mesg_set() error: %s\n", strerror(errno));
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
}

/*
 * iso_to_ascii()
 *
 * usuwa polskie litery z tekstu.
 *
 *  - c.
 */
void iso_to_ascii(unsigned char *buf)
{
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
 * strip_spaces()
 *
 * pozbywa siê spacji na pocz±tku i koñcu ³añcucha.
 */
char *strip_spaces(char *line)
{
	char *buf;
	
	for (buf = line; xisspace(*buf); buf++);

	while (xisspace(line[strlen(line) - 1]))
		line[strlen(line) - 1] = 0;
	
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

	params[0] = saprintf("%s %s", config_sound_app, sound_path);
	params[1] = NULL;

	res = cmd_exec("exec", (const char**) params, NULL, NULL, 1);

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
	child_t c;

	c.plugin = plugin;
	c.pid = pid;
	c.name = xstrdup(name);
	c.handler = handler;
	c.private = private;
	
	return list_add(&children, &c, sizeof(c));
}

int child_pid_get(child_t *c)
{
	return (c) ? c->pid : -1;
}

const char *child_name_get(child_t *c)
{
	return (c) ? c->name : NULL;
}

void *child_private_get(child_t *c)
{
	return (c) ? c->private : NULL;
}

plugin_t *child_plugin_get(child_t *c)
{
	return (c) ? c->plugin : NULL;
}

child_handler_t child_handler_get(child_t *c)
{
	return (c) ? c->handler : NULL;
}

/*
 * prepare_path()
 *
 * zwraca pe³n± ¶cie¿kê do podanego pliku katalogu ~/.gg/
 *
 *  - filename - nazwa pliku,
 *  - do_mkdir - czy tworzyæ katalog ~/.gg ?
 */
const char *prepare_path(const char *filename, int do_mkdir)
{
	static char path[PATH_MAX];

	if (do_mkdir) {
		if (config_profile) {
			char *cd = xstrdup(config_dir), *tmp;

			if ((tmp = strrchr(cd, '/')))
				*tmp = 0;

			if (mkdir(cd, 0700) && errno != EEXIST) {
				xfree(cd);
				return NULL;
			}

			xfree(cd);
		}

		if (mkdir(config_dir, 0700) && errno != EEXIST)
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

	while (fgets(buf, sizeof(buf), f)) {
		int first = (res) ? 0 : 1;
		size_t new_size = ((res) ? strlen(res) : 0) + strlen(buf) + 1;

		res = xrealloc(res, new_size);
		if (first)
			*res = 0;
		strcpy(res + strlen(res), buf);

		if (strchr(buf, '\n'))
			break;
	}

	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\n')
		res[strlen(res) - 1] = 0;
	if (res && strlen(res) > 0 && res[strlen(res) - 1] == '\r')
		res[strlen(res) - 1] = 0;

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
	strftime(buf, sizeof(buf), format, tm);

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

	if (!strcasecmp(value, "on") || !strcasecmp(value, "true") || !strcasecmp(value, "yes") || !strcasecmp(value, "tak") || !strcmp(value, "1"))
		return 1;

	if (!strcasecmp(value, "off") || !strcasecmp(value, "false") || !strcasecmp(value, "no") || !strcasecmp(value, "nie") || !strcmp(value, "0"))
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
struct timer *timer_add(plugin_t *plugin, const char *name, time_t period, int persist, void (*function)(int, void *), void *data)
{
	struct timer t;
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

				if (!strcmp(tt->name, itoa(i))) {
					gotit = 1;
					break;
				}
			}

			if (!gotit)
				name = itoa(i);
		}
	}

	memset(&t, 0, sizeof(t));

	gettimeofday(&tv, &tz);
	tv.tv_sec += period;
	memcpy(&t.ends, &tv, sizeof(tv));
	t.name = xstrdup(name);
	t.persist = persist;
	t.function = function;
	t.data = data;
	t.plugin = plugin;

	return list_add(&timers, &t, sizeof(t));
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

		if (t->plugin == plugin && !strcasecmp(name, t->name)) {
			xfree(t->name);
			list_remove(&timers, t, 1);
			removed = 1;
		}
	}

	return ((removed) ? 0 : -1);
}

/*
 * timer_handle_command()
 *
 * obs³uga timera wywo³uj±cego komendê.
 */
void timer_handle_command(int destroy, void *data)
{
	if (!destroy)
		command_exec(NULL, NULL, (char*) data, 0);
	else
		xfree(data);
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

	if (start > strlen(str))
		start = strlen(str);

	if (length == -1)
		length = strlen(str) - start;

	if (length < 1)
		return xstrdup("");

	if (length > strlen(str) - start)
		length = strlen(str) - start;
	
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
 * strcasestr()
 *
 * robi to samo co strstr() tyle ¿e bez zwracania uwagi na wielko¶æ
 * znaków.
 */
char *strcasestr(const char *haystack, const char *needle)
{
	int i, hlen = strlen(haystack), nlen = strlen(needle);

	for (i = 0; i <= hlen - nlen; i++) {
		if (!strncasecmp(haystack + i, needle, nlen))
			return (char*) (haystack + i);
	}

	return NULL;
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
	pid_t pid;

	if (!config_speech_app || !str || !strcmp(str, ""))
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
char *base64_encode(const char *buf)
{
	char *out, *res;
	int i = 0, j = 0, k = 0, len = strlen(buf);
	
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

	save = res = xcalloc(1, (strlen(buf) / 4 + 1) * 3 + 2);

	end = buf + strlen(buf);

	while (*buf && buf < end) {
		if (*buf == '\r' || *buf == '\n') {
			buf++;
			continue;
		}
		if (!(foo = strchr(base64_charset, *buf)))
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

        if (!ptr || !*ptr || !strcmp(*ptr, ""))
                return NULL;

        res = *ptr;

        if (!(foo = strchr(*ptr, '\n')))
                *ptr += strlen(*ptr);
        else {
                *ptr = foo + 1;
                *foo = 0;
                if (strlen(res) > 1 && res[strlen(res) - 1] == '\r')
                        res[strlen(res) - 1] = 0;
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

	snprintf(buf, sizeof(buf), "%s%s%s", (prefix) ? prefix : "", status, (descr) ? "_descr" : "");

	return buf;
}

/*
 * strtrim()
 *
 * usuwa spacje z pocz±tku i koñca tekstu.
 *
 *  - s - ci±g znaków.
 *
 * 0/-1
 */
int strtrim(char *s)
{
	char *t;

	if (!s)
		return -1;
	
	while (xisspace(s[strlen(s) - 1]))
		s[strlen(s) - 1] = 0;

	for (t = s; xisspace(*t); t++)
		;
	
	memmove(s, t, strlen(t) + 1);

	return 0;
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
	char var[100], file[100];
	variable_t *v;	

	if (!strcmp(status, EKG_STATUS_NA) || !strcmp(status, EKG_STATUS_INVISIBLE)) {
		strcpy(var, "quit_reason");
		strcpy(file, "quit.reasons");
	} else if (!strcmp(status, EKG_STATUS_AVAIL)) {
		strcpy(var, "back_reason");
		strcpy(file, "back.reasons");
	} else {
		snprintf(var, sizeof(var), "%s_reason", status);
		snprintf(file, sizeof(file), "%s.reasons", status);
	}

	if (!(v = variable_find(var)) || v->type == VAR_STR)
		return NULL;

	value = *(char**)(v->ptr);

	if (!value)
		return NULL;

	if (!strcmp(value, "*"))
		return random_line(prepare_path(file, 0));

	return xstrdup(value);
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
//	if (!strpbrk(text, "\x02\x03\x12\x14\x1f"))
//		return NULL;

	/* oblicz d³ugo¶æ tekstu bez znaczków formatuj±cych */
	for (p = text, len = 0; *p; p++) {
		if (!strchr("\x02\x03\x12\x14\x1f", *p))
			len++;
	}

	if (len == strlen(text))
		return NULL;
	
	newtext = xmalloc(len + 1);
	format = xmalloc(len * 4);

	end = text + strlen(text);

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
 * porównuje dwa ci±gi o okre¶lonej przez n d³ugo¶ci
 * dzia³a analogicznie do strncasecmp()
 * obs³uguje polskie znaki
 */

int strncasecmp_pl(const char * cs,const char * ct,size_t count)
{
        register signed char __res = 0;

        while (count) {
                if ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) != 0 || !*cs++)
                        break;
                count--;
        }

        return __res;
}


/*
 * zamienia podany znak na ma³y je¶li to mo¿liwe
 * obs³uguje polskie znaki
 */
int tolower_pl(const unsigned char c) {
        switch(c) {
                case 161: // ¡
                        return 177;
                case 198: // Æ
                        return 230;
                case 202: // Ê
                        return 234;
                case 163: // £
                        return 179;
                case 209: // Ñ
                        return 241;
                case 211: // Ó
                        return 243;
                case 166: // ¦
                        return 182;
                case 175: // ¯
                        return 191;
                case 172: // ¬
                        return 188;
                default: //reszta
                        return tolower(c);
        }
}


/*
 * zamienia wszystkie znaki ci±gu na ma³e
 * zwraca ci±g po zmianach (wraz z zaalokowan± pamiêci±)
 */
char *str_tolower(const char *text) {
        int i;
        char *tmp;

        tmp = xmalloc(strlen(text) + 1);

        for(i=0; i < strlen(text); i++)
                tmp[i] = tolower_pl(text[i]);
        tmp[i] = '\0';
        return tmp;
}

/*
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

