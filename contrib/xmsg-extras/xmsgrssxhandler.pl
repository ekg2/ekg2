#!/usr/bin/perl
# to be called from xmsghandler.pl
# symlink into handlers.d/

use strict;
use warnings;
use FindBin;
use lib "$FindBin::RealBin";
use SimpleXMSG qw/replyxmsg/;
use File::Temp qw/tempfile/;
use YAML qw/DumpFile LoadFile Dump/;
use Digest::MD5 qw/md5_hex/;

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
		my @db = LoadFile($dbfile);
		my $r;

		foreach (@db) {
			if ($$_{Delete_Confirmation} eq $what) {
				my @newdb;
				$$_{Delete_Confirmation} eq $what or push(@newdb, $_) foreach (@db);
				DumpFile($dbfile, @newdb);
				die('Feed removed successfully');
			} elsif ($$_{Name} eq $what) {
				$$_{Delete_Confirmation} = md5_hex(rand());
				DumpFile($dbfile, @db);
				die("Please confirm feed removal by typing 'rm $$_{Delete_Confirmation}'");
			}
		}

		die('feed not found');
	};
	
	if ($@) {
		local $_ = $@;
		s/ at.*$//;
		chomp;
		if (/(confirm|successfully)/) {
			replyxmsg($_);
		} else {
			replyxmsg("Unable to remove feed: $_");
		}
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
			my @arr = LoadFile($dbfile);
			foreach (@arr) {
				my %h = %$_;
				$outmsg .= "$h{Name} => $h{URL}\n";
			}
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
			my @tmparr = ({Name => $1, URL => $2});

			open($f, ">>$dbfile") or die('unable to open database');
			print($f Dump(@tmparr)) or die('db write error');
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
