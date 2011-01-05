#ifndef __EKG_NCURSES_BACKLOG_H
#define __EKG_NCURSES_BACKLOG_H

int ncurses_backlog_add(window_t *w, fstring_t *str);
int ncurses_backlog_split(window_t *w, int full, int removed);

#endif

