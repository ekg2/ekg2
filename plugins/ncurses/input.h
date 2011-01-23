#ifndef __EKG_NCURSES_INPUT_H
#define __EKG_NCURSES_INPUT_H

#include <ekg/strings.h>

extern int ncurses_noecho;
extern CHAR_T *ncurses_passbuf;

#define input ncurses_input

void ncurses_input_update(int new_line_index);
void ncurses_lines_adjust(void);
#define lines_adjust ncurses_lines_adjust

#define line_index ncurses_line_index
#define line_start ncurses_line_start
#define lines_index ncurses_lines_index
#define lines_start ncurses_lines_start
#define input_size ncurses_input_size
#define yanked ncurses_yanked

extern CHAR_T *ncurses_line;
extern CHAR_T *ncurses_yanked;
extern CHAR_T **ncurses_lines;
extern int ncurses_line_start;
extern int ncurses_line_index;
extern int ncurses_lines_start;
extern int ncurses_lines_index;
extern int ncurses_input_size;

#endif
