/* $Id: themes.h 4542 2008-08-28 18:42:26Z darkjames $ */

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

#define _(a) (a)
#define N_(a) (a)

typedef struct {
	union {
		char	*b;			/* ekg2-remote: almost OK */
		CHAR_T	*w;
	} str;

	short		*attr;			/* ekg2-remote: almost OK */
	time_t		ts;			/* ekg2-remote: OK */

	int		prompt_len;		/* ekg2-remote: BAD */
	unsigned int	prompt_empty	: 1;	/* ekg2-remote: BAD */
	int		margin_left;		/* ekg2-remote: BAD */
	void		*private;		/* ekg2-remote: NULL, unused? */
} fstring_t;

#define print(x...)		print_window_w(NULL, EKG_WINACT_JUNK, x) 
#define print_status(x...)	print_window_w(window_status, EKG_WINACT_JUNK, x)

void format_add(const char *name, const char *value, int replace);
void remote_format_add(const char *name, const char *value);
const char *format_find(const char *name);
#define format_ok(format_find_result)	(format_find_result[0])
#define format_exists(format)		(format_ok(format_find(format)))
char *format_string(const char *format, ...);

void theme_init();
void theme_free();

fstring_t *fstring_new(const char *str);
fstring_t *fstring_new_format(const char *format, ...);
void fstring_free(fstring_t *str);

typedef enum {
	FSTR_FOREA		= 1,
	FSTR_FOREB		= 2,
	FSTR_FOREC		= 4,
	FSTR_FOREMASK		= (FSTR_FOREA|FSTR_FOREB|FSTR_FOREC),
	FSTR_BACKA		= 8,
	FSTR_BACKB		= 16,
	FSTR_BACKC		= 32,
	FSTR_BACKMASK		= (FSTR_BACKA|FSTR_BACKB|FSTR_BACKC),
	FSTR_BOLD		= 64,
	FSTR_NORMAL		= 128,
	FSTR_BLINK		= 256,
	FSTR_UNDERLINE		= 512,
	FSTR_REVERSE		= 1024,
	FSTR_ALTCHARSET		= 2048
} fstr_t;

#endif /* __EKG_THEMES_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
