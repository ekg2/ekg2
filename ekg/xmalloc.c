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
#include "win32.h"

#define __EXTENSIONS__
#include <sys/types.h>
#include <stddef.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "configfile.h"
#include "stuff.h"
#include "userlist.h"
#include "xmalloc.h"

#ifdef NO_POSIX_SYSTEM
#include <winbase.h>

#define strcasecmp(s1, s2) lstrcmpiA(s1, s2)
#endif

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

/**
 * xcalloc()
 *
 * @bug same as in xmalloc()
 *
 */

void *xcalloc(size_t nmemb, size_t size)
{
	void *tmp = calloc(nmemb, size);

	if (!tmp)
		ekg_oom_handler();

	return tmp;
}

/** 
 * xmalloc() 
 *
 * Allocate memory for @a size bytes, clears it [set it with \\0], and returns pointer to allocated memory.
 * If malloc() fails with NULL, ekg_oom_handler() kills program.<br>
 * Wrapper to <code>malloc()+memset()</code><br>
 *
 * @bug Possible bug: Some libc may return NULL if size is 0, from man malloc:<br>
 *	<i>If @a size is 0 (...) a <b>null pointer</b> (...) shall be returned.</i><br>
 *	XXX, check it in configure.ac if malloc() returns NULL on 0 size, and check here if size is 0.
 *
 * @sa xcalloc()
 * @sa xfree()
 *
 * @param size - the same as in malloc()
 *
 * @return pointer to allocated memory 
 */

void *xmalloc(size_t size)
{
	void *tmp = malloc(size);

	if (!tmp)
		ekg_oom_handler();

	/* na wszelki wypadek wyczy¶æ bufor */
	memset(tmp, 0, size);
	
	return tmp;
}

/** 
 * xfree()
 *
 * Free memory pointed by @a ptr
 * if @a ptr == NULL do nothing.<br>
 * Equivalent to: <code>if (ptr) free(ptr);</code>
 *
 * @sa xrealloc() - If you want change size of allocated memory.
 */

void xfree(void *ptr)
{
	if (ptr)
		free(ptr);
}

/**
 * xrealloc()
 *
 * @bug same as in xmalloc()
 *
 */

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

	if (!(tmp = (char *) strdup(s)))
		ekg_oom_handler();

	return tmp;
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
#ifdef NO_POSIX_SYSTEM
	char tmp[10000];
#else
	char tmp[2];
#endif
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
#ifdef NO_POSIX_SYSTEM
	if (n == 0) return 0;
	while (n-- != 0 && tolower(*s1) == tolower(*s2)) {
		if (n == 0 || *s1 == '\0' || *s2 == '\0')
			break;
		s1++;
		s2++;
	}
	return tolower(*(unsigned char *) s1) - tolower(*(unsigned char *) s2);
#else
	return strncasecmp(fix(s1), fix(s2), n);
#endif
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
#ifdef NO_POSIX_SYSTEM
	return xstrchr(s, c);
#else
	return index(fix(s), c);
#endif
}

char *xrindex(const char *s, int c)
{
#ifdef NO_POSIX_SYSTEM
	return xstrrchr(s, c);
#else
	return rindex(fix(s), c);
#endif
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
