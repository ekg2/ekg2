/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
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
#include <stdarg.h>

void ekg_oom_handler();

void *xcalloc(size_t nmemb, size_t size);
void *xmalloc(size_t size);
void xfree(void *ptr);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
size_t xstrnlen(const char *s, size_t n);
char *xstrndup(const char *s, size_t n);
void *xmemdup(void *ptr, size_t size);

char *vsaprintf(const char *format, va_list ap);

#ifdef __GNUC__
char *saprintf(const char *format, ...) __attribute__ ((format (printf, 1, 2)));
#else
char *saprintf(const char *format, ...);
#endif

#endif /* __EKG_XMALLOC_H */
