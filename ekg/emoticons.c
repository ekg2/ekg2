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
#include <stdio.h>

#include "dynstuff.h"
#include "dynstuff_inline.h"
#include "stuff.h"
#include "xmalloc.h"

typedef struct emoticon {
	struct emoticon *next;

        char *name;
        char *value;
} emoticon_t;

emoticon_t *emoticons = NULL;

static LIST_FREE_ITEM(list_emoticon_free, emoticon_t *) { xfree(data->name); xfree(data->value); }

DYNSTUFF_LIST_DECLARE(emoticons, emoticon_t, list_emoticon_free,
	static __DYNSTUFF_LIST_ADD,		/* emoticons_add() */
	__DYNSTUFF_NOREMOVE,
	__DYNSTUFF_LIST_DESTROY)		/* emoticons_destroy() */

int config_emoticons = 1;

/*
 * emoticon_add()
 *
 * dodaje dany emoticon do listy.
 *
 *  - name - nazwa,
 *  - value - warto¶æ.
 *
 * 0/-1
 */
static int emoticon_add(const char *name, const char *value) {
	emoticon_t *e, *el;

	if (!name || !value)
		return -1;

	for (el = emoticons; el; el = el->next) {
		e = el;
		if (!xstrcasecmp(name, e->name)) {
			xfree(e->value);
			e->value = xstrdup(value);
			return 0;
		}
	}
	e = xmalloc(sizeof(emoticon_t));
	e->name = xstrdup(name);
	e->value = xstrdup(value);

	emoticons_add(e);

	return 0;
}

/*
 * emoticon_read()
 *
 * ³aduje do listy wszystkie makra z pliku ~/.gg/emoticons
 * format tego pliku w dokumentacji.
 *
 * 0/-1
 */
int emoticon_read() {
	const char *filename;
	char *buf;
	FILE *f;

	if (!(filename = prepare_pathf("emoticons")))
		return -1;
	
	if (!(f = fopen(filename, "r")))
		return -1;

	while ((buf = read_file(f, 0))) {
		char **emot;
	
		if (buf[0] == '#')
			continue;

		emot = array_make(buf, "\t", 2, 1, 1);
	
		if (array_count(emot) == 2)
			emoticon_add(emot[0], emot[1]);

		array_free(emot);
	}
	
	fclose(f);
	
	return 0;
}

/*
 * emoticon_expand()
 *
 * rozwija definicje makr (najczê¶ciej bêd± to emoticony)
 *
 *  - s - string z makrami.
 *
 * zwraca zaalokowany, rozwiniêty string.
 */
char *emoticon_expand(const char *s) {
	emoticon_t *el = NULL;
	const char *ss;
	char *ms;
	size_t n = 0;

	if (!s)
		return NULL;

	for (ss = s; *ss; ss++) {
		emoticon_t *e = NULL;
		size_t ns = xstrlen(ss);
		int ret = 1;

		for (el = emoticons; el && ret; el = (ret ? el->next : el)) {
			size_t nn;

			e = el;
			nn = xstrlen(e->name);
			if (ns >= nn)
				ret = xstrncmp(ss, e->name, nn);
		}

		if (el) {
			e = el;
			n += xstrlen(e->value);
			ss += xstrlen(e->name) - 1;
		} else
			n++;
	}

	ms = xcalloc(1, n + 1);

	for (ss = s; *ss; ss++) {
		emoticon_t *e = NULL;
		size_t ns = xstrlen(ss);
		int ret = 1;

		for (el = emoticons; el && ret; el = (ret ? el->next : el)) {
			size_t n;

			e = el;
			n = xstrlen(e->name);
			if (ns >= n)
				ret = xstrncmp(ss, e->name, n);
		}

		if (el) {
			e = el;
			xstrcat(ms, e->value);
			ss += xstrlen(e->name) - 1;
		} else
			ms[xstrlen(ms)] = *ss;
	}

	return ms;
}

/*
 * emoticon_remove()
 *
 * usuwa emoticon o danej nazwie.
 *
 *  - name.
 *
 * 0/-1
 */
#if 0 /* never used, never exported, uncomment when needed */
static int emoticon_remove(const char *name)
{
	list_t l;

	for (l = emoticons; l; l = l->next) {
		emoticon_t *f = l->data;

		if (!xstrcasecmp(f->name, name)) {
			emoticons_remove(f);
			return 0;
		}
	}
	
	return -1;
}
#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
