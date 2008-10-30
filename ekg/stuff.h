/* $Id$ */

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

#include "win32.h"

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "dynstuff.h"
#include "plugins.h"
#include "sessions.h"
#include "userlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_MAX_LINES	50	/* ile linii z debug zrzucaæ do pliku */

/* obs³uga procesów potomnych */

struct child_s;

typedef void (*child_handler_t)(struct child_s *c, pid_t pid, const char *name, int status, void *data);

typedef struct child_s {
	struct child_s	*next;

	pid_t		pid;		/* id procesu */
	plugin_t	*plugin;	/* obs³uguj±cy plugin */
	char		*name;		/* nazwa, wy¶wietlana przy /exec */
	child_handler_t	handler;	/* zak³ad pogrzebowy */
	void		*private;	/* dane procesu */
} child_t;

#ifndef EKG2_WIN32_NOFUNCTION
child_t *child_add(plugin_t *plugin, pid_t pid, const char *name, child_handler_t handler, void *private);
child_t *children_removei(child_t *c);
void children_destroy(void);
#endif


#ifndef EKG2_WIN32_NOFUNCTION
typedef struct alias {
	struct alias	*next;

	char		*name;		/* nazwa aliasu */
	list_t		commands;	/* commands->data to (char*) */
} alias_t;
#endif

enum mesg_t {
	MESG_CHECK = -1,
	MESG_OFF,
	MESG_ON,
	MESG_DEFAULT
};

#define TIMER(x)		int x(int type, void *data)
#define TIMER_SESSION(x)	int x(int type, session_t *s)

struct timer {
	struct timer	*next;

	char		*name;			/* nazwa timera */
	plugin_t	*plugin;		/* wtyczka obs³uguj±ca deksryptor */
	struct timeval	ends;			/* kiedy siê koñczy? */
	unsigned int	period;			/* ile sekund ma trwaæ czekanie */
	int	(*function)(int, void *);	/* funkcja do wywo³ania */
	void		*data;			/* dane dla funkcji */

	unsigned int	persist		: 1;	/* czy ma byæ na zawsze? */
	unsigned int	at		: 1;	/* /at? trzeba siê tego jako¶ pozbyæ
						 * i ujednoliciæ z /timer */
	unsigned int	is_session	: 1;	/* czy sesyjny */
};

struct conference {
	struct conference	*next;

	char		*name;
	ignore_t	ignore;
	list_t		recipients;
};

typedef struct newconference {
	struct newconference	*next;

	char		*session;
	char		*name;
	struct userlist	*participants;
	void		*private;
} newconference_t;

struct buffer {
	struct buffer	*next;

	time_t		ts;
	char		*target;
	char		*line;
};

struct buffer_info {
	struct buffer	*data;
	int		count;
	int		max_lines;
	struct buffer	*last;		/* fast access to last element, esp. for log_raw */
};

struct color_map {
	int		color;
	unsigned char	r, g, b;
};

#ifndef EKG2_WIN32_NOFUNCTION
extern child_t *children;
extern alias_t *aliases;
extern list_t autofinds; /* char* data */
extern struct timer *timers;
extern struct conference *conferences;
extern newconference_t *newconferences;
extern struct buffer_info buffer_debug;
extern struct buffer_info buffer_speech;

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
extern int config_display_welcome;
extern int config_emoticons;
extern int config_events_delay;
extern int config_expert_mode;
extern int config_history_savedups;
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
extern char *config_subject_reply_prefix;
extern char *config_tab_command;
extern char *config_theme;
extern int config_time_deviation;
extern char *config_timestamp;
extern int config_timestamp_show;
extern int config_use_unicode;	/* for instance in jabber plugin if this is on, than we don't need to make iconv from / to unicode.. */
extern int config_use_iso;  /* this for ncurses */
extern char *config_console_charset;	/* */
extern int config_window_session_allow;
extern char *config_windows_layout;
extern int config_windows_save;
extern char *config_dcc_dir;
extern int config_version;
extern char *config_exit_exec;
extern int config_session_locks;
extern char *config_nickname;

extern char *home_dir;
extern char *config_dir;
extern char *console_charset;
extern int in_autoexec;
extern int ekg_watches_removed;
extern time_t ekg_started;

extern int quit_message_send;
extern int batch_mode;
extern char *batch_line;
extern struct color_map color_map_default[16+10];

