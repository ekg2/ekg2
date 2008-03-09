#include "module.h"

MODULE = Ekg2::Plugin  PACKAGE = Ekg2
PROTOTYPES: ENABLE

Ekg2::Plugin plugin_find(const char *name)

int plugin_register(char *name, int type, void *formatinit)
CODE:
        perl_plugin_register(name, type, formatinit);
	
void plugins()
PREINIT:
        plugin_t *p;
PPCODE:
        for (p = plugins; p; p = p->next) {
                XPUSHs(sv_2mortal(bless_plugin( p )));
        }

int plugin_load(const char *name)
CODE:
	RETVAL = plugin_load(name, -254, 1);
OUTPUT:
	RETVAL

#*******************************
MODULE = Ekg2::Plugin	PACKAGE = Ekg2::Plugin  PREFIX = plugin_
#*******************************

int plugin_unload(Ekg2::Plugin plugin)

