#!/usr/bin/perl
#
# FUSE-based logs plugin-like directory structure emulator for logsqlite
# (C) 2007 Michał Górny
#

use Fuse;
#use threads;
#use threads::shared;
use File::Spec;
use File::HomeDir;
use POSIX qw/ENOENT ENOTDIR/;
use DBI;
use List::Util qw/first/;

$, = ' / ';
$\ = "\n";

die "syntax: $0 mountpoint [mountopts]" unless (@ARGV > 0);

my $dbpath = File::HomeDir->my_home . '/.ekg2/logsqlite.db';

my $db = DBI->connect("dbi:SQLite:dbname=$dbpath", '', '') or die;
my $dbq_getsids = $db->prepare('SELECT DISTINCT session FROM log_msg;');
my $dbq_getuids = $db->prepare('SELECT DISTINCT uid FROM log_msg WHERE session = ?1;');
my $dbq_getn	= $db->prepare('SELECT ts FROM log_msg ORDER BY ts DESC LIMIT 1;');
my $dbq_getns	= $db->prepare('SELECT ts FROM log_msg WHERE session = ?1 ORDER BY ts DESC LIMIT 1;');
my $dbq_getnu	= $db->prepare('SELECT ts FROM log_msg WHERE session = ?1 AND uid = ?2 ORDER BY ts DESC LIMIT 1;');

Fuse::main(debug => 0, mountpoint => shift, mountopts => '', threaded => 0,
	getattr => \&myGetAttr, getdir => \&myGetDir);

sub myGetAttr {
	my (undef, $sid, $uid, @rest) = File::Spec->splitdir($_[0]);
	my @r;

	print STDERR ('myGetAttr', $sid, $uid, @rest);
	if (@rest) {
		$dbq_getnu->execute($sid, $uid);
		@r = $dbq_getnu->fetchrow_array();
		if (@r) {
			return -ENOTDIR();
		} else {
			return -ENOENT();
		}
	} elsif ($uid) {
		$dbq_getnu->execute($sid, $uid);
		@r = $dbq_getnu->fetchrow_array();
	} elsif ($sid) {
		$dbq_getns->execute($sid);
		@r = $dbq_getns->fetchrow_array();
	} else {
		$dbq_getn->execute();
		@r = $dbq_getn->fetchrow_array();
	}
	return -ENOENT() unless (@r);
	$timestamp = $r[0];

	return (0, 0, ($uid ? 0100444 : 040555), 1, $<, $(, 0, 0, $timestamp, $timestamp, $timestamp, 4096, 1);
}

sub myGetDir {
	my (undef, $sid, $uid, @rest) = File::Spec->splitdir($_[0]);
	my @out;

	print STDERR ('myGetDir', $sid, $uid, @rest);

	if (!$sid) {
		$dbq_getsids->execute();
		push @out, $_[0] while (@_ = $dbq_getsids->fetchrow_array());
	} elsif (!$uid) {
		$dbq_getuids->execute($sid);
		push @out, $_[0] while (@_ = $dbq_getuids->fetchrow_array());
		return -ENOENT() unless (@out);
	} else {
		return -ENOENT();
	}

	return @out, 0;
}

