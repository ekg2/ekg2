/* $Id$ */

#ifndef __EKG_NCURSES_ECURSES_H
#define __EKG_NCURSES_ECURSES_H

#include "ekg2-config.h"

#if USE_UNICODE
# ifndef _XOPEN_SOURCE_EXTENDED
#  define _XOPEN_SOURCE_EXTENDED
# endif
# include <ncursesw/ncurses.h>
#else	/* USE_UNICODE */
# ifdef HAVE_NCURSES_H
#   include <ncurses.h>
#  else	/* HAVE_NCURSES_H */
#   ifdef HAVE_NCURSES_NCURSES_H
#     include <ncurses/ncurses.h>
#   endif	/* HAVE_NCURSES_NCURSES_H */
# endif	/* HAVE_NCURSES_H */
#endif	/* USE_UNICODE */

#include <ekg/strings.h>

#endif	/* __EKG_NCURSES_ECURSES_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
