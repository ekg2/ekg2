/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Wojciech Bojdo³ <wojboj@htc.net.pl>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *                          Kuba Kowalski <qbq@kofeina.net>
 *			    Piotr Kupisiewicz <deli@rzepaknet.us>
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commands.h"
#include "configfile.h"
#include "dynstuff.h"
#include "log.h"
#include "msgqueue.h"
#include "protocol.h"
#include "sessions.h"
#ifndef HAVE_STRLCAT
#  include "compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "stuff.h"
#include "themes.h"
#include "vars.h"
#include "userlist.h"
#include "windows.h"
#include "xmalloc.h"

char *send_nicks[SEND_NICKS_MAX] = { NULL };
int send_nicks_count = 0, send_nicks_index = 0;
static int quit_command = 0;

list_t commands = NULL;

/*
 * match_arg()
 *
 * sprawdza, czy dany argument funkcji pasuje do podanego.
 */
int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen)
{
	if (!arg || *arg != '-')
		return 0;

	arg++;

	if (*arg == '-') {
		int len = xstrlen(++arg);

		if (longoptlen > len)
			len = longoptlen;

		return !strncmp(arg, longopt, len);
	}
	
	return (*arg == shortopt) && (*(arg + 1) == 0);
}

/*
 * tabnick_add()
 *
 * dodaje do listy nicków dope³nianych automagicznie tabem.
 */
void tabnick_add(const char *nick)
{
	int i;

	for (i = 0; i < send_nicks_count; i++)
		if (send_nicks[i] && !xstrcmp(nick, send_nicks[i])) {
			tabnick_remove(nick);
			break;
		}

	if (send_nicks_count == SEND_NICKS_MAX) {
		xfree(send_nicks[SEND_NICKS_MAX - 1]);
		send_nicks_count--;
	}

	for (i = send_nicks_count; i > 0; i--)
		send_nicks[i] = send_nicks[i - 1];

	if (send_nicks_count != SEND_NICKS_MAX)
		send_nicks_count++;
	
	send_nicks[0] = xstrdup(nick);
}

/*
 * tabnick_remove()
 *
 * usuwa z listy dope³nianych automagicznie tabem.
 */
void tabnick_remove(const char *nick)
{
	int i, j;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && !xstrcmp(send_nicks[i], nick)) {
			xfree(send_nicks[i]);

			for (j = i + 1; j < send_nicks_count; j++)
				send_nicks[j - 1] = send_nicks[j];

			send_nicks_count--;
			send_nicks[send_nicks_count] = NULL;

			break;
		}
	}
}

/*
 * tabnick_flush()
 *
 * czy¶ci listê nicków dope³nianych tabem.
 */
void tabnick_flush()
{
	int i;

	for (i = 0; i < send_nicks_count; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}

	send_nicks_count = 0;
	send_nicks_index = 0;
}

