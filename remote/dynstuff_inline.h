#ifndef __EKG_DYNSTUFF_INLINE_H
#define __EKG_DYNSTUFF_INLINE_H

/* we could use typeof() instead of passing paramtype, but let's be more portable */

#include "dynstuff.h"

#define __DYNSTUFF_LIST_ADD(lista, typ, __notused)		\
	void lista##_add(typ *new) { list_add3((list_t *) (void *) &lista, (list_t) new); }

#define __DYNSTUFF_LIST_ADD_BEGINNING(lista, typ, __notused)	\
	void lista##_add(typ *new) { list_add_beginning3((list_t *) (void *) &lista, (list_t) new); }

#define __DYNSTUFF_LIST_ADD_SORTED(lista, typ, comparision)	\
	void lista##_add(typ *new) { list_add_sorted3((list_t *) (void *) &lista, (list_t) new, (void *) comparision); }

#define __DYNSTUFF_LIST_REMOVE_SAFE(lista, typ, free_func)	\
	void lista##_remove(typ *elem) { list_remove3((list_t *) (void *) &lista, (list_t) elem, (void *) free_func); }

#define __DYNSTUFF_LIST_REMOVE_ITER(lista, typ, free_func)	\
	typ *lista##_removei(typ *elem) { return list_remove3i((list_t *) (void *) &lista, (list_t) elem, (void *) free_func); }

#define __DYNSTUFF_LIST_DESTROY(lista, typ, free_func)		\
	void lista##_destroy(void) { list_destroy3((list_t) lista, (void *) free_func);	lista = NULL; }

/* !!! for other lists !!! [when we (have many || don't know) head of list during compilation time] */

#define __DYNSTUFF_ADD(prefix, typ, __notused)		\
	void prefix##_add(typ **lista, typ *new) { list_add3((list_t *) lista, (list_t) new); }

#define __DYNSTUFF_ADD_BEGINNING(prefix, typ, __notused) \
	void prefix##_add(typ **lista, typ *new) { list_add_beginning3((list_t *) lista, (list_t) new); }

#define __DYNSTUFF_ADD_SORTED(prefix, typ, comparision) \
	void prefix##_add(typ **lista, typ *new) { list_add_sorted3((list_t *) lista, (list_t) new, (void *) comparision); }

#define __DYNSTUFF_REMOVE_SAFE(prefix, typ, free_func)					\
	void prefix##_remove(typ **lista, typ *elem) {					\
		list_remove3((list_t *) lista, (list_t) elem, (void *) free_func);	\
	}

#define __DYNSTUFF_DESTROY(prefix, typ, free_func)			\
	void prefix##_destroy(typ **lista) {				\
		list_destroy3((list_t) *lista, (void *) free_func);	\
		*lista = NULL;						\
	}

#define __DYNSTUFF_NOREMOVE(lista, typ, free_func)
#define __DYNSTUFF_NOUNLINK(lista, typ)
#define __DYNSTUFF_NOCOUNT(lista, typ)
#define __DYNSTUFF_NODESTROY(lista, typ, free_func)

#define DYNSTUFF_LIST_DECLARE_FULL(lista, type, compare_func, free_func, list_add, list_remove, list_remove2, list_unlink, list_destroy, list_count)	\
		list_add(lista, type, compare_func)	\
		list_remove(lista, type, free_func)	\
		list_remove2(lista, type, free_func)	\
		list_unlink(lista, type)		\
		list_destroy(lista, type, free_func)	\
		list_count(lista, type)

#define DYNSTUFF_LIST_DECLARE(lista, type, free_func, list_add, list_remove, list_destroy)	\
		DYNSTUFF_LIST_DECLARE_WC(lista, type, free_func, list_add, list_remove, list_destroy, __DYNSTUFF_NOCOUNT)

#define DYNSTUFF_LIST_DECLARE_NF(lista, type, list_add, list_unlink) \
		DYNSTUFF_LIST_DECLARE_FULL(lista, type, NULL, NULL, list_add, __DYNSTUFF_NOREMOVE, __DYNSTUFF_NOREMOVE, list_unlink, __DYNSTUFF_NODESTROY, __DYNSTUFF_NOCOUNT)

#define DYNSTUFF_LIST_DECLARE_WC(lista, type, free_func, list_add, list_remove, list_destroy, list_count) \
		DYNSTUFF_LIST_DECLARE_FULL(lista, type, NULL, free_func, list_add, list_remove, __DYNSTUFF_NOREMOVE, __DYNSTUFF_NOUNLINK, list_destroy, list_count)

#define DYNSTUFF_LIST_DECLARE_SORTED(lista, type, compare_func, free_func, list_add, list_remove, list_destroy)	\
		DYNSTUFF_LIST_DECLARE_FULL(lista, type, compare_func, free_func, list_add, list_remove, __DYNSTUFF_NOREMOVE, __DYNSTUFF_NOUNLINK, list_destroy, __DYNSTUFF_NOCOUNT)


#define DYNSTUFF_LIST_DECLARE2(lista, type, free_func, list_add, list_remove, list_remove2, list_destroy)	\
		DYNSTUFF_LIST_DECLARE_FULL(lista, type, NULL, free_func, list_add, list_remove, list_remove2, __DYNSTUFF_NOUNLINK, list_destroy, __DYNSTUFF_NOCOUNT)

#define DYNSTUFF_LIST_DECLARE2_SORTED(lista, type, compare_func, free_func, list_add, list_remove, list_remove2, list_destroy)	\
		DYNSTUFF_LIST_DECLARE_FULL(lista, type, compare_func, free_func, list_add, list_remove, list_remove2, __DYNSTUFF_NOUNLINK, list_destroy, __DYNSTUFF_NOCOUNT)

#define DYNSTUFF_LIST_DECLARE_SORTED_NF(lista, type, compare_func, list_add, list_unlink) \
		DYNSTUFF_LIST_DECLARE_FULL(lista, type, compare_func, NULL, list_add, __DYNSTUFF_NOREMOVE, __DYNSTUFF_NOREMOVE, list_unlink, __DYNSTUFF_NODESTROY, __DYNSTUFF_NOCOUNT)


#endif
