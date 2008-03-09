#include "module.h"

MODULE = Ekg2::Timer  PACKAGE = Ekg2
PROTOTYPES: ENABLE

void timers()
PREINIT:
        struct timer *t;
PPCODE:
        for (t = timers; t; t = t->next) {
                XPUSHs(sv_2mortal(bless_timer( t )));
        }

# Ekg2::Timer timer_find(const char *uid)

Ekg2::Timer timer_bind(int freq, char *handler)
PREINIT:
	script_timer_t *tmp;
CODE:
	if ((tmp = perl_timer_bind(freq, handler)))
		RETVAL = tmp->self;
	else	RETVAL = NULL;
OUTPUT:
	RETVAL
	
#*******************************
MODULE = Ekg2::Timer	PACKAGE = Ekg2::Timer  PREFIX = timer_
#*******************************

void timer_destroy(Ekg2::Timer timer)
CODE:
	timer_remove(timer->plugin, timer->name);


