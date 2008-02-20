#include "module.h"

MODULE = Ekg2::Watch  PACKAGE = Ekg2
PROTOTYPES: ENABLE

void watches()
PREINIT:
        list_t l;
PPCODE:
        for (l = watches; l; l = l->next) {
		XPUSHs(sv_2mortal(bless_watch( (watch_t *) l->data)));
        }

