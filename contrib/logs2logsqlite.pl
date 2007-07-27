#!/usr/bin/perl
#
# Simple logs -> logsqlite logs converter
# - no XML support
# - no status support
# - no conferences support
# - no magic pathes support
# (C) 2007 Michał Górny

use DBI;
use File::HomeDir;
use File::Basename;
use IO::File;
use Date::Parse;

# homedir, inserted instead of '~', by default automagically detected, but you can change it
my $home = File::HomeDir->my_home;
# source path
(my $logspath = '~/.ekg2/logs')
	=~ s/~/$home/;
# destination db
(my $logsqlitedb = '~/.ekg2/logsqlite.db')
	=~ s/~/$home/;

my $db = DBI->connect("dbi:SQLite:dbname=$logsqlitedb", '', '') or die;
$db->begin_work;
my $st = $db->prepare("INSERT INTO log_msg VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

my $n;

while (<$logspath/*>) {
	(my $sid = basename($_)) =~ /^(.*?)\:/;
	my $prefix = $1;
	
	next if ($sid =~ /^_/ || !$prefix); # skip internals and wrong UID-s
	print "-> $sid\n";

	while (<$logspath/$sid/$prefix:*.txt>) {
		(my $uid = basename($_)) =~ s/\.txt$//;
		my $f = IO::File->new("<$logspath/$sid/$uid.txt");

		print "\t-> $uid\n";
		while (<$f>) {
			s/^(.*?),(.*?),(.*?),(.*?),//;
			my $text = $_;
			my ($type, $nick, $date) = ($1, $2, $4);
			next if ($type eq 'status');
			my $is_sent = ($type eq 'chatsend' || $type eq 'msgsend');
			my $sent;
			if ($is_sent || ($type eq 'msgsystem')) {
				$sent = $date;
			} else {
				if ($text =~ s/^(.*?),//) {
					$sent = $1;
				} else {
					$sent = $date;
				}
			}

			$text =~ s/\n$//;
			if ($text =~ /^"(.*)"$/) { # need to unescape
				$_ = $1;
				s/\\n/\n/g, s/\\r/\r/g, s/\\(.)/$1/g;
				$text = $_;
			}

			if ($type =~ /^(msg|chat)(send|recv)/) {
				$type = $1;
			} elsif ($type =~ /^msg(system)$/) {
				$type = $1;
			} else {
				$type = 'chat';
			}

			my $datetmp = str2time($date);
			$date = $datetmp if ($datetmp);
			$datetmp = str2time($sent);
			$sent = $datetmp if ($sent);
			
			$st->execute($sid, $uid, $nick, $type, $is_sent, $date, $sent, $text);
		}
	}
}

$db->commit;