COMMAND(cmd_tabclear)
{
	int i;

	if (!params[0]) {
		tabnick_flush();
		return 0;
	}

	if (match_arg(params[0], 'o', "offline", 2)) {
		for (i = 0; i < send_nicks_count; i++) {
			userlist_t *u = NULL;

			if (send_nicks[i])
				u = userlist_find(session, send_nicks[i]);

			if (!u || xstrcasecmp(u->status, EKG_STATUS_NA))
				continue;

			tabnick_remove(send_nicks[i]);
		}

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_modify)
{
	userlist_t *u;
	char **argv = NULL;
	int i, res = 0, modified = 0;

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!(u = userlist_find(session, params[0]))) {
		printq("user_not_found", params[0]);
		return -1;
	}

	argv = array_make(params[1], " \t", 0, 1, 1);

	for (i = 0; argv[i]; i++) {
		
		if (match_arg(argv[i], 'f', "first", 2) && argv[i + 1]) {
			xfree(u->first_name);
			u->first_name = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (match_arg(argv[i], 'l', "last", 2) && argv[i + 1]) {
			xfree(u->last_name);
			u->last_name = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (match_arg(argv[i], 'n', "nickname", 2) && argv[i + 1]) {
			char *tmp1, *tmp2;

			tmp1 = xstrdup(u->nickname);
			tmp2 = xstrdup(argv[++i]);
			query_emit(NULL, "userlist-renamed", &tmp1, &tmp2);
			xfree(tmp1);
				
			xfree(u->nickname);
			u->nickname = tmp2;
			
			modified = 1;
			continue;
		}
		
		if ((match_arg(argv[i], 'p', "phone", 2) || match_arg(argv[i], 'm', "mobile", 2)) && argv[i + 1]) {
			xfree(u->mobile);
			u->mobile = xstrdup(argv[++i]);
			modified = 1;
			continue;
		}
		
		if (match_arg(argv[i], 'g', "group", 2) && argv[i + 1]) {
			char **tmp = array_make(argv[++i], ",", 0, 1, 1);
			int x, off;	/* je¶li zaczyna siê od '@', pomijamy pierwszy znak */
			
			for (x = 0; tmp[x]; x++)
				switch (*tmp[x]) {
					case '-':
						off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

						if (ekg_group_member(u, tmp[x] + 1 + off)) {
							ekg_group_remove(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							printq("group_member_not_yet", format_user(session, u->uid), tmp[x] + 1);
							if (!modified)
								modified = -1;
						}
						break;
					case '+':
						off = (tmp[x][1] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

						if (!ekg_group_member(u, tmp[x] + 1 + off)) {
							ekg_group_add(u, tmp[x] + 1 + off);
							modified = 1;
						} else {
							printq("group_member_already", format_user(session, u->uid), tmp[x] + 1);
							if (!modified)
								modified = -1;
						}
						break;
					default:
						off = (tmp[x][0] == '@' && xstrlen(tmp[x]) > 1) ? 1 : 0;

						if (!ekg_group_member(u, tmp[x] + off)) {
							ekg_group_add(u, tmp[x] + off);
							modified = 1;
						} else {
							printq("group_member_already", format_user(session, u->uid), tmp[x]);
							if (!modified)
								modified = -1;
						}
				}

			array_free(tmp);
			continue;
		}
		
		if (match_arg(argv[i], 'u', "uid", 2) && argv[i + 1]) {
			userlist_t *existing;
			char *tmp;

			if (!valid_uid(argv[i + 1])) {
				printq("invalid_uid");
				array_free(argv);
				return -1;
			}

			if ((existing = userlist_find(session, argv[i + 1]))) {
				if (existing->nickname) {
					printq("user_exists_other", argv[i], format_user(session, existing->uid));
					array_free(argv);
					return -1;
				} else {
					char *egroups = group_to_string(existing->groups, 1, 0);
					
					if (egroups) {
						char **arr = array_make(egroups, ",", 0, 0, 0);
						int i;

						for (i = 0; arr[i]; i++)
							ekg_group_add(u, arr[i]);

						array_free(arr);
					}

					userlist_remove(session, existing);
				}
			}

			tmp = xstrdup(u->uid);
			query_emit(NULL, "userlist-removed", &tmp);
			xfree(tmp);

			userlist_clear_status(session, u->uid);

			tmp = xstrdup(argv[i + 1]);
			query_emit(NULL, "userlist-added", &tmp);

			xfree(u->uid);
			u->uid = tmp;

			modified = 1;
			continue;
		}

#if 0
		if (match_arg(argv[i], 'o', "offline", 2)) {
			gg_remove_notify_ex(sess, u->uin, userlist_type(u));
			group_add(u, "__offline");
			printq("modify_offline", format_user(u->uin));
			modified = 2;
			gg_add_notify_ex(sess, u->uin, userlist_type(u));
			continue;
		}

		if (match_arg(argv[i], 'O', "online", 2)) {
			gg_remove_notify_ex(sess, u->uin, userlist_type(u));
			group_remove(u, "__offline");
			printq("modify_online", format_user(u->uin));
			modified = 2;
			gg_add_notify_ex(sess, u->uin, userlist_type(u));
			continue;
		}
#endif
		
		printq("invalid_params", name);
		array_free(argv);
		return -1;
	}

	if (xstrcasecmp(name, "add")) {
		switch (modified) {
			case 0:
				printq("not_enough_params", name);
				res = -1;
				break;
			case 1:
				printq("modify_done", params[0]);
			case 2:
				config_changed = 1;
				break;
		}
	} else
		config_changed = 1;

	array_free(argv);

	return res;
}

COMMAND(cmd_add)
{
	int params_free = 0;	/* zmienili¶my params[] i trzeba zwolniæ */
	int result = 0;
	userlist_t *u = NULL;

	if (!session_current) {
		return -1;
	}
	
	if (params[0] && !params[1] && params[0][0] != '-' && window_current->target) {
		const char *name = params[0], *s1 = params[1], *s2 = params[2];
		params_free = 1;
		params = xmalloc(4 * sizeof(char *));
		params[0] = window_current->target;
		params[1] = name;
		params[2] = (s1) ? saprintf("%s %s", s1, ((s2) ? s2 : "")) : NULL;
		params[3] = NULL;
	}

	if (params[0] && match_arg(params[0], 'f', "find", 2)) {
		int nonick = 0;
		char *nickname, *tmp;

		if (!last_search_uid || !last_search_nickname) {
			printq("search_no_last");
			return -1;
		}

		tmp = strip_spaces(last_search_nickname);

		if ((nonick = !xstrcmp(tmp, "")) && !params[1]) {
			printq("search_no_last_nickname");
			return -1;
		}

		if (nonick || params[1])
			nickname = (char *) params[1];
		else
			nickname = tmp;

		params_free = 1;

		params = xmalloc(4 * sizeof(char *));
		params[0] = last_search_uid;
		params[1] = nickname;
		params[2] = saprintf("-f \"%s\" -l \"%s\"", ((last_search_first_name) ? last_search_first_name : ""), ((last_search_last_name) ? last_search_last_name : ""));
		params[3] = NULL;
	}

	if (!params[0] || !params[1]) {
		printq("not_enough_params", name);
		result = -1;
		goto cleanup;
	}

	if (!plugin_find_uid(params[0])) {
		printq("invalid_uid");
		result = -1;
		goto cleanup;
	}

	if (!valid_nick(params[1])) {
		printq("invalid_nick");
		result = -1;
		goto cleanup;
	}

	if (((u = userlist_find(session, params[0])) && u->nickname) || ((u = userlist_find(session, params[1])) && u->nickname)) {
		if (!xstrcasecmp(params[1], u->nickname) && !xstrcasecmp(params[0], u->uid))
			printq("user_exists", params[1]);
		else
			printq("user_exists_other", params[1], format_user(session, u->uid));

		result = -1;
		goto cleanup;
	}

	/* kto¶ by³ tylko ignorowany/blokowany, nadajmy mu nazwê */
	if (u) {
		xfree(u->nickname);
		u->nickname = xstrdup(params[1]);
	}

	if (u || userlist_add(session, params[0], params[1])) {
		char *uid = xstrdup(params[0]);

		query_emit(NULL, "userlist-added", &uid, &params[1]);
                query_emit(NULL, "add-notify", &session_current->uid, &uid);
                xfree(uid);

		printq("user_added", params[1]);

		tabnick_remove(params[0]);
		config_changed = 1;

#if 0
		if (uid == session_uid_get(session_current)) {
			update_status();
			update_status_myip();
		}
#endif
	}

	if (params[2])
		cmd_modify("add", &params[1], session, NULL, quiet);

cleanup:
	if (params_free) {
		xfree((char*) params[2]);
		xfree(params);
	}

	return result;
}

COMMAND(cmd_alias)
{
	if (match_arg(params[0], 'a', "add", 2)) {
		if (!params[1] || !xstrchr(params[1], ' ')) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!alias_add(params[1], quiet, 0)) {
			config_changed = 1;
			return 0;
		}

		return -1;
	}

	if (match_arg(params[0], 'A', "append", 2)) {
		if (!params[1] || !xstrchr(params[1], ' ')) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!alias_add(params[1], quiet, 1)) {
			config_changed = 1;
			return 0;
		}

		return -1;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		int ret;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*"))
			ret = alias_remove(NULL, quiet);
		else
			ret = alias_remove(params[1], quiet);

		if (!ret) {
			config_changed = 1;
			return 0;
		}

		return -1;
	}
	
	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		list_t l;
		int count = 0;
		const char *aname = NULL;

		if (params[0] && match_arg(params[0], 'l', "list", 2))
			aname = params[1];
		else if (params[0])
			aname = params[0];

		for (l = aliases; l; l = l->next) {
			struct alias *a = l->data;
			list_t m;
			int first = 1, i;
			char *tmp;
			
			if (aname && xstrcasecmp(aname, a->name))
				continue;

			tmp = xcalloc(xstrlen(a->name) + 1, 1);

			for (i = 0; i < xstrlen(a->name); i++)
				xstrcat(tmp, " ");

			for (m = a->commands; m; m = m->next) {
				printq((first) ? "aliases_list" : "aliases_list_next", a->name, (char*) m->data, tmp);
				first = 0;
				count++;
			}

			xfree(tmp);
		}

		if (!count) {
			if (aname) {
				printq("aliases_noexist", aname);
				return -1;
			}

			printq("aliases_list_empty");
		}

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_status)
{
        struct tm *t;
        time_t n;
        int now_days;
	char buf1[100];
	session_t *s = NULL;

	if (!params[0] && session)
		query_emit(plugin_find_uid(session->uid), "status-show", &session->uid);
	else if (!(s = session_find(params[0]))) {
		printq("invalid_uid", params[0]);
		return -1;
	} else
		query_emit(plugin_find_uid(s->uid), "status-show", &s->uid);

        n = time(NULL);
        t = localtime(&n);
        now_days = t->tm_yday;

        t = localtime(&ekg_started);
        strftime(buf1, sizeof(buf1), format_find((t->tm_yday == now_days) ? "show_status_ekg_started_today" : "show_status_ekg_started"), t);

        printq("show_status_ekg_started_since", buf1);
	return 0;
}

COMMAND(cmd_del)
{
	userlist_t *u;
	char *tmp;
	int del_all = ((params[0] && !xstrcmp(params[0], "*")) ? 1 : 0);

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (del_all) {
		list_t l;
		for (l = session->userlist; l; ) {
			userlist_t *u = l->data;
			char *tmp;
	
			l = l->next;

			tmp = xstrdup(u->uid);
			query_emit(NULL, "userlist-removed", &tmp);
			xfree(tmp);

			userlist_remove(session, u);
		}

		printq("user_cleared_list");
		tabnick_flush();
		config_changed = 1;

		return 0;
	}

	if (!(u = userlist_find(session, params[0])) || !u->nickname) {
		printq("user_not_found", params[0]);
		return -1;
	}

	tmp = xstrdup(u->uid);
	query_emit(NULL, "userlist-removed", &params[0], &tmp);
	query_emit(NULL, "remove-notify", &session_current->uid, &tmp);

        printq("user_deleted", params[0]);
	xfree(tmp);

	tabnick_remove(u->uid);
	tabnick_remove(u->nickname);

	config_changed = 1;

	userlist_remove(session, u);
	
	return 0;
}

typedef struct {
	char *target;		/* je¶li wysy³amy wynik, to do kogo? */
	char *session;		/* jaka sesja? */
	int quiet;		/* czy mamy byæ cicho? */
	string_t buf;		/* je¶li buforujemy, to tutaj */
} cmd_exec_info_t;

void cmd_exec_watch_handler(int type, int fd, const char *line, cmd_exec_info_t *i)
{
	int quiet = (i) ? i->quiet : 0;

	if (!i)
		return;

	if (type == 1) {
		if (i->buf) {
			string_insert(i->buf, 0, "/ ");
			command_exec(i->target, session_find(i->session), i->buf->str, quiet);
			string_free(i->buf, 1);
		}
		xfree(i->target);
		xfree(i->session);
		xfree(i);
		return;
	}

	if (!i->target) {
		printq("exec", line);
		return;
	}

	if (i->buf) {
		string_append(i->buf, line);
		string_append(i->buf, "\r\n");
	} else {
		char *tmp = saprintf("/ %s", line);
		command_exec(i->target, session_find(i->session), tmp, quiet);
		xfree(tmp);
	}
}

void cmd_exec_child_handler(child_t *c, int pid, const char *name, int status, void *priv)
{
	int quiet = (name && name[0] == '^');

	printq("process_exit", itoa(pid), name, itoa(status));
}

COMMAND(cmd_exec)
{
	list_t l;
	int pid;

	if (params[0]) {
		int fd[2] = { 0, 0 }, buf = 0, add_commandline = 0;
		const char *command = params[0], *target = NULL;
		char **args = NULL;
		cmd_exec_info_t *i;
		watch_t *w;

		if (params[0][0] == '-') {
			int big_match = 0;
			args = array_make(params[0], " \t", 3, 1, 1);

			if (match_arg(args[0], 'M', "MSG", 2) || (buf = match_arg(args[0], 'B', "BMSG", 2)))
				big_match = add_commandline = 1;

			if (big_match || match_arg(args[0], 'm', "msg", 2) || (buf = match_arg(args[0], 'b', "bmsg", 2))) {
				const char *uid;

				if (!args[1] || !args[2]) {
					printq("not_enough_params", name);
					array_free(args);
					return -1;
				}

				if (!(uid = get_uid(session, args[1]))) {
					printq("user_not_found", args[1]);
					array_free(args);
					return -1;
				}

				target = uid;
				command = args[2];
			} else {
				printq("invalid_params", name);
				array_free(args);
				return -1;
			}
		} 

		if (pipe(fd) == -1) {
			printq("exec_error", strerror(errno));
			array_free(args);
			return -1;
		}

		if (!(pid = fork())) {
			dup2(open("/dev/null", O_RDONLY), 0);
			dup2(fd[1], 1);
			dup2(fd[1], 2);

			close(fd[0]);
			close(fd[1]);

			execl("/bin/sh", "sh", "-c", (command[0] == '^') ? command + 1 : command, (void *) NULL);

			exit(1);
		}

		if (pid < 0) {
			printq("exec_error", strerror(errno));
			array_free(args);
			return -1;
		}
	
		i = xmalloc(sizeof(cmd_exec_info_t));
		
		i->quiet = quiet;
		i->target = xstrdup(target);
		i->session = xstrdup(session_uid_get(session));

		if (buf)
			i->buf = string_init(NULL);

		w = watch_add(NULL, fd[0], WATCH_READ_LINE, 1, cmd_exec_watch_handler, i);

		if (add_commandline) {
			char *tmp = format_string(format_find("exec_prompt"), ((command[0] == '^') ? command + 1 : command));
			string_append(w->buf, tmp);
			xfree(tmp);
		}

		fcntl(fd[0], F_SETFL, O_NONBLOCK);

		close(fd[1]);
		
		child_add(NULL, pid, command, cmd_exec_child_handler, NULL);
			
		array_free(args);
	} else {
		for (l = children; l; l = l->next) {
			child_t *c = l->data;
			
			printq("process", itoa(c->pid), (c->name) ? c->name : "?");
		}

		if (!children) {
			printq("no_processes");
			return -1;
		}
	}

	return 0;
}

COMMAND(cmd_help)
{
	list_t l;
	
	if (params[0]) {
		const char *p = (params[0][0] == '/' && xstrlen(params[0]) > 1) ? params[0] + 1 : params[0];

		if (!xstrcasecmp(p, "set") && params[1]) {
			if (!quiet)
				variable_help(params[1]);
			return 0;
		}
			
		for (l = commands; l; l = l->next) {
			command_t *c = l->data;
			
			if (!xstrcasecmp(c->name, p) && c->alias) {
				printq("help_alias", p);
				return -1;
			}

			if (!xstrcasecmp(c->name, p) && !c->alias) {
			    	char *tmp = NULL;

				if (xstrstr(c->brief_help, "%"))
				    	tmp = format_string(c->brief_help);
				
				printq("help", c->name, c->params_help, tmp ? tmp : c->brief_help, "");
				xfree(tmp);

				if (c->long_help && xstrcmp(c->long_help, "")) {
					char *foo, *tmp, *bar = format_string(c->long_help);

					foo = bar;

					while ((tmp = split_line(&foo)))
						printq("help_more", tmp);

					xfree(bar);
				}

				return 0;
			}
		}
	}

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		if (xisalnum(*c->name) && !c->alias) {
		    	char *blah = NULL;

			if (xstrstr(c->brief_help, "%"))
			    	blah = format_string(c->brief_help);
	
			printq("help", c->name, c->params_help, blah ? blah : c->brief_help, "");
			xfree(blah);
		}
	}

	printq("help_footer");
	printq("help_quick");

	return 0;
}

COMMAND(cmd_ignore)
{
	char *tmp;
	const char *uid;

	if (*name == 'i' || *name == 'I') {
		int flags;

		if (!params[0]) {
			list_t l;
			int i = 0;
			for (l = session->userlist; l; l = l->next) {
				userlist_t *u = l->data;
				int level;

				if (!(level = ignored_check(session, u->uid)))
					continue;

				i = 1;

				printq("ignored_list", format_user(session, u->uid), ignore_format(level));
			}

			if (!i)
				printq("ignored_list_empty");

			return 0;
		}

		if (params[0][0] == '#') {
			int res;
			
			tmp = saprintf("/conference --ignore %s", params[0]);
			res = command_exec(NULL, NULL, tmp, quiet);
			xfree(tmp);
			return res;
		}

		if (params[1]) {
			flags = ignore_flags(params[1]);

			if (!flags) {
				printq("invalid_params", name);
				return -1;
			}

		} else
			flags = IGNORE_ALL;

		if (!(uid = get_uid(session, params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}

		if (!ignored_add(session, uid, flags)) {
			printq("ignored_added", format_user(session, uid));
			config_changed = 1;
		} else {
			printq("ignored_exist", format_user(session, uid));
			return -1;
		}

	} else {
		int unignore_all = ((params[0] && !xstrcmp(params[0], "*")) ? 1 : 0);
		int level;

		if (!params[0]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[0][0] == '#') {
			int res;
			
			tmp = saprintf("/conference --unignore %s", params[0]);
			res = command_exec(NULL, NULL, tmp, quiet);
			xfree(tmp);
			return res;
		}
		
		if (!unignore_all && !(uid = get_uid(session, params[0]))) {
			printq("user_not_found", params[0]);
			return -1;
		}

		if (unignore_all) {
			list_t l;
			int x = 0;
			
			for (l = session->userlist; l; ) {
				userlist_t *u = l->data;

				l = l->next;

				if (!ignored_remove(session, u->uid))
					x = 1;

				level = ignored_check(session, u->uid);
			}

			if (x) {
				printq("ignored_deleted_all");
				config_changed = 1;
			} else {
				printq("ignored_list_empty");
				return -1;
			}
			
			return 0;
		}

		level = ignored_check(session, uid);
		
		if (!ignored_remove(session, uid)) {
			printq("ignored_deleted", format_user(session, uid));
			config_changed = 1;
		} else {
			printq("error_not_ignored", format_user(session, uid));
			return -1;
		}
	
	}

	return 0;
}

COMMAND(cmd_list)
{
	list_t l;
	int count = 0, show_all = 1, show_away = 0, show_active = 0, show_inactive = 0, show_invisible = 0, show_descr = 0, show_blocked = 0, show_offline = 0, j;
	char **argv = NULL, *show_group = NULL, *ip_str, *last_ip_str;
	const char *tmp;

	/* sprawdzamy czy session istnieje - je¶li nie to nie mamy po co robiæ co¶ dalej ... */
        if(!session) {
                if(session_current)
                        session = session_current;
                else
                        return -1;
        }

	if (params[0] && *params[0] != '-') {
		char *status, *groups, *last_status;
		const char *group = params[0];
		userlist_t *u;
		int invert = 0;
		
		/* list !@grupa */
		if (group[0] == '!' && group[1] == '@') {
			group++;
			invert = 1;
		}

		/* list @grupa */
		if (group[0] == '@' && xstrlen(group) > 1) {
			string_t members = string_init(NULL);
			char *__group;
			int count = 0;
			
			for (l = session->userlist; l; l = l->next) {
				u = l->data;

				if (u->groups || invert) {
					if ((!invert && ekg_group_member(u, group + 1)) || (invert && !ekg_group_member(u, group + 1))) {
						if (count++)
							string_append(members, ", ");
						string_append(members, u->nickname);
					}
				}
			}
			
			__group = saprintf("%s%s", ((invert) ? "!" : ""), group + 1);

			if (count)
				printq("group_members", __group, members->str);
			else
				printq("group_empty", __group);

			xfree(__group);

			string_free(members, 1);

			return 0;
		}

		if (!(u = userlist_find(session, params[0])) || !u->nickname) {
			printq("user_not_found", params[0]);
			return -1;
		}

		/* list <alias> [opcje] */
		if (params[1])
			return cmd_modify("list", params, NULL, NULL, quiet);

		status = format_string(format_find(ekg_status_label(u->status, u->descr, "user_info_")), (u->first_name) ? u->first_name : u->nickname, u->descr);

                last_status = format_string(format_find(ekg_status_label(u->last_status, u->last_descr, "user_info_")), (u->first_name) ? u->first_name : u->nickname, u->last_descr);


		groups = group_to_string(u->groups, 0, 1);

		ip_str = saprintf("%s:%s", inet_ntoa(*((struct in_addr*) &u->ip)), itoa(u->port));

		last_ip_str = saprintf("%s:%s", inet_ntoa(*((struct in_addr*) &u->last_ip)), itoa(u->last_port));

		printq("user_info_header", u->nickname, u->uid);
		if (u->nickname && xstrcmp(u->nickname, u->nickname)) 
			printq("user_info_nickname", u->nickname);

		if (u->first_name && xstrcmp(u->first_name, "") && u->last_name && u->last_name && xstrcmp(u->last_name, ""))
			printq("user_info_name", u->first_name, u->last_name);
		if (u->first_name && xstrcmp(u->first_name, "") && (!u->last_name || !xstrcmp(u->last_name, "")))
			printq("user_info_name", u->first_name, "");
		if ((!u->first_name || !xstrcmp(u->first_name, "")) && u->last_name && xstrcmp(u->last_name, ""))
			printq("user_info_name", u->last_name, "");

		printq("user_info_status", status);
                if (u->status_time) {
		        struct tm *status_time;
			char buf[100];		

			status_time = localtime(&(u->status_time));
	        	strftime(buf, sizeof(buf), format_find("user_info_status_time_format") ,status_time);

			printq("user_info_status_time", buf);
		}

		if (u->last_status)
			printq("user_info_last_status", last_status);

		if (ekg_group_member(u, "__blocked"))
			printq("user_info_block", ((u->first_name) ? u->first_name : u->nickname));
		if (ekg_group_member(u, "__offline"))
			printq("user_info_offline", ((u->first_name) ? u->first_name : u->nickname));
		if (u->port == 2)
			printq("user_info_not_in_contacts");
		if (u->port == 1)
			printq("user_info_firewalled");
		
		if (u->ip)
			printq("user_info_ip", ip_str);
                else if (u->last_ip) {
                        printq("user_info_last_ip", last_ip_str);
		}

		if (u->mobile && xstrcmp(u->mobile, ""))
			printq("user_info_mobile", u->mobile);
		if (xstrcmp(groups, ""))
			printq("user_info_groups", groups);
		if (!xstrcasecmp(u->status, EKG_STATUS_NA) || !xstrcasecmp(u->status, EKG_STATUS_INVISIBLE)) {
			char buf[100];
			struct tm *last_seen_time;
			
			if (u->last_seen) {
				last_seen_time = localtime(&(u->last_seen));
				strftime(buf, sizeof(buf), format_find("user_info_last_seen_time"), last_seen_time);
				printq("user_info_last_seen", buf);
			} else
				printq("user_info_never_seen");
		}
			
		printq("user_info_footer", u->nickname, u->uid);
		
		xfree(ip_str);
		xfree(groups);
		xfree(status);
		xfree(last_ip_str);
		xfree(last_status);
		return 0;
	}

	/* list --active | --away | --inactive | --invisible | --description | --member | --blocked | --offline */
	for (j = 0; params[j]; j++) {
		int i;

		argv = array_make(params[j], " \t", 0, 1, 1);

	 	for (i = 0; argv[i]; i++) {
			if (match_arg(argv[i], 'a', "active", 2)) {
				show_all = 0;
				show_active = 1;
			}
				
			if (match_arg(argv[i], 'i', "inactive", 2) || match_arg(argv[i], 'n', "notavail", 2)) {
				show_all = 0;
				show_inactive = 1;
			}
			
			if (match_arg(argv[i], 'A', "away", 2)) {
				show_all = 0;
				show_away = 1;
			}
			
			if (match_arg(argv[i], 'I', "invisible", 2)) {
				show_all = 0;
				show_invisible = 1;
			}

			if (match_arg(argv[i], 'B', "blocked", 2)) {
				show_all = 0;
				show_blocked = 1;
			}

			if (match_arg(argv[i], 'o', "offline", 2)) {
				show_all = 0;
				show_offline = 1;
			}

			if (match_arg(argv[i], 'm', "member", 2)) {
				if (j && argv[i+1]) {
					int off = (argv[i+1][0] == '@' && xstrlen(argv[i+1]) > 1) ? 1 : 0;

					show_group = xstrdup(argv[i+1] + off);
				} else
					if (params[i+1]) {
						char **tmp = array_make(params[i+1], " \t", 0, 1, 1);
						int off = (params[i+1][0] == '@' && xstrlen(params[i+1]) > 1) ? 1 : 0;

 						show_group = xstrdup(tmp[0] + off);
						array_free(tmp);
					}
			}

			if (match_arg(argv[i], 'd', "description", 2))
				show_descr = 1;
		}
		array_free(argv);
	}

	for (l = session->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		int show;

		if (!u->nickname)
			continue;

		tmp = ekg_status_label(u->status, u->descr, "list_");

		show = show_all;

		if (show_away && !xstrcasecmp(u->status, EKG_STATUS_AWAY))
			show = 1;

		if (show_active && !xstrcasecmp(u->status, EKG_STATUS_AVAIL))
			show = 1;

		if (show_inactive && !xstrcasecmp(u->status, EKG_STATUS_NA))
			show = 1;

		if (show_invisible && !xstrcasecmp(u->status, EKG_STATUS_INVISIBLE))
			show = 1;

		if (show_blocked && !xstrcasecmp(u->status, EKG_STATUS_BLOCKED))
			show = 1;

		if (show_descr && !u->descr)
			show = 0;

		if (show_group && !ekg_group_member(u, show_group))
			show = 0;

		if (show_offline && ekg_group_member(u, "__offline"))
			show = 1;

		if (show) {
			printq(tmp, format_user(session, u->uid), (u->first_name) ? u->first_name : u->nickname, inet_ntoa(*((struct in_addr*) &u->ip)), itoa(u->port), u->descr);
			count++;
		}
	}

	if (!count && !(show_descr || show_group) && show_all)
		printq("list_empty");

	return 0;
}

COMMAND(cmd_save)
{
	last_save = time(NULL);
	
        /* sprawdzamy czy session istnieje - je¶li nie to nie mamy po co robiæ czego¶ dalej ... */
        if(!session) {
		if(session_current)
			session = session_current;
		else
			return -1;
	}

	if (!userlist_write(session) && !config_write(params[0]) && !session_write()) {
		printq("saved");
		config_changed = 0;
	} else {
		printq("error_saving");
		return -1;
	}

	return 0;
}

COMMAND(cmd_set)
{
	const char *arg = NULL, *val = NULL;
	int unset = 0, show_all = 0, res = 0;
	char *value = NULL;
	list_t l;

	if (match_arg(params[0], 'a', "all", 1)) {
		show_all = 1;
		arg = params[1];
		if (arg)
			val = params[2];
	} else {
		arg = params[0];
		if (arg)
			val = params[1];
	}
	
	if (arg && arg[0] == '-') {
		unset = 1;
		arg++;
	}

	if (arg && val) {
		char **tmp = array_make(val, "", 0, 0, 1);

		value = tmp[0];
		tmp[0] = NULL;
		array_free(tmp);
	}

	if ((!arg || !val) && !unset) {
		int displayed = 0;

		for (l = variables; l; l = l->next) {
			variable_t *v = l->data;
			
			if ((!arg || !xstrcasecmp(arg, v->name)) && (v->display != 2 || xstrcmp(name, "set"))) {
				char *string = *(char**)(v->ptr);
				int value = *(int*)(v->ptr);

				if (!show_all && !arg && v->dyndisplay && !((v->dyndisplay)(v->name)))
					continue;

				if (!v->display) {
					printq("variable", v->name, "(...)");
					displayed = 1;
					continue;
				}

				if (v->type == VAR_STR) {
					char *tmp = (string) ? saprintf("\"%s\"", string) : "(none)";

					printq("variable", v->name, tmp);
					
					if (string)
						xfree(tmp);
				}

				if (v->type == VAR_BOOL)
					printq("variable", v->name, (value) ? "1 (on)" : "0 (off)");
				
				if ((v->type == VAR_INT || v->type == VAR_MAP) && !v->map)
					printq("variable", v->name, itoa(value));

				if (v->type == VAR_INT && v->map) {
					char *tmp = NULL;
					int i;

					for (i = 0; v->map[i].label; i++)
						if (v->map[i].value == value) {
							tmp = saprintf("%d (%s)", value, v->map[i].label);
							break;
						}

					if (!tmp)
						tmp = saprintf("%d", value);

					printq("variable", v->name, tmp);

					xfree(tmp);
				}

				if (v->type == VAR_MAP && v->map) {
					string_t s = string_init(itoa(value));
					int i, first = 1;

					for (i = 0; v->map[i].label; i++) {
						if ((value & v->map[i].value) || (!value && !v->map[i].value)) {
							string_append(s, (first) ? " (" : ",");
							first = 0;
							string_append(s, v->map[i].label);
						}
					}

					if (!first)
						string_append_c(s, ')');

					printq("variable", v->name, s->str);

					string_free(s, 1);
				}

				displayed = 1;
			}
		}

		if (!displayed && params[0]) {
			printq("variable_not_found", params[0]);
			return -1;
		}
	} else {
		theme_cache_reset();
		switch (variable_set(arg, (unset) ? NULL : value, 0)) {
			case 0:
			{
				const char *my_params[2] = { (!unset) ? params[0] : params[0] + 1, NULL };

				cmd_set("set-show", my_params, NULL, NULL, quiet);
				config_changed = 1;
				last_save = time(NULL);
				break;
			}
			case -1:
				printq("variable_not_found", arg);
				res = -1;
				break;
			case -2:
				printq("variable_invalid", arg);
				res = -1;
				break;
		}
	}

	xfree(value);

	return res;
}

COMMAND(cmd_quit)
{
    	char *reason, *tmp;
	list_t l;
	
	reason = xstrdup(params[0]);
	query_emit(NULL, "quitting", &reason);
	xfree(reason);

	tmp = saprintf("/disconnect %s", (params[0]) ? params[0] : "");

	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;

		command_exec(NULL, s, tmp, 3);
	}

	xfree(tmp);

	/* nie wychodzimy tutaj, ¿eby command_exec() mia³o szansê zwolniæ
	 * u¿ywan± przez siebie pamiêæ. */
	quit_command = 1;

	return 0;
}

COMMAND(cmd_version) 
{
    	printq("ekg_version", VERSION, compile_time());
	query_emit(NULL, "plugin-print-version");

	return 0;
}

COMMAND(cmd_test_segv)
{
	char *foo = (char*) 0x41414141;

	*foo = 0x41;

	return 0;
}

COMMAND(cmd_test_send)
{
	const char *sender, *rcpts[2] = { NULL, NULL };

	if (!params[0] || !params[1] || !window_current || !window_current->session)
		return -1;

	sender = params[0];

	if (sender[0] == '>') {
		rcpts[0] = sender + 1;
		sender = window_current->session->uid;
	}

	message_print((session) ? session->uid : NULL, sender, (rcpts[0]) ? rcpts : NULL, params[1], NULL, time(NULL), EKG_MSGCLASS_CHAT, "1234");

	return 0;
}

COMMAND(cmd_test_addtab)
{
	if (params[0])
		tabnick_add(params[0]);

	return 0;
}

COMMAND(cmd_test_deltab)
{
	if (params[0])
		tabnick_remove(params[0]);

	return 0;
}

COMMAND(cmd_test_debug)
{
	if (params[0])
		debug("%s", params[0]);

	return 0;
}

COMMAND(cmd_test_debug_dump)
{
	char *tmp = saprintf("Zapisa³em debug do pliku debug.%d", (int) getpid());

	debug_write_crash();
	printq("generic", tmp);
	xfree(tmp);

	return 0;
}

COMMAND(cmd_debug_watches)
{
	list_t l;
	char buf[256];
	
	printq("generic_bold", "fd     wa   plugin  pers tout  started     rm");
	
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;
		char wa[4], *plugin;

		xstrcpy(wa, "");

		if ((w->type & WATCH_READ))
			xstrcat(wa, "R");
		if (w->buf)
			xstrcat(wa, "L");
		if ((w->type & WATCH_WRITE))
			xstrcat(wa, "W");

		if (w->plugin)
			plugin = w->plugin->name;
		else
			plugin = "-";

		snprintf(buf, sizeof(buf), "%-5d  %-3s  %-8s  %-2d  %-4ld  %-10ld  %-2d", w->fd, wa, plugin, w->persist, w->timeout, w->started, w->removed);
		printq("generic", buf);
	}

	return 0;
}

COMMAND(cmd_debug_queries)
{
	list_t l;
	
	printq("generic", "name                             | plugin   | count");
	printq("generic", "---------------------------------|----------|------");
	
	for (l = queries; l; l = l->next) {
		query_t *q = l->data;
		char buf[256], *plugin;

		plugin = (q->plugin) ? q->plugin->name : "-";

		snprintf(buf, sizeof(buf), "%-32s | %-8s | %d", q->name, plugin, q->count);
		printq("generic", buf);
	}

	return 0;
}

COMMAND(cmd_debug_query)
{
	char *p[10];
	int i;

	memset(p, 0, sizeof(p));

	for (i = 0; params[i] && i < 10; i++)
		p[i] = xstrdup(params[i]);

	query_emit(NULL, p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6], &p[7], &p[8], &p[9]);

	for (i = 0; i < 10; i++)
		xfree(p[i]);

	return 0;
}

