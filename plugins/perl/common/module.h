#define NEED_PERL_H
#define HAVE_CONFIG_H

#undef VERSION

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <ekg/scripts.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>
#include <ekg/vars.h>

#undef _
#include "../perl_ekg.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include "../perl_bless.h"

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

typedef userlist_t	*Ekg2__User;

typedef struct list	*Ekg2__Userlist;

typedef session_param_t *Ekg2__Session__Param;
typedef script_t	*Ekg2__Script;

script_var_t *perl_variable_add(char *var, char *value, char *handler);
void *perl_watch_add(int fd, int type, void *handler, void *data);
void *perl_handler_bind(char *query_name, char *handler);
void *perl_command_bind(char *command, char *params, char *poss, char *handler);
void *perl_plugin_register(char *name, int type, void *formatinit);
script_timer_t *perl_timer_bind(int freq, char *handler);
int perl_timer_unbind(script_timer_t *stimer);
void ekg2_callXS(void (*subaddr)(pTHX_ CV* cv), CV *cv, SV **mark);

void *Ekg2_ref_object(SV *o);
