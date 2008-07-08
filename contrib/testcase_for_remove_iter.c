/* short note (in Polish, sorry) 
 *
 * testcase majacy na celu sprawdzenie czy __DYNSTUFF_LIST_REMOVE_ITER() 
 * dziala poprawnie przy zwalnianiu pierwszych elementow listy.
 *
 * Powinno byc troche inaczej, w innym .o zeby kompilator nie mogl sobie tego zoptymalizowac.
 * ale dla -O0 ... -O3 dziala poprawnie,
 *
 * prawidlowy output:
 * 	f == 0x602070: 40, 30, 20, 10, 
 * 	f == 0x602050: 30, 20, 10, 
 * 	f == 0x602030: 20, 10, 
 *	f == 0x602010: 10, 
 * 	f == (nil): 
 *
 * jesli twoj kompilator dla pewnych wartosci -O lub nie korzystasz z gcc, lub korzystach z innych kosmicznych flag kompilatora
 * generuje nieprawidlowy kod, ktory generuje nieprawidlowy output (adresy moga byc rozne :>)
 * daj info.
 *
 * wiechu chcial aby to bylo testowane w czasie ./configure moze kiedys.
 * Na razie wrzucam do contrib/
 *
 * Ogolnie korzystamy z faktu, ze w C:
 *   - strukturka->pierwszy_element_strukturki == *(strukturka + 0) == *strukturka
 *   - *(&foo) == foo
 */

/* __DYNSTUFF_LIST_REMOVE_ITER() jest ogolnie b. fajne, i b. przydatne. */
/* thx goes to wiechu. */

#include <stdio.h>
#include <stdlib.h>

#define xfree free

#include <ekg/dynstuff_inline.h>

#ifdef DYNSTUFF_USE_LIST3	
#include <errno.h>

/* stuff copied from dynstuff.c */

void *list_add_beginning3(list_t *list, list_t new) {
	if (!list) {
		errno = EFAULT;
		return NULL;
	}

	new->next = *list;
	*list	  = new;

	return new;
}

void *list_remove3i(list_t *list, list_t elem, void (*func)(list_t data)) {
	list_t tmp, last = NULL;
	void *ret = NULL;

	if (!list) {
		errno = EFAULT;
		return ret;
	}

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

#endif

typedef struct foo {
	struct foo *next;
	int val;
} foo;

foo *foos;

void do_nothing(foo *f) { }

__DYNSTUFF_LIST_ADD_BEGINNING(foos, foo, NULL);
__DYNSTUFF_LIST_REMOVE_ITER(foos, foo, do_nothing);

foo *nowy_element(int val) {
	foo *tmp = malloc(sizeof(foo));

	tmp->val = val;

	foos_add(tmp);
	return tmp;
}

void print(foo *f) {
	printf("f == %p: ", f);
	for (; f; f = f->next)
		printf("%d, ", f->val);
	printf("\n");
}

int main() {
	foo *f;

	nowy_element(10);
	nowy_element(20);
	nowy_element(30);
	nowy_element(40);

	for (f = foos; f; f = f->next) {
		print(foos);

		f = foos_removei(f);
	}
	print(foos);
	return 0;
}

