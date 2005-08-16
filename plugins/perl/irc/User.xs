#include "module.h"

MODULE = Ekg2::Irc::User PACKAGE = Ekg2::Irc
PROTOTYPES: ENABLE

MODULE = Ekg2::Irc::User   PACKAGE = Ekg2::Irc::User  PREFIX = user_


void user_channels(Ekg2::Irc::User user)
PREINIT:
        list_t l;
PPCODE:
        for (l = user->channels ; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_channperson( (people_chan_t *) l->data)));
        }


void user_kill(Ekg2::Irc::User user, char *reason)
CODE:
	debug("KILL %s :%s\n", user->nick+4, reason);