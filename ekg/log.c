/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "log.h"
#ifdef HAVE_ZLIB_H
#  include <zlib.h>
#endif

#include "dynstuff.h"
#ifndef HAVE_STRLCAT
#  include "compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "stuff.h"
#include "xmalloc.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

list_t lasts = NULL;

int config_last_size = 10;
int config_last = 0;
int config_log = 0;
int config_log_ignored = 0;
int config_log_status = 0;
char *config_log_path = NULL;
char *config_log_timestamp = NULL;

/*
 * last_add()
 *
 * dodaje wiadomo¶æ do listy ostatnio otrzymanych.
 * 
 *  - type - rodzaj wiadomo¶ci,
 *  - uid - nadawca,
 *  - t - czas,
 *  - st - czas nadania,
 *  - msg - tre¶æ wiadomo¶ci.
 */
void last_add(int type, const char *uid, time_t t, time_t st, const char *msg)
{
	list_t l;
	struct last ll;
	int count = 0;

	/* nic nie zapisujemy, je¿eli user sam nie wie czego chce. */
	if (config_last_size <= 0)
		return;
	
	if (config_last & 2) 
		count = last_count(uid);
	else
		count = list_count(lasts);
				
	/* usuwamy ostatni± wiadomo¶æ, w razie potrzeby... */
	if (count >= config_last_size) {
		time_t tmp_time = 0;
		
		/* najpierw j± znajdziemy... */
		for (l = lasts; l; l = l->next) {
			struct last *lll = l->data;

			if (config_last & 2 && xstrcasecmp(lll->uid, uid))
				continue;

			if (!tmp_time)
				tmp_time = lll->time;
			
			if (lll->time <= tmp_time)
				tmp_time = lll->time;
		}
		
		/* ...by teraz usun±æ */
		for (l = lasts; l; l = l->next) {
			struct last *lll = l->data;

			if (lll->time == tmp_time && !xstrcasecmp(lll->uid, uid)) {
				xfree(lll->message);
				list_remove(&lasts, lll, 1);
				break;
			}
		}

	}

	ll.type = type;
	ll.uid = xstrdup(uid);
	ll.time = t;
	ll.sent_time = st;
	ll.message = xstrdup(msg);
	
	list_add(&lasts, &ll, sizeof(ll));

	return;
}

/*
 * last_del()
 *
 * usuwa wiadomo¶ci skojarzone z dan± osob±.
 *
 *  - uin - numerek osoby.
 */
void last_del(const char *uid)
{
	list_t l;

	for (l = lasts; l; ) {
		struct last *ll = l->data;

		l = l->next;

		if (!xstrcasecmp(uid, ll->uid)) {
			xfree(ll->uid);
			xfree(ll->message);
			list_remove(&lasts, ll, 1);
		}
	}
}

/*
 * last_count()
 *
 * zwraca ilo¶æ wiadomo¶ci w last dla danej osoby.
 *
 *  - uin.
 */
int last_count(const char *uid)
{
	int count = 0;
	list_t l;

	for (l = lasts; l; l = l->next) {
		struct last *ll = l->data;

		if (!xstrcasecmp(uid, ll->uid))
			count++;
	}

	return count;
}

/*
 * last_free()
 *
 * zwalnia miejsce po last.
 */
void last_free()
{
	list_t l;

	if (!lasts)
		return;

	for (l = lasts; l; l = l->next) {
		struct last *ll = l->data;
		
		xfree(ll->uid);
		xfree(ll->message);
	}

	list_destroy(lasts, 1);
	lasts = NULL;
}

/*
 * log_escape()
 *
 * je¶li trzeba, eskejpuje tekst do logów.
 * 
 *  - str - tekst.
 *
 * zaalokowany bufor.
 */
char *log_escape(const char *str)
{
	const char *p;
	char *res, *q;
	int size, needto = 0;

	if (!str)
		return NULL;
	
	for (p = str; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == ',')
			needto = 1;
	}

	if (!needto)
		return xstrdup(str);

	for (p = str, size = 0; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == '\\')
			size += 2;
		else
			size++;
	}

	q = res = xmalloc(size + 3);
	
	*q++ = '"';
	
	for (p = str; *p; p++, q++) {
		if (*p == '\\' || *p == '"' || *p == '\'') {
			*q++ = '\\';
			*q = *p;
		} else if (*p == '\n') {
			*q++ = '\\';
			*q = 'n';
		} else if (*p == '\r') {
			*q++ = '\\';
			*q = 'r';
		} else
			*q = *p;
	}
	*q++ = '"';
	*q = 0;

	return res;
}

