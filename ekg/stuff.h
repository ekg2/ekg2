/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@go2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
 *                          Adam Mikuta <adammikuta@poczta.onet.pl>
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
#include <sys/socket.h>
#include <netinet/in.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "dynstuff.h"
#include "plugins.h"
#include "debug.h"
#include "xmalloc.h"
#include "sessions.h"
#include "stuff.h"

#define DEBUG_MAX_LINES	50	/* ile linii z debug zrzucaæ do pliku */

#ifndef INADDR_NONE
#  define INADDR_NONE (unsigned long) 0xffffffff
#endif

/* obs³uga procesów potomnych */

struct child_s;

typedef void (*child_handler_t)(struct child_s *c, int pid, const char *name, int status, void *data);

typedef struct child_s {
	int pid;			/* id procesu */
	plugin_t *plugin;		/* obs³uguj±cy plugin */
	char *name;			/* nazwa, wy¶wietlana przy /exec */
	child_handler_t handler;	/* zak³ad pogrzebowy */
	void *private;			/* dane procesu */
} child_t;

child_t *child_add(plugin_t *plugin, int pid, const char *name, child_handler_t handler, void *private);
int child_pid_get(child_t *c);
const char *child_name_get(child_t *c);
plugin_t *child_plugin_get(child_t *c);
void *child_private_get(child_t *c);
child_handler_t child_handler_get(child_t *c);

struct alias {
	char *name;		/* nazwa aliasu */
	list_t commands;	/* commands->data to (char*) */
};

struct binding {
	char *key;

	char *action;			/* akcja */
	int internal;			/* czy domy¶lna kombinacja? */
	void (*function)(const char *arg);	/* funkcja obs³uguj±ca */
	char *arg;			/* argument funkcji */

	char *default_action;		/* domy¶lna akcja */
	void (*default_function)(const char *arg);	/* domy¶lna funkcja */
	char *default_arg;		/* domy¶lny argument */
};

typedef struct {
        char *sequence;
        struct binding *binding;
} binding_added_t;

enum mesg_t {
	MESG_CHECK = -1,
	MESG_OFF,
	MESG_ON,
	MESG_DEFAULT
};

#define TIMER(x) int x(int type, void *data)

struct timer {
	char *name;		/* nazwa timera */
	plugin_t *plugin;	/* wtyczka obs³uguj±ca deksryptor */
	struct timeval ends;	/* kiedy siê koñczy? */
	time_t period;		/* ile sekund ma trwaæ czekanie */
	int persist;		/* czy ma byæ na zawsze? */
	int (*function)(int, void *);
				/* funkcja do wywo³ania */
	void *data;		/* dane dla funkcji */
	int at;			/* /at? trzeba siê tego jako¶ pozbyæ
				 * i ujednoliciæ z /timer */
};

struct conference {
	char *name;
	int ignore;
	list_t recipients;
};

enum buffer_t {
	BUFFER_DEBUG,	/* na zapisanie n ostatnich linii debug */
	BUFFER_EXEC,	/* na buforowanie tego, co wypluwa exec */
	BUFFER_SPEECH	/* na wymawiany tekst */
};

struct buffer {
	int type;
	char *target;
	char *line;
};

struct color_map {
	int color;
	unsigned char r, g, b;
};

list_t children;
list_t autofinds;
list_t aliases;
list_t bindings;
list_t bindings_added;
list_t timers;
list_t conferences;
list_t buffers;
list_t searches;

time_t last_save;
char *config_profile;
int config_changed;
int reason_changed;

pid_t speech_pid;

int no_mouse;

int old_stderr;
int mesg_startup;

char *config_audio_device;
char *config_away_reason;
int config_auto_save;
int config_auto_user_add;
char *config_back_reason;
int config_beep;
int config_beep_msg;
int config_beep_chat;
int config_beep_notify;
int config_beep_mail;
int config_completion_notify;
char *config_completion_char;
int config_debug;
int config_default_status_window;
int config_display_ack;
int config_display_blinking;
int config_display_color;
char *config_display_color_map;
int config_display_notify;
int config_display_pl_chars;
int config_display_sent;
int config_display_welcome;
int config_emoticons;
int config_events_delay;
int config_keep_reason;
int config_last;
int config_last_size;
int config_make_window;
int config_mesg;
int config_query_commands;
char *config_quit_reason;
int config_reason_limit;
int config_save_password;
int config_save_quit;
char *config_session_default;
int config_sessions_save;
int config_sort_windows;
char *config_sound_app;
char *config_sound_chat_file;
char *config_sound_msg_file;
char *config_sound_sysmsg_file;
char *config_sound_notify_file;
char *config_sound_mail_file;
char *config_speech_app;
char *config_subject_prefix;
char *config_tab_command;
char *config_theme;
int config_time_deviation;
char *config_timestamp;
int config_timestamp_show;
int config_use_unicode; 	/* for instance in jabber plugin if this is on, than we don't need to make iconv from / to unicode.. */
char *config_console_charset;	/* */
char *config_windows_layout;
int config_windows_save;

