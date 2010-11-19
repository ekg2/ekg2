#include "module.h"

MODULE = Ekg2::Window  PACKAGE = Ekg2
PROTOTYPES: ENABLE

Ekg2::Window window_findid(int id)
CODE:
	RETVAL = window_exist(id);
OUTPUT:
	RETVAL

Ekg2::Window window_find(const char *target)


# sesja ? 
Ekg2::Window window_new(const char *target, int new_id)
CODE:
	RETVAL = window_new(target, NULL, new_id);
OUTPUT:
	RETVAL


Ekg2::Window window_current()
CODE:
        RETVAL = window_current;
OUTPUT:
        RETVAL

void windows()
PREINIT:
        window_t *w;
PPCODE:
        for (w = windows; w; w = w->next) {
                XPUSHs(sv_2mortal(bless_window( w )));
        }

#####################
MODULE = Ekg2::Window	PACKAGE = Ekg2::Window  PREFIX = window_
#####################

Ekg2::Window window_next(Ekg2::Window wind)
CODE:
	if (!(RETVAL = window_exist(wind->id+1))) 
		RETVAL = window_exist(0);
OUTPUT:
	RETVAL

Ekg2::Window window_prev(Ekg2::Window wind)
CODE:
	if (!(RETVAL = window_exist(wind->id-1))) 
		RETVAL = window_exist(1);  /* fixme, to ma byc ostatnie okno */
OUTPUT:
	RETVAL

Ekg2::Userlist window_userlist(Ekg2::Window wind)
CODE:
	RETVAL = (wind->userlist);
OUTPUT:
	RETVAL

void window_print_format(Ekg2::Window wind, char *format, ...)
PREINIT:
	char *p[10];
	int i;
CODE:
	for (i=0; i<10; i++) {
		if (i+2>=items) {
			p[i] = "(missing arg)";
		} else {
			p[i] = (char *)SvPV_nolen(ST(i+2));
		}
	}
	print_info(window_target(wind), wind->session, format, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);

void window_print(Ekg2::Window wind, char *line)
CODE:
	print_info(window_target(wind), wind->session, "generic", line);

void window_switch(Ekg2::Window wind)
CODE:
	window_switch(wind->id);
	
void window_kill(Ekg2::Window wind)
CODE:
	window_kill(wind);

