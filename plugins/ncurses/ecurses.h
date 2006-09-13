/* $Id$ */

#ifndef __EKG_NCURSES_ECURSES_H
#define __EKG_NCURSES_ECURSES_H

#include "ekg2-config.h"

#if USE_UNICODE
# define _XOPEN_SOURCE_EXTENDED
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

extern int config_use_unicode;	/* not everyone want to include stuff.h */

#if USE_UNICODE
#define CHAR_T wchar_t 	
	/* be carefull!   use __S() macro to get true value. */
	/* be carefull111 use __SN() macr^H^H ekhm, function to move to real index */
	/* 		  use __SPTR() function to get ptr to real index */
#define CHAR CHAR_T 	/* silent warnings */

#define TEXT(x) (config_use_unicode ? (CHAR_T *) L##x : (CHAR_T *) x)

#define __S(str, i) (CHAR_T) (config_use_unicode ? ((wchar_t *) str)[i] : ((unsigned char *) str)[i]) 

static inline CHAR_T *__SPTR(CHAR_T *str, int offset) { /* #define __SPTR(str, i) (CHAR_T *) (config_use_unicode ? &((wchar_t *) str[i]) : &((unsigned char *) str)[i]) */
	if (config_use_unicode) return (CHAR_T *) (str + offset);
	else 			return (CHAR_T *) (((char *) str) + offset);
}

static inline CHAR_T *__SN(CHAR_T **str, int offset) { /* #define __SN(str) (CHAR_T) (config_use_unicode ? ((wchar_t *) str)++ : ((unsigned char *) str)++) */
	if (config_use_unicode) return (CHAR_T *) ((*str) += offset);
	else 			return (CHAR_T *) ((*((char **) str)) += offset);
}

inline void free_utf(void *ptr);
inline int xwcslen(CHAR_T *str);
inline CHAR_T *xwcscpy(CHAR_T *dst, CHAR_T *src);
inline CHAR_T *xwcsdup(CHAR_T *str);
inline CHAR_T *xwcscat(CHAR_T *dst, const CHAR_T *src);
inline int xwcscmp(const CHAR_T *s1, const CHAR_T *s2);
inline CHAR_T *xwcschr(const CHAR_T *s, CHAR_T c);
inline char *wcs_to_normal(const CHAR_T *str);
inline CHAR_T *normal_to_wcs(const char *str);
inline CHAR_T **wcs_array_make(const CHAR_T *string, const CHAR_T *sep, int max, int trim, int quotes);
inline CHAR_T *wcs_array_join(CHAR_T **array, const CHAR_T *sep);
inline size_t xwcslcpy(CHAR_T *dst, const CHAR_T *src, size_t size);

#else	/* USE_UNICODE */
#define CHAR_T unsigned char
#define TEXT(x) x
#define __S(str, i) str[i]
#define __SN(str, i) ((*str) += i)
#define __SPTR(str, i) (&str[i]) 
#define xwcslen(str) xstrlen(str)
#define xwcscpy(dst, str) xstrcpy(dst, str)
#define xwcsdup(str) xstrdup(str)
#define xwcscat(dst, src) xstrcat(dst, src)
#define xwcscmp(s1, s2)	xstrcmp(s1, s2)
#define xwcschr(s, c) xstrchr(s, c)
#define wcs_array_make(str, sep, max, trim, quotes) (CHAR_T **) array_make(str, sep, max, trim, quotes)
#define wcs_to_normal(x) x
#define free_utf(x) 
#define wcs_array_join(arr, sep) array_join((char **) arr, sep)
#define xwcslcpy(dst, src, size) strlcpy(dst, src, size)

#endif	/* USE_UNICODE */

#endif	/* __EKG_NCURSES_ECURSES_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
