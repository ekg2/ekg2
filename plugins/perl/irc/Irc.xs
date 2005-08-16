#include "module.h"

MODULE = Ekg2::Irc  PACKAGE = Ekg2::Irc

PROTOTYPES: ENABLE


###############################################################################################

BOOT:
	ekg2_boot(Irc__Server);
	ekg2_boot(Irc__Channel);
	ekg2_boot(Irc__User);