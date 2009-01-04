/* $Id: vars.h 4062 2008-07-08 08:17:16Z darkjames $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EKG_VARS_H
#define __EKG_VARS_H

#include "plugins.h"

typedef enum {
	VAR_STR,		/* ci±g znaków */
	VAR_INT,		/* liczba ca³kowita */
	VAR_BOOL,		/* 0/1, tak/nie, yes/no, on/off */
	VAR_MAP,		/* bitmapa */
	VAR_FILE,		/* plik */
	VAR_DIR,		/* katalog */
	VAR_THEME,		/* theme */

	VAR_REMOTE		/* remote, not used by plugins */
} variable_class_t;

typedef struct {
	char *label;		/* nazwa warto¶ci */
	int value;		/* warto¶æ */
	int conflicts;		/* warto¶ci, z którymi koliduje */
} variable_map_t;

typedef void (variable_notify_func_t)(const char *);
typedef void (variable_check_func_t)(const char *, const char *);
typedef int (variable_display_func_t)(const char *);

typedef struct variable {
	struct variable *next;

	char *name;				/* ekg2-remote: OK */
	plugin_t *plugin;			/* ekg2-remote: NONE */
	int name_hash;				/* ekg2-remote: OK */
	int type;				/* ekg2-remote: VAR_STR, ncurses completion BAD */
	int display;				/* ekg2-remote: ? 0 bez warto¶ci, 1 pokazuje, 2 w ogóle */
	void *ptr;				/* ekg2-remote: OK, olewamy wartosc */
	variable_check_func_t *check;		/* ekg2-remote: BAD */
	variable_notify_func_t *notify;		/* ekg2-remote: ? */
	variable_map_t *map;			/* ekg2-remote: BAD */
	variable_display_func_t *dyndisplay;	/* ekg2-remote: BAD */
} variable_t;

extern variable_t *variables;

void variable_init();
variable_t *variable_find(const char *name);
variable_map_t *variable_map(int count, ...);

variable_t *variable_add(plugin_t *plugin, const char *name, int type, int display, void *ptr, variable_notify_func_t *notify, variable_map_t *map, variable_display_func_t *dyndisplay);
variable_t *remote_variable_add(const char *name, const char *value);

variable_t *variables_removei(variable_t *v);

void variables_destroy();

#endif /* __EKG_VARS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
