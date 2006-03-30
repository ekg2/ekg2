/* $Id$ */

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

#include "dynstuff.h"
#include "plugins.h"

typedef enum {
	VAR_STR,		/* ci±g znaków */
	VAR_INT,		/* liczba ca³kowita */
	VAR_BOOL,		/* 0/1, tak/nie, yes/no, on/off */
	VAR_FOREIGN,		/* nieznana zmienna */
	VAR_MAP,		/* bitmapa */
	VAR_FILE,		/* plik */
	VAR_DIR,		/* katalog */
	VAR_THEME		/* theme */
} variable_class_t;

typedef struct {
	char *label;		/* nazwa warto¶ci */
	int value;		/* warto¶æ */
	int conflicts;		/* warto¶ci, z którymi koliduje */
} variable_map_t;

typedef void (variable_notify_func_t)(const char *);
typedef void (variable_check_func_t)(const char *, const char *);
typedef int (variable_display_func_t)(const char *);

typedef struct {
	char *name;		/* nazwa zmiennej */
	plugin_t *plugin;	/* wstyczka obs³uguj±ca zmienn± */
	int name_hash;		/* hash nazwy zmiennej */
	int type;		/* rodzaj */
	int display;		/* 0 bez warto¶ci, 1 pokazuje, 2 w ogóle */
	void *ptr;		/* wska¼nik do zmiennej */
	variable_check_func_t *check;
				/* funkcja sprawdzaj±ca czy warto¶æ jest
				 * prawid³owa */
	variable_notify_func_t *notify;
				/* funkcja wywo³ywana po zmianie warto¶ci */
	variable_map_t *map;	/* mapa warto¶ci i etykiet */
	variable_display_func_t *dyndisplay;
				/* funkcja sprawdzaj±ca, czy zmienn± mo¿na
				 * wy¶wietliæ na li¶cie zmiennych */
} variable_t;

list_t variables;

void variable_init();
void variable_set_default();
variable_t *variable_find(const char *name);
variable_map_t *variable_map(int count, ...);
#define variable_hash ekg_hash

variable_t *variable_add(
	plugin_t *plugin,
	const CHAR_T *name_,
	int type,
	int display,
	void *ptr,
	variable_notify_func_t *notify,
	variable_map_t *map,
	variable_display_func_t *dyndisplay);

int variable_remove(plugin_t *plugin, const char *name);

int variable_set(const char *name, const char *value, int allow_foreign);
void variable_help(const char *name);
void variable_free();

#endif /* __EKG_VARS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
