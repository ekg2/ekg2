/* $Id: commands.c 4592 2008-09-01 19:12:07Z peres $ */

/*
 *  (C) Copyright 2001-2005 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Pawe³ Maziarz <drg@infomex.pl>
 *			    Wojciech Bojdo³ <wojboj@htc.net.pl>
 *			    Piotr Wysocki <wysek@linux.bydg.org>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
 *			    Kuba Kowalski <qbq@kofeina.net>
 *			    Piotr Kupisiewicz <deli@rzepaknet.us>
 *			    Leszek Krupiñski <leafnode@wafel.com>
 *			    Adam Mikuta <adamm@ekg2.org>
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

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#define _BSD_SOURCE

#include <string.h>
#include <ctype.h>

#include "commands.h"
#include "debug.h"
#include "dynstuff.h"
#include "sessions.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "windows.h"
#include "xmalloc.h"
#include "queries.h"
#include "dynstuff_inline.h"

char *send_nicks[SEND_NICKS_MAX] = { NULL };
int send_nicks_count = 0, send_nicks_index = 0;
static int quit_command = 0;

command_t *commands = NULL;

static LIST_ADD_COMPARE(command_add_compare, command_t *) { return xstrcasecmp(data1->name, data2->name); }
static LIST_FREE_ITEM(list_command_free, command_t *) { xfree(data->name); array_free(data->params); array_free(data->possibilities); }

DYNSTUFF_LIST_DECLARE_SORTED(commands, command_t, command_add_compare, list_command_free,
	static __DYNSTUFF_LIST_ADD_SORTED,	/* commands_add() */
	EXPORTNOT __DYNSTUFF_LIST_REMOVE_ITER,	/* commands_removei() */
	EXPORTNOT __DYNSTUFF_LIST_DESTROY)	/* commands_destroy() */

int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen)
{
	if (!arg || *arg != '-')
		return 0;

	arg++;
	if (*arg == '-') {
		int len = strlen(++arg);

		if (longoptlen > len)
			len = longoptlen;

		return !xstrncmp(arg, longopt, len);
	}
	return (*arg == shortopt) && (*(arg + 1) == 0);
}

void tabnick_add(const char *nick) {
	int i;

	if (!nick)
		return;

	for (i = 0; i < send_nicks_count; i++) {
		if (send_nicks[i] && !strcmp(nick, send_nicks[i])) {
			tabnick_remove(nick);
			break;
		}
	}

	if (send_nicks_count == SEND_NICKS_MAX) {
		xfree(send_nicks[SEND_NICKS_MAX - 1]);
		send_nicks_count--;
	}

	memmove(&send_nicks[1], &send_nicks[0], send_nicks_count * sizeof(send_nicks[0]) );

	send_nicks_count++;
	
	send_nicks[0] = xstrdup(nick);
}

