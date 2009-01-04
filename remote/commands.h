/* $Id: commands.h 4528 2008-08-28 08:19:35Z darkjames $ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
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

#include "plugins.h"
#include "themes.h"
#include "sessions.h"

#define printq(x...) do { if (!quiet) { print(x); } } while(0)

#define COMMAND(x) int x(const char *name, const char **params, session_t *session, const char *target, int quiet)

typedef COMMAND(command_func_t);

#define COMMAND_REMOTE 0x01

typedef struct command {
	struct command	*next;

/* public: */
	char		*name;				/* ekg2-remote: OK */
	plugin_t 	*__plugin;			/* ekg2-remote: NONE */		/* ncurses: OK */

/* private: */
	char		**params;			/* ekg2-remote: OK */
	command_func_t	*__function;			/* ekg2-remote: NONE, OK */
	int		flags;				/* ekg2-remote: NONE */
	char		**possibilities;		/* ekg2-remote: NONE */
} command_t;

extern command_t *commands;

command_t *command_add(plugin_t *plugin, const char *name, char *params, command_func_t function, int flags, char *possibilities);
command_t *remote_command_add(const char *name, char *params);

void command_init();
command_t *commands_removei(command_t *c);
void commands_destroy();
int command_exec(const char *target, session_t *session, const char *line, int quiet);
int command_exec_format(const char *target, session_t *session, int quiet, const char *format, ...);
/*
 * jaka¶ malutka lista tych, do których by³y wysy³ane wiadomo¶ci.
 */
#define SEND_NICKS_MAX 100

extern char *send_nicks[SEND_NICKS_MAX];
extern int send_nicks_count, send_nicks_index;

void tabnick_add(const char *nick);
void tabnick_remove(const char *nick);

int binding_help(int a, int b);
int binding_quick_list(int a, int b);

int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen);

#endif /* __EKG_COMMANDS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
