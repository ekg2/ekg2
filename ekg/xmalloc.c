/* $Id$ */

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

/*
 * Dodane s± tutaj funkcje opakowuj±ce, które maj± zapobiec problemoom
 * z dostarczeniem do funkcji argumentu o warto¶ci NULL. W niektórych
 * systemach doprowadza to do segmeentation fault.
 * 
 * Problem zostaje rozwi±zany poprzez zamienienie warto¶ci NULL 
 * warto¶ci± "", która jest ju¿ prawid³ow± dan± wej¶ciow±.
 * Makro fix(s) zosta³o w³a¶nie stworzone w tym celu 
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
#include "char.h"

#include "configfile.h"
#include "stuff.h"
#include "userlist.h"

#ifndef HAVE_STRNDUP
#  include "compat/strndup.h"
#endif

#ifndef HAVE_STRNLEN
#  include "compat/strnlen.h"
#endif

#ifndef HAVE_STRFRY
#  include "compat/strfry.h"
#endif

#define fix(s) ((s) ? (s) : "")

void ekg_oom_handler()
{
	if (old_stderr)
		dup2(old_stderr, 2);

	fprintf(stderr,
"\r\n"
"\r\n"
"*** Brak pamiêci ***\r\n"
"\r\n"
"Próbujê zapisaæ ustawienia do plików z przyrostkami .%d, ale nie\r\n"
"obiecujê, ¿e cokolwiek z tego wyjdzie.\r\n"
"\r\n"
"Do pliku %s/debug.%d zapiszê ostatanie komunikaty\r\n"
"z okna debugowania.\r\n"
"\r\n", (int) getpid(), config_dir, (int) getpid());

	config_write_crash();
	userlist_write_crash();
	debug_write_crash();

	exit(1);
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *tmp = calloc(nmemb, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

void *xmalloc(size_t size)
{
	void *tmp = malloc(size);

	if (!tmp)
		ekg_oom_handler();

	/* na wszelki wypadek wyczy¶æ bufor */
	memset(tmp, 0, size);
	
	return tmp;
}

void xfree(void *ptr)
{
	if (ptr)
		free(ptr);
}

void *xrealloc(void *ptr, size_t size)
{
	void *tmp = realloc(ptr, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

char *xstrdup(const char *s)
{
	char *tmp;

	if (!s)
		return NULL;

	if (!(tmp = strdup(s)))
		ekg_oom_handler();

	return tmp;
}

CHAR_T *xwcsdup(const CHAR_T *s) 
{
#if USE_UNICODE
	CHAR_T *tmp;
	if (!s) return NULL;
	if (!(tmp = wcsdup(s)))
		ekg_oom_handler();
#else
	return xstrdup(s);
#endif
}

size_t xstrnlen(const char *s, size_t n) 
{
        return strnlen(fix(s), n);
}


char *xstrndup(const char *s, size_t n)
{
        char *tmp;

        if (!s)
                return NULL;

	if (!(tmp = strndup((char *) s, n)))
		ekg_oom_handler();

        return tmp;
}

void *xmemdup(void *ptr, size_t size)
{
	void *tmp = xmalloc(size);

	memcpy(tmp, ptr, size);

	return tmp;
}

/*
 * XXX póki co, obs³uguje tylko libce zgodne z C99
 */
char *vsaprintf(const char *format, va_list ap)
{
	char *res, tmp[2];
	int size;

#if defined(va_copy)
	va_list aq;
	va_copy(aq, ap);
#elif defined(__va_copy)
	va_list aq;
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

CHAR_T *vwcssaprintf(const CHAR_T *format, va_list ap) 
{
#if USE_UNICODE
	CHAR_T *res, tmp[2];
	int size;

#if defined(va_copy)
	va_list aq;
	va_copy(aq, ap);
#elif defined(__va_copy)
	va_list aq;
	__va_copy(aq, ap);
#endif
	size = vswprintf(tmp, sizeof(tmp), format, ap);
	res = xmalloc((size + 1)*sizeof(CHAR_T));

#if defined(va_copy) || defined(__va_copy)
	vswprintf(res, size + 1, format, aq);
	va_end(aq);
#else
	vswprintf(res, size + 1, format, ap);
#endif
	return res;
#else
	return vsaprintf(format, ap);
#endif
}

char *xstrstr(const char *haystack, const char *needle)
{
	return strstr(fix(haystack), fix(needle));		
}

char *xstrcasestr(const char *haystack, const char *needle)
{
        return strcasestr(fix(haystack), fix(needle));
}

int xstrcasecmp(const char *s1, const char *s2) 
{
	return strcasecmp(fix(s1), fix(s2));
}

char *xstrcat(char *dest, const char *src) 
{
	return strcat(dest, fix(src));
}

char *xstrchr(const char *s, int c) 
{
	return strchr(fix(s), c);
}

int xstrcmp(const char *s1, const char *s2)
{
	return strcmp(fix(s1), fix(s2));
}

int xstrcoll(const char *s1, const char *s2)
{
	return strcoll(fix(s1), fix(s2));
}

char *xstrcpy(char *dest, const char *src) 
{
	return strcpy(dest, fix(src));
}

size_t xstrcspn(const char *s, const char *reject)
{
	return strcspn(fix(s), fix(reject));
}

char *xstrfry(char *string)
{
	return strfry(fix(string));
}

size_t xstrlen(const char *s)
{
	return strlen(fix(s));
}

size_t xwcslen(const CHAR_T *s)
{
#if USE_UNICODE
	if (!s) return 0; /* fix() ? */
	return wcslen(s);
#else
	return xstrlen(s);
#endif
}

int xstrncasecmp_pl(const char *s1, const char *s2, size_t n)
{
	return strncasecmp_pl(fix(s1), fix(s2), n);
}

char *xstrncat(char *dest, const char *src, size_t n)
{
	return strncat(dest, fix(src), n);
}

int xstrncmp(const char *s1, const char *s2, size_t n)
{
	return strncmp(fix(s1), fix(s2), n);
}

char *xstrncpy(char *dest, const char *src, size_t n)
{
	return strncpy(dest, fix(src), n);
}

int xstrncasecmp(const char *s1, const char *s2, size_t n)
{
	return strncasecmp(fix(s1), fix(s2), n);
}

char *xstrpbrk(const char *s, const char *accept) 
{
	return strpbrk(fix(s), fix(accept));
}

char *xstrrchr(const char *s, int c) 
{
	return strrchr(fix(s), c);
}
#if 0
nic tego nie u¿ywa, a to nie jest zaimplementowane na wszystkich systemach
i tylko powoduje problemy z kompilacj± [Solaris again]
char *xstrsep(char **stringp, const char *delim)
{
	/* kiedy stringo -- NULL funkcja po prostu nic nie robi - man strsep */
	return strsep(stringp, fix(delim));
}
#endif
size_t xstrspn(const char *s, const char *accept)
{
	return strspn(fix(s), fix(accept));
}

char *xstrtok(char *s, const char *delim)
{
	/* pierwszy argument powinien byæ NULL */
	return strtok(s, fix(delim));
}

char *xindex(const char *s, int c)
{
	return index(fix(s), c);
}

char *xrindex(const char *s, int c)
{
	return rindex(fix(s), c);
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
