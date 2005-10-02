#define NEED_PERL_H
#define HAVE_CONFIG_H

#undef VERSION

#include "../perl_ekg.h"
#include <XSUB.h>

#include "../perl_bless.h"
#include <ekg/dynstuff.h>

#define ekg2_boot(x) { \
        extern void boot_Ekg2__##x(pTHX_ CV *cv); \
        ekg2_callXS(boot_Ekg2__##x, cv, mark); \
        }
	
typedef session_t	*Ekg2__Session;
typedef variable_t	*Ekg2__Variable;
typedef command_t 	*Ekg2__Command;
typedef window_t	*Ekg2__Window;
typedef plugin_t	*Ekg2__Plugin;

typedef struct timer	*Ekg2__Timer;
typedef struct timer	*Ekg2__PTimer;

typedef userlist_t      *Ekg2__User;

typedef list_t          *Ekg2__Userlist;
typedef script_timer_t  *Ekg2__STIMER;

typedef session_param_t *Ekg2__Session__Param;
typedef script_t	*Ekg2__Script;

SV *ekg2_bless(int flag, int flag2, void *object);
void *Ekg2_ref_object(SV *o);
void *perl_variable_add(char *var, char *value, char *handler);
void *perl_watch_add(int fd, int type, int persist, void *handler, void *data);
void *perl_handler_bind(char *query_name, char *handler);
void *perl_command_bind(char *command, char *params, char *poss, char *handler);
void *perl_timer_bind(int freq, char *handler);
int perl_timer_unbind(script_timer_t *stimer);
void ekg2_callXS(void (*subaddr)(pTHX_ CV* cv), CV *cv, SV **mark);
char *ekg2_window_target(window_t *window);

