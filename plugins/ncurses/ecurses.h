/* $Id$ */

#ifndef __EKG_NCURSES_ECURSES_H
#define __EKG_NCURSES_ECURSES_H

#include "config.h"

#ifdef HAVE_NCURSES_H
#  include <ncurses.h>
#else
#  ifdef HAVE_NCURSES_NCURSES_H
#    include <ncurses/ncurses.h>
#  endif
#endif

#endif
