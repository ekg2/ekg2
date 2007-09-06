#!/usr/bin/perl
#
# Strip resources logsqlite database
# (C) 2007 Michał Górny
#

use DBI;
use File::HomeDir;
$| = 1;

my $home = File::HomeDir->my_home;
my $db = DBI->connect("dbi:SQLite:dbname=$home/.ekg2/logsqlite.db") or die;

my $q = $db->prepare('SELECT DISTINCT uid FROM log_msg WHERE uid LIKE ?1 LIMIT 500;') or die;
my $u = $db->prepare('UPDATE log_msg SET uid = ?2 WHERE uid = ?1;') or die;

while (1) {
	$q->execute('%:%@%/%') or die;
	my $res = $q->fetchall_arrayref();
	last unless (@$res);
	$db->begin_work;
	foreach (@$res) {
		my ($uid) = @$_;
		print STDERR '.', next unless ($uid =~ /^.*:.*@.*\/.*$/);
		print STDERR '*';

		my ($newuid) = ($uid);
		$newuid =~ s/\/.*$//;
		$u->execute($uid, $newuid) or die;
	}
	$db->commit;
	print '!';
}

