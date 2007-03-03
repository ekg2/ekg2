#!/usr/bin/perl
#
# XMSG-based YouTube downloader
# based on some script found here: http://bashscripts.org/viewtopic.php?t=210
# (C) 2007 Michał Górny
#
use warnings;
use strict;
use LWP::Simple;
use FindBin;
use lib "$FindBin::RealBin";
use SimpleXMSG qw/replyxmsg/;

exit(1) if ($ARGV[0] ne 'youtube');

my $path;
my $dldir = "$ENV{HOME}/youtube-dl";
my $wgetopts = '-c --limit-rate=32k';
my $player = 'mplayer -vo dxr3 -ao alsa -fs -framedrop';
my $preplayer = 'mpc pause; sleep 2';
my $postplayer = 'sleep 2; mpc play';
my $autoplay = 1;
my $convertera = 'ffmpeg -i';
my $converterb = '';
my $converterc = '-sameq -vcodec mpeg2video -acodec copy';
my $converterext = 'mpg';
my $len;

{
	my ($f);
	local ($\);

	open($f, $ARGV[1]);
	$path = <$f>;
	close($f);
}

LWP::Simple::get($path) =~ /<title>YouTube - (.*?)<\/title>.*player2.swf\?(video_id=[0-9a-z]+.+?)\"/is or replyxmsg("Can't find download URL!"), exit(0);

my ($title, $id) = ($1, $2);

{
	my @res = LWP::Simple::head("http://youtube.com/get_video.php?$id");

	$len = ($res[1] / 1024) if (@res > 1);
	if ($len > 1024) {
		$len = sprintf("%.2f MiB", $len / 1024);
	} else {
		$len = sprintf("%.2f KiB", $len);
	}
}

replyxmsg("Download of '$title'" . ($len ? " [$len] " : "") . "( http://youtube.com/get_video.php?$id ) started.");

mkdir($dldir) if (! -d $dldir);

my $fn = $title;
($fn =~ s/\//_/g);

if (!$convertera || ! -f "$dldir/$fn.$converterext") {
	`wget $wgetopts -O "$dldir/$fn.flv" "http://youtube.com/get_video.php?$id"`;

	if ($convertera) {
		replyxmsg('Starting video converter...');
		`$convertera "$dldir/$fn.flv" $converterb "$dldir/$fn.$converterext" $converterc`;
		if (($?>>8) == 0) {
			unlink("$dldir/$fn.flv");
			$fn .= ".$converterext";
		} else {
			replyxmsg('Converter somewhat failed.');
			$fn .= '.flv';
		}
	} else {
		$fn .= '.flv';
	}
} else {
	$fn .= ".$converterext";
}

if (($?>>8) == 0) {
	if ($autoplay) {
		`$preplayer`;
		replyxmsg("Starting playback of '$title'...");
		`$player "$dldir/$fn" &> /dev/null`;
		if (($?>>8) == 0) {
			replyxmsg('Playback finished.');
		} else {
			replyxmsg("Playback somewhat failed. Please try by hand:\n\t$player \"$dldir/$fn\"");
		}
		`$postplayer`;
	} else {
		replyxmsg("Download of '$title' finished. To see it:\n\t$player \"$dldir/$fn\"");
	}
} else {
	replyxmsg("Download of '$title' somewhat failed.");
}

exit(0);
