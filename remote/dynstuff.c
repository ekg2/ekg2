/* $Id: dynstuff.c 4542 2008-08-28 18:42:26Z darkjames $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "dynstuff.h"
#include "dynstuff_inline.h"
#include "xmalloc.h"

EXPORTNOT void *list_add_beginning(list_t *list, void *data) {
	list_t new;

	new = xmalloc2(sizeof(struct list));
	new->next = *list;
	*list	  = new;

	new->data = data;

	return new->data;
}

void *list_add_sorted3(list_t *list, list_t new, int (*comparision)(void *, void *))
{
	list_t tmp;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new->next = NULL;
	if (!(tmp = *list)) {
		*list = new;
	} else {
		if (!comparision) {
			while (tmp->next)
				tmp = tmp->next;
			tmp->next = new;
		} else {
			list_t prev = NULL;
			
			while (comparision(new, tmp) > 0) {
				prev = tmp;
				tmp = tmp->next;
				if (!tmp)
					break;
			}
			
			if (!prev) {
				new->next = *list;
				*list = new;
			} else {
				prev->next = new;
				new->next = tmp;
			}
		}
	}

	return new;
}

EXPORTNOT void list_add_beginning3(list_t *list, list_t new) {
	new->next = *list;
	*list	  = new;
}

void *list_add3(list_t *list, list_t new)
{
	return list_add_sorted3(list, new, NULL);
}

int list_count(list_t list)
{
	int count = 0;

	for (; list; list = list->next)
		count++;

	return count;
}

EXPORTNOT int list_remove_safe(list_t *list, void *data) {
	list_t tmp;

	for (tmp = *list; tmp; tmp = tmp->next) {
		if (tmp->data == data) {
			xfree(tmp->data);
			tmp->data = NULL;
			return 0;
		}
	}

	errno = ENOENT;
	return -1;
}

EXPORTNOT void list_cleanup(list_t *list) {
	list_t tmp;
	list_t last = NULL;

	for (tmp = *list; tmp;) {
		if (tmp->data == NULL) {
			list_t cur = tmp;

			tmp = tmp->next;		/* move to next item */

			if (!last)
				*list = tmp;		/* repoint list to next item */	
			else
				last->next = tmp;	/* repoint last->next to next item */

			xfree(cur);			/* free current item struct */
		} else {
			last = tmp;
			tmp = tmp->next;
		}
	}
}

void *list_remove3(list_t *list, list_t elem, void (*func)(list_t data)) {
	list_t tmp, last = NULL;
	void *ret = NULL;

	if (!list) {
		errno = EFAULT;
		return ret;
	}

	tmp = *list;
	if (tmp && tmp == elem) {
		*list = ret = tmp->next;
	} else {
		for (; tmp && tmp != elem; tmp = tmp->next)
			last = tmp;
		if (!tmp) {
			errno = ENOENT;
			return ret;
		}
		last->next = ret = tmp->next;
	}

	if (func)
		func(tmp);
	xfree(tmp);

	return ret;
}

EXPORTNOT void *list_remove3i(list_t *list, list_t elem, void (*func)(list_t data)) {
	list_t tmp, last = NULL;
	void *ret = NULL;

	tmp = *list;
	if (tmp && tmp == elem) {
		*list = tmp->next;
		ret = list;
	} else {
		for (; tmp && tmp != elem; tmp = tmp->next)
			last = tmp;
		if (!tmp) {
			errno = ENOENT;
			return ret;
		}
		last->next = tmp->next;
		ret = last;
	}

	if (func)
		func(tmp);
	xfree(tmp);

	return ret;
}

EXPORTNOT void *list_unlink3(list_t *list, list_t elem) {
	list_t tmp, last = NULL;
	void *ret = NULL;

	tmp = *list;
	if (tmp && tmp == elem) {
		*list = ret = tmp->next;
	} else {
		for (; tmp && tmp != elem; tmp = tmp->next)
			last = tmp;
		if (!tmp) {
			errno = ENOENT;
			return ret;
		}
		last->next = ret = tmp->next;
	}

	return ret;
}

