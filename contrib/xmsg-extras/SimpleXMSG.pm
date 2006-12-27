#!/usr/bin/perl
#
# SimpleXMSG.pm
# (C) 2006 Michał Górny <peres@peres.int.pl>

package SimpleXMSG;
require Exporter;
@ISA = qw/Exporter/;
@EXPORT_OK = qw/replyxmsg sendxmsg/;
use File::Temp qw/tempfile/;

# XMSG incoming message dir
our $msgdir = '/var/xmsg/';

# Send message in reply, designed for xmsghandler.d
# Only arg is reply text, we get rcpt from ARGV[0]
sub replyxmsg
{
	($fh, $fn) = tempfile("/tmp/$ARGV[0]:XXXXXX");
	print $fh shift;
	close $fh;
	`mv "$fn" "$msgdir"`
}

# Send XMSG message
# first arg is rcpt, second is message text
sub sendxmsg
{
	my ($from, $text) = (shift, shift);
	my ($fh, $fn) = tempfile("/tmp/$from:XXXXXX");

	print ($fh $text);
	close ($fh);
	`mv "$fn" "$msgdir"`
}

# XXX: some object-oriented methods
