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

/*
 * Ekg2 char and string utilities with optional unicode (wide character
 * strings) support.
 */

#include "ekg2-config.h"

#if USE_UNICODE
#include "ecurses.h"

/* some unicode/ascii stuff */
#include <wchar.h>	/* wchar_t stuff */
#include <string.h>	/* ascii stuff */
#include <stdlib.h>	/* ascii <==> wchar_t stuff */
#include <errno.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include <ekg/dynstuff.h>
#include <ekg/xmalloc.h>

int sizeofchart;

/* stringo-naprawiacze */
#define fix(x)	((char *) x ? (char *) x : (char *) "")
#define ufix(x)	((wchar_t *) x ? (wchar_t *) x : (wchar_t *) L"")

inline int xwcslen(CHAR_T *str) {
	if (config_use_unicode) return wcslen((wchar_t *) ufix(str));
	else			return strlen((char *) fix(str));
}

inline CHAR_T *xwcscpy(CHAR_T *dst, CHAR_T *src) {
	if (config_use_unicode) return (CHAR_T *) wcscpy((wchar_t *) ufix(dst), (wchar_t *) ufix(src));
	else			return (CHAR *) strcpy((char *) fix(dst), (char *) fix(src));
}

inline CHAR_T *xwcsdup(CHAR_T *str) {
	if (!str) return NULL;
	return xmemdup(str, (xwcslen(str)+1) * sizeof(CHAR_T));
}

inline CHAR_T *xwcscat(CHAR_T *dst, const CHAR_T *src) {
	if (config_use_unicode)	return (CHAR_T *) wcscat((wchar_t *) ufix(dst), (wchar_t *) ufix(src));
	else			return (CHAR *)	strcat((char *) dst, (char *) src);
}

inline int xwcscmp(const CHAR_T *s1, const CHAR_T *s2) {
	if (config_use_unicode) return wcscmp((wchar_t *) ufix(s1), (wchar_t *) ufix(s2));
	else			return strcmp((char *) fix(s1), (char *) fix(s2));
}

inline CHAR_T *xwcschr(const CHAR_T *s, CHAR_T c) {
	if (config_use_unicode) return (CHAR_T *) wcschr((wchar_t *) ufix(s), (wchar_t) ufix(c));
	else			return (CHAR *) strchr((char *) fix(s), (char) c);
}

inline char *wcs_to_normal(const CHAR_T *str) {
	if (!str) return NULL;
	if (config_use_unicode) {
		int len		= wcstombs(NULL, (wchar_t *) str,0);
		char *tmp 	= xmalloc(len+1);
		int ret;

		ret = wcstombs(tmp, (wchar_t *) str, len);
//		if (ret != len) printf("[wcs_to_normal_n, err] len = %d wcstombs = %d\n", ret, len);
		return tmp;
	} else	return (char *) str;
}

inline CHAR_T *normal_to_wcs(const char *str) {
	if (!str) return NULL;
	if (config_use_unicode) {
		int len = mbstowcs(NULL, str, 0)+1;
		wchar_t *tmp = xcalloc(len+1, sizeof(wchar_t));
		mbstowcs(tmp, str, len);
		return (CHAR_T *) tmp;
	} else	return (CHAR_T *) str;
}

inline CHAR_T **wcs_array_make(const CHAR_T *string, const CHAR_T *sep, int max, int trim, int quotes) {
	if (config_use_unicode) {
		char *str = wcs_to_normal(string);
		char *sp  = wcs_to_normal(sep);
		char **arr, **tmp;
		CHAR_T **newarr, **tmp2;

		arr	= tmp	= array_make(str, sp, max, trim, quotes);
		if (!arr) return NULL;

		newarr	= tmp2	= (CHAR_T **) xmalloc((array_count(arr)+1) * sizeof(CHAR_T *));
	
		for (; *tmp; tmp++, tmp2++)
			*tmp2 = normal_to_wcs(*tmp);
		
		array_free(arr);
		free_utf(str);
		free_utf(sp);
		return newarr;
	} else	return (CHAR **) array_make((char *) string, (char *) sep, max, trim, quotes);
}

inline size_t xwcslcpy(CHAR_T *dst, const CHAR_T *src, size_t size) {
	if (config_use_unicode) {
		/* copied from strlcpy.c (c Piotr Domagalski) */
		register size_t i, n = size;

		for (i = 0; n > 1 && src[i]; i++, n--)
			dst[i] = src[i];

		if (n)
			dst[i] = 0;

		while (src[i])
			i++;

		return i;
	} else return strlcpy((char *) dst, (char *) src, size);
}

inline CHAR_T *wcs_array_join(CHAR_T **array, const CHAR_T *sep) {
	if (config_use_unicode) {
		char **arr;
		char *sp = wcs_to_normal(sep);
		char *tmp;
		CHAR_T *ret;
		int i;

		arr = xmalloc( (array_count((char **) array)+1) * sizeof(char *));
		for (i = 0; array[i]; i++)
			arr[i] = wcs_to_normal(array[i]);
		
		tmp = array_join(arr, sp);
		ret = normal_to_wcs(tmp);
		array_free(arr);
		free_utf(tmp);
		free_utf(sp);
		return ret;
	} else	return (CHAR *) array_join((char **) array, (const char *) sep);
}

#endif

