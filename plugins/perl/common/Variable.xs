#include "module.h"

MODULE = Ekg2::Variable  PACKAGE = Ekg2
PROTOTYPES: ENABLE

Ekg2::Variable variable_find(const char *name)

void variables()
PREINIT:
        GSList *vl;
PPCODE:
        for (vl = variables; vl; vl = vl->next) {
                XPUSHs(sv_2mortal(bless_variable( (variable_t *) vl->data )));
        }

Ekg2::Variable variable_add_ext(char *name, char *value, char *handler)
CODE:
        RETVAL = perl_variable_add(name, VAR_STR, value, handler)->self;
OUTPUT:
	RETVAL
	
Ekg2::Variable variable_add(char *name, char *value)
CODE:
	RETVAL = perl_variable_add(name, VAR_STR, value, NULL)->self;
OUTPUT:
	RETVAL
		
Ekg2::Variable variable_add_bool_ext(char *name, char *value, char *handler)
CODE:
	RETVAL = perl_variable_add(name, VAR_BOOL, value, handler)->self;
OUTPUT:
	RETVAL

Ekg2::Variable variable_add_bool(char *name, char *value)
CODE:
	RETVAL = perl_variable_add(name, VAR_BOOL, value, NULL)->self;
OUTPUT:
	RETVAL

Ekg2::Variable variable_add_int_ext(char *name, char *value, char *handler)
CODE:
	RETVAL = perl_variable_add(name, VAR_INT, value, handler)->self;
OUTPUT:
	RETVAL

Ekg2::Variable variable_add_int(char *name, char *value)
CODE:
	RETVAL = perl_variable_add(name, VAR_INT, value, NULL)->self;
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
PREINIT:
	int ret;
CODE:
	ret = variable_set(var->name, value);
	if (ret == 0)
		config_changed = 1;
	RETVAL = ret;
OUTPUT:
	RETVAL