COMMAND(cmd_test_fds)
{
	struct stat st;
	char buf[1000];
	int i;
	
	for (i = 0; i < 2048; i++) {
		if (fstat(i, &st) == -1)
			continue;

		sprintf(buf, "%d: ", i);

		if (S_ISREG(st.st_mode))
			sprintf(buf + xstrlen(buf), "file, inode %lu, size %lu", st.st_ino, st.st_size);

		if (S_ISSOCK(st.st_mode)) {
			struct sockaddr sa;
//			struct sockaddr_un *sun = (struct sockaddr_un*) &sa;
			struct sockaddr_in *sin = (struct sockaddr_in*) &sa;
			int sa_len = sizeof(sa);
			
			if (getpeername(i, &sa, &sa_len) == -1) {
				getsockname(i, &sa, &sa_len);

				if (sa.sa_family == AF_INET) {
					xstrcat(buf, "socket, inet, *:");
					xstrcat(buf, itoa(ntohs(sin->sin_port)));
				} else
					xstrcat(buf, "socket");
			} else {
				switch (sa.sa_family) {
					case AF_UNIX:
						xstrcat(buf, "socket, unix");
						break;
					case AF_INET:
						xstrcat(buf, "socket, inet, ");
						xstrcat(buf, inet_ntoa(sin->sin_addr));
						xstrcat(buf, ":");
						xstrcat(buf, itoa(ntohs(sin->sin_port)));
						break;
					default:
						xstrcat(buf, "socket, ");
						xstrcat(buf, itoa(sa.sa_family));
				}
			}
		}
		
		if (S_ISDIR(st.st_mode))
			xstrcat(buf, "directory");
		
		if (S_ISCHR(st.st_mode))
			xstrcat(buf, "char");

		if (S_ISBLK(st.st_mode))
			xstrcat(buf, "block");

		if (S_ISFIFO(st.st_mode))
			xstrcat(buf, "fifo");

		if (S_ISLNK(st.st_mode))
			xstrcat(buf, "symlink");

		printq("generic", buf);
	}

	return 0;
}

