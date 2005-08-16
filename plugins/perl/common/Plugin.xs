#include "module.h"

MODULE = Ekg2::Plugin  PACKAGE = Ekg2
PROTOTYPES: ENABLE

Ekg2::Plugin plugin_find(const char *name)

void plugins()
PREINIT:
        list_t l;
PPCODE:
        for (l = plugins; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_plugin( (plugin_t *) l->data)));
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

