#ifndef __EKG_NCURSES_OLD_H
#define __EKG_NCURSES_OLD_H

#include "ecurses.h"

#include <ekg/themes.h>
#include <ekg/windows.h>

void ncurses_init();
void ncurses_deinit();

int ncurses_screen_width;
int ncurses_screen_height;
int ncurses_resize_term;

int ncurses_plugin_destroyed;

#define LINE_MAXLEN 1000		/* rozmiar linii */

#define ncurses_current ((ncurses_window_t *) window_current->private)

void update_statusbar(int commit);

struct screen_line {
	int len;		/* d³ugo¶æ linii */
	
	char *str;		/* tre¶æ */
	short *attr;		/* atrybuty */
	
	char *prompt_str;	/* tre¶æ promptu */
	short *prompt_attr;	/* atrybuty promptu */
	int prompt_len;		/* d³ugo¶æ promptu */
	
	char *ts;		/* timestamp */
	int ts_len;		/* d³ugo¶æ timestampu */

	int backlog;		/* z której linii backlogu pochodzi? */
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
} ncurses_window_t;

struct format_data {
	char *name;			/* %{nazwa} */
	char *text;			/* tre¶æ */
};

int ncurses_debug;

WINDOW *ncurses_status;
WINDOW *ncurses_header;
WINDOW *ncurses_input;
WINDOW *ncurses_contacts;

void ncurses_resize();
int ncurses_backlog_add(window_t *w, fstring_t *str);
int ncurses_backlog_split(window_t *w, int full, int removed);
void ncurses_redraw(window_t *w);
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
char *ncurses_history[HISTORY_MAX];
int ncurses_history_index;
char *ncurses_line;
char *ncurses_yanked;
char **ncurses_lines;
int ncurses_line_start;
int ncurses_line_index;
int ncurses_lines_start;
int ncurses_lines_index;
int ncurses_input_size;
int ncurses_debug;

void header_statusbar_resize();

int config_backlog_size;
void changed_backlog_size(const char *var);
int config_contacts_size;
int config_contacts;
char *config_contacts_options;
char *config_contacts_groups;
int config_display_transparent;
int config_display_crap;
int config_enter_scrolls;
int config_header_size;
int config_statusbar_size;

void ncurses_watch_stdin(int fd, int watch, void *data);
int ncurses_command_window(void *data, va_list ap);

#ifndef COLOR_DEFAULT
#  define COLOR_DEFAULT (-1)
#endif

#endif
