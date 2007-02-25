#!/usr/bin/perl
# to be called from xmsghandler.pl
# symlink into handlers.d/

use strict;
use warnings;
use FindBin;
use lib "$FindBin::RealBin";
use SimpleXMSG qw/replyxmsg/;
use File::Temp qw/tempfile/;

our $helpmsg =
	"XMSG-RSS command interface\n" .
	"Supported commands:\n" .
	"\thelp\t\tthis message\n" .
	"\trefresh\t\tcalls xmsgrss.pl to refresh all feeds\n" .
	"\tlist\t\tlist feeds in db\n" .
	"\tadd %n %u\tadd new feed to db, %n = name, %u = url\n" .
	"\trm [%n]\t\tremove feed, [%n] = name, if writing to xmsg:rss-feedname, MUST be NULL";
our $dbfile = "$FindBin::RealBin/db";
our $xmsgrss = "$FindBin::RealBin/xmsgrss.pl";
our $f;

sub DoRemove
{
	my $what = shift;
	
	eval {
		my ($out, $line, @arr);
		my ($found, $confirmed) = (0, 0);

		open($f, "+<$dbfile") or die('unable to open database');
		while (<$f>) {
			$out .= $_, next if (/^#/ || /^$/);
			$line = $_;
			@arr = split(/##/);
			if ($arr[0] eq $what) {
				if ($arr[2] ne '1') {
					$arr[2] = '1';
					$out .= join('##', @arr);
				} else {
					$confirmed = 1;
				}
				$found = 1;
			} else {
				if ($arr[2] eq '1') {
					$arr[2] = '0';
					$out .= join('##', @arr);
				} else {
					$out .= $line;
				}
			}
		}
		close($f), die('feed not found') if (!$found);
		
		seek($f, 0, 0);
		truncate($f, 0);
		print($f $out);
		close($f);
		
		die('Please confirm feed deletion by typing rm command again') if (!$confirmed);
	};
	
	if ($@) {
		local $_ = $@;
		s/ at.*$//;
		chomp;
		if (/confirm/) {
			replyxmsg($_);
		} else {
			replyxmsg("Unable to remove feed: $_");
		}
	} else {
		replyxmsg('Feed removed successfully.');
	}
}

my $cmd;

if ($ARGV[0] =~ /^rss-(.*)$/) {
	{
		local $/;

		open($f, "<$ARGV[1]");
		$cmd = <$f>;
		close($f);
	}

	chomp $cmd;
	if ($cmd eq 'rm') {
		DoRemove($1);
	} else {
		replyxmsg('This UID is used only for sending RSS notifications, please write to xmsg:rss instead');
	}

	exit 0;
} elsif ($ARGV[0] eq 'rss') {
	{
		local ($/, $f);
		open($f, "<$ARGV[1]");
		$cmd = <$f>;
		close($f);
	}
	
	chomp $cmd;
	if ($cmd eq 'list') {
		my ($outmsg, @arr);
		
		eval {
			open($f, "<$dbfile") or die;
			while (<$f>) {
				chomp;
				next if (/^#/ || /^$/);
				@arr = split(/##/);
				
				$outmsg .= "$arr[0] = $arr[1]\n";
			}
			close($f);
			if ($outmsg) {
				chomp $outmsg;
				replyxmsg($outmsg);
			} else {
				replyxmsg('Database is empty');
			}
		};

		replyxmsg('Unable to open database') if ($@);
	} elsif ($cmd =~ /^add[[:space:]]+(.*?)[[:space:]]+(https?:\/\/.*)$/) {
		eval {
			open($f, ">>$dbfile") or die('unable to open database');
			print($f "$1##$2##0####\n") or die('db write error');
			close($f);
		};
		
		if ($@) {
			local $_ = $@;
			s/ at.*$//;
			chomp;
			replyxmsg("Unable to add feed: $_");
		} else {
			replyxmsg('Feed added successfully.');
		}
	} elsif ($cmd =~ /^rm[[:space:]]+(.*)$/) {
		DoRemove($1);
	} elsif ($cmd eq 'refresh') {
		`$xmsgrss`;
		if ($?>>8) {
			replyxmsg('Execution failed: ' . ($?>>8));
		} else {
			replyxmsg('RSS refresh finished');
		}
	} elsif ($cmd eq 'help') {
		replyxmsg($helpmsg);
	} else {
		replyxmsg('Syntax error, try: help');
	}

	exit 0;
}

exit 1;
