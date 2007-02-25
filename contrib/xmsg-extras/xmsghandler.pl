#!/usr/bin/perl

use strict;
use warnings;
use FindBin;
use lib "$FindBin::RealBin";
use SimpleXMSG qw/replyxmsg/;

my $handlerddir = '~/xmsghandler.d';

exit 1 if (! ($ARGV[0] && $ARGV[1]));
exit 2 if (! -f $ARGV[1]);

my $found = 0;

foreach (<$handlerddir/*>) {
	`$_ $ARGV[0] $ARGV[1]`;
	$found = 1, last if (($?>>8) == 0);
}

replyxmsg('Message delivery failed - unknown recipient') if (!$found);

unlink $ARGV[1];

exit 0;

