#!/usr/bin/perl
#
# change 'jid:'->'xmpp:' in logsqlite database
# (C) 2007 Michał Górny
#

use DBI;
use File::HomeDir;
$| = 1;

my $home = File::HomeDir->my_home;
my $db = DBI->connect("dbi:SQLite:dbname=$home/.ekg2/logsqlite.db") or die;

my $q = $db->prepare('SELECT DISTINCT session, uid FROM log_msg WHERE session LIKE ?1 OR uid LIKE ?2 LIMIT 500;') or die;
my $u = $db->prepare('UPDATE log_msg SET session = ?3, uid = ?4 WHERE session = ?1 AND uid = ?2;') or die;

while (1) {
	$q->execute('jid:%', 'jid:%') or die;
	my $res = $q->fetchall_arrayref();
	last unless (@{@$res});
	$db->begin_work;
	foreach (@$res) {
		my ($sid, $uid) = @$_;
		print STDERR '.', next unless ($sid =~ /^jid:/ || $uid =~ /^jid:/);
		print STDERR '*';

		my ($newsid, $newuid) = ($sid, $uid);
		$newsid =~ s/^jid:/xmpp:/;
		$newuid =~ s/^jid:/xmpp:/;
		$u->execute($sid, $uid, $newsid, $newuid) or die;
	}
	$db->commit;
	print '!';
}

