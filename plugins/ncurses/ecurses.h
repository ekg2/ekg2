/* $Id$ */

#ifndef __EKG_NCURSES_ECURSES_H
#define __EKG_NCURSES_ECURSES_H

#define NCURSES_OPAQUE 0	/* for broken macOSX 10.6.1 header
                                   XXX detect during configure
				 */

#ifdef HAVE_WADDNWSTR
#	define USE_UNICODE 1
#endif

#ifdef HAVE_NCURSESW_NCURSES_H
#	include <ncursesw/ncurses.h>
#else /*HAVE_NCURSESW_NCURSES_H*/
#	ifdef HAVE_NCURSES_NCURSES_H
#		include <ncurses/ncurses.h>
#	else
#		include <ncurses.h>
#   endif /*HAVE_NCURSES_NCURSES_H*/
#endif /*HAVE_NCURSESW_NCURSES_H*/

#include "nc-strings.h"

#endif	/* __EKG_NCURSES_ECURSES_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
