/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
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

#ifndef __EKG_COMMANDS_H
#define __EKG_COMMANDS_H

#include "dynstuff.h"
#include "plugins.h"
#include "sessions.h"

#define printq(x...) do { if (!quiet) { print(x); } } while(0)

#define COMMAND(x) int x(const char *name, const char **params, session_t *session, const char *target, int quiet)

typedef COMMAND(command_func_t);

typedef struct {
	/* public: */
	char *name;
	plugin_t *plugin;

	/* private: */
	char **params;
	command_func_t *function;
	int alias;
	char **possibilities;
} command_t;

list_t commands;

int command_add(plugin_t *plugin, const char *name, char *params, command_func_t function, int alias, char *possibilities);
int command_remove(plugin_t *plugin, const char *name);
command_t *command_find (const char *name);
void command_init();
void command_free();
int command_exec(const char *target, session_t *session, const char *line, int quiet);

COMMAND(cmd_alias_exec);
COMMAND(cmd_exec);
COMMAND(cmd_list);
COMMAND(cmd_dcc);
COMMAND(session_command);	/* sessions.c */
COMMAND(cmd_on);	/* events.c */
COMMAND(cmd_metacontact); /* metacontacts.c */

/*
 * jaka¶ malutka lista tych, do których by³y wysy³ane wiadomo¶ci.
 */
#define SEND_NICKS_MAX 100

char *send_nicks[SEND_NICKS_MAX];
int send_nicks_count, send_nicks_index;

void tabnick_add(const char *nick);
void tabnick_remove(const char *nick);
void tabnick_flush();

int binding_help(int a, int b);
int binding_quick_list(int a, int b);
int binding_toggle_contacts(int a, int b);

int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen);

/* wyniki ostatniego szukania */
char *last_search_first_name;
char *last_search_last_name;
char *last_search_nickname;
char *last_search_uid;


#endif /* __EKG_COMMANDS_H */
