use vars qw($VERSION %EKG2);
use Ekg2;
use Ekg2::Irc;

our $VERSION = '0.1';
our %EKG2 = (
    authors     => 'Wieslaw Ochminski',
    contact     => 'wiechu@wiechu.com',
    description => 'All lights to cycki',
    license     => 'GPL',
    changed     => '2010-11-28',
);

my $C		= "\003";		# Ctrl-C
my $cycki	= "${C}5(${C}4*${C}5)(${C}4*${C}5)\017";

sub cycki {
	my ($sess, $from, $dest, $text, $to_us) = @_;

	$$to_us = 1 if ( $$text =~ s/^(.*)(\bcycki\b)(.*)$/$1$cycki$3/i );

}

Ekg2::handler_bind("irc-privmsg",	'cycki');
Ekg2::handler_bind("irc-notice",	'cycki');


1;
