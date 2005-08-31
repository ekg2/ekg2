#
# Perl interface to irssi functions.
#

package Ekg2;

use strict;
use Carp;
use vars qw($VERSION $in_ekg2 @ISA @EXPORT @EXPORT_OK);

sub in_ekg2 { 
     return $in_ekg2;
}

$VERSION = "0.9";

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw( EKG_MSGCLASS_SENT EKG_MSGCLASS_SENT_CHAT
	      EKG_NO_THEMEBIT WATCH_READ
);

my $static = 0;

eval {
  $static = Ekg2::Core::is_static();
};
$in_ekg2 = $@ ? 0 : 1;

if (!in_ekg2()) {
     print "Warning: This script should be run inside Ekg2!!!!111\n";
     return 0;
} else {

  bootstrap Ekg2;
  Ekg2::init();
  
#  Ekg2::EXPORT_ALL();

   Ekg2::debug("Ekg2.pm loaded, enjoy!\n");
   return 1;
};
