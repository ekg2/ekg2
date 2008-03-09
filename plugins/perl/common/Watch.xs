#include "module.h"

MODULE = Ekg2::Watch  PACKAGE = Ekg2
PROTOTYPES: ENABLE

void watches()
PREINIT:
        watch_t *w;
PPCODE:
        for (w = watches; w; w = w->next) {
		XPUSHs(sv_2mortal(bless_watch( w )));
        }

