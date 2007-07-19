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

#ifndef __EKG_THEMES_H
#define __EKG_THEMES_H

#include "strings.h"

#include "gettext.h" 
#define _(a) gettext(a)
#define N_(a) gettext_noop(a)

#include "dynstuff.h"
#include "sessions.h"

typedef struct {
	union {
		char *b;	/* possibly multibyte string */
		CHAR_T *w;	/* wide char string */
	} str;		/* A \0-terminated string of characters. Before the
	fstring_t is added to history, should be referred to using 'str->b'.
	Adding to history recodes it to CHAR_T, so afterwards it should be
	referred to by 'str->w'. */
	short *attr;	/* atrybuty, ci±g o d³ugo¶ci strlen(str) */
	int ts;		/* timestamp */

	int prompt_len;	/* d³ugo¶æ promptu, który bêdzie powtarzany przy i
			   przej¶ciu do kolejnej linii. */
	int prompt_empty;	/* prompt przy przenoszeniu bêdzie pusty */
	int margin_left; 	/* where the margin is set (on what char) */
	void *private;          /* can be helpfull */
} fstring_t;

#define print(x...)		print_window_w(NULL, 0, x) 
#define wcs_print(x...) 	print_window_w(NULL, 0, x)
#define print_status(x...) 	print_window_w(window_status, 0, x)

#ifndef EKG2_WIN32_NOFUNCTION

void print_window(const char *target, session_t *session, int separate, const char *theme, ...);

void format_add(const char *name, const char *value, int replace);
const char *format_find(const char *name);
char *format_string(const char *format, ...);

void theme_init();
void theme_plugins_init();
int theme_read(const char *filename, int replace);
void theme_cache_reset();
void theme_free();

fstring_t *fstring_new(const char *str);
void fstring_free(fstring_t *str);

#endif

/*
 * makro udaj±ce isalpha() z LC_CTYPE="pl_PL". niestety ncurses co¶ psuje
 * i ¼le wykrywa p³eæ.
 */
#define isalpha_pl_PL(x) ((x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || x == '±' || x == 'æ' || x == 'ê' || x == '³' || x == 'ñ' || x == 'ó' || x == '¶' || x == '¿' || x == '¼' || x == '¡' || x == 'Æ' || x == 'Ê' || x == '£' || x == 'Ñ' || x == 'Ó' || x == '¦' || x == '¯' || x == '¬')

#define FSTR_FOREA 		1
#define FSTR_FOREB 		2
#define FSTR_FOREC 		4
#define FSTR_FOREMASK 		(FSTR_FOREA|FSTR_FOREB|FSTR_FOREC)
#define FSTR_BACKA 		8
#define FSTR_BACKB 		16
#define FSTR_BACKC 		32
#define FSTR_BACKMASK 		(FSTR_BACKA|FSTR_BACKB|FSTR_BACKC)
#define FSTR_BOLD 		64
#define FSTR_NORMAL 		128
#define FSTR_BLINK 		256
#define FSTR_UNDERLINE 		512
#define FSTR_REVERSE 		1024

#endif /* __EKG_THEMES_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
