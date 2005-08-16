#include "module.h"
MODULE = Ekg2::Irc::Channel PACKAGE = Ekg2::Irc
PROTOTYPES: ENABLE


MODULE = Ekg2::Irc::Channel   PACKAGE = Ekg2::Irc::Channel  PREFIX = channel_

void channel_part(Ekg2::Irc::Channel chan, char *reason)
CODE:
	debug("PART: %s %s\n", chan->name, reason);

