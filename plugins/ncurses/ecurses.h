/* $Id$ */

#ifndef __EKG_NCURSES_ECURSES_H
#define __EKG_NCURSES_ECURSES_H

/*
 *  (C) Copyright 2003-2006 Maciej Pietrzak <maciej@hell.org.pl>
 *  		  	    Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

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
