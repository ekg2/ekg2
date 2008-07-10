#include "module.h"

MODULE = Ekg2::Watch  PACKAGE = Ekg2
PROTOTYPES: ENABLE

void watches()
PREINIT:
	list_t l;
PPCODE:
        for (l = watches; l; l = l->next) {
		watch_t *w = l->data;
		if (w)
			XPUSHs(sv_2mortal(bless_watch( w )));
        }

