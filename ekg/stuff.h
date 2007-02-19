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

#include "win32.h"

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "dynstuff.h"
#include "plugins.h"
#include "sessions.h"
#include "userlist.h"

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

#ifndef EKG2_WIN32_NOFUNCTION
child_t *child_add(plugin_t *plugin, int pid, const char *name, child_handler_t handler, void *private);
#endif


#ifndef EKG2_WIN32_NOFUNCTION
struct alias {
	char *name;		/* nazwa aliasu */
	list_t commands;	/* commands->data to (char*) */
};
#endif

#define BINDING_FUNCTION(x) void x(const char *arg) 

struct binding {
	char *key;

	char *action;					/* akcja */
	int internal;					/* czy domy¶lna kombinacja? */
	void (*function)(const char *arg);		/* funkcja obs³uguj±ca */
	char *arg;					/* argument funkcji */

	char *default_action;				/* domy¶lna akcja */
	void (*default_function)(const char *arg);	/* domy¶lna funkcja */
	char *default_arg;				/* domy¶lny argument */
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

typedef struct {
	char *session;
	char *name;
	list_t participants;
	void *private;
} newconference_t;

struct buffer {
	time_t ts;
	char *target;
	char *line;
};

struct color_map {
	int color;
	unsigned char r, g, b;
};

#ifndef EKG2_WIN32_NOFUNCTION
extern list_t children;
extern list_t autofinds;
extern list_t aliases;
extern list_t bindings;
extern list_t bindings_added;
extern list_t timers;
extern list_t conferences;
extern list_t newconferences;

extern list_t buffer_debug;
extern list_t buffer_speech;

extern time_t last_save;
extern char *config_profile;
extern int config_changed;
extern int reason_changed;

extern pid_t speech_pid;

extern int no_mouse;

extern int old_stderr;
extern int mesg_startup;

extern char *config_audio_device;
extern char *config_away_reason;
extern int config_auto_save;
extern int config_auto_user_add;
extern char *config_back_reason;
extern int config_beep;
extern int config_beep_msg;
extern int config_beep_chat;
extern int config_beep_notify;
extern int config_completion_notify;
extern char *config_completion_char;
extern int config_debug;
extern int config_default_status_window;
extern int config_display_ack;
extern int config_display_blinking;
extern int config_display_color;
extern char *config_display_color_map;
extern int config_display_crap;
extern int config_display_day_changed;
extern int config_display_notify;
extern int config_display_pl_chars;
extern int config_display_sent;
extern int config_display_unknown;
extern int config_display_welcome;
extern int config_emoticons;
extern int config_events_delay;
extern int config_keep_reason;
extern int config_last;
extern int config_last_size;
extern int config_lastlog_case;
extern int config_lastlog_noitems;
extern int config_lastlog_display_all;
extern int config_make_window;
extern int config_mesg;
extern int config_query_commands;
extern int config_slash_messages;
extern char *config_quit_reason;
extern int config_reason_limit;
extern int config_save_password;
extern int config_save_quit;
extern char *config_session_default;
extern int config_sessions_save;
extern int config_sort_windows;
extern char *config_sound_app;
extern char *config_sound_chat_file;
extern char *config_sound_msg_file;
extern char *config_sound_sysmsg_file;
extern char *config_sound_notify_file;
extern char *config_sound_mail_file;
extern char *config_speech_app;
extern char *config_subject_prefix;
extern char *config_tab_command;
extern char *config_theme;
extern int config_time_deviation;
extern char *config_timestamp;
extern int config_timestamp_show;
extern int config_use_unicode; 	/* for instance in jabber plugin if this is on, than we don't need to make iconv from / to unicode.. */
extern char *config_console_charset;	/* */
extern char *config_windows_layout;
extern int config_windows_save;
extern char *config_dcc_dir;

extern char *home_dir;
extern char *config_dir;
extern char *console_charset;
extern int in_autoexec;
extern int ekg_watches_removed;
extern time_t last_action;
extern time_t ekg_started;

extern int quit_message_send;
extern int batch_mode;
extern char *batch_line;
extern struct color_map color_map_default[16+10];

void windows_save();

int alias_add(const char *string, int quiet, int append);
int alias_remove(const char *name, int quiet);
void alias_free();

char *base64_encode(const char *buf, size_t len);
char *base64_decode(const char *buf);

void binding_list(int quiet, const char *name, int all);
void binding_free();

int buffer_add(list_t *type, const char *target, const char *line, int max_lines);
int buffer_add_str(list_t *type, const char *target, const char *str, int max_lines);
char *buffer_flush(list_t *type, const char *target);
char *buffer_tail(list_t *type);
void buffer_free(list_t *type);

void changed_auto_save(const char *var);
void changed_display_blinking(const char *var);
void changed_mesg(const char *var);
void changed_theme(const char *var);

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

/* BEGIN OF newconference API HERE */
userlist_t *newconference_member_add(newconference_t *conf, const char *uid, const char *nick);
userlist_t *newconference_member_find(newconference_t *conf, const char *uid);
int newconference_member_remove(newconference_t *conf, userlist_t *u);
newconference_t *newconference_create(session_t *s, const char *name, int create_wnd);
newconference_t *newconference_find(session_t *s, const char *name);
void newconference_destroy(newconference_t *conf, int kill_wnd);
void newconference_free();
/* END of newconference API */

void ekg_connect();
void ekg_reconnect();

int ekg_hash(const char *name);

FILE *help_path(char *name, char *plugin);

int mesg_set(int what);
void iso_to_ascii(unsigned char *buf);
char *strip_quotes(char *line);
char *strip_spaces(char *line);
int strncasecmp_pl(const char * cs,const char * ct,size_t count);
int strcasecmp_pl(const char *cs, const char *ct);


#ifdef __GNUC__
char *saprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
char *saprintf(const char *format, ...);
#endif

int play_sound(const char *sound_path);

const char *prepare_path(const char *filename, int do_mkdir);
char *random_line(const char *path);
char *read_file(FILE *f);

const char *timestamp(const char *format);
void unidle();
int on_off(const char *value);
char *xstrmid(const char *str, int start, int length);
void xstrtr(char *text, char from, char to);
char color_map(unsigned char r, unsigned char g, unsigned char b);
char *strcasestr(const char *haystack, const char *needle);
int msg_all(session_t *s, const char *function, const char *what);
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

#endif
	
#endif /* __EKG_STUFF_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