COMMAND(cmd_beep)
{
	query_emit(NULL, "ui-beep", NULL);

	return 0;
}

COMMAND(cmd_play)
{
	if (!params[0] || !config_sound_app) {
		printq("not_enough_params", name);
		return -1;
	}

	return play_sound(params[0]);
}

COMMAND(cmd_say)
{
	if (!params[0] || !config_speech_app) {
		printq("not_enough_params", name);
		return -1;
	}

	if (match_arg(params[0], 'c', "clear", 2)) {
		xfree(buffer_flush(BUFFER_SPEECH, NULL));
		return 0;
	}

	return say_it(params[0]);
}

COMMAND(cmd_reload)
{
	const char *filename = NULL;
	int res = 0;

	if (params[0])
		filename = params[0];

	if ((res = config_read(filename)))
		printq("error_reading_config", strerror(errno));

	if (res != -1) {
		printq("config_read_success", (res != -2 && filename) ? filename : prepare_path("config", 0));
		config_changed = 0;
	}

	return res;
}

COMMAND(cmd_query)
{
	char **p = xcalloc(3, sizeof(char*));
	int i, res = 0;

        /* sprawdzamy czy session istnieje - je¶li nie to nie mamy po co robiæ czego¶ dalej ... */
        if(!session) {
                if(session_current)
                        session = session_current;
                else
                        return -1;
        }

	/* skopiuj argumenty dla wywo³ania /chat */
	for (i = 0; params[i]; i++)
		p[i] = xstrdup(params[i]);

	p[i] = NULL;

	if (params[0] && (params[0][0] == '@' || xstrchr(params[0], ','))) {
		struct conference *c = conference_create(params[0]);
		
		if (!c) {
			res = -1;
			goto cleanup;
		}

		xfree(p[0]);
		p[0] = xstrdup(c->name);

		goto query;
	}

	if (params[0] && params[0][0] == '#') {
		struct conference *c = conference_find(params[0]);

		if (!c) {
			printq("conferences_noexist", params[0]);
			res = -1;
			goto cleanup;
		}
		
		xfree(p[0]);
		p[0] = xstrdup(c->name);

		goto query;
	}

	if (params[0] && !get_uid(session, params[0])) {
		printq("user_not_found", params[0]);
		res = -1;
		goto cleanup;
	}

query:
	if (!p[0] && !window_current->target)
		goto chat;

	if (p[0]) {
		window_t *w;

		if ((w = window_find(p[0]))) {
			window_switch(w->id);
			goto chat;
		}

		if (config_make_window == 1) {
			list_t l;

			for (l = windows; l; l = l->next) {
				window_t *v = l->data;

				if (v->id < 2 || v->floating || v->target)
					continue;

				w = v;
				break;
			}

			if (!w)
				w = window_new(p[0], session, 0);

			window_switch(w->id);
		}

		if (config_make_window == 2) {
			w = window_new(p[0], session, 0);
			window_switch(w->id);
		}

		xfree(window_current->target);
		window_current->target = xstrdup(p[0]);
		query_emit(NULL, "ui-window-target-changed", &window_current);

		if (!quiet) {
			print_window(p[0], session, 0, "query_started", p[0]);
			print_window(p[0], session, 0, "query_started_window", p[0]);
		}
	} else {
		query_emit(NULL, "ui-window-target-changed", &window_current);

		printq("query_finished", window_current->target);
	}

chat:
	if (params[0] && params[1]) {
		char *tmp = saprintf("/ %s", params[1]);
		command_exec(params[0], session, tmp, quiet);
		xfree(tmp);
	}

cleanup:
	for (i = 0; p[i]; i++)
		xfree(p[i]);

	xfree(p);

	return res;
}

#if 0
COMMAND(cmd_on)
{
	if (match_arg(params[0], 'a', "add", 2)) {
		int flags, res;
		userlist_t *u = NULL;
		const char *t = params[2];
		uin_t uin = 0;
		
		if (!params[1] || !params[2] || !params[3]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!(flags = event_flags(params[1]))) {
			printq("invalid_params", name);
			return -1;
		}

		if (!(uin = get_uin(params[2])) && xstrcmp(params[2], "*") && params[2][0] != '@') {
			printq("user_not_found", params[2]);
			return -1;
		}

		if (uin) {
			if ((u = userlist_find(uin, NULL)) && u->nickname)
				t = u->nickname;
			else
				t = itoa(uin);
		}

		if (!(res = event_add(flags, t, params[3], quiet)))
			config_changed = 1;

		return res;
	}

	if (match_arg(params[0], 'd', "del", 2)) {

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!event_remove(params[1], quiet)) {
			config_changed = 1;
			return 0;
		} else
			return -1;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		list_t l;
                int count = 0;
		const char *ename = NULL;

		if (params[0] && params[0][0] != '-')
			ename = params[0];
		else if (params[0] && match_arg(params[0], 'l', "list", 2))
			ename = params[1];

		for (l = events; l; l = l->next) {
			struct event *ev = l->data;

			if (ename && xstrcasecmp(ev->name, ename))
				continue;

			printq((ev->flags & INACTIVE_EVENT) ? "events_list_inactive" : "events_list", event_format(abs(ev->flags)), event_format_target(ev->target), ev->action, ev->name);
			count++;
		}

		if (!count)
			printq("events_list_empty");

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}
#endif

COMMAND(cmd_echo)
{
	printq("generic", (params && params[0]) ? params[0] : "");

	return 0;
}

COMMAND(cmd_bind)
{
	query_emit(NULL, "binding-command", (params) ? params[0] : NULL, (params && params[0]) ? params[1] : NULL, (params && params[1]) ? params[2] : NULL, NULL);
//	ui_event("command", quiet, "bind", (params) ? params[0] : NULL, (params && params[0]) ? params[1] : NULL, (params && params[1]) ? params[2] : NULL, NULL); 

	return 0;
}

/*
 * command_exec()
 * 
 * wykonuje polecenie zawarte w linii tekstu. 
 *
 *  - target - w którym oknie nast±pi³o (NULL je¶li to nie query)
 *  - session - sesja, dla której dzia³amy
 *  - xline - linia tekstu
 *  - quiet - =1 ukrywamy wynik, =2 ukrywamy nieistnienie polecenia
 *
 * 0/-1.
 */
int command_exec(const char *target, session_t *session, const char *xline, int quiet)
{
	char *cmd = NULL, *tmp, *p = NULL, short_cmd[2] = ".", *last_name = NULL, *last_params = NULL, *line_save = NULL, *line = NULL;
	command_func_t *last_abbr = NULL;
	int abbrs = 0;
        command_func_t *last_abbr_plugins = NULL;
        int abbrs_plugins = 0;

	list_t l;

	if (!xline)
		return 0;

	/* wysy³amy do kogo¶ i nie ma na pocz±tku slasha */
	if (target && *xline != '/') {
		int correct_command = 0;
	
		/* wykrywanie przypadkowo wpisanych poleceñ */
		if (config_query_commands) {
			for (l = commands; l; l = l->next) {
				command_t *c = l->data;
				int l = xstrlen(c->name);

				if (l < 3 || xstrncasecmp(xline, c->name, l))
					continue;
				
				if (!xline[l] || xisspace(xline[l])) {
					correct_command = 1;
					break;
				}
			}		
		}

		if (!correct_command) {
			char *tmp = saprintf("/ %s", xline);
			command_exec(target, session, tmp, quiet);
			xfree(tmp);
			return 0;
		}
	}
	
	send_nicks_index = 0;
	line = line_save = xstrdup(xline);

	if (*line == '/')
		line++;

	if (*line == '^') {
		quiet = 1;
		line++;
	}

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		if (!isalpha_pl_PL(c->name[0]) && xstrlen(c->name) == 1 && line[0] == c->name[0]) {
			short_cmd[0] = c->name[0];
			cmd = short_cmd;
			p = line + 1;
		}
	}

	if (!cmd) {
		tmp = cmd = line;
		while (*tmp && !xisspace(*tmp))
			tmp++;
		p = (*tmp) ? tmp + 1 : tmp;
		*tmp = 0;
		p = strip_spaces(p);
	}

	/* poszukaj najpierw komendy dla danej sesji */
	if (session && session->uid) {
		int plen = (int)(xstrchr(session->uid, ':') - session->uid) + 1;
		
		for (l = commands; l; l = l->next) {
			command_t *c = l->data;

			if (xstrncasecmp(c->name, session->uid, plen))
				continue;
	
			if (!xstrcasecmp(c->name + plen, cmd)) {
				last_abbr = c->function;
				last_name = c->name;
				last_params = (c->alias) ? "?" : c->params;
				abbrs = 1;
				goto exact_match;
			}

			if (!xstrncasecmp(c->name + plen, cmd, xstrlen(cmd))) {
				abbrs_plugins++;
				last_abbr_plugins = c->function;
				last_name = c->name;
				last_params = (c->alias) ? "?" : c->params;
			} else {
				if (last_abbr_plugins && abbrs_plugins == 1)
					break;
			} 
		}
	}

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		if (!xstrcasecmp(c->name, cmd)) {
			last_abbr = c->function;
			last_name = c->name;
			last_params = (c->alias) ? "?" : c->params;
			abbrs = 1;
			break;
		}
		if (!xstrncasecmp(c->name, cmd, xstrlen(cmd))) {
			abbrs++;
			last_abbr = c->function;
			last_name = c->name;
			last_params = (c->alias) ? "?" : c->params;
		} else {
			if (last_abbr && abbrs == 1)
				break;
		}
	} 

exact_match:
	if ((last_abbr && abbrs == 1 && !last_abbr_plugins && !abbrs_plugins) || (last_abbr_plugins && abbrs_plugins == 1 && !last_abbr && !abbrs)) {
		char **par, *tmp;
		int res, len = xstrlen(last_params);

		if(last_abbr_plugins)
			last_abbr = last_abbr_plugins;
		if(abbrs_plugins)
			abbrs = abbrs_plugins;
		
		if ((tmp = xstrchr(last_name, ':')))
			last_name = tmp + 1;
		
		window_lock_inc_n(target);

		par = array_make(p, " \t", len, 1, 1);
		res = (last_abbr)(last_name, (const char**) par, (session) ? session : window_current->session, target, (quiet & 1));
		array_free(par);

		window_lock_dec_n(target);

		xfree(line_save);

		if (quit_command)
			ekg_exit();

		return res;
	}

	if (xstrcmp(cmd, "")) {
		quiet = quiet & 2;
		printq("unknown_command", cmd);
	}

	xfree(line_save);

	return -1;
}

int binding_help(int a, int b)  
{
	print("help_quick");  

	return 0;  
}

/*
 * binding_quick_list()
 *
 * wy¶wietla krótk± i zwiêz³a listê dostêpnych, zajêtych i niewidocznych
 * ludzi z listy kontaktów.
 */
int binding_quick_list(int a, int b)
{
	string_t list = string_init(NULL);
	list_t l, sl;
	for (sl = sessions; sl; sl = sl->next) {
		session_t *s = sl->data;
		for (l = s->userlist; l; l = l->next) {
			userlist_t *u = l->data;
			const char *format = NULL;
			char *tmp;

			if (!u->nickname)
				continue;
		
			format = format_find(ekg_status_label(u->status, NULL, "quick_list_"));

			if (!format)
				continue;

			if (!(tmp = format_string(format, u->nickname)))
				continue;

			string_append(list, tmp);

			xfree(tmp);
		}
	}

	if (xstrlen(list->str) > 0)
		print("quick_list", list->str);

	string_free(list, 1);

	return 0;
}

