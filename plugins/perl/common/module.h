#define NEED_PERL_H
#define HAVE_CONFIG_H

#undef VERSION

#include "../perl_ekg.h"
#include <XSUB.h>

#undef _

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

typedef list_t           Ekg2__Userlist;
typedef script_timer_t  *Ekg2__STIMER;

typedef session_param_t *Ekg2__Session__Param;