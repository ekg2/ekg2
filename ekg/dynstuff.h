/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
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

#include "char.h"
/*
 * typedef list_t
 *
 * list_t jest prostym typem listy u¿ywanej w praktycznie wszystkich
 * dynamicznych strukturach ekg. obecnie jest to lista jednokierunkowa
 * (pole `prev' jest równe NULL), ale zostawiono mo¿liwo¶æ rozbudowy
 * do dwukierunkowej bez zmiany ABI. dane s± przechowywane w polu `data',
 * kolejny element w `next'. przyk³adowy kod iteracji:
 *
 *     list_t l;
 *
 *     for (l = lista; l; l = l->next) {
 *         struct cokolwiek *c = l->data;
 *         printf("%s\n", c->cokolwiek);
 *     }
 *
 * wiêkszo¶æ list wystêpuj±cych w ekg mo¿na iterowaæ bez obawy o zmiany
 * ABI. pierwsze pole, bêd±ce jednoznacznym identyfikatorem elementu listy
 * jest dostêpne bezpo¶rednio, reszta przez odpowiednie funkcje.
 */

struct list {
	void *data;
	/*struct list *prev;*/
	struct list *next;
};

typedef struct list *list_t;

#ifndef EKG2_WIN32_NOFUNCTION
void *list_add(list_t *list, void *data, int alloc_size);
void *list_add_sorted(list_t *list, void *data, int alloc_size, int (*comparision)(void *, void *));
int list_remove(list_t *list, void *data, int free_data);
int list_count(list_t list);
int list_destroy(list_t list, int free_data);
#endif

/*
 * typedef string_t
 *
 * prosty typ tekstowy pozwalaj±cy tworzyæ ci±gi tekstowe o dowolnej
 * d³ugo¶ci bez obawy o rozmiar bufora. ci±g tekstowy jest dostêpny
 * przez pole `str'. nie nale¿y go zmieniaæ bezpo¶rednio. przyk³adowy
 * kod:
 *
 *     string_t s;
 *
 *     s = string_init("ala");
 *     string_append_c(s, ' ');
 *     string_append(s, "ma kota");
 *     printf("%s\n", s->str);
 *     string_free(s, 1);
 */

struct string {
	char *str;
	int len, size;
};

struct wcs_string {
	CHAR_T *str;
	int len, size;
};

typedef struct string *string_t;
typedef struct wcs_string *wcs_string_t;

#ifndef EKG2_WIN32_NOFUNCTION

string_t string_init(const char *str);
wcs_string_t wcs_string_init(const CHAR_T *value);
int string_append(string_t s, const char *str);
int wcs_string_append(wcs_string_t s, const CHAR_T *str);
int string_append_n(string_t s, const char *str, int count);
int wcs_string_append_n(wcs_string_t s, const CHAR_T *str, int count);
int string_append_c(string_t s, char ch);
int wcs_string_append_c(wcs_string_t s, CHAR_T c);
void string_insert(string_t s, int index, const char *str);
void string_insert_n(string_t s, int index, const char *str, int count);
void string_clear(string_t s);
void wcs_string_clear(wcs_string_t s);
char *string_free(string_t s, int free_string);
CHAR_T *wcs_string_free(wcs_string_t s, int free_string);

/* tablice stringów */
char **wcs_array_to_str(CHAR_T **arr);

char **array_make(const char *string, const char *sep, int max, int trim, int quotes);
CHAR_T **wcs_array_make(const CHAR_T *string, const CHAR_T *sep, int max, int trim, int quotes);
char *array_join(char **array, const char *sep);
CHAR_T *wcs_array_join(CHAR_T **array, const CHAR_T *sep);

void array_add(char ***array, char *string);
void wcs_array_add(CHAR_T ***array, CHAR_T *string);
void array_add_check(char ***array, char *string, int casesensitive);
void wcs_array_add_check(CHAR_T ***array, CHAR_T *string, int casesensitive);
int array_count(char **array);
int wcs_array_count(CHAR_T **array);
int array_contains(char **array, const char *string, int casesensitive);
int wcs_array_contains(CHAR_T **array, const CHAR_T *string, int casesensitive);
int array_item_contains(char **array, const char *string, int casesensitive);
int wcs_array_item_contains(CHAR_T **array, const CHAR_T *string, int casesensitive);
void array_free(char **array);
void wcs_array_free(CHAR_T **array);

/* rozszerzenia libców */

const char *itoa(long int i);
const CHAR_T *wcs_itoa(long int i);
int wcs_atoi(const CHAR_T *nptr);

#endif

#endif /* __EKG_DYNSTUFF_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
