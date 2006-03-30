/* $Id$ */

#ifndef __EKG_NCURSES_ECURSES_H
#define __EKG_NCURSES_ECURSES_H

#include "ekg2-config.h"

#if USE_UNICODE
#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/ncurses.h>
#else

#ifdef HAVE_NCURSES_H
#  include <ncurses.h>
#else
#  ifdef HAVE_NCURSES_NCURSES_H
#    include <ncurses/ncurses.h>
#  endif
#endif

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
