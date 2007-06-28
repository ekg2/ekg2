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
use YAML qw/DumpFile LoadFile/;
use List::Util qw/first/;

our $myemail	= 'me@someserver.pl'; # set to '', if you don't want to send From: header
our $myua	= 'xmsgrss/0.1 (I hate feedburner!)'; # attached to LWP UA, if set; this one disables WordPress->FeedBurner redirects

our $dbfile	= "$FindBin::RealBin/db";
our $dtformat	= '%x %X';

$dbfile = $ARGV[0] if ($ARGV[0]);

sub Loguj {
	my $msg = shift;
	syslog('info', $msg);
}

$\ = "\n";

my $ua = LWP::UserAgent->new;
$ua->timeout(30);
$ua->env_proxy;
$ua->default_header('Accept' => 'application/rss+xml, application/rdf+xml, application/rss+rdf+xml, text/xml;q=0.7');
$ua->default_header('Accept-Language' => 'pl, en;q=0.5');
$ua->default_header('Accept-Charset' => 'utf-8, *;q=0.5');
$ua->default_header('From' => $myemail) if ($myemail);
$ua->agent($ua->_agent() . " $myua") if ($myua);

openlog('rssxmsg', 'nofatal,pid', 'user');

die('Database not writable') if (! -w $dbfile);
my @db = LoadFile($dbfile);
foreach (@db) {
	my $arr = $_;
	my $r;
	
	if ($$arr{Last_Modified}) {
		$r = $ua->get($$arr{URL}, 'If-Modified-Since' => $$arr{Last_Modified});
		Loguj("$$arr{Name}: Skipping because of Last-Modified"), next if (($r->code == 304) || ($r->header('Last-Modified') eq $$arr{Last_Modified}));
	} else {
		$r = $ua->get($$arr{URL});
	}
	Loguj("$$arr{Name}: Download failed"), next if (!$r->is_success);
	
	$$arr{Last_Modified} = $r->header('Last-Modified');
	
	my $feed = XML::Feed->parse(\$r->content);
	if ($feed) {
		my $sawfirst = 0;
		my $newid = '';
		my ($msgtext, $xtext, $content, @wholemsg);

		foreach my $hl ($feed->entries) {
			next if (first { $_ eq $hl->id } @{$$arr{Seen}});
			push(@{$$arr{Seen}}, $hl->id);
			if (!$sawfirst) {
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

			sendxmsg("rss-$$arr{Name}", $msgtext);
			Loguj("$$arr{Name}: New headlines sent");
		} else {
			Loguj("$$arr{Name}: No new headlines");
		}
	} else {
		Loguj("$$arr{Name}: Feed parsing failed");
	}
}

DumpFile($dbfile, @db);
closelog();

