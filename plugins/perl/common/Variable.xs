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

#*******************************
MODULE = Ekg2::Variable	PACKAGE = Ekg2::Variable  PREFIX = variable_
#*******************************

void variable_help(Ekg2::Variable var)
CODE:
	variable_help(var->name);

int variable_set(Ekg2::Variable var, const char *value)
CODE:
	variable_set(var->name, value, 0);

int variable_new()
CODE:
	debug("[VARIABLE.XS] variable_new() TODO\n");
