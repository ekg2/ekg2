#include "module.h"

MODULE = Ekg2::Command PACKAGE = Ekg2
PROTOTYPES: ENABLE

void commands()
PREINIT:
        command_t *c;
PPCODE:
        for (c = commands; c; c = c->next) {
                XPUSHs(sv_2mortal(bless_command( c)));
        }

int command(char *what)
CODE:
	RETVAL = command_exec(window_current->target, window_current->session, what, 0);
OUTPUT:
	RETVAL

int command_exec(Ekg2::Window window, Ekg2::Session session, char *what)
CODE:
	RETVAL = command_exec(window ? window->target : NULL, session, what, 0);
OUTPUT:
	RETVAL


int command_bind(char *command, char *handler)
CODE:
        perl_command_bind(command, NULL, NULL, handler);
	
int command_bind_ext(char *command, char *params, char *poss, char *handler)
CODE:
        perl_command_bind(command, params, poss, handler);
		

#*******************************
MODULE = Ekg2::Command PACKAGE = Ekg2::Command PREFIX = command_
#*******************************

int command_execute(Ekg2::Command comm, char *param)
CODE:
	char *tmp = saprintf("%s %s", comm->name, param);
        RETVAL = command_exec(window_current->target, window_current->session, comm->name, 0);
	xfree(tmp);
OUTPUT:
	RETVAL

void command_remove(Ekg2::Command comm)
CODE:
	commands_remove(comm);
