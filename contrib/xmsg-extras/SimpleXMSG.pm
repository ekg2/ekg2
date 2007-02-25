#!/usr/bin/perl

package SimpleXMSG;
use strict;
use warnings;
require Exporter;
our @ISA = qw/Exporter/;
our @EXPORT_OK = qw/replyxmsg sendxmsg/;
use File::Temp qw/tempfile/;

# XMSG incoming message dir
our $msgdir = '/var/xmsg';
# name separator
our $namesep = '.';

# Send message in reply, designed for xmsghandler.d
# Only arg is reply text, we get rcpt from ARGV[0]
sub replyxmsg
{
	my ($fh, $fn) = tempfile("$msgdir/.SimpleXMSG-$ARGV[0]$namesep" . 'XXXXXX');
	my $nfn = $fn;
	$nfn =~ s/^($msgdir\/)\.SimpleXMSG-/$1/s;

	print ($fh shift);
	close ($fh);
	rename ($fn, $nfn);
}

# Send XMSG message
# first arg is rcpt, second is message text
sub sendxmsg
{
	my ($from, $text) = (shift, shift);
	my ($fh, $fn) = tempfile("$msgdir/.SimpleXMSG-$from$namesep" . 'XXXXXX');
	my $nfn = $fn;
	$nfn =~ s/^($msgdir\/)\.SimpleXMSG-/$1/s;

	print ($fh $text);
	close ($fh);
	rename ($fn, $nfn);
}

# XXX: some object-oriented methods