COMMAND(cmd_alias_exec)
{	
	list_t l, tmp = NULL, m = NULL;
	int need_args = 0;

	for (l = aliases; l; l = l->next) {
		struct alias *a = l->data;

		if (!xstrcasecmp(name, a->name)) {
			tmp = a->commands;
			break;
		}
	}

	for (; tmp; tmp = tmp->next) {
		char *p;
		int __need = 0;

		for (p = tmp->data; *p; p++) {

			if (*p == '\\' && p[1] == '%') {
				p += 2;
				continue;
			}

			if (*p != '%')
				continue;

			p++;

			if (!*p)
				break;

			if (*p >= '1' && *p <= '9' && (*p - '0') > __need)
				__need = *p - '0';

			if (need_args < __need)
				need_args = __need;
		}

		list_add(&m, tmp->data, xstrlen(tmp->data) + 1);
	}
	
	for (tmp = m; tmp; tmp = tmp->next) {
		string_t str;

		if (*((char *) tmp->data) == '/')
			str = string_init(NULL);
		else
			str = string_init("/");

		if (need_args) {
			char *args[9], **arr, *s;
			int i;

			if (!params[0]) {
				printq("aliases_not_enough_params", name);
				string_free(str, 1);
				list_destroy(m, 1);
				return -1;
			}

			arr = array_make(params[0], "\t ", need_args, 1, 1);

			if (array_count(arr) < need_args) {
				printq("aliases_not_enough_params", name);
				string_free(str, 1);
				array_free(arr);
				list_destroy(m, 1);
				return -1;
			}

			for (i = 0; i < 9; i++) {
				if (i < need_args)
					args[i] = arr[i];
				else
					args[i] = NULL;
			}

			s = format_string((char *) tmp->data, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]);
			string_append(str, s);
			xfree(s);

			array_free(arr);

		} else {
			string_append(str, (char *) tmp->data);
			
			if (params[0]) {
				string_append(str, " ");
				string_append(str, params[0]);
			}
		}

		command_exec(target, session, str->str, quiet);
		string_free(str, 1);
	}
	
	list_destroy(m, 1);
		
	return 0;
}