int list_destroy3(list_t list, void (*func)(void *)) {
	list_t tmp;
	
	while (list) {
		if (func)
			func(list);

		tmp = list->next;

		xfree(list);

		list = tmp;
	}

	return 0;
}

EXPORTNOT int list_destroy(list_t list) {
	list_t tmp;
	
	while (list) {
		tmp = list->next;

		xfree(list);

		list = tmp;
	}

	return 0;
}

static void string_realloc(string_t s, int count)
{
	if ((count + 1) <= s->size)
		return;
	
	s->str = xrealloc(s->str, count + 81);
	s->size = count + 81;
}

int string_append_c(string_t s, char c)
{
	if (!s) {
		errno = EFAULT;
		return -1;
	}
	
	string_realloc(s, s->len + 1);

	s->str[s->len++] = c;
	s->str[s->len] = '\0';

	return 0;
}

EXPORTNOT int string_append_n(string_t s, const char *str, int count)
{
	string_realloc(s, s->len + count);

	strncpy(s->str + s->len, str, count);

	s->len += count;
	s->str[s->len] = '\0';

	return 0;
}

EXPORTNOT int string_append_raw(string_t s, const char *str, int count) {
	string_realloc(s, s->len + count);

	memcpy(s->str + s->len, str, count);

	s->len += count;
	s->str[s->len] = '\0';

	return 0;
}

int string_append(string_t s, const char *str)
{
	if (!s || !str) {
		errno = EFAULT;
		return -1;
	}

	return string_append_n(s, str, strlen(str));
}

string_t string_init(const char *value) {
	string_t tmp = xmalloc2(sizeof(struct string));
	int valuelen;

	if (!value) {
		value = "";
		valuelen = 0;
	} else {
		valuelen = strlen(value);
	}

	tmp->str = xstrdup(value);
	tmp->len = valuelen;
	tmp->size = valuelen + 1;

	return tmp;
}

EXPORTNOT void string_remove(string_t s, int count) {
	if (!s || count <= 0)
		return;
	
	if (count >= s->len) {
		/* string_clear() */
		s->str[0]	= '\0';
		s->len		= 0;

	} else {
		memmove(s->str, s->str + count, s->len - count);
		s->len -= count;
		s->str[s->len] = '\0';
	}
}

char *string_free(string_t s, int free_string)
{
	char *tmp = NULL;

	if (!s)
		return NULL;

	if (free_string)
		xfree(s->str);
	else
		tmp = s->str;

	xfree(s);

	return tmp;
}

const char *itoa(long int i)
{
	static char bufs[10][16];
	static int index = 0;
	char *tmp = bufs[index++];

	if (index > 9)
		index = 0;
	
	snprintf(tmp, 16, "%ld", i);

	return tmp;
}

char **array_make(const char *string, const char *sep, int max, int trim, int quotes)
{
	const char *p, *q;
	char **result = NULL;
	int items = 0, last = 0;

	if (!string || !sep)
		return xcalloc(1, sizeof(char*));

	for (p = string; ; ) {
		int len = 0;
		char *token = NULL;

		if (max && items >= max - 1)
			last = 1;
		
		if (trim) {
			while (*p && strchr(sep, *p))
				p++;
			if (!*p)
				break;
		}

		if (quotes && (*p == '\'' || *p == '\"')) {
			char sep = *p;
			char *r;

			for (q = p + 1, len = 0; *q; q++, len++) {
				if (*q == '\\') {
					q++;
					if (!*q)
						break;
				} else if (*q == sep)
					break;
			}

			if (last && q[0] && q[1])
				goto way2;

			len++;

			r = token = xmalloc(len + 1);
			for (q = p + 1; *q; q++, r++) {
				if (*q == '\\') {
					q++;

					if (!*q)
						break;

					switch (*q) {
						case 'n':
							*r = '\n';
							break;
						case 'r':
							*r = '\r';
							break;
						case 't':
							*r = '\t';
							break;
						default:
							*r = *q;
					}
				} else if (*q == sep) {
					break;
				} else 
					*r = *q;
			}

			*r = 0;
			
			p = (*q) ? q + 1 : q;

		} else {
way2:
			for (q = p, len = 0; *q && (last || !strchr(sep, *q)); q++, len++);
			token = xstrndup(p, len);
			p = q;
		}
		
		result = xrealloc(result, (items + 2) * sizeof(char*));
		result[items] = token;
		result[++items] = NULL;

		if (!*p)
			break;

		p++;
	}

	if (!items)
		result = xcalloc(1, sizeof(char*));

	return result;
}

