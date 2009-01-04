/* $Id: stuff.h 4597 2008-09-03 21:10:01Z darkjames $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Pawe³ Maziarz <drg@go2.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
 *			    Adam Mikuta <adammikuta@poczta.onet.pl>
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

#ifndef __EKG_STUFF_H
#define __EKG_STUFF_H

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "plugins.h"
#include "sessions.h"

#define BINDING_FUNCTION(x) void x(const char *arg) 

struct binding {
	struct binding	*next;

	char		*key;

	char		*action;			/* akcja */
	unsigned int	internal		: 1;	/* czy domy¶lna kombinacja? */
	void	(*function)(const char *arg);		/* funkcja obs³uguj±ca */
	char		*arg;				/* argument funkcji */

	char		*default_action;		/* domy¶lna akcja */
	void	(*default_function)(const char *arg);	/* domy¶lna funkcja */
	char		*default_arg;			/* domy¶lny argument */
};

typedef struct binding_added {
	struct binding_added	*next;

	char		*sequence;
	struct binding	*binding;
} binding_added_t;

#define TIMER(x)		int x(int type, void *data)

struct timer {
	struct timer	*next;

	char		*name;			/* nazwa timera */
	plugin_t	*plugin;		/* wtyczka obs³uguj±ca deksryptor */
	struct timeval	ends;			/* kiedy siê koñczy? */
	time_t		period;			/* ile sekund ma trwaæ czekanie */
	int	(*function)(int, void *);	/* funkcja do wywo³ania */
	void		*data;			/* dane dla funkcji */

	unsigned int	persist		: 1;	/* czy ma byæ na zawsze? */
	unsigned int	at		: 1;	/* /at? trzeba siê tego jako¶ pozbyæ
						 * i ujednoliciæ z /timer */
	unsigned int	is_session	: 1;	/* czy sesyjny */
};

extern char *config_console_charset;	/* */
extern char *server_console_charset;
extern int config_use_unicode;	/* for instance in jabber plugin if this is on, than we don't need to make iconv from / to unicode.. */
extern int config_use_iso;  /* this for ncurses */
extern struct binding *bindings;
extern struct timer *timers;
extern binding_added_t *bindings_added;
extern int config_debug;
extern int config_display_welcome;
extern int config_query_commands;
extern int config_slash_messages;
extern int config_display_color;
extern int config_display_pl_chars;
extern int config_display_crap;
extern int config_default_status_window;
extern char *config_timestamp;
extern int config_timestamp_show;
extern int config_history_savedups;
extern int config_make_window;
extern int config_sort_windows;

extern char *config_tab_command;
extern int config_save_quit;
extern int config_lastlog_noitems;
extern int config_lastlog_case;
extern int config_lastlog_display_all;
extern char *config_completion_char;

extern int config_changed;

extern int no_mouse;

extern int old_stderr;

extern int in_autoexec;

void binding_free();

void changed_theme(const char *var);

const char *compile_time();

void iso_to_ascii(unsigned char *buf);

#ifdef __GNUC__
char *saprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
char *saprintf(const char *format, ...);
#endif

const char *timestamp(const char *format);
const char *timestamp_time(const char *format, time_t t);

int isalpha_pl(unsigned char c);
/* makra, dziêki którym pozbywamy siê warning'ów */
#define xisxdigit(c) isxdigit((int) (unsigned char) c)
#define xisdigit(c) isdigit((int) (unsigned char) c)
#define xisalpha(c) isalpha_pl((int) (unsigned char) c)
#define xisalnum(c) isalnum((int) (unsigned char) c)
#define xisspace(c) isspace((int) (unsigned char) c)
#define xtolower(c) tolower((int) (unsigned char) c)
#define xtoupper(c) toupper((int) (unsigned char) c)

struct timer *timer_add(plugin_t *plugin, const char *name, time_t period, int persistent, int (*function)(int, void *), void *data);
int timer_remove(plugin_t *plugin, const char *name);
struct timer *timers_removei(struct timer *t);
void timers_destroy();

const char *ekg_status_string(const int status, const int cmd);

/* funkcje poza stuff.c */
void ekg_exit();
void ekg_debug_handler(int level, const char *format, va_list ap);

int ekg_write(int fd, const char *buf, int len);
int remote_request(char *what, ...);

#endif /* __EKG_STUFF_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
