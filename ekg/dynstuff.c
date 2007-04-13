/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "dynstuff.h"
#include "xmalloc.h"

/*
 * list_add_sorted()
 *
 * dodaje do listy dany element. przy okazji mo¿e te¿ skopiowaæ zawarto¶æ.
 * je¶li poda siê jako ostatni parametr funkcjê porównuj±c± zawarto¶æ
 * elementów, mo¿e posortowaæ od razu.
 *
 *  - list - wska¼nik do listy,
 *  - data - wska¼nik do elementu,
 *  - alloc_size - rozmiar elementu, je¶li chcemy go skopiowaæ.
 *
 * zwraca wska¼nik zaalokowanego elementu lub NULL w przpadku b³êdu.
 */
void *list_add_sorted(list_t *list, void *data, int alloc_size, int (*comparision)(void *, void *))
{
	list_t new, tmp;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new = xmalloc(sizeof(struct list));

	new->data = data;
	new->next = NULL;
	/*new->prev = NULL;*/

	if (alloc_size) {
		new->data = xmalloc(alloc_size);
		memcpy(new->data, data, alloc_size);
	}

	if (!(tmp = *list)) {
		*list = new;
	} else {
		if (!comparision) {
			while (tmp->next)
				tmp = tmp->next;
			tmp->next = new;
		} else {
			list_t prev = NULL;
			
			while (comparision(new->data, tmp->data) > 0) {
				prev = tmp;
				tmp = tmp->next;
				if (!tmp)
					break;
			}
			
			if (!prev) {
				tmp = *list;
				*list = new;
				new->next = tmp;
			} else {
				prev->next = new;
				new->next = tmp;
			}
		}
	}

	return new->data;
}

/**
 * list_add_beginning()
 *
 * Add item @a data to the begining of the @a list<br>
 * (Once again), item will be added at begining of the list - as <b>first</b> item<br>
 *
 * @sa list_add()
 * @sa list_remove()
 */

void *list_add_beginning(list_t *list, void *data, int alloc_size) {

	list_t new;

	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new = xmalloc(sizeof(struct list));
	new->next = *list;
	*list	  = new;

	if (alloc_size) {	/* xmemdup() ? */
		new->data = xmalloc(alloc_size);
		memcpy(new->data, data, alloc_size);
	} else	new->data = data;

	return new->data;

}

/**
 * list_add()
 *
 * Add item @a data to @a list without sorting<br>
 * Item will be added at end of list - as <b>last</b> item<br>
 * Wrapper to: <code>list_add_sorted(list, data, alloc_size, NULL)</code>
 *
 * @sa list_remove()
 * @sa list_add_beginning() - If you can have items of list in reverse sequence [third_added_item, second_added_item, first_added_item] and not sorted
 * @sa list_add_sorted()
 */

void *list_add(list_t *list, void *data, int alloc_size)
{
	return list_add_sorted(list, data, alloc_size, NULL);
}

/**
 * list_remove_safe()
 *
 * Remove item @a data from list_t pointed by @a list.<br>
 * <b>Don't</b> free whole list_t item struct. only set item_list_t->data to NULL<br>
 *
 * @note XXX, add note here why we should do it.
 *
 * @param list - pointer to list_t
 * @param data - data to remove from @a list
 * @param free_data -if set and item was found it'll call xfree() on it.
 *
 * @sa list_cleanup() - to remove NULL items from list.
 */

int list_remove_safe(list_t *list, void *data, int free_data) {
	list_t tmp;

	if (!list) {
		errno = EFAULT;
		return -1;
	}

	for (tmp = *list; tmp; tmp = tmp->next) {
		if (tmp->data == data) {
			if (free_data)
				xfree(tmp->data);
			tmp->data = NULL;
			return 0;
		}
	}

	errno = ENOENT;
	return -1;
}

/**
 * list_cleanup()
 *
 * Remove from list_t all items with l->data set to NULL.<br>
 * Use with list_remove_safe() after list is not in use.
 */

