#ifndef __EKG_NCURSES_BACKLOG_H
#define __EKG_NCURSES_BACKLOG_H

typedef struct {
	fstring_t *line;
	int height;
} backlog_line_t;

#define EKG_NCURSES_BACKLOG_END -1
#define ncurses_backlog_seek_end(n) (n->index = EKG_NCURSES_BACKLOG_END, n->first_row = 0 )
#define ncurses_backlog_seek_start(n) (n->index = n->first_row = 0)

void ncurses_backlog_new(window_t *w);
void ncurses_backlog_destroy(window_t *w);

void ncurses_backlog_reset_heights(window_t *w, int height);

void ncurses_backlog_add_real(window_t *w, /*locale*/ fstring_t *str);
void ncurses_backlog_add(window_t *w, const fstring_t *str);

void ncurses_backlog_display(window_t *w);
void ncurses_backlog_scroll(window_t *w, int offset);

backlog_line_t *ncurses_backlog_mouse_click(window_t *w, int y);


#endif

