#!/usr/bin/perl
#
# FUSE-based logs plugin-like directory structure emulator for logsqlite
# (C) 2007 Michał Górny
#

use strict;
use warnings;
#use threads;
#use threads::shared;

use Fuse;
use File::Spec;
use File::HomeDir;
use POSIX qw/ENOENT ENOTDIR EISDIR EROFS O_RDONLY O_WRONLY O_RDWR/;
use DBI;
use List::Util qw/first/;
use Cache::MemoryCache;

$, = ' / ';
$\ = "\n";

#die "syntax: $0 mountpoint [mountopts]" unless (@ARGV > 0);

my $dbpath = File::HomeDir->my_home . '/.ekg2/logsqlite.db';

my $db = DBI->connect("dbi:SQLite:dbname=$dbpath", '', '') or die;
my $dbq_getsids = $db->prepare('SELECT DISTINCT session FROM log_msg;');
my $dbq_getuids = $db->prepare('SELECT DISTINCT uid FROM log_msg WHERE session = ?1;');
my $dbq_getn	= $db->prepare('SELECT ts FROM log_msg ORDER BY ts DESC LIMIT 1;');
my $dbq_getns	= $db->prepare('SELECT ts FROM log_msg WHERE session = ?1 ORDER BY ts DESC LIMIT 1;');
my $dbq_getnu	= $db->prepare('SELECT ts FROM log_msg WHERE session = ?1 AND uid = ?2 ORDER BY ts DESC LIMIT 1;');
my $dbq_getdata = $db->prepare('SELECT nick, type, sent, ts, sentts, body FROM log_msg WHERE session = ?1 AND uid = ?2 ORDER BY ts ASC LIMIT 20;');

my $cache = Cache::MemoryCache->new({namespace => 'logsqliteFuseCache', default_expires_in => 300});

Fuse::main(debug => 1, mountpoint => shift, threaded => 0,
	getattr => \&main::myGetAttr, getdir => \&main::myGetDir, open => \&main::myOpen, read => \&main::myRead);

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
	my $timestamp = $r[0];

	return (0, 0, ($uid ? 0100444 : 040555), 1, $<, $(, 0, 0, $timestamp, $timestamp, $timestamp, 1024, 0);
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

sub myOpen {
	my ($path, $mode) = @_;
	my $r = myGetAttr($path);
	print STDERR ('myOpen', $path, sprintf '0x%x', $mode);
	return $r unless ($r >= 0);

	my (undef, $sid, $uid) = File::Spec->splitdir($path);
	return -EEISDIR() if (!$uid);
	return -EROFS() if ($mode & O_WRONLY() || $mode & O_RDWR());

	return 0;
}

sub myRead {
	my ($path, $count, $offset) = @_;
	my (undef, $sid, $uid) = File::Spec->splitdir($path);
	my $cacheref = $cache->get("$sid/$uid");
	my %cachedata;
	my $outbuf = '';

	print STDERR ('myRead', $path, $count, $offset);

	%cachedata = %$cacheref if ($cacheref);
	%cachedata = (db_offset => 0, text_offset => 0, buffer_size => 0, buffer => undef) if (!%cachedata || $cachedata{text_offset} > $offset);

	while ($cachedata{text_offset} + $cachedata{buffer_size} < $offset) {
		myStepBuf(\%cachedata, $sid, $uid);
	}

	return 0;
}

sub myStepBuf {
	my ($cachedata, $sid, $uid) = @_;

	$$cachedata{text_offset} += $$cachedata{buffer_size};
	$$cachedata{buffer} = '';

	$dbq_getdata->execute($sid, $uid, $$cachedata{db_offset});
	while (my @row = $dbq_getdata->fetchrow_array()) {
		my $out_type;

		if ($row[1] eq 'system') {
			$out_type = 'msgsystem';
		} else {
			if ($row[1] eq 'msg') {
				$out_type = 'msg';
			} else {
				$out_type = 'chat';
			}
			if ($row[2]) {
				$out_type .= 'send';
			} else {
				$out_type .= 'recv';
			}
		}

		my $body = logEscape($row[5]);

		$$cachedata{buffer} = sprintf('%s,%s,%s,%d,%s', $out_type, $uid, $row[0], $row[3], ($row[2] ? $body : sprintf('%d,%s', $row[4], $body)));
		$$cachedata{db_offset}++;
	}

	$$cachedata{buffer_size} = length($$cachedata{buffer});

	use YAML qw/Dump/;
	print STDERR Dump(%$cachedata);
}
