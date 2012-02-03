use Ekg2;
use Time::HiRes;
use strict;

our $VERSION = '0.01';
our %EKG2 = (
	authors     => 'Wiesław Ochmiński',
	contact     => 'wiechu@wiechu.com',
	name        => 'ping',
	description => 'Ping irc servers.',
	license     => 'GPL',
	changed     => '2012-02-03',
);

##############################################################################
#
# Configure
#

my $custom_SNS		= '-';		# set your namespace for script, 'A:' for example

##############################################################################
#
# Global variables
#
my $SNS;		# script namespace

my $timer;
my $period = 0;
my $myname;		# script name
##############################################################################
#
# Functions
#
sub cprint($) {
	my $format = shift;
	my $win = Ekg2::variable_find('display_crap')->{value} ? Ekg2::window_current->{id} : 1;
	Ekg2::print($win, $format);
}

sub lceq($$) {
	return lc(shift) eq lc(shift);
}

sub welcome() {
	return unless Ekg2::variable_find("${SNS}welcome")->{value};
	cprint("\n%! %R$myname%n for %ge%Gk%gg%G2%n irc plugin.\n\n");
}

sub set_timer() {
	my $v = Ekg2::variable_find("${SNS}time")->{value};
	if ( ( ($v <= 0) || ($v != $period) ) && $timer ) {
		Ekg2::debug("Remove timer($period) $timer\n");
		Ekg2::Timer::destroy($timer);
		undef $timer;
	}
	if ($v != $period) {
		Ekg2::Timer::destroy($timer) if $timer;
		$timer = Ekg2::timer_bind($v, 'ping_timer');
		Ekg2::debug("Create timer($v) $timer\n");
	}
	$period = $v;
}

##############################################################################
#
# Handlers
#
sub ping_timer() {
	my ($type, $data) = @_;

	return if $type;

	foreach my $session (Ekg2::sessions) {
		next unless $session->{uid} =~ /^irc:/;
		next unless $session->{connected};
		
		my $t = Time::HiRes::gettimeofday;
		Ekg2::command_exec( '', $session, "/quote ping $t" )
	}
}

sub variable_changed() {
	my ($name) = ${$_[0]};
	set_timer() if lceq($name, "${SNS}time");
}

sub parse_line_hanler() {
	my ($session, $line) = map $$_, @_;

	return unless $line =~ /^:([^ ]+) PONG [^:]+:(.*)$/;

	my $display = Ekg2::variable_find("${SNS}display")->{value};

	return unless $display>0;

	my $server = $1;
	my $t0 = $2;
	my $delay = sprintf("%.3f", Time::HiRes::gettimeofday - $t0);
	my $format = "%y$server %RPONG%g response%b:%n ${delay}s\n";
	if ($display>1) {
		cprint($format);
	} else {
		Ekg2::print(0, $format);
	}
}

##############################################################################
#
# Init
#
{
	$myname = $_[0];
	$myname =~ s/^.+:([^:]+)$/$1/;
	if ($custom_SNS eq '-') {
		# script name space is script name
		$SNS = "$myname:";
	} else {
		$SNS = $custom_SNS;
	}

	#
	# variables
	#
	Ekg2::variable_add_bool("${SNS}welcome", 1);
	Ekg2::variable_add_int("${SNS}display", 2);	# 0 - no display, 1 - debug window, 2 - status or current window
	Ekg2::variable_add_int("${SNS}time", 180);

	#
	# handlers
	#
	Ekg2::handler_bind('irc-parse-line', 'parse_line_hanler');
	Ekg2::handler_bind('variable-changed',	'variable_changed');

	welcome();

	set_timer();

}


# Make perl happy
1;
