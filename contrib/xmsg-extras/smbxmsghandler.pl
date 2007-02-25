#!/usr/bin/perl

use strict;
use warnings;
use FindBin;
use lib "$FindBin::RealBin";
use SimpleXMSG qw/replyxmsg/;

if ($ARGV[0] =~ /^smbmsg-(.*)/) {
	`cat "$ARGV[1]" | smbclient -M "$1"`;
	replyxmsg('WinPopup request failed - probably second side is not connected')
		if ($?);

	exit 0;
}

exit 1;
