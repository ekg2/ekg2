#
# Perl interface to irssi functions.
#

package Ekg2::Irc;

use Ekg2;
use strict;
use Carp;
use vars qw(@ISA @EXPORT @EXPORT_OK);

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw(  );

# heh jesli jest w autoloadzie to wtedy Ekg2::var_find("perl:autoload")->{value} zawsze ma
# domyslna wartosc po zaladowaniu plugina perl czyli 1...
# dupa!

if (!(Ekg2::plugin_find("irc"))) 
{
	Ekg2::debug("Plugin irc not founded (" . Ekg2::var_find("perl:autoload")->{value} . ") \n"); 
	if (Ekg2::var_find("perl:autoload")->{value} > 0) {
		if (!(Ekg2::plugin_load("irc"))) {
			return 0;
		}
	} else { return 0; }
	
};

bootstrap Ekg2::Irc;

# Ekg2::Irc::EXPORT_ALL();

Ekg2::debug("Irc.pm loaded, enjoy!\n");

return 1;
