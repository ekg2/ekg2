/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "configfile.h"
#include "stuff.h"
#include "userlist.h"

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

char *saprintf(const char *format, ...)
{
	va_list ap;
	char *res;

	va_start(ap, format);
	res = vsaprintf(format, ap);
	va_end(ap);
	
	return res;
}


