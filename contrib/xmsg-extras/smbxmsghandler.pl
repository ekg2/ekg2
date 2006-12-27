#!/usr/bin/perl
#
# smbxmsghandler.pl
# (C) 2006 Michał Górny <peres@peres.int.pl>

use lib "/home/peres/perl/xmsghandler/";
use SimpleXMSG qw/replyxmsg/;

if ($ARGV[0] =~ /^smbmsg-(.*)/) {
	`cat "$ARGV[1]" | smbclient -M "$1"`;
	replyxmsg('WinPopup request failed - probably second side is not connected')
		if ($?);

	exit 0;
}

exit 1;