void tabnick_remove(const char *nick) {
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

static COMMAND(cmd_forward_to_server) {
	debug_error("cmd_forward_to_server(%s) shouldn't be called!\n", name);
	return -1;
}

static COMMAND(cmd_real_quit) {
	int ret;

	quit_command = 1;	/* czemu nie? */
	if (params[0]) {
		char *tmp = saprintf("/QUIT %s", params[0]);
		ret = remote_request("REQEXECUTE", tmp, NULL);
		xfree(tmp);
	} else
		ret = remote_request("REQEXECUTE", "/QUIT", NULL);

	return ret;
}

static COMMAND(cmd_quit) {
	quit_command = 1;

	return 0;
}

static char *strip_spaces(char *line) {
	size_t linelen;
	char *buf;

	if (!(linelen = strlen(line)))
		return line;
	
	for (buf = line; xisspace(*buf); buf++);

	while (linelen > 0 && xisspace(line[linelen - 1])) {
		line[linelen - 1] = 0;
		linelen--;
	}
	
	return buf;
}

int command_exec(const char *target, session_t *session, const char *xline, int quiet)
{
	const char *p = NULL;
	const char *line = NULL;
	char *cmd = NULL;
	size_t cmdlen;

	command_t *last_command = NULL;
	command_t *last_command_plugin = NULL; /* niepotrzebne, ale ktos to napisal tak ze moze kiedys mialobyc potrzebne.. wiec zostaje. */
	int abbrs = 0;
	int abbrs_plugins = 0;

	int exact = 0;

	command_t *c;

	if (!xline)
		return 0;

	/* wysy³amy do kogo¶ i nie ma na pocz±tku slasha */
	if (target && *xline != '/') {
		int correct_command = 0;
	
		/* wykrywanie przypadkowo wpisanych poleceñ */
		if (config_query_commands) {
			for (c = commands; c; c = c->next) {
				size_t l = strlen(c->name);

				if (l < 3 || xstrncasecmp(xline, c->name, l))
					continue;
				
				if (!xline[l] || xisspace(xline[l])) {
					correct_command = 1;
					break;
				}
			}		
		}

		if (!correct_command)
			return command_exec_format(target, session, quiet, ("/ %s"), xline);
	}
	if (target && *xline == '/' && config_slash_messages == 1) {
	/* send message if we have two '/' in 1 word for instance /bin/sh /dev/hda1 other... */
		/* code stolen from ekg1, idea stolen from irssi. */
		char *p = strchr(xline + 1, '/');
		char *s = strchr(xline + 1, ' ');

		if (p && (!s || p < s)) 
			return command_exec_format(target, session, quiet, ("/ %s"), xline);
	}

	
	send_nicks_index = 0;
	line = xline;

	if (*line == '/')
		line++;

	if (*line == '^') {
		quiet = 1;
		line++;
	}

	if (line[0] && !isalpha(line[0])) {
		for (c = commands; c; c = c->next) {
			if (line[0] == c->name[0] && c->name[1] == '\0') {
				cmd = xstrndup(c->name, 1);
				cmdlen = 1;
				p = line + 1;
				break;
			}
		}
	}

	if (!cmd) {
		char *tmp;

		tmp = cmd = xstrdup(line);
		while (*tmp && !xisspace(*tmp))
			tmp++;
		p = (*tmp) ? tmp + 1 : tmp;
		*tmp = 0;
		p = strip_spaces((char *) p);

		cmdlen = strlen(cmd);
	}

	/* poszukaj najpierw komendy dla danej sesji */
	if (!session && session_current)
		session = session_current;
	if (session && session->uid) {
		int plen = (int)(strchr(session->uid, ':') - session->uid) + 1;
		
		for (c = commands; c; c = c->next) {
			if (xstrncasecmp(c->name, session->uid, plen))
				continue;
		
			if (!xstrcasecmp(c->name + plen, cmd)) {
				last_command = c;
				abbrs = 1;
				exact = 1;
				break;
			}

			if (!xstrncasecmp(c->name + plen, cmd, cmdlen)) {
				last_command_plugin = c;
				abbrs_plugins++;
			} else {
				if (last_command_plugin && abbrs_plugins == 1)
					break;
			} 
		}
	}
	if (!exact) {
		for (c = commands; c; c = c->next) {
			if (!xstrcasecmp(c->name, cmd)) {
				last_command = c;
				abbrs = 1;
				exact = 1;
				/* if this is exact_match we should zero those below, they won't be used */
				abbrs_plugins = 0; 
				last_command_plugin = NULL;
				break;
			}
			if (!xstrncasecmp(c->name, cmd, cmdlen)) {
				last_command = c;
				abbrs++;
			} else {
				if (last_command && abbrs == 1)
					break;
			}
		} 
	}

	if ((last_command && abbrs == 1 && !abbrs_plugins) || ( (last_command = last_command_plugin) && abbrs_plugins == 1 && !abbrs)) {
		session_t *s = session ? session : window_current->session;
		const char *last_name = last_command->name;
		char *tmp;
		int res;
		char **par;

		if (last_command->flags & COMMAND_REMOTE)
			goto send_dont_matter;

		if ((tmp = strchr(last_name, ':')))
			last_name = tmp + 1;
		
		par = array_make(p, (" \t"), array_count(last_command->params), 1, 1);

		res = (last_command->__function)(last_name, (const char **) par, s, target, (quiet & 1));

		query_emit_id(NULL, UI_WINDOW_REFRESH);		/* XXX? */

		array_free(par);
		xfree(cmd);

		if (quit_command)
			ekg_exit();

		return res;
	}

	xfree(cmd);

	if (cmdlen) {
send_dont_matter:
		/* XXX, /for -s sesja -w okno? */
		/* XXX, quiet */
		windows_lock_all();			/* XXX!!! */
		return remote_request("REQEXECUTE", xline, NULL);
	}

	return -1;
}

int command_exec_format(const char *target, session_t *session, int quiet, const char *format, ...) {
	char *command;
	va_list ap;
	int res;
	
	va_start(ap, format);
	command = vsaprintf(format, ap);
	va_end(ap);
	
	if (!command) 
		return 0;
/*	debug("[command_exec_format] %s\n", command); */
	res = command_exec(target, session, command, quiet); /* segvuje na tym ? wtf ?! */
	xfree(command);
	return res;
}

int binding_help(int a, int b)	
{
	print("help_quick");  

	return 0;  
}

static const char *ekg_status_label(const int status)
{
	static char buf[100];
	const char *status_string = ekg_status_string(status, 0);
	
	snprintf(buf, sizeof(buf), "quick_list_%s", status_string);

	return buf;
}

int binding_quick_list(int a, int b)
{
	string_t list = string_init(NULL);
	userlist_t *ul;
	session_t *s;

	for (s = sessions; s; s = s->next) {
		for (ul = s->userlist; ul; ul = ul->next) {
			userlist_t *u = ul;
			const char *format;

			if (!u->nickname)
				continue;
		
			format = format_find(ekg_status_label(u->status));

			if (format_ok(format)) {
				char *tmp = format_string(format, u->nickname);
				string_append(list, tmp);

				xfree(tmp);
			}
		}
	}

	if (list->len > 0)
		print("quick_list", list->str);

	string_free(list, 1);

	return 0;
}

static command_t *command_find(const char *name) {
	command_t *c;

	if (!name)
		return NULL;

	for (c = commands; c; c = c->next) {
		if (!xstrcasecmp(c->name, name))
			return c;
	}
	return NULL;
}

static command_t *command_add_c(plugin_t *plugin, const char *name, char *params, command_func_t function, int flags, char *possibilities) {
	command_t *c;

	if (!name) 
		return NULL;

	c = xmalloc(sizeof(command_t));

	c->name = xstrdup(name);
	c->params = params ? array_make(params, (" "), 0, 1, 1) : NULL;
	c->__function = function;
	c->flags = flags;
	c->__plugin = plugin;
	c->possibilities = possibilities ? array_make(possibilities, " ", 0, 1, 1) : NULL;

	commands_add(c);
	return c;
}

command_t *command_add(plugin_t *plugin, const char *name, char *params, command_func_t function, int flags, char *possibilities) {
	if (flags) {
		debug_error("command_add(%s) flags=%d this might not work!\n", name, flags);
	}

	return command_add_c(plugin, name, params, function, 0, possibilities);
}

EXPORTNOT command_t *remote_command_add(const char *name, char *params) {
	if (command_find(name)) {
		debug_error("remote_command_add(%s) already registered!\n", name);
		return NULL;
	}

	return command_add_c(NULL, name, params, cmd_forward_to_server, COMMAND_REMOTE, NULL);
}

EXPORTNOT void command_init()
{
	/* XXX, ekg2-remote: zastanowic sie czy nie mamy innych potrzebnych.. */

	command_add(NULL, ("quit"), NULL, cmd_quit, 0, NULL);
	command_add(NULL, ("exit"), "r", cmd_real_quit, 0, NULL);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
