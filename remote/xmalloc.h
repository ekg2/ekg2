/* $Id: xmalloc.h 4520 2008-08-27 09:16:57Z peres $ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *		       2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#ifndef __EKG_XMALLOC_H
#define __EKG_XMALLOC_H

#include <sys/types.h>
#include <stddef.h>
#include <stdarg.h>

#define EXPORTNOT  __attribute__ ((visibility("hidden")))

void *xcalloc(size_t nmemb, size_t size);
void *xmalloc(size_t size);
void *xmalloc2(size_t size);
void xfree(void *ptr);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
void *xmemdup(void *ptr, size_t size);

int xstrcasecmp(const char *s1, const char *s2);
char *xstrcat(char *dest, const char *src);
char *xstrchr(const char *s, int c);
int xstrcmp(const char *s1, const char *s2);
char *xstrcpy(char *dest, const char *src);
size_t xstrlen(const char *s);
int xstrncasecmp_pl(const char *s1, const char *s2, size_t n);
char *xstrncat(char *dest, const char *src, size_t n);
int xstrncmp(const char *s1, const char *s2, size_t n);
int xstrncasecmp(const char *s1, const char *s2, size_t n);
char *xstrrchr(const char *s, int c);
char *xstrstr(const char *haystack, const char *needle);
char *xstrcasestr(const char *haystack, const char *needle);

char *vsaprintf(const char *format, va_list ap);

	/* stuff.h */
#ifdef __GNUC__
char *saprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
char *saprintf(const char *format, ...);
#endif

#endif /* __EKG_XMALLOC_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
