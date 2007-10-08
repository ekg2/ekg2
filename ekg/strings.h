/* $Id$ */

#ifndef __EKG_STRINGS_H
#define __EKG_STRINGS_H

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

#include "ekg2-config.h"


#if USE_UNICODE

#include <wchar.h>	/* wchar_t stuff */

extern int config_use_unicode;	/* not everyone want to include stuff.h */

#define CHAR_T wchar_t 	
#define TEXT(x) (wchar_t *) L##x
#define CHAR(x) (wchar_t)   L##x
#define STRING_FORMAT "%ls"
#define CHAR_FORMAT "%lc"

inline int xwcslen(const CHAR_T *str);
inline int xmbslen(const char *str);
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

#define free_utf(x) xfree(x)

#else	/* USE_UNICODE */

#include <ekg/xmalloc.h>

#define CHAR_T unsigned char
#define TEXT(x) x
#define CHAR(x) x
#define STRING_FORMAT "%s"
#define CHAR_FORMAT "%c"

#define xwcslen(str) xstrlen((char *) str)
#define xmbslen(str) xstrlen(str)
#define xwcscpy(dst, str) xstrcpy((char *) dst, (char *) str)
#define xwcsdup(str) (CHAR_T *) xstrdup((char *) str)
#define xwcscat(dst, src) xstrcat((char *) dst, (char *) src)
#define xwcscmp(s1, s2)	xstrcmp((char *) s1, s2)
#define xwcschr(s, c) xstrchr((char *) s, c)
#define wcs_to_normal(x) (char *) x
#define wcs_array_make(str, sep, max, trim, quotes) (CHAR_T **) array_make((char *) str, sep, max, trim, quotes)
#define wcs_array_join(arr, sep) (CHAR_T *) array_join((char **) arr, sep)
#define xwcslcpy(dst, src, size) strlcpy((char *) dst, (char *) src, size)
#define free_utf(x)

#endif	/* USE_UNICODE */


#endif /* __EKG_STRINGS_H */
