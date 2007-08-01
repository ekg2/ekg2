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
#include "themes.h"
#include "sessions.h"

#define printq(x...) do { if (!quiet) { print(x); } } while(0)
#define wcs_printq(x...) do { if (!quiet) { print(x); } } while(0)

#define COMMAND(x) int x(const char *name, const char **params, session_t *session, const char *target, int quiet)

/* INFORMATIONAL FLAGS */
	/* command is binded by alias managment */
#define COMMAND_ISALIAS 		0x01
	/* command is binded by script mangament */
#define COMMAND_ISSCRIPT		0x02
/* .... */

/* CONDITIONAL FLAGS */
	/* '!' in params means that arg must exist in par[..] (?) */
#define COMMAND_ENABLEREQPARAMS 	0x10
	/* when par[0] != NULL, than target = par[0] and than par list moves up (par++ ; par[0] == par[1] and so on */
#define COMMAND_PARAMASTARGET		0x20
	/* session must be connected to execute that command */
#define SESSION_MUSTBECONNECTED 	0x40
	/* command must come from the same plugin as session (?) */
#define SESSION_MUSTBELONG		0x80
	/* if session == NULL, we try session_current, if still NULL. we return -1... mh, i really don't know if this 
	 * flag is obsolete... but we do simillar thing in many places in code, so implemented. */
#define SESSION_MUSTHAS			0x100
	/* session must exist and has private struct */
#define SESSION_MUSTHASPRIVATE		0x200
	/* before executing handler, check if target (or params[0] if COMMAND_PARAMASTARGET set) is valid uid for current session, or we've got smb with this nickname
	 * on userlist... (read: we check if get_uid(session, target) return smth, if not we print message) */
#define COMMAND_TARGET_VALID_UID	0x400

typedef COMMAND(command_func_t);

typedef struct {
	/* public: */
	const char *name;
	plugin_t *plugin;

	/* private: */
	char**params;
	command_func_t *function;
	int flags;
	char **possibilities;
} command_t;

#ifndef EKG2_WIN32_NOFUNCTION
extern list_t commands;
extern list_t *commands_lock;

command_t *command_add(plugin_t *plugin, const char *name, char *params, command_func_t function, int flags, char *possibilities);
void command_freeone(command_t *c);
int command_remove(plugin_t *plugin, const char *name);
command_t *command_find (const char *name);
void command_init();
void command_free();
int command_exec(const char *target, session_t *session, const char *line, int quiet);
int command_exec_format(const char *target, session_t *session, int quiet, const char *format, ...);

COMMAND(cmd_add);
COMMAND(cmd_alias_exec);
COMMAND(cmd_exec);
COMMAND(cmd_list);
COMMAND(cmd_dcc);
COMMAND(session_command);	/* sessions.c */
COMMAND(cmd_on);		/* events.c */
COMMAND(cmd_metacontact);	/* metacontacts.c */
COMMAND(cmd_streams);		/* audio.c */
COMMAND(cmd_script);		/* script.c */
#endif
/*
 * jaka¶ malutka lista tych, do których by³y wysy³ane wiadomo¶ci.
 */
#define SEND_NICKS_MAX 100

extern char *send_nicks[SEND_NICKS_MAX];
extern int send_nicks_count, send_nicks_index;

#ifndef EKG2_WIN32_NOFUNCTION
void tabnick_add(const char *nick);
void tabnick_remove(const char *nick);
void tabnick_flush();

int binding_help(int a, int b);
int binding_quick_list(int a, int b);
int binding_toggle_contacts(int a, int b);

int match_arg(const char *arg, char shortopt, const char *longopt, int longoptlen);

/* wyniki ostatniego szukania */
extern char *last_search_first_name;
extern char *last_search_last_name;
extern char *last_search_nickname;
extern char *last_search_uid;
#endif

#endif /* __EKG_COMMANDS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