COMMAND(cmd_at)
{
	list_t l;

	if (match_arg(params[0], 'a', "add", 2)) {
		const char *p, *a_name = NULL;
		char *a_command = NULL;
		time_t period = 0, freq = 0;
		struct timer *t;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!strncmp(params[2], "*/", 2) || xisdigit(params[2][0])) {
			a_name = params[1];

			if (!xstrcmp(a_name, "(null)")) {
				printq("invalid_params", name);
				return -1;
			}

			for (l = timers; l; l = l->next) {
				t = l->data;

				if (!xstrcasecmp(t->name, a_name)) {
					printq("at_exist", a_name);
					return -1;
				}
			}

			p = params[2];
		} else
			p = params[1];

		{
			struct tm *lt;
			time_t now = time(NULL);
			char *tmp, *freq_str = NULL, *foo = xstrdup(p);
			int wrong = 0;

			lt = localtime(&now);
			lt->tm_isdst = -1;

			/* czêstotliwo¶æ */
			if ((tmp = xstrchr(foo, '/'))) {
				*tmp = 0;
				freq_str = ++tmp;
			}

			/* wyci±gamy sekundy, je¶li s± i obcinamy */
			if ((tmp = xstrchr(foo, '.')) && !(wrong = (xstrlen(tmp) != 3))) {
				sscanf(tmp + 1, "%2d", &lt->tm_sec);
				tmp[0] = 0;
			} else
				lt->tm_sec = 0;

			/* pozb±d¼my siê dwukropka */
			if ((tmp = xstrchr(foo, ':')) && !(wrong = (xstrlen(tmp) != 3))) {
				tmp[0] = tmp[1];
				tmp[1] = tmp[2];
				tmp[2] = 0;
			}

			/* jedziemy ... */
			if (!wrong) {
				switch (xstrlen(foo)) {
					int ret;

					case 12:
						ret = sscanf(foo, "%4d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 5)
							wrong = 1;
						lt->tm_year -= 1900;
						lt->tm_mon -= 1;
						break;
					case 10:
						ret = sscanf(foo, "%2d%2d%2d%2d%2d", &lt->tm_year, &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 5)
							wrong = 1;
						lt->tm_year += 100;
						lt->tm_mon -= 1;
						break;
					case 8:
						ret = sscanf(foo, "%2d%2d%2d%2d", &lt->tm_mon, &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 4)
							wrong = 1;
						lt->tm_mon -= 1;
						break;
					case 6:
						ret = sscanf(foo, "%2d%2d%2d", &lt->tm_mday, &lt->tm_hour, &lt->tm_min);
						if (ret != 3)
							wrong = 1;
						break;	
					case 4:
						ret = sscanf(foo, "%2d%2d", &lt->tm_hour, &lt->tm_min);
						if (ret != 2)
							wrong = 1;
						break;
					default:
						wrong = 1;
				}
			}

			/* nie ma b³êdów ? */
			if (wrong || lt->tm_hour > 23 || lt->tm_min > 59 || lt->tm_sec > 59 || lt->tm_mday > 31 || !lt->tm_mday || lt->tm_mon > 11) {
				printq("invalid_params", name);
				xfree(foo);
				return -1;
			}

			if (freq_str) {
				for (;;) {
					time_t _period = 0;

					if (xisdigit(*freq_str))
						_period = atoi(freq_str);
					else {
						printq("invalid_params", name);
						xfree(foo);
						return -1;
					}

					freq_str += xstrlen(itoa(_period));

					if (xstrlen(freq_str)) {
						switch (xtolower(*freq_str++)) {
							case 'd':
								_period *= 86400;
								break;
							case 'h':
								_period *= 3600;
								break;
							case 'm':
								_period *= 60;
								break;
							case 's':
								break;
							default:
								printq("invalid_params", name);
								xfree(foo);
								return -1;
						}
					}

					freq += _period;
					
					if (!*freq_str)
						break;
				}
			}

			xfree(foo);

			/* plany na przesz³o¶æ? */
			if ((period = mktime(lt) - now) <= 0) {
				if (freq) {
					while (period <= 0)
						period += freq;
				} else {
					printq("at_back_to_past");
					return -1;
				}
			}
		}

		if (a_name)
			a_command = xstrdup(params[3]);
		else
			a_command = array_join((char **) params + 2, " ");

		if (a_command) {
			char *tmp = a_command;

			a_command = strip_spaces(a_command);
			
			if (!xstrcmp(a_command, "")) {
				printq("not_enough_params", name);
				xfree(tmp);
				return -1;
			}

			a_command = tmp;
		} else {
			printq("not_enough_params", name);
			return -1;
		}

		if ((t = timer_add(NULL, a_name, period, ((freq) ? 1 : 0), timer_handle_command, a_command))) {
			t->at = 1;
			printq("at_added", t->name);
			if (freq)
				t->period = freq;
			if (!in_autoexec)
				config_changed = 1;
		}

		xfree(a_command);
		return 0;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		int del_all = 0;
		int ret = 1;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*")) {
			del_all = 1;
			ret = timer_remove_user(1);
		} else
			ret = timer_remove(NULL, params[1]);
		
		if (!ret) {
			if (del_all)
				printq("at_deleted_all");
			else
				printq("at_deleted", params[1]);
			
			config_changed = 1;
		} else {
			if (del_all)
				printq("at_empty");
			else {
				printq("at_noexist", params[1]);
				return -1;
			}
		}

		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		const char *a_name = NULL;
		int count = 0;

		if (params[0] && match_arg(params[0], 'l', "list", 2))
			a_name = params[1];
		else if (params[0])
			a_name = params[0];

		for (l = timers; l; l = l->next) {
			struct timer *t = l->data;
			struct timeval tv;
			struct tm *at_time;
			char tmp[100], tmp2[150];
			time_t sec, minutes = 0, hours = 0, days = 0;

			if (!t->at || (a_name && xstrcasecmp(t->name, a_name)))
				continue;

			if (t->function != timer_handle_command)
				continue;

			count++;

			gettimeofday(&tv, NULL);

			at_time = localtime((time_t *) &t->ends.tv_sec);
			strftime(tmp, sizeof(tmp), format_find("at_timestamp"), at_time);

			if (t->persist) {
				sec = t->period;

				if (sec > 86400) {
					days = sec / 86400;
					sec -= days * 86400;
				}

				if (sec > 3600) {
					hours = sec / 3600;
					sec -= hours * 3600;
				}
			
				if (sec > 60) {
					minutes = sec / 60;
					sec -= minutes * 60;
				}

				strlcpy(tmp2, "every ", sizeof(tmp2));

				if (days) {
					strlcat(tmp2, itoa(days), sizeof(tmp2));
					strlcat(tmp2, "d ", sizeof(tmp2));
				}

				if (hours) {
					strlcat(tmp2, itoa(hours), sizeof(tmp2));
					strlcat(tmp2, "h ", sizeof(tmp2));
				}

				if (minutes) {
					strlcat(tmp2, itoa(minutes), sizeof(tmp2));
					strlcat(tmp2, "m ", sizeof(tmp2));
				}

				if (sec) {
					strlcat(tmp2, itoa(sec), sizeof(tmp2));
					strlcat(tmp2, "s", sizeof(tmp2));
				}
			}

			printq("at_list", t->name, tmp, (char*)(t->data), "", ((t->persist) ? tmp2 : ""));
		}

		if (!count) {
			if (a_name) {
				printq("at_noexist", a_name);
				return -1;
			} else
				printq("at_empty");
		}

		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_timer)
{
	list_t l;

	if (match_arg(params[0], 'a', "add", 2)) {
		const char *t_name = NULL, *p;
		char *t_command = NULL;
		time_t period = 0;
		struct timer *t;
		int persistent = 0;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (xisdigit(params[2][0]) || !strncmp(params[2], "*/", 2)) {
			t_name = params[1];

			if (!xstrcmp(t_name, "(null)")) {
				printq("invalid_params", name);
				return -1;
			}

			for (l = timers; l; l = l->next) {
				t = l->data;

				if (!t->at && !xstrcasecmp(t->name, t_name)) {
					printq("timer_exist", t_name);
					return -1;
				}
			}

			p = params[2];
			t_command = xstrdup(params[3]);
		} else {
			p = params[1];
			t_command = array_join((char **) params + 2, " ");
		}

		if ((persistent = !strncmp(p, "*/", 2)))
			p += 2;

		for (;;) {
			time_t _period = 0;

			if (xisdigit(*p))
				_period = atoi(p);
			else {
				printq("invalid_params", name);
				xfree(t_command);
				return -1;
			}

			p += xstrlen(itoa(_period));

			if (xstrlen(p)) {
				switch (xtolower(*p++)) {
					case 'd':
						_period *= 86400;
						break;
					case 'h':
						_period *= 3600;
						break;
					case 'm':
						_period *= 60;
						break;
					case 's':
						break;
					default:
						printq("invalid_params", name);
						xfree(t_command);
						return -1;
				}
			}

			period += _period;
			
			if (!*p)
				break;
		}

		if (t_command) {
			char *tmp = t_command;

			t_command = strip_spaces(t_command);

			if (!xstrcmp(t_command, "")) {
				printq("not_enough_params", name);
				xfree(tmp);
				return -1;
			}

			t_command = tmp;
		} else {
			printq("not_enough_params", name);
			return -1;
		}

		if ((t = timer_add(NULL, t_name, period, persistent, timer_handle_command, t_command))) {
			printq("timer_added", t->name);
			if (!in_autoexec)
				config_changed = 1;
		}

		xfree(t_command);
		return 0;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		int del_all = 0, ret;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*")) {
			del_all = 1;
			ret = timer_remove_user(0);
		} else
			ret = timer_remove(NULL, params[1]);
		
		if (!ret) {
			if (del_all)
				printq("timer_deleted_all");
			else
				printq("timer_deleted", params[1]);

			config_changed = 1;
		} else {
			if (del_all)
				printq("timer_empty");
			else {
				printq("timer_noexist", params[1]);
				return -1;	
			}
		}

		return 0;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		const char *t_name = NULL;
		int count = 0;

		if (params[0] && match_arg(params[0], 'l', "list", 2))
			t_name = params[1];
		else if (params[0])
			t_name = params[0];

		for (l = timers; l; l = l->next) {
			struct timer *t = l->data;
			struct timeval tv;
			char *tmp;
			long usec, sec, minutes = 0, hours = 0, days = 0;

			if (t->function != timer_handle_command)
				continue;

			if (t->at || (t_name && xstrcasecmp(t->name, t_name)))
				continue;

			count++;

			gettimeofday(&tv, NULL);

			if (t->ends.tv_usec < tv.tv_usec) {
				sec = t->ends.tv_sec - tv.tv_sec - 1;
				usec = (t->ends.tv_usec - tv.tv_usec + 1000000) / 1000;
			} else {
				sec = t->ends.tv_sec - tv.tv_sec;
				usec = (t->ends.tv_usec - tv.tv_usec) / 1000;
			}

			if (sec > 86400) {
				days = sec / 86400;
				sec -= days * 86400;
			}

			if (sec > 3600) {
				hours = sec / 3600;
				sec -= hours * 3600;
			}
		
			if (sec > 60) {
				minutes = sec / 60;
				sec -= minutes * 60;
			}

			if (days)
				tmp = saprintf("%ldd %ldh %ldm %ld.%.3ld", days, hours, minutes, sec, usec);
			else
				if (hours)
					tmp = saprintf("%ldh %ldm %ld.%.3ld", hours, minutes, sec, usec);
				else
					if (minutes)
						tmp = saprintf("%ldm %ld.%.3ld", minutes, sec, usec);
					else
						tmp = saprintf("%ld.%.3ld", sec, usec);

			printq("timer_list", t->name, tmp, (char*)(t->data), "", (t->persist) ? "*" : "");
			xfree(tmp);
		}

		if (!count) {
			if (t_name) {
				printq("timer_noexist", t_name);
				return -1;
			} else
				printq("timer_empty");
		}

		return 0;
	}	

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_conference) 
{
        /* sprawdzamy czy session istnieje - je¶li nie to nie mamy po co robiæ czego¶ dalej ... */
        if(!session) {
                if(session_current)
                        session = session_current;
                else
                        return -1;
        }

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] == '#') {
		list_t l, r;
		int count = 0;
		const char *cname = NULL;
	
		if (params[0] && match_arg(params[0], 'l', "list", 2))
			cname = params[1];
		else if (params[0])
			cname = params[0];

		for (l = conferences; l; l = l->next) {
			struct conference *c = l->data;
			string_t recipients;
			const char *recipient;
			int first = 0;

			recipients = string_init(NULL);

			if (cname && xstrcasecmp(cname, c->name))
				continue;
			
			for (r = c->recipients; r; r = r->next) {
				char *uid = r->data;
				userlist_t *u = userlist_find(session, uid);

				if (u && u->nickname)
					recipient = u->nickname;
				else
					recipient = uid;

				if (first++)
					string_append_c(recipients, ',');
				
				string_append(recipients, recipient);

				count++;
			}

		        print(c->ignore ? "conferences_list_ignored" : "conferences_list", c->name, recipients->str);

			string_free(recipients, 1);
		}

		if (!count) {
			if (params[0] && params[0][0] == '#') {
				printq("conferences_noexist", params[0]);
				return -1;
			} else
				printq("conferences_list_empty");
		}

		return 0;
	}

	if (match_arg(params[0], 'j', "join", 2)) {
		struct conference *c;
		const char *uid;

		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		if (!(c = conference_find(params[1]))) {
			printq("conferences_noexist");
			return -1;
		}

		if (!(uid = get_uid(session, params[2]))) {
			printq("unknown_user", params[2]);
			return -1;
		}

		if (conference_participant(c, uid)) {
			printq("conferences_already_joined", format_user(session, uid), params[1]);
			return -1;
		}

		list_add(&c->recipients, (void*) uid, xstrlen(uid) + 1);

		printq("conferences_joined", format_user(session, uid), params[1]);

		return 0;
	}

	if (match_arg(params[0], 'a', "add", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[2]) {
			if (params[1][0] != '#') {
				printq("conferences_name_error");
				return -1;
			} else
				conference_add(params[1], params[2], quiet);
		} else
			conference_create(params[1]);

		return 0;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*"))
			conference_remove(NULL, quiet);
		else {
			if (params[1][0] != '#') {
				printq("conferences_name_error");
				return -1;
			}

			conference_remove(params[1], quiet);
		}

		return 0;
	}

	if (match_arg(params[0], 'r', "rename", 2)) {
		if (!params[1] || !params[2]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#' || params[2][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		conference_rename(params[1], params[2], quiet);

		return 0;
	}
	
	if (match_arg(params[0], 'i', "ignore", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		conference_set_ignore(params[1], 1, quiet);

		return 0;
	}

	if (match_arg(params[0], 'u', "unignore", 2)) {
		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		conference_set_ignore(params[1], 0, quiet);

		return 0;
	}

	if (match_arg(params[0], 'f', "find", 2)) {
		struct conference *c;
		char *tmp = NULL;
		list_t l;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (params[1][0] != '#') {
			printq("conferences_name_error");
			return -1;
		}

		c = conference_find(params[1]);

		if (c) {
			for (l = c->recipients; l; l = l->next) {
				tmp = saprintf("/find %s", (char*) (l->data));
				command_exec(target, session, tmp, quiet);
				xfree(tmp);
			}
		} else {
			printq("conferences_noexist", params[1]);
			return -1;
		}


		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_last)
{
        list_t l;
	const char *uid = NULL;
	int show_sent = 0, last_n = 0, count = 0, i = 0;
	char **arr = NULL;
	const char *nick = NULL;
	time_t n;
	struct tm *now;

        /* sprawdzamy czy session istnieje - je¶li nie to nie mamy po co robiæ czego¶ dalej ... */
        if(!session) {
                if(session_current)
                        session = session_current;
                else
                        return -1;
        }

	if (match_arg(params[0], 'c', "clear", 2)) {
		if (params[1] && !(uid = get_uid(session, params[1]))) {
			printq("user_not_found", params[1]);
			return -1;
		}

		if ((uid && !last_count(uid)) || !list_count(lasts)) {
			if (uid)
				printq("last_list_empty_nick", format_user(session, uid));
			else
				printq("last_list_empty");

			return -1;
		}

		if (uid) {
			last_del(uid);
			printq("last_clear_uin", format_user(session, uid));
		} else {
			last_free();
			printq("last_clear");
		}

		return 0;
	}		

	if (params[0]) {
		show_sent = match_arg(params[0], 's', "stime", 2);

		if (!show_sent)
			nick = params[0];

		if (params[1]) {
			arr = array_make(params[1], " \t", 0, 1, 0);

			nick = arr[0];
			
			if (match_arg(params[0], 'n', "number", 2)) {
				last_n = strtol(arr[0], NULL, 0);
				nick = arr[1];
				
				if (arr[1] && (show_sent = match_arg(arr[1], 's', "stime", 2)))
					nick = arr[2];
			}

			if (arr[1] && show_sent && match_arg(arr[0], 'n', "number", 2)) {
				last_n = atoi(arr[1]);
				nick = arr[2];
			}

		}
	}

	if (nick && !(uid = get_uid(session, nick))) {
		printq("user_not_found", nick);
		array_free(arr);
		return -1;
	}

	array_free(arr);
		
	if (!((uid > 0) ? (count = last_count(uid)) : (count = list_count(lasts)))) {
		if (uid) {
			printq("last_list_empty_nick", format_user(session, uid));
			return -1;
		}

		printq("last_list_empty");
		return 0;
	}

	n = time(NULL);
	now = localtime(&n);

        for (l = lasts; l; l = l->next) {
                struct last *ll = l->data;
		struct tm *tm, *st;
		char buf[100], buf2[100], *time_str = NULL;

		if (!uid || !xstrcasecmp(uid, ll->uid)) {

			if (last_n && i++ < (count - last_n))
				continue;

			tm = localtime(&ll->time);
			strftime(buf, sizeof(buf), format_find("last_list_timestamp"), tm);

			if (show_sent && ll->type == 0 && !(ll->sent_time - config_time_deviation <= ll->time && ll->time <= ll->sent_time + config_time_deviation)) {
				st = localtime(&ll->sent_time);
				strftime(buf2, sizeof(buf2), format_find((tm->tm_yday == now->tm_yday) ? "last_list_timestamp_today" : "last_list_timestamp"), st);
				time_str = saprintf("%s/%s", buf, buf2);
			} else
				time_str = xstrdup(buf);

			if (config_last & 4 && ll->type == 1)
				printq("last_list_out", time_str, format_user(session, ll->uid), ll->message);
			else
				printq("last_list_in", time_str, format_user(session, ll->uid), ll->message);

			xfree(time_str);
		}
        }

	return 0;
}

COMMAND(cmd_queue)
{
	list_t l;
	
	if (match_arg(params[0], 'c', "clear", 2)) {
		if ((params[1] && !msg_queue_count_uid(params[1])) || !msg_queue_count()) {
			if (params[1])
				printq("queue_empty_uid", format_user(session, params[1]));
			else
				printq("queue_empty");

			return 0;
		}

		if (params[1]) {
			msg_queue_remove_uid(params[1]);
			printq("queue_clear_uid", format_user(session, params[1]));
		} else {
			msg_queue_free();
			printq("queue_clear");
		}

		return 0;
	}		

	if ((params[0] && !msg_queue_count_uid(params[0])) || !msg_queue_count()) {
		if (params[0])
			printq("queue_empty_uid", format_user(session, params[0]));
		else
			printq("queue_empty");

		return 0;
	}

        for (l = msg_queue; l; l = l->next) {
                msg_queue_t *m = l->data;
		struct tm *tm;
		char buf[100];

		if (!params[0] || !xstrcasecmp(m->rcpts, params[0])) {
			tm = localtime(&m->time);
			strftime(buf, sizeof(buf), format_find("queue_list_timestamp"), tm);

			printq("queue_list_message", buf, m->rcpts, m->message);
		}
	}

	return 0;
}

COMMAND(cmd_dcc)
{
	list_t l;

	if (!params[0] || !xstrncasecmp(params[0], "li", 2)) {	/* list */
		int empty = 1, passed = 0;

		for (l = dccs; l; l = l->next) {
			dcc_t *d = l->data;

			if (d->active)
				continue;
			
			empty = 0;
			
			if (!passed++)
				printq("dcc_show_pending_header");

			switch (d->type) {
				case DCC_SEND:
					printq("dcc_show_pending_send", itoa(d->id), format_user(session, d->uid), d->filename);
					break;
				case DCC_GET:
					printq("dcc_show_pending_get", itoa(d->id), format_user(session, d->uid), d->filename);
					break;
				case DCC_VOICE:
					printq("dcc_show_pending_voice", itoa(d->id), format_user(session, d->uid));
					break;
				default:
					break;
			}
		}

		passed = 0;

		for (l = dccs; l; l = l->next) {
			dcc_t *d = l->data;

			if (!d->active)
				continue;

			empty = 0;

			if (!passed++)
				printq("dcc_show_active_header");

			switch (d->type) {
				case DCC_SEND:
					printq("dcc_show_active_send", itoa(d->id), format_user(session, d->uid), d->filename, itoa(d->offset), itoa(d->size), itoa(100 * d->offset / d->size));
					break;
				case DCC_GET:
					printq("dcc_show_active_get", itoa(d->id), format_user(session, d->uid), d->filename, itoa(d->offset), itoa(d->size), itoa(100 * d->offset / d->size));
					break;
				case DCC_VOICE:
					printq("dcc_show_active_voice", itoa(d->id), format_user(session, d->uid));
					break;
				default:
					break;
			}
		}

		if (empty)
			printq("dcc_show_empty");
		
		return 0;
	}

	if (!xstrncasecmp(params[0], "c", 1)) {		/* close */
		dcc_t *d = NULL;
		const char *uid;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		uid = get_uid(session, params[1]);		

		for (l = dccs; l; l = l->next) {
			dcc_t *D = l->data;

			if (params[1][0] == '#' && atoi(params[1] + 1) == D->id) {
				d = D;
				break;
			}

			if (uid && !xstrcasecmp(D->uid, uid)) {
				d = D;
				break;
			}
		}

		if (!d) {
			printq("dcc_not_found", params[1]);
			return -1;
		}

		dcc_close(d);

		printq("dcc_close", format_user(session, uid));
		
		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

COMMAND(cmd_plugin)
{
	if (!params[0]) {
		list_t l;
		
		for (l = plugins; l; l = l->next) {
			plugin_t *p = l->data;

			printq("generic", (p && p->name) ? p->name : "?");
		}

		return 0;
	}

	if (params[0][0] == '+')
		return plugin_load(params[0] + 1);

	if (params[0][0] == '-')
		return plugin_unload(plugin_find(params[0] + 1));

	printq("invalid_params", name);

	return -1;
}

/*
 * command_add_compare()
 *
 * funkcja porównuj±ca nazwy komend, by wystêpowa³y alfabetycznie w li¶cie.
 *
 *  - data1, data2 - dwa wpisy komend do porównania.
 *
 * zwraca wynik xstrcasecmp() na nazwach komend.
 */
static int command_add_compare(void *data1, void *data2)
{
	command_t *a = data1, *b = data2;

	if (!a || !a->name || !b || !b->name)
		return 0;

	return xstrcasecmp(a->name, b->name);
}

/*
 * command_add()
 *
 * dodaje komendê. 
 *
 *  - plugin - plugin obs³uguj±cy komendê,
 *  - name - nazwa komendy,
 *  - params - definicja parametrów (szczegó³y poni¿ej),
 *  - function - funkcja obs³uguj±ca komendê,
 *  - help_params - opis parametrów,
 *  - help_brief - krótki opis komendy,
 *  - help_long - szczegó³owy opis komendy.
 *
 * 0 je¶li siê uda³o, -1 je¶li b³±d.
 */
int command_add(plugin_t *plugin, const char *name, const char *params, command_func_t function, int alias, const char *params_help, const char *brief_help, const char *long_help)
{
	command_t c;

	memset(&c, 0, sizeof(c));

	c.name = xstrdup(name);
	c.params = xstrdup(params);
	c.function = function;
	c.alias = alias;
	c.params_help = xstrdup(params_help);
	c.brief_help = xstrdup(brief_help);
	c.long_help = xstrdup(long_help);
	c.plugin = plugin;

	return (list_add_sorted(&commands, &c, sizeof(c), command_add_compare) != NULL) ? 0 : -1;
}

/*
 * command_remove()
 *
 * usuwa komendê z listy.
 *
 *  - plugin - plugin obs³uguj±cy,
 *  - name - nazwa komendy.
 */
int command_remove(plugin_t *plugin, const char *name)
{
	list_t l;

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		if (!xstrcasecmp(name, c->name) && plugin == c->plugin) {
			xfree(c->name);
			xfree(c->params);
			xfree(c->params_help);
			xfree(c->brief_help);
			xfree(c->long_help);
			list_remove(&commands, c, 1);
			return 0;
		}
	}

	return -1;
}

/*
 * rodzaje parametrów komend:
 *
 * '?' - olewamy,
 * 'U' - rêcznie wpisany uin, nadawca mesgów,
 * 'u' - nazwa lub uin z kontaktów, nazwa konferencji, rêcznie wpisany uin,
 *       nadawca mesgów,
 * 'c' - komenda,
 * 'i' - nicki z listy ignorowanych osób,
 * 'b' - nicki z listy blokowanych osób,
 * 'v' - nazwa zmiennej,
 * 'd' - komenda dcc,
 * 'p' - komenda python,
 * 'w' - komenda window,
 * 'f' - plik,
 * 'e' - nazwy zdarzeñ,
 * 'I' - poziomy ignorowania.
 * 's' - nazwa sesji
 * 'S' - zmienna sesji 
 * 'A' - nazwa lub zmienna sesji
 */

/*
 * command_init()
 *
 * inicjuje listê domy¶lnych komend.
 */
void command_init()
{
	command_add(NULL, "add", "U??", cmd_add, 0,
	  " [numer] [alias] [opcje]", "dodaje u¿ytkownika do listy kontaktów",
	  "\n"
	  "  -f, --find [alias]  dodaje ostatnio wyszukan± osobê\n"
	  "\n"
	  "W przypadku opcji %T--find%n alias jest wymagany, je¶li w ostatnim "
	  "wyszukiwaniu nie znaleziono pseudonimu. "
	  "Pozosta³e opcje identyczne jak dla polecenia %Tlist%n (dotycz±ce "
	  "wpisu). W oknie rozmowy z kim¶ spoza naszej listy kontaktów jako "
	  "parametr mo¿na podaæ sam alias.");
	  
	command_add(NULL, "alias", "??", cmd_alias, 0,
	  " [opcje]", "zarz±dzanie aliasami",
	  "\n"
	  "  -a, --add <alias> <komenda>     dodaje alias\n"
          "  -A, --append <alias> <komenda>  dodaje komendê do aliasu\n"
	  "  -d, --del <alias>|*             usuwa alias\n"
	  " [-l, --list] [alias]             wy¶wietla listê aliasów\n"
	  "\n"
	  "W komendzie mo¿na u¿yæ formatów od %T\\%1%n do %T\\%9%n i w "
	  "ten sposób ustaliæ kolejno¶æ przekazywanych argumentów.");
	  
	command_add(NULL, "at", "???c", cmd_at, 0,
	  " [opcje]", "planuje wykonanie komend",
	  "\n"
	  "  -a, --add [nazwa] <czas>[/czêst.] <komenda>  tworzy nowy plan\n"
	  "  -d, --del <nazwa>|*                   usuwa plan\n"
	  " [-l, --list] [nazwa]                   wy¶wietla listê planów\n"
	  "\n"
	  "Czas podaje siê w formacie "
	  "[[[yyyy]mm]dd]HH[:]MM[.SS], gdzie %Tyyyy%n to rok, %Tmm%n to miesi±c, "
	  "%Tdd%n to dzieñ, %THH:MM%n to godzina, a %T.SS%n to sekundy. "
	  "Minimalny format to %THH:MM%n (dwukropek mo¿na pomin±æ). "
	  "Po kropce mo¿na podaæ sekundy, a przed godzin± odpowiednio: dzieñ "
	  "miesi±ca, miesi±c, rok. Je¶li podanie zostana czêstotliwo¶æ, wyra¿ona "
	  "w sekundach lub za pomoc± przyrostków takich, jak dla komendy %Ttimer%n, "
	  "to komenda bêdzie wykonywana w zadanych odstepach czasu od momentu jej "
	  "pierwszego wykonania.");
 
	command_add(NULL, "beep", "", cmd_beep, 0,
	  "", "wydaje d¼wiêk", "");
	  
	command_add(NULL, "bind", "???", cmd_bind, 0,
	  " [opcje]", "przypisywanie akcji klawiszom",
	  "\n"
	  "  -a, --add <sekwencja> <akcja>   przypisuje now± sekwencjê\n"
	  "  -d, --del <sekwencja>           usuwa podan± sekwencjê\n"
	  " [-l, --list] [sekwencja]         wy¶wietla przypisane sekwencje\n"
	  "  -L, --list-default [sekwencja]  j.w. plus domy¶lne sekwencje\n"
	  "\n"
	  "Dostêpne sekwencje to: Ctrl-<znak>, Alt-<znak>, F<liczba>, Enter, "
	  "Backspace, Delete, Insert, Home, End, Left, Right, Up, Down, "
	  "PageUp, PageDown.\n"
	  "\n"
	  "Dostêpne akcje to: backward-word, forward-word, kill-word, toggle-input, cancel-input, backward-delete-char, beginning-of-line, end-of-line, delete-char, backward-page, forward-page, kill-line, yank, accept-line, line-discard, quoted-insert, word-rubout, backward-char, forward-char, previous-history, next-history, complete, quick-list, toggle-contacts, next-contacts-group, ignore-query, forward-contacts-page, backward-contacts-page, forward-contacts-line, backward-contacts-line."
	  "Ka¿da inna akcja bêdzie traktowana jako komenda do wykonania.");
  
	command_add(NULL, "clear", "", cmd_window, 0,
	  "", "czy¶ci ekran", "");
  
	command_add(NULL, "dcc", "duf?", cmd_dcc, 0,
	  " <komenda> [opcje]", "obs³uga bezpo¶rednich po³±czeñ",
	  "\n"
	  "  [r]send <numer/alias> <¶cie¿ka>  wysy³a podany plik\n"
	  "  get [numer/alias/#id]            akceptuje przysy³any plik\n"
	  "  resume [numer/alias/#id]         wznawia pobieranie pliku\n"
	  "  [r]voice <numer/alias/#id>       rozpoczyna rozmowê g³osow±\n"
	  "  close <numer/alias/#id>          zamyka po³±czenie\n"
	  "  list                             wy¶wietla listê po³±czeñ\n"
	  "\n"
	  "Po³±czenia bezpo¶rednie wymagaj± w³±czonej opcji %Tdcc%n. "
	  "Komendy %Trsend%n i %Trvoice%n wysy³aj± ¿±danie po³±czenia siê "
	  "drugiego klienta z naszym i s± przydatne, gdy nie jeste¶my w stanie "
	  "siê z nim sami po³±czyæ.");

	command_add(NULL, "del", "u?", cmd_del, 0,
	  " <numer/alias>|*", "usuwa u¿ytkownika z listy kontaktów",
	  "");
	
	command_add(NULL, "echo", "?", cmd_echo, 0,
	  " [tekst]", "wy¶wietla podany tekst",
	  "");
	  
	command_add(NULL, "exec", "?", cmd_exec, 0,
	  " [opcje] <polecenie>", "uruchamia polecenie systemowe",
	  "\n"
	  "  -m, --msg  [numer/alias]  wysy³a wynik do danej osoby\n"
	  "  -b, --bmsg [numer/alias]  wysy³a wynik w jednej wiadomo¶ci\n"
	  "\n"
	  "Poprzedzenie polecenia znakiem ,,%T^%n'' ukryje informacjê o "
	  "zakoñczeniu. Zapisanie opcji wielkimi literami (np. %T-B%n) "
	  "spowoduje umieszczenie polecenia w pierwszej linii wysy³anego "
	  "wyniku. Ze wzglêdu na budowê klienta, numery i aliasy "
	  "%Tnie bêd±%n dope³niane Tabem.");
	  
	command_add(NULL, "!", "?", cmd_exec, 0,
	  " [opcje] <polecenie>", "synonim dla %Texec%n",
	  "");

	command_add(NULL, "help", "cv", cmd_help, 0,
	  " [polecenie] [zmienna]", "wy¶wietla informacjê o poleceniach",
	  "\n"
	  "Mo¿liwe jest wy¶wietlenie informacji o zmiennych, je¶li jako "
	  "polecenie poda siê %Tset%n");
	  
	command_add(NULL, "?", "cv", cmd_help, 0,
	  " [polecenie] [zmienna]", "synonim dla %Thelp%n",
	  "");
	 
	command_add(NULL, "ignore", "uI", cmd_ignore, 0,
	  " [numer/alias] [poziom]", "dodaje do listy ignorowanych",
	  "\n"
	  "Dostêpne poziomy ignorowania:\n"
	  "  - status - ca³kowicie ignoruje stan\n"
	  "  - descr - ignoruje tylko opisy\n"
	  "  - notify - nie wy¶wietla zmian stanu\n"
	  "  - msg - ignoruje wiadomo¶ci\n"
	  "  - dcc - ignoruje po³±czenia DCC\n"
	  "  - events - ignoruje zdarzenia zwi±zane z u¿ytkownikiem\n"
	  "  - * - wszystkie poziomy\n"
	  "\n"
	  "Poziomy mo¿na ³±czyæ ze sob± za pomoc± przecinka lub ,,%T|%n''.");
	  
	command_add(NULL, "last", "uu", cmd_last, 0,
	  " [opcje]", "wy¶wietla lub czy¶ci ostatnie wiadomo¶ci",
	  "\n"
	  "  -c, --clear [numer/alias]      czy¶ci podane wiadomo¶ci lub wszystkie\n"
	  "  -s, --stime [numer/alias]      wy¶wietla czas wys³ania wiadomo¶ci\n"
	  "  -n, --number <n> [numer/alias] wy¶wietla %Tn%n ostatnich wiadomo¶ci\n"
	  "  [numer/alias]                  wy¶wietla ostatnie wiadomo¶ci\n"
	  "\n"
	  "W przypadku opcji %T--stime%n czas wy¶wietlany jest "
	  ",,inteligentnie'' zgodnie ze zmienn± %Ttime_deviation.%n");

	command_add(NULL, "list", "u?", cmd_list, 0,
          " [alias|@grupa|opcje]", "zarz±dzanie list± kontaktów",
	  "\n"
	  "Wy¶wietlanie osób o podanym stanie \"list [-a|-A|-i|-B|-d|-m|-o]\":\n"
	  "  -a, --active           dostêpne\n"
	  "  -A, --away             zajête\n"
	  "  -i, --inactive         niedostêpne\n"
	  "  -B, --blocked          blokuj±ce nas\n"
	  "  -d, --description      osoby z opisem\n"
	  "  -m, --member <@grupa>  osoby nale¿±ce do danej grupy\n"
	  "  -o, --offline          osoby dla których jeste¶my niedostêpni\n"
	  "\n"
	  "Wy¶wietlanie cz³onków grupy: \"list @grupa\". Wy¶wietlanie osób "
	  "spoza grupy: \"list !@grupa\".\n"
	  "\n"
	  "Zmiana wpisów listy kontaktów \"list <alias> <opcje...>\":\n"
	  "  -f, --first <imiê>\n"
	  "  -l, --last <nazwisko>\n"
	  "  -n, --nick <pseudonim>     pseudonim (nie jest u¿ywany)\n"
	  "  -d, --display <nazwa>      wy¶wietlana nazwa\n"
	  "  -u, --uin <numerek>\n"
	  "  -g, --group [+/-]<@grupa>  dodaje lub usuwa z grupy\n"
	  "                             mo¿na podaæ wiêcej grup po przecinku\n"
	  "  -p, --phone <numer>        numer telefonu komórkowego\n"
	  "  -o, --offline              b±d¼ niedostêpny dla danej osoby\n"
	  "  -O, --online               b±d¼ widoczny dla danej osoby\n"
	  "\n"
	  "Dwie ostatnie opcje dzia³aj± tylko, gdy w³±czony jest tryb ,,tylko "
	  "dla znajomych''.\n");
	  
#if 0
	command_add(NULL, "on", "?euc", cmd_on, 0,
	  " [opcje]", "zarz±dzanie zdarzeniami",
	  "\n"
	  "  -a, --add <zdarzenie> <numer/alias/@grupa> <komenda>  dodaje zdarzenie\n"
	  "  -d, --del <numer>|*         usuwa zdarzenie o podanym numerze\n"
	  " [-l, --list] [numer]         wy¶wietla listê zdarzeñ\n"
	  "\n"
	  "Dostêpne zdarzenia to:\n"
	  "  - avail, away, notavail - zmiana stanu na podany (bez przypadku ,,online'')\n"
	  "  - online - zmiana stanu z ,,niedostêpny'' na ,,dostêpny''\n"
	  "  - descr - zmiana opisu\n"
	  "  - blocked - zostali¶my zablokowani\n"
	  "  - msg, chat - wiadomo¶æ\n"
	  "  - query - nowa rozmowa\n"
	  "  - delivered, queued - wiadomo¶æ dostarczona lub zakolejkowana na serwerze\n"
	  "  - dcc - kto¶ przysy³a nam plik\n"
	  "  - sigusr1, sigusr2 - otrzymanie przez ekg danego sygna³u\n"
	  "  - newmail - otrzymanie nowej wiadomo¶ci e-mail\n"
	  "\n"
	  "W przypadku sigusr i newmail nale¿y podaæ ,,%T*%n'' jako sprawcê zdarzenia\n"
	  "\n"
	  "  - * - wszystkie zdarzenia\n"
	  "\n"
	  "Zdarzenia mo¿na ³±czyæ ze sob± za pomoc± przecinka lub ,,%T|%n''. Jako numer/alias "
	  "mo¿na podaæ ,,%T*%n'', dziêki czemu zdarzenie bêdzie dotyczyæ ka¿dego u¿ytkownika. "
	  "Je¶li kto¶ posiada indywidualn± akcjê na dane zdarzenie, to tylko ona zostanie "
	  "wykonana. Mo¿na podaæ wiêcej komend, oddzielaj±c je ¶rednikiem. W komendzie, %T\\%1%n "
	  "zostanie zast±pione numerkiem sprawcy zdarzenia, a je¶li istnieje on na naszej "
	  "li¶cie kontaktów, %T\\%2%n bêdzie zast±pione jego pseudonimem. Zamiast %T\\%3%n i "
	  "%T\\%4%n wpisana bêdzie tre¶æ wiadomo¶ci, opis u¿ytkownika, ca³kowita ilo¶æ "
	  "nowych wiadomo¶ci e-mail lub nazwa pliku - w zale¿no¶ci od zdarzenia. "
	  "Format %T\\%4%n ró¿ni siê od %T\\%3%n tym, ¿e wszystkie niebiezpieczne znaki, "
	  "które mog³yby zostaæ zinterpretowane przez shell, zostan± poprzedzone backslashem. "
	  "U¿ywanie %T\\%3%n w przypadku komendy ,,exec'' jest %Tniebezpieczne%n i, je¶li naprawdê "
	  "musisz wykorzystaæ tre¶æ wiadomo¶ci lub opis, u¿yj %T\"\\%4\"%n (w cudzys³owach).");
#endif

	command_add(NULL, "play", "f", cmd_play, 0,
	  " <plik>", "odtwarza plik d¼wiêkowy", "");

	command_add(NULL, "plugin", "?", cmd_plugin, 0,
	  " [-|+][nazwa]", "³aduje lub usuwa rozszerzenie ekg", "");

#ifdef WITH_PYTHON
	command_add(NULL, "python", "p?", cmd_python, 0,
	  " [komenda] [opcje]", "obs³uga skryptów",
	  "\n"
	  "  load <skrypt>    ³aduje skrypt\n"
	  "  unload <skrypt>  usuwa skrypt z pamiêci\n"
	  "  run <plik>       uruchamia skrypt\n"
	  "  exec <komenda>   uruchamia komendê\n"
	  " [list]            wy¶wietla listê za³adowanych skryptów");
#endif

	command_add(NULL, "query", "u?", cmd_query, 0,
	  " <numer/alias/@grupa> [wiadomo¶æ]", "w³±cza rozmowê",
	  "\n"
	  "Mo¿na podaæ wiêksz± ilo¶æ odbiorców oddzielaj±c ich numery lub "
	  "pseudonimy przecinkiem (ale bez odstêpów). W takim wypadku "
          "zostanie rozpoczêta rozmowa grupowa.");

	command_add(NULL, "queue", "uu", cmd_queue, 0,
	  " [opcje]", "zarz±dzanie wiadomo¶ciami do wys³ania po po³±czeniu",
	  "\n"
	  "  -c, --clear [numer/alias]  usuwa podane wiadomo¶ci lub wszystkie\n"
	  "  [numer/alias]              wy¶wietla kolejkê wiadomo¶ci\n"
	  "\n"
	  "Podaj±c numer lub alias, nale¿y podaæ ten, który by³ u¿ywany "
	  "przy wysy³aniu wiadomo¶ci, lub nazwê okna, w którym wiadomo¶ci "
	  "by³y wysy³ane.");
	  
	command_add(NULL, "quit", "?", cmd_quit, 0,
	  " [powód/-]", "wychodzi z programu",
	  "\n"
          "Je¶li w³±czona jest odpowiednia opcja %Trandom_reason%n i nie "
	  "podano powodu, zostanie wylosowany z pliku %Tquit.reasons%n. "
	  "Podanie ,,%T-%n'' zamiast powodu spowoduje wyczyszczenie bez "
	  "wzglêdu na ustawienia zmiennych.");
	  
	command_add(NULL, "reload", "f", cmd_reload, 0,
	  " [plik]", "wczytuje plik konfiguracyjny u¿ytkownika lub podany",
	  "");
	  
	command_add(NULL, "save", "?", cmd_save, 0,
	  " [plik]", "zapisuje ustawienia programu",
	  "\n"
	  "Aktualny stan zostanie zapisany i bêdzie przywrócony przy "
	  "nastêpnym uruchomieniu programu. Mo¿na podaæ plik, do którego "
	  "ma byæ zapisana konfiguracja.");

	command_add(NULL, "say", "?", cmd_say, 0,
	  " [tekst]", "wymawia tekst",
	  "\n"
	  "  -c, --clear  usuwa z bufora tekst do wymówienia\n"
	  "\n"
	  "Polecenie wymaga zdefiniowana zmiennej %Tspeech_app%n");
	  
	command_add(NULL, "set", "v?", cmd_set, 0,
  	  " [-]<zmienna> [[+/-]warto¶æ]", "wy¶wietla lub zmienia ustawienia",
	  "\n"
	  "U¿ycie %Tset -zmienna%n czy¶ci zawarto¶æ zmiennej. Dla zmiennych "
	  "bêd±cymi mapami bitowymi mo¿na okre¶liæ, czy warto¶æ ma byæ "
	  "dodana (poprzedzone plusem), usuniêta (minusem) czy ustawiona "
	  "(bez prefiksu). Warto¶æ zmiennej mo¿na wzi±æ w cudzys³ów. "
	  "Poprzedzenie opcji parametrem %T-a%n lub %T--all%n spowoduje "
	  "wy¶wietlenie wszystkich, nawet aktualnie nieaktywnych zmiennych.");

        command_add(NULL,  "status", "s", cmd_status, 0,
          " [opcje]", "wy¶wietla aktualny stan",
	  "\n"
          "  <uid> wy¶wietla aktualny stan dla konkretnej sesji");

	command_add(NULL, "tabclear", "?", cmd_tabclear, 0,
	  " [opcje]", "czy¶ci listê nicków do dope³nienia",
	  "\n"
	  "  -o, --offline  usuwa tylko nieobecnych");

	command_add(NULL, "conference", "??u", cmd_conference, 0,
	  " [opcje]", "zarz±dzanie konferencjami",
	  "\n"
	  "  -a, --add [#nazwa] <numer/alias/@grupa>  tworzy now± konferencjê\n"
	  "  -j, --join [#nazwa] <numer/alias>  przy³±cza osobê do konferencji\n"
	  "  -d, --del <#nazwa>|*        usuwa konferencjê\n"
	  "  -i, --ignore <#nazwa>       oznacza konferencjê jako ignorowan±\n"
	  "  -u, --unignore <#nazwa>     oznacza konferencjê jako nieignorowan±\n"
	  "  -r, --rename <#old> <#new>  zmienia nazwê konferencji\n"
	  "  -f, --find <#nazwa>         wyszukuje uczestników w katalogu\n"
	  " [-l, --list] [#nazwa]        wy¶wietla listê konferencji\n"
	  "\n"
	  "Dodaje nazwê konferencji i definiuje, kto bierze w niej udzia³. "
	  "Kolejne numery, pseudonimy lub grupy mog± byæ oddzielone "
	  "przecinkiem lub spacj±.");

	command_add(NULL, "timer", "???c", cmd_timer, 0,
	  " [opcje]", "zarz±dzanie timerami",
	  "\n"
	  "  -a, --add [nazwa] [*/]<czas> <komenda>  tworzy nowy timer\n"
	  "  -d, --del <nazwa>|*                 zatrzymuje timer\n"
	  " [-l, --list] [nazwa]                 wy¶wietla listê timerów\n"
	  "\n"
	  "Czas, po którym wykonana zostanie komenda, podaje siê w sekundach. "
	  "Mo¿na te¿ u¿yæ przyrostków %Td%n, %Th%n, %Tm%n, %Ts%n, "
	  "oznaczaj±cych dni, godziny, minuty, sekundy, np. 5h20m. Timer po "
	  "jednorazowym uruchomieniu jest usuwany, chyba ¿e czas poprzedzimy "
	  "wyra¿eniem ,,%T*/%n''. Wtedy timer bêdzie uruchamiany w zadanych odstêpach "
	  "czasu, a na li¶cie bêdzie oznaczony gwiazdk±.");

	command_add(NULL, "unignore", "i?", cmd_ignore, 0,
	  " <numer/alias>|*", "usuwa z listy ignorowanych osób",
	  "");
	  
	command_add(NULL, "version", "", cmd_version, 0,
	  "", "wy¶wietla wersjê programu",
	  "");
	  
	command_add(NULL, "window", "w?", cmd_window, 0,
	  " <komenda> [numer_okna]", "zarz±dzanie okno",
	  "\n"
	  "  active               prze³±cza do pierwszego okna,\n"
	  "                       w którym co¶ siê dzieje\n"
	  "  clear                czy¶ci aktualne okno\n"
	  "  kill [numer_okna]    zamyka aktualne lub podane okno\n"
	  "  last                 prze³±cza do ostatnio wy¶wietlanego\n"
	  "                       okna\n"
	  "  list                 wy¶wietla listê okien\n"
	  "  new [nazwa]          tworzy nowe okno\n"
	  "  next                 prze³±cza do nastêpnego okna\n"
	  "  prev                 prze³±cza do poprzedniego okna\n"
	  "  switch <numer_okna>  prze³±cza do podanego okna\n"
	  "  refresh              od¶wie¿a aktualne okno");

	command_add(NULL, "_watches", "", cmd_debug_watches, 0, "", "wy¶wietla listê przegl±danych deskryptorów", "");
	command_add(NULL, "_queries", "", cmd_debug_queries, 0, "", "wy¶wietla listê zapytañ", "");
	command_add(NULL, "_query", "??????????", cmd_debug_query, 0, " <zapytanie> [parametry...]", "generuje zapytanie", "");

	command_add(NULL, "_addtab", "??", cmd_test_addtab, 0, "", "dodaje do listy dope³niania TABem", "");
	command_add(NULL, "_deltab", "??", cmd_test_deltab, 0, "", "usuwa z listy dope³niania TABem", "");
	command_add(NULL, "_fds", "", cmd_test_fds, 0, "", "wy¶wietla otwarte pliki", "");
	command_add(NULL, "_msg", "u?", cmd_test_send, 0, "", "udaje, ¿e wysy³a wiadomo¶æ", "");
	command_add(NULL, "_segv", "", cmd_test_segv, 0, "", "wywo³uje naruszenie segmentacji pamiêci", "");
	command_add(NULL, "_debug", "?", cmd_test_debug, 0, "", "wy¶wietla tekst w oknie debug", "");
	command_add(NULL, "_debug_dump", "", cmd_test_debug_dump, 0, "", "zrzuca debug do pliku", "");

	command_add(NULL, "session", "AA??", session_command, 0,
	  " [opcje]", "zarz±dzanie sesjami",
	  "\n"
	  " <uid>	  	informacje o sesji\n"
	  "  -a, --add <uid>    tworzy now± sesjê\n"
	  "  -d, --del <uid>    usuwa sesjê\n"
	  " [-l, --list]        wy¶wietla listê\n"
	  " [-g, --get] [<uid>] <opcja>\n"
	  "                     wy¶wietla opcjê sesji\n"
	  " [-s, --set] [<uid>] <opcja> <warto¶æ>\n"
	  "                     zmienia opcjê sesji\n"
	  " [-s, --set] [<uid>] -<opcja>\n"
	  "                     usuwa opcjê sesji\n"
	  " [-w, --sw] <uid> zmienia aktualn± sesjê\n"
	  "");
}

/*
 * command_free()
 *
 * usuwa listê komend z pamiêci.
 */
void command_free()
{
	list_t l;

	for (l = commands; l; l = l->next) {
		command_t *c = l->data;

		xfree(c->name);
		xfree(c->params);
		xfree(c->params_help);
		xfree(c->brief_help);
		xfree(c->long_help);
	}

	list_destroy(commands, 1);
	commands = NULL;
}
