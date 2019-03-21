/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include <stdarg.h>
#include <glib.h>

#include "commands.h"
#include "dynstuff.h"
#include "sessions.h"
#include "themes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reserved window id: 1000-1999
 * windows reverved for special use.
 *	1000 - __contacts,
 *	1001 - __lastlog
 */
typedef enum {
	WINDOW_DEBUG_ID		= 0,
	WINDOW_RESERVED_MIN_ID	= 1000,
	WINDOW_CONTACTS_ID	= 1000,
	WINDOW_LASTLOG_ID	= 1001,
	WINDOW_RESERVED_MAX_ID	= 1999
} window_reserved_id_t;

typedef struct {
	void *w;			/* window, if NULL it means current */
	int casense		: 2;	/* 0 - ignore case; 1 - don't ignore case, -1 - use global variable */
	unsigned int lock	: 1;	/* if 0, don't update */
	unsigned int isregex	: 1;	/* 1 - in target regexp */
	GRegex *reg;			/* regexp compilated expression */
	char *expression;		/* expression */
} window_lastlog_t;

typedef enum {
	EKG_WINACT_NONE = 0,		/* No activity in window */
	EKG_WINACT_JUNK,		/* Junks: status change, irc join/part, etc. */
	EKG_WINACT_MSG,			/* Message, but not to us */
	EKG_WINACT_IMPORTANT		/* important message */
} winact_t;

typedef struct window {
	struct window *next;

	unsigned short id;		/* numer okna */
	char *target;			/* nick query albo inna nazwa albo NULL */
	char *alias;			/* name for display */
	session_t *session;		/* kt�rej sesji dotyczy okno */

	unsigned short left, top;	/* pozycja (x, y) wzgl�dem pocz�tku ekranu */
	unsigned short width, height;	/* wymiary okna */

	unsigned int act	: 2;	/* activity: 1 - status/junk; 2 - msg ; 3 - msg to us */
	unsigned int in_typing	: 1;	/* user is composing a message to us */
	unsigned int more	: 1;	/* pojawi�o si� co� poza ekranem */
	unsigned int floating	: 1;	/* czy p�ywaj�ce? */
	unsigned int doodle	: 1;	/* czy do gryzmolenia?		[we don't set it anywhere] */

	unsigned int frames	: 4;	/* informacje o ramkach */
	unsigned int edge	: 4;	/* okienko brzegowe */

	unsigned int nowrap	: 1;	/* nie zawijamy linii */
	unsigned int hide	: 1;	/* ukrywamy, bo jest zbyt du�e */

	unsigned int last_chatstate;	/* last chat state */
	time_t last_update;		/* czas ostatniego uaktualnienia */
	unsigned short lock;		/* blokowanie zmian w obr�bie komendy */

	struct userlist *userlist;	/* sometimes window may require separate userlist */

	window_lastlog_t *lastlog;	/* prywatne informacje lastloga */
	void *priv_data;			/* prywatne informacje ui */
} window_t;

#ifndef EKG2_WIN32_NOFUNCTION

extern window_t *windows;
extern window_t *window_debug;
extern window_t *window_status;
extern window_t *window_current;

window_t *window_find(const char *target);
window_t *window_find_sa(session_t *session, const char *target, int session_null_means_no_session);

#define window_find_s(s, target) window_find_sa(s, target, 1)	/* XXX, need checking */
window_t *window_find_ptr(window_t *w);
window_t *window_new(const char *target, session_t *session, int new_id);
void window_kill(window_t *w);
void window_switch(int id);
window_t *window_exist(int id);
void window_print(window_t *w, fstring_t *line);
void print_window_w(window_t *w, int activity, const char *theme, ...);	/* themes.c */
void vprint_window_w(window_t *w, int activity, const char *theme, va_list ap);
char *window_target(window_t *window);

void window_session_set(window_t *w, session_t *newsession);
int window_session_cycle(window_t *w);

int window_lock_inc(window_t *w);
int window_lock_dec(window_t *w);

void windows_destroy(void);
#endif

COMMAND(cmd_window);

#ifdef __cplusplus
}
#endif

#endif /* __EKG_WINDOW_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
