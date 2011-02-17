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

#include "ekg2-config.h"
#include "win32.h"

#include <glib.h>

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

#ifndef EKG_NO_DEPRECATED

/**
 * xcalloc()
 *
 * @bug same as in xmalloc()
 *
 * @note Deprecated, please use g_new(), g_new0(), g_slice_new()
 * or g_slice_new0() instead.
 */

void *xcalloc(size_t nmemb, size_t size)
{
	return g_malloc0_n(nmemb, size);
}

/** 
 * xmalloc() 
 *
 * Allocate memory for @a size bytes, clears it [set it with \\0], and returns pointer to allocated memory.
 * If malloc() fails with NULL, ekg_oom_handler() kills program.<br>
 * Wrapper to <code>malloc()+memset()</code><br>
 *
 * @sa xcalloc()
 * @sa xfree()
 *
 * @param size - the same as in malloc()
 *
 * @return pointer to allocated memory 
 *
 * @note Deprecated, please use g_malloc(), g_malloc0(), g_new(),
 * g_new0(), g_slice_new() or g_slice_new0() instead.
 */

void *xmalloc(size_t size)
{
	return g_malloc0(size);
}

/** 
 * xfree()
 *
 * Free memory pointed by @a ptr
 * if @a ptr == NULL do nothing.<br>
 * Equivalent to: <code>if (ptr) free(ptr);</code>
 *
 * @sa xrealloc() - If you want change size of allocated memory.
 *
 * @note Deprecated, please use g_free() (or g_slice_free()) instead.
 */

void xfree(void *ptr)
{
	g_free(ptr);
}

/**
 * xrealloc()
 *
 * @bug same as in xmalloc()
 *
 * @note Deprecated, please use g_realloc() instead.
 */

void *xrealloc(void *ptr, size_t size)
{
	return g_realloc(ptr, size);
}

/**
 * xstrdup()
 *
 * @note Deprecated, please use g_strdup() instead.
 */
char *xstrdup(const char *s)
{
	return g_strdup((char *) s);
}

/**
 * xstrndup()
 *
 * @note Deprecated, please use g_strndup() and g_strdup() instead.
 */
char *xstrndup(const char *s, size_t n)
{
	if (n == (size_t) -1)
		return g_strdup((char *) s);
	return g_strndup((char *) s, n);
}

#endif

char *utf8ndup(const char *s, size_t n) {
	/* XXX any suggestions for function name? IDKHTNI_ndup()? (IDKHTNI - I Don't Know How To Name It) */
	char *tmp = xstrndup(s, n);

	if (!tmp) return NULL;

	if (!is_utf8_string(s)) return tmp;

	if (n<0) n = xstrlen(tmp);

	while (!is_utf8_string(tmp) && n--)
		*(tmp + n) = 0;

	return tmp;
}

#ifndef EKG_NO_DEPRECATED

/**
 * vsaprintf()
 *
 * @note Deprecated, please use g_strdup_vprintf() instead.
 */
char *vsaprintf(const char *format, va_list ap)
{
	return g_strdup_vprintf(format, ap);
}

#endif

/* XXX: most of the below funcs seem braindead, deprecate them as well? */

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