void windows_save();

int alias_add(const char *string, int quiet, int append);
int alias_remove(const char *name, int quiet);
void aliases_destroy();

char *base64_encode(const char *buf, size_t len);
char *base64_decode(const char *buf);

int buffer_add(struct buffer_info *type, const char *target, const char *line);
int buffer_add_str(struct buffer_info *type, const char *target, const char *str);
char *buffer_tail(struct buffer_info *type);
void buffer_free(struct buffer_info *type);

void changed_auto_save(const char *var);
void changed_display_blinking(const char *var);
void changed_make_window(const char *var);
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
void conferences_destroy();

/* BEGIN OF newconference API HERE */
userlist_t *newconference_member_add(newconference_t *conf, const char *uid, const char *nick);
userlist_t *newconference_member_find(newconference_t *conf, const char *uid);
int newconference_member_remove(newconference_t *conf, userlist_t *u);
newconference_t *newconference_create(session_t *s, const char *name, int create_wnd);
newconference_t *newconference_find(session_t *s, const char *name);
void newconference_destroy(newconference_t *conf, int kill_wnd);
void newconferences_destroy();
/* END of newconference API */

int ekg_hash(const char *name);

FILE *help_path(char *name, char *plugin);

int mesg_set(int what);
void iso_to_ascii(unsigned char *buf);
char *strip_spaces(char *line);
int strncasecmp_pl(const char * cs,const char * ct,size_t count);
int strcasecmp_pl(const char *cs, const char *ct);
int mkdir_recursive(const char *pathname, int isdir);

#ifdef __GNUC__
char *saprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
char *saprintf(const char *format, ...);
#endif

int play_sound(const char *sound_path);

const char *prepare_path(const char *filename, int do_mkdir);
const char *prepare_pathf(const char *filename, ...);
const char *prepare_path_user(const char *path);
char *read_file(FILE *f, int alloc);

const char *timestamp(const char *format);
const char *timestamp_time(const char *format, time_t t);
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

struct timer *timer_add(plugin_t *plugin, const char *name, unsigned int period, int persistent, int (*function)(int, void *), void *data);
struct timer *timer_add_session(session_t *session, const char *name, unsigned int period, int persist, int (*function)(int, session_t *));
struct timer *timer_find_session(session_t *session, const char *name);
int timer_remove(plugin_t *plugin, const char *name);
int timer_remove_session(session_t *session, const char *name);
int timer_remove_user();
void timers_remove(struct timer *t);
struct timer *timers_removei(struct timer *t);
void timers_destroy();
TIMER(timer_handle_command);

const char *ekg_status_label(const int status, const char *descr, const char *prefix);
void ekg_update_status(session_t *session);
#define ekg_update_status_n(a) ekg_update_status(session_find(a))
const char *ekg_status_string(const int status, const int cmd);
int ekg_status_int(const char *text);

char *ekg_draw_descr(const int status);
uint32_t *ekg_sent_message_format(const char *text);

void ekg_yield_cpu();

/* recode.c XXX, przeniesc do recode.h */
void *ekg_convert_string_init(const char *from, const char *to, void **rev);
void ekg_convert_string_destroy(void *ptr);
char *ekg_convert_string_p(const char *ps, void *ptr);
char *ekg_convert_string(const char *ps, const char *from, const char *to);
string_t ekg_convert_string_t_p(string_t s, void *ptr);
string_t ekg_convert_string_t(string_t s, const char *from, const char *to);
int ekg_converters_display(int quiet);

char *ekg_locale_to_cp(char *buf);
char *ekg_cp_to_locale(char *buf);
char *ekg_locale_to_latin2(char *buf);
char *ekg_latin2_to_locale(char *buf);
char *ekg_locale_to_utf8(char *buf);
char *ekg_utf8_to_locale(char *buf);
char *ekg_any_to_locale(char *buf, char *inp);
char *ekg_locale_to_any(char *buf, char *inp);


char *password_input(const char *prompt, const char *rprompt, const bool norepeat);

/* funkcje poza stuff.c */
void ekg_exit();
void ekg_debug_handler(int level, const char *format, va_list ap);

int ekg_close(int fd);
int ekg_write(int fd, const char *buf, int len);
int ekg_writef(int fd, const char *format, ...);

#endif
	
#ifdef __cplusplus
}
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
