#include "module.h"

MODULE = Ekg2::Timer  PACKAGE = Ekg2
PROTOTYPES: ENABLE

void timers()
PREINIT:
        list_t l;
PPCODE:
        for (l = timers; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_timer( (struct timer *) l->data)));
        }

# Ekg2::Timer timer_find(const char *uid)

#*******************************
MODULE = Ekg2::Timer	PACKAGE = Ekg2::Timer  PREFIX = timer_
#*******************************

int timer_delete(Ekg2::PTimer timer)
CODE:
	timer_remove(timer->plugin, timer->name);
