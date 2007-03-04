/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 * 		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#ifndef __EKG_WINDOWS_H
#define __EKG_WINDOWS_H

#include "ekg2-config.h"

#include "commands.h"
#include "dynstuff.h"
#include "sessions.h"
#include "themes.h"

#ifdef HAVE_REGEX_H
#include <sys/types.h>
#include <regex.h>
#endif

typedef struct {
	void *w;		/* window, if NULL it means current */
	int casense;		/* 0 - ignore case; 1 - don't ignore case, -1 - use global variable */
	int lock;		/* if 0, don't update */
	int isregex;		/* 1 - in target regexp */
#ifdef HAVE_REGEX_H
	regex_t reg;		/* regexp compilated expression */
#endif
	char *expression;	/* expression */
} window_lastlog_t;

typedef struct {
	int id;			/* numer okna */
	char *target;		/* nick query albo inna nazwa albo NULL */
	session_t *session;	/* której sesji dotyczy okno */

	int left, top;		/* pozycja (x, y) wzglêdem pocz±tku ekranu */
	int width, height;	/* wymiary okna */

	int act;		/* czy co¶ siê zmieni³o? */
	int more;		/* pojawi³o siê co¶ poza ekranem */

	int floating;		/* czy p³ywaj±ce? */
	int doodle;		/* czy do gryzmolenia? */
	int frames;		/* informacje o ramkach */
	int edge;		/* okienko brzegowe */
	int last_update;	/* czas ostatniego uaktualnienia */
	int nowrap;		/* nie zawijamy linii */
	int hide;		/* ukrywamy, bo jest zbyt du¿e */
	int lock;		/* blokowanie zmian w obrêbie komendy */

	list_t userlist;	/* sometimes window may require separate userlist */

	window_lastlog_t *lastlog;	/* prywatne informacje lastloga */
	void *private;		/* prywatne informacje ui */
} window_t;

#ifndef EKG2_WIN32_NOFUNCTION

extern list_t windows;
extern window_t *window_current;
extern window_lastlog_t *lastlog_current;

window_t *window_find(const char *target);
window_t *window_find_s(session_t *session, const char *target);
window_t *window_find_ptr(window_t *w);
window_t *window_new(const char *target, session_t *session, int new_id);
void window_kill(window_t *w, int quiet);
void window_switch(int id);
window_t *window_exist(int id);
void window_print(const char *target, session_t *session, int separate, fstring_t *line);
char *window_target(window_t *window);

int window_session_cycle(window_t *w);
#define window_session_cycle_n(a) window_session_cycle(window_find(a))
#define window_session_cycle_id(a) window_session_cycle_id(window_find_id(a))
int window_session_set(window_t *w, session_t *s);
#define window_session_set_n(a,b) window_session_set(window_find(a),b)
#define window_session_set_id(a,b) window_session_set(window_find_id(a),b)
session_t *window_session_get(window_t *w);
#define window_session_get_n(a) window_session_get(window_find(a))
#define window_session_get_id(a) window_session_get(window_find_id(a))

int window_lock_set(window_t *w, int lock);
int window_lock_get(window_t *w);
int window_lock_inc(window_t *w);
#define window_lock_inc_n(a) window_lock_inc(window_find(a))
int window_lock_dec(window_t *w);
#define window_lock_dec_n(a) window_lock_dec(window_find(a))
#endif

COMMAND(cmd_window);

#endif /* __EKG_WINDOW_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
