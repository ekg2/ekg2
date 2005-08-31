#include "module.h"

static int initialized = 0;
#define VAR_PREFIX ""

MODULE = Ekg2  PACKAGE = Ekg2

PROTOTYPES: ENABLE

#> MAIN

void echo(char *str)
CODE:
	char *skrypt = SvPV(perl_eval_pv("caller", TRUE), PL_na)+14;
	print("script_generic", "perl", skrypt, str);

void debug(char *debstr)
CODE:
	debug("(perldebug) %s", debstr);

void print(int dest, char *str)
CODE:
	char *line;
        while ((line = split_line(&str))) {
                window_print("__status", NULL, 0, fstring_new(va_format_string(line)));
        }

void init()
CODE:
	initialized = 1;

void deinit()
CODE:

int watch_add(int fd, int type, int persist, char *handler, void *data);
CODE:
	perl_watch_add(fd, type, persist, handler, data);

int watch_remove(int fd, int type);
CODE:	
	watch_remove(&perl_plugin, fd, type);

#> TIMERS

int timer_bind(int freq, char *handler)
CODE:
	perl_timer_bind(freq, handler);

int timer_unbind(void * scr_time)
CODE:
	perl_timer_unbind(scr_time);

#> COMMANDS

int command_bind(char *command, char *handler)
CODE:
	perl_command_bind(command, handler);

#> QUERIES

int handler_bind(char *query_name, char *handler)
CODE:
	perl_handler_bind(query_name, handler);

#> VARIABLES

Ekg2::Variable var_add_handler(char *name, char *value, char *handler)
CODE:
	char *temp = saprintf("%s%s", VAR_PREFIX, name);
	perl_variable_add(temp, value, handler);
	xfree(temp);

Ekg2::Variable variable_add(char *name, char *value)
CODE:
	perl_variable_add(name, value, NULL);

Ekg2::Variable var_add(char *name, char *value)
CODE:
	char *temp = saprintf("%s%s", VAR_PREFIX,name);
	perl_variable_add(temp, value, NULL);
	xfree(temp);

Ekg2::Variable var_find(char *name)
CODE:
	char *temp = saprintf("%s%s", VAR_PREFIX,name);
//	RETVAL = (script_var_find(NULL, name)) ->var;
	RETVAL = variable_find(temp);
	xfree(temp);
OUTPUT:
	RETVAL

char *get_ekg2_dir()
CODE:
	RETVAL = config_dir;
OUTPUT:
	RETVAL

#> STALE! 

int EKG_MSGCLASS_SENT()
CODE:
	RETVAL = EKG_MSGCLASS_SENT;
OUTPUT:
	RETVAL

int EKG_MSGCLASS_SENT_CHAT()
CODE:
	RETVAL = EKG_MSGCLASS_SENT_CHAT;
OUTPUT:
	RETVAL

int EKG_NO_THEMEBIT()
CODE:
        RETVAL = EKG_NO_THEMEBIT;
OUTPUT:
        RETVAL

int WATCH_READ()
CODE:
	RETVAL = WATCH_READ;
OUTPUT:
	RETVAL

##################################################################################

BOOT:
	ekg2_boot(Session);
	ekg2_boot(Variable);
	ekg2_boot(Plugin);
	ekg2_boot(Window);
	ekg2_boot(Command);
	ekg2_boot(Timer);
	ekg2_boot(Userlist);