int array_count(char **array)
{
	int result = 0;

	if (!array)
		return 0;

	while (*array) {
		result++;
		array++;
	}

	return result;
}

int array_add(char ***array, char *string)
{
	int count = array_count(*array);

	*array = xrealloc(*array, (count + 2) * sizeof(char*));
	(*array)[count + 1] = NULL;
	(*array)[count] = string;

	return count + 1;
}

int array_add_check(char ***array, char *string, int casesensitive)
{
	if (!array_item_contains(*array, string, casesensitive))
		return array_add(array, string);
	else
		xfree(string);
	return 0;
}

char *array_join(char **array, const char *sep)
{
	string_t s = string_init(NULL);
	int i;

	if (!array)
		return string_free(s, 0);

	for (i = 0; array[i]; i++) {
		if (i)
			string_append(s, sep);

		string_append(s, array[i]);
	}

	return string_free(s, 0);
}

int array_item_contains(char **array, const char *string, int casesensitive)
{
	int i;

	if (!array || !string)
		return 0;

	for (i = 0; array[i]; i++) {
		if (casesensitive && xstrstr(array[i], string))
			return 1;
		if (!casesensitive && xstrcasestr(array[i], string))
			return 1;
	}

	return 0;
}

void array_free(char **array)
{
	char **tmp;

	if (!array)
		return;

	for (tmp = array; *tmp; tmp++)
		xfree(*tmp);

	xfree(array);
}

/*
 * handle private data
 */
static int private_data_cmp(private_data_t *item1, private_data_t *item2) {
	return xstrcmp(item1->name, item2->name);
}

static void private_data_free(private_data_t *item) {
	xfree(item->name);
	xfree(item->value);
}

static __DYNSTUFF_ADD_SORTED(private_items, private_data_t, private_data_cmp)
static __DYNSTUFF_REMOVE_SAFE(private_items, private_data_t, private_data_free)
EXPORTNOT __DYNSTUFF_DESTROY(private_items, private_data_t, private_data_free)

static private_data_t *private_item_find(private_data_t **data, const char *item_name) {
	private_data_t *item;

	if (!item_name)
		return NULL;

	for (item = *data; item; item = item->next) {
		int cmp = xstrcmp(item->name, item_name);
		if (!cmp)
			return item;
	}

	return NULL;
}

int private_item_get_int(private_data_t **data, const char *item_name) {
	private_data_t *item;
	
	if ((item = private_item_find(data, item_name)))
		return atoi(item->value);

	return 0;
}

EXPORTNOT void private_item_set(private_data_t **data, const char *item_name, const char *value) {
	private_data_t *item = private_item_find(data, item_name);
	int unset = !(value && *value);

	if (item) {
		if (unset) {
			private_items_remove(data, item);
		} else if (xstrcmp(item->value, value)) {
			xfree(item->value);
			item->value = xstrdup(value);
		}
	} else if (!unset) {
		item = xmalloc(sizeof(private_data_t));
		item->name = xstrdup(item_name);
		item->value = xstrdup(value);
		private_items_add(data, item);
	}
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