/*
 * put_log()
 *
 * wrzuca do logów informacjê od/do danego numerka. podaje siê go z tego
 * wzglêdu, ¿e gdy `log = 2', informacje lec± do $config_log_path/$uin.
 * automatycznie eskejpuje, co trzeba.
 *
 *  - uin - numer delikwenta,
 *  - format... - akceptuje tylko %s, %d i %ld.
 */
void put_log(const char *uid, const char *format, ...)
{
 	char *lp = config_log_path;
	char path[PATH_MAX], *buf;
	const char *p;
	size_t size = 0;
	va_list ap;
	FILE *f;

	if (!config_log)
		return;

	/* oblicz d³ugo¶æ tekstu */
	va_start(ap, format);
	for (p = format; *p; p++) {
		int long_int = 0;

		if (*p == '%') {
			p++;
			if (!*p)
				break;
			
			if (*p == 'l') {
				p++;
				long_int = 1;
				if (!*p)
					break;
			}
			
			if (*p == 's') {
				char *e, *tmp = va_arg(ap, char*);

				e = log_escape(tmp);
				size += xstrlen(e);
				xfree(e);
			}
			
			if (*p == 'd') {
				int tmp = ((long_int) ? va_arg(ap, long) : va_arg(ap, int));

				size += xstrlen(itoa(tmp));
			}
		} else
			size++;
	}
	va_end(ap);

	/* zaalokuj bufor */
	buf = xmalloc(size + 1);
	*buf = 0;

	/* utwórz tekst z logiem */
	va_start(ap, format);
	for (p = format; *p; p++) {
		int long_int = 0;

		if (*p == '%') {
			p++;
			if (!*p)
				break;
			if (*p == 'l') {
				p++;
				long_int = 1;
				if (!*p)
					break;
			}

			if (*p == 's') {
				char *e, *tmp = va_arg(ap, char*);

				e = log_escape(tmp);
				xstrcat(buf, e);
				xfree(e);
			}

			if (*p == 'd') {
				int tmp = ((long_int) ? va_arg(ap, long) : va_arg(ap, int));

				xstrcat(buf, itoa(tmp));
			}
		} else {
			buf[xstrlen(buf) + 1] = 0;
			buf[xstrlen(buf)] = *p;
		}
	}

	/* teraz skonstruuj ¶cie¿kê logów */
	if (!lp)
		lp = (config_log & 2) ? (char *) prepare_path("", 0) : (char *) prepare_path("history", 0);

	if (*lp == '~')
		snprintf(path, sizeof(path), "%s%s", home_dir, lp + 1);
	else
		strlcpy(path, lp, sizeof(path));

	if ((config_log & 2)) {
		if (mkdir(path, 0700) && errno != EEXIST)
			goto cleanup;
		snprintf(path + xstrlen(path), sizeof(path) - xstrlen(path), "/%s", uid);
	}

#ifdef HAVE_ZLIB
	/* nawet je¶li chcemy gzipowane logi, a istnieje nieskompresowany log,
	 * olewamy kompresjê. je¶li loga nieskompresowanego nie ma, dodajemy
	 * rozszerzenie .gz i balujemy. */
	if (config_log & 4) {
		struct stat st;
		
		if (stat(path, &st) == -1) {
			gzFile f;

			snprintf(path + xstrlen(path), sizeof(path) - xstrlen(path), ".gz");

			if (!(f = gzopen(path, "a")))
				goto cleanup;

			gzputs(f, buf);
			gzclose(f);
			chmod(path, 0600);

			goto cleanup;
		}
	}
#endif

	if (!(f = fopen(path, "a")))
		goto cleanup;
	fputs(buf, f);
	fclose(f);
	chmod(path, 0600);

cleanup:
	xfree(buf);
}

/* 
 * log_timestamp()
 *
 * zwraca timestamp logów zgodnie z ¿yczeniem u¿ytkownika. 
 *
 *  - t - czas, który mamy zamieniæ.
 *
 * zwraca na przemian jeden z dwóch statycznych buforów, wiêc w obrêbie
 * jednego wyra¿enia mo¿na wywo³aæ tê funkcjê dwukrotnie.
 */
const char *log_timestamp(time_t t)
{
	static char buf[2][100];
	struct tm *tm = localtime(&t);
	static int i = 0;

	i = i % 2;

	if (config_log_timestamp) {
		if (!strftime(buf[i], sizeof(buf[0]), config_log_timestamp, tm)
				&& xstrlen(config_log_timestamp)>0)
			xstrcpy(buf[i], "TOOLONG");
		return buf[i++];
	} else
		return itoa(t);
}
