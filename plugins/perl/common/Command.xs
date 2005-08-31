#include "module.h"

MODULE = Ekg2::Command PACKAGE = Ekg2
PROTOTYPES: ENABLE

void commands()
PREINIT:
        list_t l;
PPCODE:
        for (l = commands; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_command( (command_t *) l->data)));
        }

int command(char *what)
CODE:
	command_exec(NULL, NULL, what, 0);


#*******************************
MODULE = Ekg2::Command PACKAGE = Ekg2::Command PREFIX = command_
#*******************************

void
command_execute(Ekg2::Command comm, char *param)
CODE:
	char *tmp = saprintf("%s %s", comm->name, param);
        command_exec(NULL, NULL, comm->name, 0);
	xfree(tmp);
