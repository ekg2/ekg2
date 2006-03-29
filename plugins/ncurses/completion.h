#ifndef __EKG_NCURSES_COMPLETION_H
#define __EKG_NCURSES_COMPLETION_H

#include <ekg/char.h>

void ncurses_complete(int *line_start, int *line_index, CHAR_T *line);
void ncurses_complete_clear();

#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
