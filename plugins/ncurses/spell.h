#ifndef __EKG_NCURSES_SPELL_H
#define __EKG_NCURSES_SPELL_H

#include "ekg2-config.h"
#include <ekg/strings.h>

#ifdef WITH_ASPELL

extern int config_aspell;
extern char *config_aspell_lang;

#include <aspell.h>

#  define ASPELLCHAR 5

extern AspellSpeller *spell_checker;

void ncurses_spellcheck_init(void);
void spellcheck(CHAR_T *what, char *where);

#endif

#endif
