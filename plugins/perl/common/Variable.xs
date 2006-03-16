#include "module.h"

MODULE = Ekg2::Variable  PACKAGE = Ekg2
PROTOTYPES: ENABLE

Ekg2::Variable variable_find(const char *name)

void variables()
PREINIT:
        list_t l;
PPCODE:
        for (l = variables; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_variable( (variable_t *) l->data)));
        }

void variables_free()
CODE:
	variable_free();
	
Ekg2::Variable variable_add_ext(char *name, char *value, char *handler)
CODE:
        RETVAL = perl_variable_add(name, value, handler)->self;
OUTPUT:
	RETVAL
	
Ekg2::Variable variable_add(char *name, char *value)
CODE:
	RETVAL = perl_variable_add(name, value, NULL)->self;
OUTPUT:
	RETVAL
		

#*******************************
MODULE = Ekg2::Variable	PACKAGE = Ekg2::Variable  PREFIX = variable_
#*******************************

void variable_help(Ekg2::Variable var)
CODE:
	variable_help(var->name);

void variable_remove(Ekg2::Variable var)
CODE:
	variable_remove(var->plugin, var->name);

int variable_set(Ekg2::Variable var, const char *value)
CODE:
	variable_set(var->name, value, 0);