char *home_dir;
char *config_dir;
int in_autoexec;
int ekg_stdin_want_more;
time_t last_action;
time_t ekg_started;

int quit_message_send;
int batch_mode;
char *batch_line;
struct color_map default_color_map[16+10];

void windows_save();

int alias_add(const char *string, int quiet, int append);
int alias_remove(const char *name, int quiet);
void alias_free();

char *base64_encode(const char *buf);
char *base64_decode(const char *buf);

void binding_list(int quiet, const char *name, int all);
void binding_free();

int buffer_add(int type, const char *target, const char *line, int max_lines);
int buffer_count(int type);
char *buffer_flush(int type, const char *target);
char *buffer_tail(int type);
void buffer_free();

void changed_var_default(session_t *s, const char *var);

void changed_auto_save(const char *var);
void changed_display_blinking(const char *var);
void changed_mesg(const char *var);
void changed_proxy(const char *var);
void changed_theme(const char *var);
void changed_uin(const char *var);
void changed_xxx_reason(const char *var);

const char *compile_time();

struct conference *conference_add(session_t *session, const char *string, const char *nicklist, int quiet);
int conference_remove(const char *name, int quiet);
struct conference *conference_create(session_t *session, const char *nicks);
struct conference *conference_find(const char *name);
struct conference *conference_find_by_uids(session_t *s, const char *from, const char **recipients, int count, int quiet);
int conference_set_ignore(const char *name, int flag, int quiet);
int conference_rename(const char *oldname, const char *newname, int quiet);
int conference_participant(struct conference *c, const char *uid);
void conference_free();

void ekg_connect();
void ekg_reconnect();

int ekg_hash(const char *name);

char * help_path(char * name, char * plugin);

int mesg_set(int what);
void iso_to_ascii(unsigned char *buf);
char *strip_quotes(char *line);
char *strip_spaces(char *line);
CHAR_T *wcs_strip_spaces(CHAR_T *line);
char *str_tolower(const char *text);
char *strip_pl_chars(const char *text);
int tolower_pl(const unsigned char c);
int strncasecmp_pl(const char * cs,const char * ct,size_t count);
int strcasecmp_pl(const char *cs, const char *ct);
void pl_to_normal(unsigned char ch);


#ifdef __GNUC__
char *saprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
char *saprintf(const char *format, ...);
#endif
CHAR_T *wcsprintf(const CHAR_T *format, ...);


int play_sound(const char *sound_path);

const char *prepare_path(const char *filename, int do_mkdir);
char *random_line(const char *path);
char *read_file(FILE *f);
CHAR_T *wcs_read_file(FILE *f);

const char *timestamp(const char *format);
void unidle();
int on_off(const char *value);
char *xstrmid(const char *str, int start, int length);
void xstrtr(char *text, char from, char to);
char color_map(unsigned char r, unsigned char g, unsigned char b);
char *strcasestr(const char *haystack, const char *needle);
int msg_all(session_t *s, const CHAR_T *function, const char *what);
int say_it(const char *str);
char *split_line(char **ptr);

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
int timer_freeone(struct timer *t);
int timer_remove(plugin_t *plugin, const char *name);
int timer_remove_user();
TIMER(timer_handle_command);
void timer_free();

const char *ekg_status_label(const char *status, const char *descr, const char *prefix);
void ekg_update_status(session_t *session);
#define ekg_update_status_n(a) ekg_update_status(session_find(a))

char *ekg_draw_descr(const char *status);
uint32_t *ekg_sent_message_format(const char *text);

void ekg_yield_cpu();

/* funkcje poza stuff.c */
void ekg_exit();
void ekg_debug_handler(int level, const char *format, va_list ap);
	
#endif /* __EKG_STUFF_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
