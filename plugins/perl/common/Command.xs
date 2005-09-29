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
	command_exec(window_current->target, window_current->session, what, 0);

int command_bind(char *command, char *handler)
CODE:
        perl_command_bind(command, NULL, NULL, handler);
	
int command_bind_ext(char *command, char *params, char *poss, char *handler)
CODE:
        perl_command_bind(command, params, poss, handler);
		

#*******************************
MODULE = Ekg2::Command PACKAGE = Ekg2::Command PREFIX = command_
#*******************************

void
command_execute(Ekg2::Command comm, char *param)
CODE:
	char *tmp = saprintf("%s %s", comm->name, param);
        command_exec(window_current->target, window_current->session, comm->name, 0);
	xfree(tmp);

void command_remove(Ekg2::Command comm)
CODE:
	command_freeone(comm);