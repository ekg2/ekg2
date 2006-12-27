#!/usr/bin/perl
#
# xmsgrss.pl
# (C) 2006 Michał Górny <peres@peres.int.pl>

use FindBin;
use lib "/home/peres/perl/xmsghandler/"; # if you want to keep SimpleXMSG.pm inside program directory, comment this
use lib "$FindBin::Bin";		 # otherwise, comment this, and change above path to match your needs
use SimpleXMSG qw/sendxmsg/;
use LWP::UserAgent;
use XML::RSS::Feed;
use FindBin qw/$Bin/;
use File::Temp qw/tempfile/;
use Sys::Syslog;

die('No db file given') if (!$ARGV[0]);

sub Loguj {
	my $msg = shift;
	syslog('info', $msg);
}

# db format:
#   name ; url ; unused [; lastnews [; lastmodified]]

my ($f, @arr, $ua, $r, @outdb, $feed, $hl);
$\ = "\n";

$ua = LWP::UserAgent->new;
$ua->timeout(30);
$ua->env_proxy;
$ua->default_header('Accept' => 'application/rss+xml, application/rdf+xml, application/rss+rdf+xml, text/xml;q=0.7');
$ua->default_header('Accept-Language' => 'pl, en;q=0.5');
$ua->default_header('Accept-Charset' => 'utf-8, *;q=0.5');

openlog('rssxmsg', 'nofatal,pid', 'user');

open ($f, "+<$ARGV[0]") or die('Cannot open db R/W');
while (<$f>) {
	chomp;
	next if (/^#/ || /^$/);
	@arr = split (/##/);
	next if (@arr < 3);
	
	if ($arr[4]) {
		$r = $ua->get($arr[1], 'If-Modified-Since' => $arr[4]);
		push(@outarr, [@arr]), Loguj("$arr[0]: Skipping because of Last-Modified"), next if (($r->code == 304) || ($r->header('Last-Modified') eq $arr[4]));
	} else {
		$r = $ua->get($arr[1]);
	}
	push(@outarr, [@arr]), Loguj("$arr[0]: Download failed"), next if (!$r->is_success);
	
	$arr[4] = $r->header('Last-Modified');
	
	$feed = XML::RSS::Feed->new(name => $arr[0], url => $arr[1]);
	if ($feed->parse($r->content)) {
		my $sawfirst = 0;
		my $newid = '';
		my $msgtext;
		
		foreach $hl ($feed->headlines) {
			last if ($hl->id eq $arr[3]);
			if (!$sawfirst) {
				$newid = $hl->id;
				$msgtext = sprintf("[ %s ]\n< %s >", $feed->title, $feed->url);
				$sawfirst = 1;
			}
			$msgtext .= sprintf("\n\n\n[ %s ]\n< %s >\n\n%s", $hl->headline, $hl->url, $hl->description);
		}
		if ($sawfirst) {
			sendxmsg("rss-$arr[0]", $msgtext);
			$arr[3] = $newid if ($newid);
			Loguj("$arr[0]: New headlines sent");
		} else {
			Loguj("$arr[0]: No new headlines");
		}
	} else {
		$arr[3] = '';
		Loguj("$arr[0]: RSS parsing failed");
	}
	
	push(@outarr, [@arr]);
}

seek($f, 0, 0);
truncate($f, 0);
if ($0 =~ /^\//) {
	print($f "#!$0");
} elsif ($0 =~ /^\.\//) {
	print($f "#!$Bin/" . substr($0, 2));
} else {
	print($f "#!$Bin/$0");
}
print($f "#name##url##unused[##lastid[##lastmodified]]");
foreach my $val (@outarr) {
	print($f join('##', @{$val}));
}
close($f);

closelog;
