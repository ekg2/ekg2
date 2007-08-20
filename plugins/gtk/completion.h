#ifndef __EKG_NCURSES_COMPLETION_H
#define __EKG_NCURSES_COMPLETION_H

#define COMPLETION_MAXLEN 2048		/* rozmiar linii */

void ncurses_complete(int *line_index, char *line);
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
