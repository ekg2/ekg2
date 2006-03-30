/* $Id$ */

/*
 *  (C) Copyright 2003 Maciej Pietrzak <maciej@hell.org.pl>
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


#ifndef EKG2__CHAR_H__
#define EKG2__CHAR_H__


#include "ekg2-config.h"


#if USE_UNICODE
#	include <wchar.h>
#	define TEXT(x)	L##x
#	define CHAR_T	wchar_t
#	define free_utf(x) xfree(x)
#else
#	define TEXT(x)	x
#	define CHAR_T	char
#	define free_utf(x) 
#endif
char *wcs_to_normal(const wchar_t *str);
CHAR_T *normal_to_wcs(const char *str);


#endif /* EKG2__CHAR_H__ */

