#include "module.h"

MODULE = Ekg2::Window  PACKAGE = Ekg2
PROTOTYPES: ENABLE

##########################################################

Ekg2::Window window_find(const char *target)

Ekg2::Window window_current()
CODE:
        RETVAL = window_current;
OUTPUT:
        RETVAL

##########################################################

void windows()
PREINIT:
        list_t l;
PPCODE:
        for (l = windows; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_window( (window_t *) l->data)));
        }

#*******************************
MODULE = Ekg2::Window	PACKAGE = Ekg2::Window  PREFIX = window_
#*******************************

void window_next()

void window_print(Ekg2::Window wind, char *line)
CODE:
	print_window(wind->target, wind->session, 0, "generic", line);

########### window switching #############

void window_switch(Ekg2::Window wind)
CODE:
	window_switch(wind->id);
	
void window_switch_id(int id)
CODE:
	window_switch(id);

##########################################

void window_kill(Ekg2::Window wind)
CODE:
	window_kill(wind, 0);

########## window finding ##############

Ekg2::Window window_exists(int id)
