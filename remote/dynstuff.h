/* $Id: dynstuff.h 4542 2008-08-28 18:42:26Z darkjames $ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Adam Wysocki <gophi@ekg.chmurka.net>
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

#ifndef __EKG_DYNSTUFF_H
#define __EKG_DYNSTUFF_H

struct list {
		/* it is important that we keep 'next' first field,
		 * because C allows us to call first field of any structure
		 * without knowing its' type
		 *
		 * so *3() would work peacefully w/ both list_t and not-list_t lists */
	struct list *next;

	void *data;
};

typedef struct list *list_t;

#define LIST_ADD_COMPARE(x, type)			int x(const type data1, const type data2)
#define LIST_ADD_SORTED2(list, data, comp)		list_add_sorted3((list_t *) (void *) list, (list_t) data, (void *) comp)
#define LIST_ADD_BEGINNING2(list, data)			list_add_beginning3((list_t *) (void *) list, (list_t) data)
#define LIST_ADD2(list, data)				list_add3((list_t *) (void *) list, (list_t) data)

#define LIST_COUNT2(list)				list_count((list_t) list)
#define LIST_REMOVE2(list, elem, func)			list_remove3((list_t *) (void *) list, (list_t) elem, (void *) func)
#define LIST_UNLINK2(list, elem)			list_unlink3((list_t *) (void *) list, (list_t) elem)
#define LIST_FREE_ITEM(x, type)				void x(type data)

#define LIST_DESTROY2(list, func)			list_destroy3((list_t) list, (void *) func)

void *list_add_beginning(list_t *list, void *data);

void *list_add3(list_t *list, list_t new);
void *list_add_sorted3(list_t *list, list_t new, int (*comparision)(void *, void *));
void list_add_beginning3(list_t *list, list_t new);

void *list_remove3(list_t *list, list_t elem, void (*func)(list_t));
void *list_remove3i(list_t *list, list_t elem, void (*func)(list_t data));
void *list_unlink3(list_t *list, list_t elem);

int list_destroy(list_t list);
int list_destroy3(list_t list, void (*func)(void *));

void list_cleanup(list_t *list);
int list_remove_safe(list_t *list, void *data);

struct string {
	char *str;
	int len, size;
};

typedef struct string *string_t;

string_t string_init(const char *str);
int string_append(string_t s, const char *str);
int string_append_n(string_t s, const char *str, int count);
int string_append_c(string_t s, char ch);
int string_append_raw(string_t s, const char *str, int count);
void string_remove(string_t s, int count);
char *string_free(string_t s, int free_string);

/* tablice stringow */
char **array_make(const char *string, const char *sep, int max, int trim, int quotes);
char *array_join(char **array, const char *sep);

int array_add(char ***array, char *string);
int array_add_check(char ***array, char *string, int casesensitive);
int array_count(char **array);
int array_item_contains(char **array, const char *string, int casesensitive);
void array_free(char **array);

/* rozszerzenia libców */

const char *itoa(long int i);

/*
 * handle private data
 */
typedef struct private_data_s {
	struct private_data_s *next;

	char *name;
	char *value;
} private_data_t;

void private_item_set(private_data_t **data, const char *item_name, const char *value);
int private_item_get_int(private_data_t **data, const char *item_name);
void private_items_destroy(private_data_t **data);

#endif /* __EKG_DYNSTUFF_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
