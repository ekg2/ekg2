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
#include <time.h>

#include "dynstuff.h"
#include "log.h"
#include "xmalloc.h"

#ifndef PATH_MAX
# ifdef MAX_PATH
#  define PATH_MAX MAX_PATH
# else
#  define PATH_MAX _POSIX_PATH_MAX
# endif
#endif

list_t lasts = NULL;

int config_last_size = 10;
int config_last = 0;

static char *utf_ent[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, "&quot;", 0, 0, 0, "&amp;", "&apos;", 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "&lt;", 0, "&gt;", 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

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
	struct last *ll;
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
			ll = l->data;

			if (config_last & 2 && xstrcasecmp(ll->uid, uid))
				continue;

			if (!tmp_time)
				tmp_time = ll->time;
			
			if (ll->time <= tmp_time)
				tmp_time = ll->time;
		}
		
		/* ...by teraz usun±æ */
		for (l = lasts; l; l = l->next) {
			ll = l->data;

			if (ll->time == tmp_time && !xstrcasecmp(ll->uid, uid)) {
				xfree(ll->uid);
				xfree(ll->message);
				list_remove(&lasts, ll, 1);
				break;
			}
		}

	}
	ll = xmalloc(sizeof(struct last));
	ll->type = type;
	ll->uid = xstrdup(uid);
	ll->time = t;
	ll->sent_time = st;
	ll->message = xstrdup(msg);
	
	list_add(&lasts, ll, 0);
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
 * xml_escape()
 *
 *    escapes text to be xml-compliant
 *
 *  - text
 *
 * allocated buffer
 */
char *xml_escape(const char *text)
{
	const char *p;
	char *res, *q;
	int len;

	if (!text)
		return NULL;

	for (p = text, len = 0; *p; p++) {
		if (*p >= sizeof(utf_ent)/sizeof(char)) len += 1;
		else len += (utf_ent[(int) *p] ? xstrlen(utf_ent[(int) *p]) : 1);
	}

	res = xmalloc((len + 1)*sizeof(char));
	for (p = text, q = res; *p; p++) {
		const char *ent = utf_ent[(int) *p];
		if (*p >= sizeof(utf_ent)/sizeof(char)) ent = NULL;

		if (ent)
			xstrcpy(q, ent);
		else
			*q = *p;

		q += ent ? xstrlen(ent) : 1;
	}

	return res;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