void list_cleanup(list_t *list) {
	list_t tmp;
	list_t last = NULL;

	if (!list)
		return;

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

/**
 * list_remove()
 *
 * Remove item @a data from list_t pointed by @a list
 *
 * @param list - pointer to list_t
 * @param data - data to remove from @a list
 * @param free_data - if set and item was found it'll call xfree() on it.
 * @sa list_destroy() - to remove whole list
 *
 * @return 	 0 if item was founded, and was removed from list_t pointed by @a list<br>
 * 		-1 and errno set to EFAULT, if @a list was NULL<br>
 * 		-1 and errno set to ENOENT, if item was not found
 */

int list_remove(list_t *list, void *data, int free_data)
{
	list_t tmp, last = NULL;

	if (!list) {
		errno = EFAULT;
		return -1;
	}

	tmp = *list;
	if (tmp && tmp->data == data) {
		*list = tmp->next;
	} else {
		for (; tmp && tmp->data != data; tmp = tmp->next)
			last = tmp;
		if (!tmp) {
			errno = ENOENT;
			return -1;
		}
		last->next = tmp->next;
	}

	if (free_data)
		xfree(tmp->data);
	xfree(tmp);

	return 0;
}

/**
 * list_count()
 *
 * @param list - list_t
 *
 * @return items count on list_t @a list.
 */

int list_count(list_t list)
{
	int count = 0;

	for (; list; list = list->next)
		count++;

	return count;
}

/**
 * list_destroy()
 *
 * Destroy all items from list_t @a list
 *
 * @note It doesn't take pointer to list_t, and it don't cleanup memory with \\0, so after list_destroy() you must remember that
 * 	@a list is <b>unaccessible</b>. So if you have list as global variable, or if you keep pointer to it in some struct.. you must NULL pointer.
 * 	so always do: <br>
 * 	<code>
 * 		list_destroy(my_list, free_data);
 * 		my_list = NULL;
 * 	</code>
 *
 * @param list - list_t
 * @param free_data - if set we will call xfree() on each item data
 * @sa list_remove() - to remove specified item.
 *
 * @return 0
 */

int list_destroy(list_t list, int free_data)
{
	list_t tmp;
	
	while (list) {
		if (free_data)
			xfree(list->data);

		tmp = list->next;

		xfree(list);

		list = tmp;
	}

	return 0;
}

/*
 * string_realloc()
 *
 * upewnia siê, ¿e w stringu bêdzie wystarczaj±co du¿o miejsca.
 *
 *  - s - ci±g znaków,
 *  - count - wymagana ilo¶æ znaków (bez koñcowego '\0').
 */
static void string_realloc(string_t s, int count)
{
	char *tmp;
	
	if (s->str && (count + 1) <= s->size)
		return;
	
	tmp = xrealloc(s->str, count + 81);
	if (!s->str)
		*tmp = 0;
	tmp[count + 80] = 0;
	s->size = count + 81;
	s->str = tmp;
}

/**
 * string_append_c()
 *
 * Append to string_t @a s char @a c.
 *
 * @param s - string_t
 * @param c - char to append
 *
 * @return	 0 on success<br>
 *		-1 and errno set to EFAULT if input params were wrong (s == NULL || format == NULL)
 */

int string_append_c(string_t s, char c)
{
	if (!s) {
		errno = EFAULT;
		return -1;
	}
	
	string_realloc(s, s->len + 1);

	s->str[s->len + 1] = 0;
	s->str[s->len++] = c;

	return 0;
}

/**
 * string_append_n()
 *
 * Append to string_t @a s, first @a count chars, from @a str<br>
 *
 * @param s     - string_t
 * @param str   - buffer to append.
 * @param count - how many chars copy copy from @a str, or -1 to copy whole.
 *
 * @todo 	We append here NUL terminated string, so maybe let's <b>always</b> do <code>count = xstrnlen(str, count);</code>?<br>
 * 		Because now programmer can pass negative value, and it'll possible do SIGSEGV<br>
 * 		Also we can allocate less memory for string, when for example str[count-3] was NUL char.<br>
 *
 * @return	 0 on success<br>
 *		-1 and errno set to EFAULT if input params were wrong (s == NULL || str == NULL)
 */

int string_append_n(string_t s, const char *str, int count)
{
	if (!s || !str) {
		errno = EFAULT;
		return -1;
	}

	if (count == -1)
		count = xstrlen(str);

	string_realloc(s, s->len + count);

	s->str[s->len + count] = 0;
	xstrncpy(s->str + s->len, str, count);

	s->len += count;

	return 0;
}

/**
 * string_append_format()
 *
 * Append to string_t @a s, formatted output of @a format + params<br>
 * Equivalent to:<br>
 * 	<code>
 * 		char *tmp = saprintf(format, ...);<br>
 * 		string_append(s, tmp);<br>
 * 		xfree(tmp);<br>
 * 	</code>
 *
 * @note For more details about string formating functions read man 3 vsnprintf
 *
 * @sa string_append()	- If you want/can use non-formating function..
 * @sa saprintf()	- If you want to format output but to normal char *, not to string_t
 *
 * @return 	 0 on success<br>
 * 		-1 and errno set to EFAULT if input params were wrong (s == NULL || format == NULL)
 */

int string_append_format(string_t s, const char *format, ...) {
	va_list ap;
	char *formatted;

	if (!s || !format) {
		errno = EFAULT;
		return -1;
	}

	va_start(ap, format);
	formatted = vsaprintf(format, ap);
	va_end(ap);
	
	if (!formatted) 
		return 0;
	
	string_append_n(s, formatted, -1);
	xfree(formatted);
	return 0;
}

/**
 * string_append_raw()
 *
 * Append to string_t @a s, @a count bytes from memory pointed by @a str<br>
 * 
 * @sa string_append_n() - If you want to append NUL terminated (C-like) String
 * @todo XXX Protect from negative count (and less than -1) ?
 */

int string_append_raw(string_t s, const char *str, int count) {
	if (!s || !str) {
		errno = EFAULT;
		return -1;
	}

	if (count == -1) return string_append_n(s, str, -1);

	string_realloc(s, s->len + count);

	s->str[s->len + count] = 0;
	memcpy(s->str + s->len, str, count);

	s->len += count;

	return 0;
}

/**
 * string_append()
 *
 * Append to string_t @a s, NUL terminated string pointed by str<br>
 * Wrapper to:<code>string_append_n(s, str, -1)</code>
 *
 * @sa string_append_n()
 */

int string_append(string_t s, const char *str)
{
	return string_append_n(s, str, -1);
}

/*
 * string_insert_n()
 *
 * wstawia tekst w podane miejsce bufora.
 *  
 *  - s - ci±g znaków,
 *  - index - miejsce, gdzie mamy wpisaæ (liczone od 0),
 *  - str - tekst do dopisania,
 *  - count - ilo¶æ znaków do dopisania (-1 znaczy, ¿e wszystkie).
 */
void string_insert_n(string_t s, int index, const char *str, int count)
{
	if (!s || !str)
		return;

	if (count == -1)
		count = xstrlen(str);

	if (index > s->len)
		index = s->len;
	
	string_realloc(s, s->len + count);

	memmove(s->str + index + count, s->str + index, s->len + 1 - index);
	memmove(s->str + index, str, count);

	s->len += count;
}

/**
 * string_insert()
 *
 * Insert given text (@a str) to given string_t (@a s) at given pos (@a index)<br>
 * Wrapper to: <code>string_insert_t(s, index, str, -1)</code>
 *
 * @param s     - string_t
 * @param index - pos
 * @param str   - text 
 *
 * @sa string_insert_n()
 */

void string_insert(string_t s, int index, const char *str)
{
	string_insert_n(s, index, str, -1);
}

/**
 * string_init()
 *
 * init string_t struct, allocating memory for string passed by @a value, and setting internal string_t data.
 *
 *  @param value - if NULL char buffer will be inited with "", otherwise with given @a value.
 *  @sa string_free() - to free memory used by string_t
 *
 *  @return pointer to allocated string_t struct.
 */
string_t string_init(const char *value) {
	string_t tmp = xmalloc(sizeof(struct string));
	size_t valuelen;

	if (!value)
		value = "";

	valuelen = xstrlen(value);

	tmp->str = xstrdup(value);
	tmp->len = valuelen;
	tmp->size = valuelen + 1;

	return tmp;
}

/**
 * string_clear()
 *
 * Clear s->str (s->str[0] == '\\0')<br>
 * If memory allocated by string_t @a s was larger than 160, than decrease to 80
 *
 *  @param s - string_t
 *  @sa string_free() - To free memory allocated by string_t
 */

void string_clear(string_t s)
{
	if (!s)
		return;
	if (s->size > 160) {
		s->str = xrealloc(s->str, 80);
		s->size = 80;
	}

	s->str[0] = 0;
	s->len = 0;
}

/**
 * string_free()
 *
 * Cleanup memory after string_t @a s, and eventually (if @a free_string set) cleanup memory after char buffer.
 *
 * @param s		- string_t which we want to free.
 * @param free_string	- do we want to free memory after char buffer?
 * @sa string_clear()	- if you just want to clear saved char buffer, and you don't want to free internal string_t struct.
 *
 * @return 	if @a free_string != 0 always NULL<br>
 * 		else returns saved char buffer, which need be free()'d after use by xfree()
 */

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

/*
 * itoa()
 *
 * prosta funkcja, która zwraca tekstow± reprezentacjê liczby. w obrêbie
 * danego wywo³ania jakiej¶ funkcji lub wyra¿enia mo¿e byæ wywo³ania 10
 * razy, poniewa¿ tyle mamy statycznych buforów. lepsze to ni¿ ci±g³e
 * tworzenie tymczasowych buforów na stosie i sprintf()owanie.
 *
 *  - i - liczba do zamiany.
 *
 * zwraca adres do bufora, którego _NIE_NALE¯Y_ zwalniaæ.
 */
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

/*
 * array_make()
 *
 * tworzy tablicê tekstów z jednego, rozdzielonego podanymi znakami.
 *
 *  - string - tekst wej¶ciowy,
 *  - sep - lista elementów oddzielaj±cych,
 *  - max - maksymalna ilo¶æ elementów tablicy. je¶li równe 0, nie ma
 *          ograniczeñ rozmiaru tablicy.
 *  - trim - czy wiêksz± ilo¶æ elementów oddzielaj±cych traktowaæ jako
 *           jeden (na przyk³ad spacje, tabulacja itp.)
 *  - quotes - czy pola mog± byæ zapisywane w cudzys³owiach lub
 *             apostrofach z escapowanymi znakami.
 *
 * zaalokowan± tablicê z zaalokowanymi ci±gami znaków, któr± nale¿y
 * zwolniæ funkcj± array_free()
 */
char **array_make(const char *string, const char *sep, int max, int trim, int quotes)
{
	const char *p, *q;
	char **result = NULL;
	int items = 0, last = 0;

	if (!string || !sep)
		goto failure;

	for (p = string; ; ) {
		int len = 0;
		char *token = NULL;

		if (max && items >= max - 1)
			last = 1;
		
		if (trim) {
			while (*p && xstrchr(sep, *p))
				p++;
			if (!*p)
				break;
		}

		if (!last && quotes && (*p == '\'' || *p == '\"')) {
			char sep = *p;

			for (q = p + 1, len = 0; *q; q++, len++) {
				if (*q == '\\') {
					q++;
					if (!*q)
						break;
				} else if (*q == sep)
					break;
			}

                        len++;

			if ((token = xcalloc(1, len + 1))) {
				char *r = token;
			
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
			}
			
			p = (*q) ? q + 1 : q;

		} else {
			for (q = p, len = 0; *q && (last || !xstrchr(sep, *q)); q++, len++);
			token = xcalloc(1, len + 1);
			xstrncpy(token, p, len);
			token[len] = 0;
			p = q;
		}
		
		result = xrealloc(result, (items + 2) * sizeof(char*));
		result[items] = token;
		result[++items] = NULL;

		if (!*p)
			break;

		p++;
	}

failure:
	if (!items)
		result = xcalloc(1, sizeof(char*));

	return result;
}

/*
 * array_count()
 *
 * zwraca ilo¶æ elementów tablicy.
 */
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

/* 
 * array_add()
 *
 * dodaje element do tablicy.
 */
void array_add(char ***array, char *string)
{
	int count = array_count(*array);

	*array = xrealloc(*array, (count + 2) * sizeof(char*));
	(*array)[count + 1] = NULL;
	(*array)[count] = string;
}

/*
 * array_add_check()
 * 
 * dodaje element do tablicy, uprzednio sprawdzaj±c
 * czy taki ju¿ w niej nie istnieje
 *
 *  - array - tablica,
 *  - string - szukany ci±g znaków,
 *  - casesensitive - czy mamy zwracaæ uwagê na wielko¶æ znaków?
 */ 
void array_add_check(char ***array, char *string, int casesensitive)
{
	if (!array_item_contains(*array, string, casesensitive))
		array_add(array, string);
	else
		xfree(string);
}

/*
 * array_join()
 *
 * ³±czy elementy tablicy w jeden string oddzielaj±c elementy odpowiednim
 * separatorem.
 *
 *  - array - wska¼nik do tablicy,
 *  - sep - seperator.
 *
 * zwrócony ci±g znaków nale¿y zwolniæ.
 */
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

char *array_join_count(char **array, const char *sep, int count) {
	string_t s = string_init(NULL);

	if (array) {
		int i;

		for (i = 0; i < count; i++) {
			if (array[i])
				string_append(s, array[i]);
			
			if (i != count-1)	
				string_append(s, sep);
		}
	}

	return string_free(s, 0);
}

/*
 * array_contains()
 *
 * stwierdza, czy tablica zawiera podany element.
 *
 *  - array - tablica,
 *  - string - szukany ci±g znaków,
 *  - casesensitive - czy mamy zwracaæ uwagê na wielko¶æ znaków?
 *
 * 0/1
 */
int array_contains(char **array, const char *string, int casesensitive)
{
	int i;

	if (!array || !string)
		return 0;

	for (i = 0; array[i]; i++) {
		if (casesensitive && !xstrcmp(array[i], string))
			return 1;
		if (!casesensitive && !xstrcasecmp(array[i], string))
			return 1;
	}

	return 0;
}

/*
 * array_item_contains()
 *
 * stwierdza czy w tablicy znajduje siê element zawieraj±cy podany ci±g
 *
 *  - array - tablica,
 *  - string - szukany ci±g znaków,
 *  - casesensitive - czy mamy zwracaæ uwagê na wielko¶æ znaków?
 *
 * 0/1
 */
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

/*
 * array_free()
 *
 * zwalnia pamieæ zajmowan± przez tablicê.
 */
void array_free(char **array)
{
	char **tmp;

	if (!array)
		return;

	for (tmp = array; *tmp; tmp++)
		xfree(*tmp);

	xfree(array);
}

void array_free_count(char **array, int count) {
	char **tmp;

	if (!array)
		return;

	for (tmp = array; count; tmp++, count--)
		xfree(*tmp);

	xfree(array);
}

/**
 * cssfind()
 *
 * Short for comma-separated string find, does check whether given string contains given element.
 * It's works like array_make()+array_contains(), but it's hell simpler and faster.
 *
 * @param haystack		- comma-separated string to search.
 * @param needle		- searched element.
 * @param sep			- separator.
 * @param caseinsensitive	- take a wild guess.
 *
 * @return Pointer to found element on success, or NULL on failure.
 */
const char *cssfind(const char *haystack, const char *needle, const char sep, int caseinsensitive) {
	const char *r = haystack-1;
	const int needlelen = xstrlen(needle);

	while ((r = (caseinsensitive ? xstrcasestr(r+1, needle) : xstrstr(r+1, needle))) &&
			(((r != haystack) && ((*(r-1) != sep)))
			|| ((*(r+needlelen) != '\0') && (*(r+needlelen) != sep)))) {};

	return r;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
