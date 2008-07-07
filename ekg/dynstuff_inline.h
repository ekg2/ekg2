#ifndef __EKG_DYNSTUFF_INLINE_H
#define __EKG_DYNSTUFF_INLINE_H

/* we could use typeof() instead of passing paramtype, but let's be more portable */

#define DYNSTUFF_USE_LIST3 1

#if DYNSTUFF_USE_LIST3

#include <ekg/dynstuff.h>

#define __DYNSTUFF_LIST_ADD_SORTED(lista, typ, comparision) \
	void lista##_add(typ *new) { list_add_sorted3((list_t *) &lista, (list_t) new, (void *) comparision); }

#define __DYNSTUFF_LIST_REMOVE_SAFE(lista, typ, free_func)				\
	void lista##_remove(typ *elem) { 						\
		list_remove3((list_t *) &lista, (list_t) elem, (void *) free_func);	\
	}

#define __DYNSTUFF_LIST_REMOVE_ITER(lista, typ, free_func)					\
	typ *lista##_removei(typ *elem) { 							\
		return list_remove3i((list_t *) &lista, (list_t) elem, (void *) free_func);	\
	}

#define __DYNSTUFF_LIST_DESTROY(lista, typ, free_func)					\
	void lista##_destroy(void) { 							\
		list_destroy3((list_t) lista, (void *) free_func);			\
		lista = NULL;								\
	}

#else

#define __DYNSTUFF_LIST_ADD(lista, typ) 			\
	void lista##_add(typ *new) {				\
		new->next = NULL;				\
		if (!lista) {					\
			lista = new;				\
		} else {					\
			typ *tmp = lista;			\
								\
			while (tmp->next)			\
				tmp = tmp->next;		\
			tmp->next = new;			\
		}						\
}

#define __DYNSTUFF_LIST_ADD_BEGINNING(lista, typ) 		\
	void lista##_add(typ *new) {				\
		new->next = lista;				\
		lista  = new;					\
	}

#define __DYNSTUFF_LIST_ADD_SORTED(lista, typ, comparision) 	\
	void lista##_add(typ *new) {				\
		new->next = NULL;				\
		if (!lista) {					\
			lista = new;				\
		} else {					\
			typ *tmp = lista;			\
			typ *prev = NULL;			\
								\
			while (tmp->next)			\
				tmp = tmp->next;		\
			tmp->next = new;			\
								\
			while (comparision(new, tmp) > 0) {	\
				prev = tmp;			\
				tmp = tmp->next;		\
				if (!tmp)			\
					break;			\
			}					\
								\
			if (!prev) {				\
				new->next = lista;		\
				lista = new;			\
			} else {				\
				prev->next = new;		\
				new->next = tmp;		\
			}					\
		}						\
	}

#define __DYNSTUFF_LIST_REMOVE_SAFE(lista, typ, free_func)	\
	void lista##_remove(typ *elem) {			\
		if (!lista)	/* programmer's fault */	\
			return;					\
								\
		if (lista == elem) 				\
			lista = lista->next;			\
		else {						\
			typ *tmp, *last;			\
								\
			for (tmp = lista; tmp; tmp = tmp->next){\
				if (tmp == elem)		\
					break;			\
				if (tmp->next == NULL) {	\
					errno = ENOENT;		\
					return;			\
				}				\
				last = tmp;			\
			}					\
			last->next = tmp->next;			\
		}						\
		free_func(elem);				\
	}

#define __DYNSTUFF_LIST_REMOVE_ITER(lista, typ, free_func)	\
	typ *lista##_removei(typ *elem) {			\
		typ *ret;					\
								\
		if (lista == elem) { 				\
			ret = lista = lista->next;		\
		} else {					\
			typ *tmp, *last;			\
								\
			for (tmp = lista; tmp; tmp = tmp->next){\
				if (tmp == elem)		\
					break;			\
				last = tmp;			\
			}					\
			last->next = tmp->next;			\
			ret = last;				\
		}						\
		free_func(elem);				\
		return ret;					\
	}


#define __DYNSTUFF_LIST_DESTROY(lista, typ, free_func)		\
	void lista##_destroy(void) {				\
		while (lista) {					\
			typ *tmp = lista;			\
								\
			lista = lista->next;			\
								\
			if (free_func)				\
				free_func(tmp);			\
								\
			xfree(tmp);				\
		}						\
	}

#endif	/* !DYNSTUFF_USE_LIST3 */

#endif
