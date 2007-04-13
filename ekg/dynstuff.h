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
void *list_add_beginning(list_t *list, void *data, int alloc_size);
void *list_add_sorted(list_t *list, void *data, int alloc_size, int (*comparision)(void *, void *));
int list_remove(list_t *list, void *data, int free_data);
int list_count(list_t list);
int list_destroy(list_t list, int free_data);

void list_cleanup(list_t *list);
int list_remove_safe(list_t *list, void *data, int free_data);
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

typedef struct string *string_t;

#ifndef EKG2_WIN32_NOFUNCTION

string_t string_init(const char *str);
int string_append(string_t s, const char *str);
int string_append_n(string_t s, const char *str, int count);
int string_append_c(string_t s, char ch);
int string_append_raw(string_t s, const char *str, int count);
int string_append_format(string_t s, const char *format, ...);
void string_insert(string_t s, int index, const char *str);
void string_insert_n(string_t s, int index, const char *str, int count);
void string_clear(string_t s);
char *string_free(string_t s, int free_string);

/* tablice stringow */
char **array_make(const char *string, const char *sep, int max, int trim, int quotes);
char *array_join(char **array, const char *sep);
char *array_join_count(char **array, const char *sep, int count);

void array_add(char ***array, char *string);
void array_add_check(char ***array, char *string, int casesensitive);
int array_count(char **array);
int array_contains(char **array, const char *string, int casesensitive);
int array_item_contains(char **array, const char *string, int casesensitive);
void array_free(char **array);
void array_free_count(char **array, int count);

/* rozszerzenia libców */

const char *itoa(long int i);
const char *cssfind(const char *haystack, const char *needle, const char sep, int caseinsensitive);

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
