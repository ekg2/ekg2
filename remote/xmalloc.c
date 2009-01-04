/* $Id: xmalloc.c 4502 2008-08-26 11:12:07Z darkjames $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
		       2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#define _GNU_SOURCE

#include "ekg2-config.h"

#define __EXTENSIONS__
#include <sys/types.h>
#include <stddef.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

#ifndef HAVE_STRNDUP
#  include "compat/strndup.h"
#endif

extern int old_stderr;

#include "xmalloc.h"

#define fix(s) ((s) ? (s) : "")

static void ekg_oom_handler() {
	if (old_stderr)
		dup2(old_stderr, 2);

	fprintf(stderr,"\r\n\r\n*** Brak pamiêci ***\r\n\r\n");

	exit(1);
}

void *xcalloc(size_t nmemb, size_t size) {
	void *tmp = calloc(nmemb, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

void *xmalloc(size_t size) {
	void *tmp = malloc(size);

	if (!tmp)
		ekg_oom_handler();

	/* na wszelki wypadek wyczy¶æ bufor */
	memset(tmp, 0, size);

	return tmp;
}

EXPORTNOT void *xmalloc2(size_t size) {
	void *tmp = malloc(size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

void xfree(void *ptr) {
	if (ptr)
		free(ptr);
}

void *xrealloc(void *ptr, size_t size) {
	void *tmp = realloc(ptr, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

char *xstrdup(const char *s) {
	char *tmp;

	if (!s)
		return NULL;

	if (!(tmp = (char *) strdup(s)))
		ekg_oom_handler();

	return tmp;
}

char *xstrndup(const char *s, size_t n) {
	char *tmp;

	if (!s)
		return NULL;

	if (!(tmp = strndup((char *) s, n)))
		ekg_oom_handler();

	return tmp;
}

void *xmemdup(void *ptr, size_t size) {
	void *tmp = xmalloc2(size);

	memcpy(tmp, ptr, size);

	return tmp;
}

/*
 * XXX póki co, obs³uguje tylko libce zgodne z C99
 */
EXPORTNOT char *vsaprintf(const char *format, va_list ap) {
	char tmp[2];
#if defined(va_copy) || defined(__va_copy)
	va_list aq;
#endif
	char *res;
	int size;

#if defined(va_copy)
	va_copy(aq, ap);
#elif defined(__va_copy)
	__va_copy(aq, ap);
#endif
	size = vsnprintf(tmp, sizeof(tmp), format, ap);
	res = xmalloc(size + 1);
#if defined(va_copy) || defined(__va_copy)
	vsnprintf(res, size + 1, format, aq);
	va_end(aq);
#else
	vsnprintf(res, size + 1, format, ap);
#endif
	return res;
}

char *xstrstr(const char *haystack, const char *needle) {
	return strstr(fix(haystack), fix(needle));		
}

char *xstrcasestr(const char *haystack, const char *needle) {
#if 0
	return strcasestr(fix(haystack), fix(needle));
#else								/* XXX, potrzebne?, dlaczego? */

	int hlen, nlen;
	int i;
	
	haystack = fix(haystack);
	needle = fix(needle);

	hlen = strlen(haystack);
	nlen = strlen(needle);

	for (i = 0; i <= hlen - nlen; i++) {
		if (!strncasecmp(haystack + i, needle, nlen))
			return (char*) (haystack + i);
	}

	return NULL;

#endif
}

int xstrcasecmp(const char *s1, const char *s2) {
	return strcasecmp(fix(s1), fix(s2));
}

char *xstrcat(char *dest, const char *src) {
	return strcat(dest, fix(src));
}

char *xstrchr(const char *s, int c) {
	return strchr(fix(s), c);
}

int xstrcmp(const char *s1, const char *s2) {
	return strcmp(fix(s1), fix(s2));
}

char *xstrcpy(char *dest, const char *src) {
	return strcpy(dest, fix(src));
}

size_t xstrlen(const char *s) {
	return (s) ? strlen(s) : 0;
}

static int tolower_pl(const unsigned char c) {
	switch(c) {
		case 161: /* ¡ */
			return 177;
		case 198: /* Æ */
			return 230;
		case 202: /* Ê */
			return 234;
		case 163: /* £ */
			return 179;
		case 209: /* Ñ */
			return 241;
		case 211: /* Ó */
			return 243;
		case 166: /* ¦ */
			return 182;
		case 175: /* ¯ */
			return 191;
		case 172: /* ¬ */
			return 188;
		default: /* reszta */
			return tolower(c);
	}
}

static int strncasecmp_pl(const char *cs, const char *ct, size_t count) {
	register signed char __res = 0;

	while (count) {
		if ((__res = tolower_pl(*cs) - tolower_pl(*ct++)) != 0 || !*cs++)
			break;
		count--;
	}

	return __res;
}

int xstrncasecmp_pl(const char *s1, const char *s2, size_t n) {
	return strncasecmp_pl(fix(s1), fix(s2), n);
}

char *xstrncat(char *dest, const char *src, size_t n) {
	return strncat(dest, fix(src), n);
}

int xstrncmp(const char *s1, const char *s2, size_t n) {
	return strncmp(fix(s1), fix(s2), n);
}

int xstrncasecmp(const char *s1, const char *s2, size_t n) {
	return strncasecmp(fix(s1), fix(s2), n);
}

char *xstrrchr(const char *s, int c) {
	return strrchr(fix(s), c);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
