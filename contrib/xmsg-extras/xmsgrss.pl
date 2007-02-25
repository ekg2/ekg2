#!/usr/bin/perl

use strict;
use warnings;
use FindBin qw/$Bin/;
use lib "$FindBin::RealBin";
use SimpleXMSG qw/sendxmsg/;
use LWP::UserAgent;
use File::Temp qw/tempfile/;
use Sys::Syslog;
use XML::Feed;
use Encode;

our $myemail = 'peres@peres.int.pl'; # remove this, if you don't want to send From: header

our $dbfile = "$FindBin::RealBin/db";
our $dtformat = '%x %X';

$dbfile = $ARGV[0] if ($ARGV[0]);

sub Loguj {
	my $msg = shift;
	syslog('info', $msg);
}

# db format:
#   name ; url ; internaluse (= 0) [; lastnews [; lastmodified]]

my ($f, @arr, $ua, $r, @outarr, $feed, $hl);
$\ = "\n";

$ua = LWP::UserAgent->new;
$ua->timeout(30);
$ua->env_proxy;
$ua->default_header('Accept' => 'application/rss+xml, application/rdf+xml, application/rss+rdf+xml, text/xml;q=0.7');
$ua->default_header('Accept-Language' => 'pl, en;q=0.5');
$ua->default_header('Accept-Charset' => 'utf-8, *;q=0.5');
$ua->default_header('From' => $myemail) if ($myemail);

openlog('rssxmsg', 'nofatal,pid', 'user');

open ($f, "+<$dbfile") or die('Cannot open db R/W');
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
	
	$feed = XML::Feed->parse(\$r->content);
	if ($feed) {
		my $sawfirst = 0;
		my $newid = '';
		my ($msgtext, $xtext, $content, @wholemsg);

		foreach $hl ($feed->entries) {
			last if ($hl->id eq $arr[3]);
			if (!$sawfirst) {
				$newid = $hl->id;
				$msgtext = sprintf("[ %s ]\n< %s >", $feed->title, $feed->link);
				$sawfirst = 1;
			}
			$xtext = $hl->author if ($hl->author);
			$xtext .= ($xtext ? ' / ' : '') . $hl->issued->strftime($dtformat) if ($hl->issued);
			$xtext .= ($xtext ? ' / ' : '') . $hl->modified->strftime($dtformat) if (!$hl->issued && $hl->modified);
			$xtext .= ($xtext ? ' / ' : '') . $hl->category if ($hl->category);
			$xtext = sprintf("\n( %s )", $xtext) if ($xtext);
			$content = ($hl->summary && $hl->summary->body ? $hl->summary->body : $hl->content->body);
			
			if ($feed->as_xml =~ /^<\?xml[^>]*?encoding=["']utf-?8["'].*?<feed[^>]*?xmlns=['"]http:\/\/www\.w3\.org\/2005\/Atom['"]/si) {
				# broke utf-8 handling
				Encode::_utf8_off($content);
			}
			push(@wholemsg, sprintf("\n\n\n[ %s ]\n< %s >%s\n\n%s", $hl->title, $hl->link, $xtext, $content));
			$xtext = '';
		}
		if ($sawfirst) {
			$msgtext .= join('', reverse(@wholemsg));
			if ($feed->as_xml =~ /^<\?xml[^>]*?encoding=["']utf-?8["'].*?<feed[^>]*?xmlns=['"]http:\/\/www\.w3\.org\/2005\/Atom['"]/si) {
				# broken utf-8 handling workaround
				$msgtext = encode('iso-8859-1', $msgtext);
			}

			sendxmsg("rss-$arr[0]", $msgtext);
			$arr[3] = $newid if ($newid);
			Loguj("$arr[0]: New headlines sent");
		} else {
			Loguj("$arr[0]: No new headlines");
		}
	} else {
		$arr[3] = '';
		Loguj("$arr[0]: Feed parsing failed");
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
print($f "#name##url##reserved[##lastid[##lastmodified]]");
foreach my $val (@outarr) {
	print($f join('##', @{$val}));
}
close($f);

closelog;
