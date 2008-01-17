#ifndef __EKG_NCURSES_OLD_H
#define __EKG_NCURSES_OLD_H

#include "ekg2-config.h"

#include "ecurses.h"

#include <ekg/plugins.h>
#include <ekg/themes.h>
#include <ekg/windows.h>

void ncurses_init();
void ncurses_deinit();

extern plugin_t ncurses_plugin;

// int ncurses_resize_term;

extern int ncurses_plugin_destroyed;

#define LINE_MAXLEN 1000		/* rozmiar linii */

#define ncurses_current ((ncurses_window_t *) window_current->private)

void update_statusbar(int commit);

struct screen_line {
	int len;		/* d³ugo¶æ linii */
	
	CHAR_T *str;		/* tre¶æ */
	short *attr;		/* atrybuty */
	
	CHAR_T *prompt_str;	/* tre¶æ promptu */
	short *prompt_attr;	/* atrybuty promptu */
	int prompt_len;		/* d³ugo¶æ promptu */
	
	char *ts;		/* timestamp */
	int ts_len;		/* d³ugo¶æ timestampu */
	short *ts_attr;		/* attributes of the timestamp */

	int backlog;		/* z której linii backlogu pochodzi? */
	int margin_left;	/* where the margin should be setted */	
};

enum window_frame_t {
	WF_LEFT = 1,
	WF_TOP = 2,
	WF_RIGHT = 4,
	WF_BOTTOM = 8,
	WF_ALL = 15
};

typedef struct {
	WINDOW *window;		/* okno okna */

	char *prompt;		/* sformatowany prompt lub NULL */
	int prompt_len;		/* d³ugo¶æ prompta lub 0 */

	int margin_left, margin_right, margin_top, margin_bottom;
				/* marginesy */

	fstring_t **backlog;	/* bufor z liniami */
	int backlog_size;	/* rozmiar backloga */

	int redraw;		/* trzeba przerysowaæ przed wy¶wietleniem */

	int start;		/* od której linii zaczyna siê wy¶wietlanie */
	int lines_count;	/* ilo¶æ linii ekranowych w backlogu */
	struct screen_line *lines;
				/* linie ekranowe */

	int overflow;		/* ilo¶æ nadmiarowych linii w okienku */

	int (*handle_redraw)(window_t *w);
				/* obs³uga przerysowania zawarto¶ci okna */

	void (*handle_mouse)(int x, int y, int state);

	CHAR_T *prompt_real;	/* prompt shortened to 2/3 of window width & converted to real chartype */
	int prompt_real_len;	/* real prompt length, including cutting, in chars instead of bytes */
} ncurses_window_t;

struct format_data {
	char *name;			/* %{nazwa} */
	char *text;			/* tre¶æ */
};

extern WINDOW *ncurses_contacts;
extern WINDOW *ncurses_input;

TIMER(ncurses_typing);
QUERY(ncurses_session_disconnect_handler);
void ncurses_main_window_mouse_handler(int x, int y, int mouse_state);

void ncurses_update_real_prompt(ncurses_window_t *n);
void ncurses_resize();
int ncurses_backlog_add(window_t *w, fstring_t *str);
int ncurses_backlog_split(window_t *w, int full, int removed);
void ncurses_redraw(window_t *w);
void ncurses_redraw_input(unsigned int ch);
void ncurses_clear(window_t *w, int full);
void ncurses_refresh();
void ncurses_commit();
void ncurses_input_update();
void ncurses_line_adjust();
#define line_adjust ncurses_line_adjust
void ncurses_lines_adjust();
#define lines_adjust ncurses_lines_adjust
int ncurses_window_kill(window_t *w);
int ncurses_window_new(window_t *w);

#define input ncurses_input
#define header ncurses_header
#define contacts ncurses_contacts
#define history ncurses_history
#define history_index ncurses_history_index
#define line_index ncurses_line_index
#define line_start ncurses_line_start
#define lines_index ncurses_lines_index
#define lines_start ncurses_lines_start
#define input_size ncurses_input_size
#define yanked ncurses_yanked
	
#define HISTORY_MAX 1000
extern CHAR_T *ncurses_history[HISTORY_MAX];
extern int ncurses_history_index;
extern CHAR_T *ncurses_line;
extern CHAR_T *ncurses_yanked;
extern CHAR_T **ncurses_lines;
extern int ncurses_line_start;
extern int ncurses_line_index;
extern int ncurses_lines_start;
extern int ncurses_lines_index;
extern int ncurses_input_size;
extern int ncurses_debug;

void header_statusbar_resize(const char *dummy);
#ifdef WITH_ASPELL
void ncurses_spellcheck_init();

extern int config_aspell;
extern char *config_aspell_lang;
#endif
void changed_backlog_size(const char *var);

extern int config_backlog_size;
extern int config_display_transparent;
extern int config_enter_scrolls;
extern int config_header_size;
extern int config_margin_size;
extern int config_statusbar_size;
extern int config_kill_irc_window;

extern int config_text_bottomalign;
extern int config_typing_timeout;
extern int config_typing_timeout_empty;

int ncurses_lastlog_update(window_t *w);
void ncurses_lastlog_new(window_t *w);
extern int config_lastlog_size;
extern int config_lastlog_lock;

WATCHER(ncurses_watch_stdin);
WATCHER(ncurses_watch_winch);
int ncurses_command_window(void *data, va_list ap);

extern int have_winch_pipe;
extern int winch_pipe[2];

#ifndef COLOR_DEFAULT
#  define COLOR_DEFAULT (-1)
#endif

#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
