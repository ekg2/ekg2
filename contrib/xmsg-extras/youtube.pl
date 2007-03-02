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

{
	my ($f);
	local ($\);

	open($f, $ARGV[1]);
	$path = <$f>;
	close($f);
}

LWP::Simple::get($path) =~ /<title>YouTube - (.*?)<\/title>.*player2.swf\?(video_id=[0-9a-z]+.+?)\"/is or replyxmsg("Can't find download URL!"), exit(0);

my ($title, $id) = ($1, $2);

replyxmsg("Download of '$title' ( http://youtube.com/get_video.php?$id ) started.");

mkdir($dldir) if (! -d $dldir);

`wget $wgetopts -O "$dldir/$title.flv" "http://youtube.com/get_video.php?$id"`;

if (($?>>8) == 0) {
	if ($autoplay) {
		replyxmsg('Download finished.');
		`$preplayer`;
		replyxmsg("Starting playback of '$title'...");
		`$player "$dldir/$title.flv" &> /dev/null`;
		if (($?>>8) == 0) {
			replyxmsg('Playback finished.');
		} else {
			replyxmsg("Playback somewhat failed. Please try by hand:\n\t$player \"$dldir/$title.flv\"");
		}
		`$postplayer`;
	} else {
		replyxmsg("Download of '$title' finished. To see it:\n\t$player \"$dldir/$title.flv\"");
	}
} else {
	replyxmsg("Download of '$title' somewhat failed.");
}

exit(0);
