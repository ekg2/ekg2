use Ekg2;
use Ekg2::Irc;
use strict;

our $VERSION = '0.01';
our %EKG2 = (
	authors     => 'Wieslaw Ochminski',
	contact     => 'wiechu@wiechu.com',
	name        => 'autoop',
	description => 'Auto op the good guys.',
	license     => 'GPL',
	changed     => '2010-12-27',
);

##############################################################################
#
# Configure
#

my $custom_SNS		= '';		# set your namespace for script, 'A:' for example

##############################################################################
#
# Global variables
#
my $SNS;		# script namespace

my $revenge;
my $debug;

my %ops;
my %voice;

my $ekg2_display_crap;
##############################################################################
#
# Functions
#
#------------------------------
sub cprint($) {
	my $format = shift;
	my $wid = $ekg2_display_crap ? Ekg2::window_current->{id} : 1;
	Ekg2::print($wid, $format);
}
#------------------------------
sub debug($) {
	return unless $debug;
	my ($txt) = @_;
	Ekg2::print(0, $txt);
}
#------------------------------
sub lceq($$) {
	return lc(shift) eq lc(shift);
}
#------------------------------
sub display_hostident_list($$) {
	my ($h, $msg) = @_;
	foreach my $s (sort keys %$h) {
		foreach my $c (sort keys %{$h->{$s}}) {
			cprint("%n(%B$s%n) %T$msg%T channel %Y$c ");
			foreach my $i (sort keys %{$h->{$s}->{$c}}) {
				my $list;
				foreach my $s (match_sessions($s)) {
					foreach my $p (Ekg2::Irc::session2server(Ekg2::session_find($s))->people) {
						next unless (grep /^$i$/, $p->{ident}.'@'.$p->{hostname});
						$list.= '%n,' if $list;
						$list.= '%c'.$p->{nick};
					}
				}
				$list = ' %n['.$list.'%n]' if $list;
				cprint("\t%g".unescape($i).$list);
			}
		}
	}
}
#------------------------------
sub is_user_mode($$$;$) {
	my ($sessid, $chan, $sign, $nick) = @_;

	return 0 unless my $server = Ekg2::Irc::session2server(Ekg2::session_find($sessid));

	$nick = $server->{nick} unless $nick;
	foreach my $user ($server->people()) {
		next unless (lceq($nick, $user->{nick}));
		foreach my $ch ($user->channels()) {
			return $ch->{sign} eq $sign if (lceq($ch->{channel}->{name}, $chan));
		}
	}
	return 0;
}
#------------------------------
sub amiop($$) {
	my ($sessid, $chan) = @_;
	is_user_mode($sessid, $chan, '@');
}
#------------------------------
sub is_channel_name($) {
	return (shift =~ /^[#\!]/);		# XXX SOP(PREFIX)
}
#------------------------------
sub is_irc_sess_uid($) {
	return (shift =~ /^irc:/);
}
#------------------------------
sub escape($) {
	my ($t) = @_;
	$t =~ s/\./\\./g;
	$t =~ s/\*/.+/g;
	return $t;
}
#------------------------------
sub unescape($) {
	my ($t) = @_;
	$t =~ s/\\\././g;
	$t =~ s/\.\+/*/g;
	return $t;
}
#------------------------------
sub check_identhost($$$$) {
	my ($sessid, $chan, $ihost, $hash) = @_;
	my (@list) = (	( keys %{$hash->{'irc:*'}->{'*'}}),
			( keys %{$hash->{'irc:*'}->{$chan}}),
			( keys %{$hash->{$sessid}->{'*'}}),
			( keys %{$hash->{$sessid}->{$chan}})
		    );
	# remove dups
	my %tmp;
	@tmp{@list} = ();

	foreach my $reg (keys %tmp) {
		debug("${SNS}check_identhost() check: $reg");
		return 1 if grep /^$reg$/, $ihost;
	}

	return 0;
}
#------------------------------
sub match_sessions($) {
	my ($sessid) = @_;
	return $sessid unless (lceq($sessid, 'irc:*'));
	my @sess;
	foreach my $s (Ekg2::sessions) {
		next unless is_irc_sess_uid($s->{uid});
		push @sess, $s->{uid};
	}
	return @sess;
}
#------------------------------
sub identhost_find($$) {
	my ($sessid, $nick) = @_;
	foreach my $sess (match_sessions($sessid)) {
		my $server = Ekg2::Irc::session2server(Ekg2::session_find($sess));
		foreach my $user ($server->people()) {
			next unless lceq($user->{nick}, $nick);
			return $user->{ident}.'@'.$user->{hostname};
		}
	}
	return
}
#------------------------------
sub get_users_by_nick($$) {
	my ($server, $channel) = @_;
	my %h;
	foreach my $user ($server->people()) {
		foreach my $ch ($user->channels()) {
			next unless lceq($ch->{channel}->{name}, $channel);
			$h{$user->{nick}}->{identhost} = $user->{ident} . '@' . $user->{hostname}
		}
	}
	return %h;
}
#------------------------------
sub smart_set_mode($$$$) {
	my ($sessid, $cmd, $channel, $nick) = @_;
	my $plus = ! ($cmd =~ /^de/i);
	my $sign = ($cmd =~ /op$/i) ? '@' : '+';
	my $mode = is_user_mode($sessid, $channel, $sign, $nick);
	return unless ($plus ^ $mode);
	Ekg2::command_exec('', Ekg2::session_find($sessid), "/$cmd $channel $nick");
}
#------------------------------
sub exec_new_regexp($$$$) {
	my ($sessid, $cmd, $chan, $regexp) = @_;
	my $allchan = $chan eq '*';
	debug("%B${SNS}exec_new_regexp() $cmd, $sessid, $chan, ".unescape($regexp));

	return unless my $server = Ekg2::Irc::session2server(Ekg2::session_find($sessid));

	foreach my $user ($server->people()) {
		foreach my $ch ($user->channels()) {
			next if lceq($server->{nick}, $user->{nick});	# it's me!
			my $ch_n = $ch->{channel}->{name};
			next unless ($allchan || lceq($ch_n, $chan));
			next unless grep /^$regexp/, $user->{ident}.'@'.$user->{hostname};
			next unless amiop($sessid, $ch_n);
			smart_set_mode($sessid, $cmd, $ch_n, $user->{nick});
		}
	}
}
#------------------------------
sub welcome(@) {
	return unless Ekg2::variable_find($SNS.'welcome')->{value};
	# XXX
	cprint("%> %|%GAutoop\n%G/${SNS}help\t%nfor more information\n");
}
#------------------------------
sub parse_op_voice($$) {
	my ($vname, $hash) = @_;
	my $tmp = Ekg2::variable_find($SNS.$vname)->{value};
	my (@a) = split ' +', $tmp;
	%{$hash} = ();
	while (@a) {
		my $sess = shift @a;
		my $chan = shift @a;
		my $list = shift @a;
		foreach my $h (split ',', $list) {
			next unless $h;
			$h = escape($h);
			$hash->{$sess}->{$chan}->{$h} = 1;
		}
	}
}
#------------------------------
##############################################################################
#
# Variable handlers
#
#------------------------------
sub parse_op() {
	parse_op_voice('__op', \%ops);
}
#------------------------------
sub parse_voice() {
	parse_op_voice('__voice', \%voice);
}
#------------------------------
sub bool_var() {
	# variable acts like bool
	my ($name, $val) = @_;

	my $v =	Ekg2::variable_find($name);
	$v->set(0 + !!$val) if $v;
}
#------------------------------
sub int_var() {
	# variable acts like integer
	my ($name, $val) = @_;

	my $v =	Ekg2::variable_find($name);
	$v->set(0 + $val) if $v;
}
#------------------------------
sub variable_changed() {

	my ($name) = ${$_[0]};
	my $val;

	if      (lceq($name, 'display_crap')) {
		$ekg2_display_crap = Ekg2::variable_find('display_crap')->{value};
	} elsif (lceq($name, $SNS.'debug')) {
		$debug = Ekg2::variable_find($name)->{value};
	} elsif (lceq($name, $SNS.'revenge')) {
		$revenge = Ekg2::variable_find($name)->{value};
	}
}
##############################################################################
#
# Commands
#
sub command_op() {
	my ($cmd, $params) = @_;
	my ($hash, $nlist, $change, $delete);
	my $exec = 1;
	my ($sessid, $chan) = (Ekg2::session_current->{uid}, '#*');

	return cprint("%! %n(%B$cmd%n) %RError! %nBrak parametrow. %nZobacz %T/${SNS}help") unless (length($params));

	# check current window
	my $a = Ekg2::window_current->{target};
	$chan = $a if ($a =~ s/^irc:(.+)$/$1/ && is_channel_name($a));	# default is current chanel

	my (@params) = split " +", $params;

	foreach my $p (@params) {
		if ($p eq '-q') {
			$exec = 0;
		} elsif ( is_channel_name($p) ) {
			$chan = $p;
		} elsif ( is_irc_sess_uid($p) ) {
			$sessid = $p;
		} else {
			$nlist .= ",$p";
		}
	}

	return cprint("%! %n(%B$cmd%n) %RError! %n%T$sessid%R is not irc session") unless is_irc_sess_uid($sessid);

	$nlist =~ s/^,//;
	$nlist =~ s/\birc://g;
	$chan = '*' if ($chan eq '#*');

	$cmd =~ s/^$SNS//;
	$delete = ($cmd =~ s/^de//i);
	$hash = lceq($cmd, 'op') ? \%ops : \%voice;

	foreach my $nick (split(',', $nlist)) {
		my $identhost = ( $nick =~ /@/ ) ? $nick : identhost_find($sessid, $nick);

		unless ($identhost) {
			cprint("%! %n(%B${SNS}$cmd%n) %rWarning: unknown user %Y%$nick%n (session:%T$sessid%n, channel:%T$chan%n)\n");
			next;
		}

		my $regexp = escape($identhost);
		if ( !( $delete ^ ($hash->{$sessid}->{$chan}->{$regexp}) ) ) {
			if ($delete) {
				delete $hash->{$sessid}->{$chan}->{$regexp};
				# XXX msg
				# exec_new_regexp() here?
			} else {
				$hash->{$sessid}->{$chan}->{$regexp} = 1;
				exec_new_regexp($sessid, $cmd, $chan, $regexp) if $exec;
				# XXX msg
			}
			$change = 1;
		} else {
			if ($delete) {
				# XXX msg
			} else {
				# XXX msg
			}
		}
	}

	return unless $change;

	# store variables
	my $str;
	foreach my $s (sort keys %$hash) {
		foreach my $c (sort keys %{$hash->{$s}}) {
			my $list;
			for my $i (sort keys %{$hash->{$s}->{$c}}) {
				$list .= ',' if $list;
				$list .= unescape($i);
			}
			if ($list) {
				$str .= ' ' if $str;
				$str .= "$s $c $list";
			} else {
				delete $hash->{$s}->{$c};
				delete $hash->{$s} unless keys %{$hash->{$s}};
			}
		}
	}
	my $v =	Ekg2::variable_find($SNS.'__'.$cmd);
	$v->set($str) if $v;

}
#------------------------------
sub command_list() {
	my ($name, $params) = @_;
	my ($all, $lo, $lv);
	$all = 1 unless $params;

	foreach my $p (split ' +', $params) {
		$all	|= ($p eq '-a') || ($p eq '--all');
		$lo	|= ($p eq '-o') || ($p eq '--op');
		$lv	|= ($p eq '-v') || ($p eq '--voice');
	}

	return cprint("%! %n(%B$name%n)%r unknown option. %nSee %T/${SNS}help") unless ($all||$lo||$lv);

	display_hostident_list(\%ops, 'Auto op') if ($all || $lo);
	display_hostident_list(\%voice, 'Auto voice') if ($all || $lv);
}
#------------------------------
sub command_help() {
	my ($name, $param) = @_;

	# XXX
	if (grep /^$param$/, qw/op deop voice devoice/) {
		cprint("Command %T/${SNS}$param\t[session] [channel] [nick|indent\@host]");
		cprint("   session - ... XXX");
		return;
	}
	# XXX
	cprint("%>%1help
%GCommands:%n
  %T/${SNS}op\t\t[session] [channel] [nick|indent\@host]
  %T/${SNS}voice\t[session] [channel] [nick|indent\@host]
  %T/${SNS}deop\t[session] [channel] [nick|indent\@host]
  %T/${SNS}devoice\t[session] [channel] [nick|indent\@host]
  %T/${SNS}list\t%n[%g-a%n%T|%g-o%n%T|%g-v%n]
  %T/${SNS}help

%GVariables:%n
  %T/${SNS}welcome
  %T/${SNS}debug
  %T/${SNS}revenge

See %T/${SNS}help [command]%n ...");
}
##############################################################################
#
# Handlers
#
sub join_hanler() {
	my ($sessid, $chan, $nick, $isour, $identhost) = map $$_, @_;
	my $amiop = amiop($sessid, $chan);

	debug("%B${SNS}join_hanler(".($amiop ? '@' : '').") $sessid, $chan, $nick, $isour, $identhost");

	return 0 if ($isour || !$amiop);

	smart_set_mode($sessid, 'op', $chan, $nick) if (check_identhost($sessid, $chan, $identhost, \%ops));
	smart_set_mode($sessid, 'voice', $chan, $nick) if (check_identhost($sessid, $chan, $identhost, \%voice));

	return 0;
}
#------------------------------
sub mode_hanler() {
	my ($sessid, $nickihost, $channel, $plus, $mode, $param) = map $$_, @_;

	return 0 unless my $server = Ekg2::Irc::session2server(Ekg2::session_find("$sessid"));

	my ($who, $ihost) = split('!', $nickihost);

	return 0 if lceq($server->{nick}, $who);	# it's my set mode

	my $amiop = amiop($sessid, $channel);

	if ($plus && ($mode eq 'o') && lceq($server->{nick}, $param) && !$amiop) {
		debug("I'm $channel op now!");
		my %users = get_users_by_nick($server, $channel);
		foreach my $nick (keys %users) {
			my $identhost = $users{$nick}->{identhost};
			smart_set_mode($sessid, 'op', $channel, $nick) if (check_identhost($sessid, $channel, $identhost, \%ops));
			smart_set_mode($sessid, 'voice', $channel, $nick) if (check_identhost($sessid, $channel, $identhost, \%voice));
		}
		return 0;
	}

	return 0 unless $amiop;

	if (!$plus && ($mode =~ /[ov]/)) {

		my %users = get_users_by_nick($server, $channel);
		if ($users{$param}) {
			my $sob = 0;
			$sob = ($revenge==1) ? !check_identhost($sessid, $channel, identhost_find($sessid, $who), \%ops) : 1 if $revenge;
			my $identhost = $users{$param}->{identhost};
			if (($mode eq 'o') && check_identhost($sessid, $channel, $identhost, \%ops)) {
				smart_set_mode($sessid, 'deop', $channel, $who) if $sob;
				Ekg2::command_exec('', Ekg2::session_find($sessid), "/op $channel $param");
			}
			if (($mode eq 'v') && check_identhost($sessid, $channel, $identhost, \%voice)) {
				smart_set_mode($sessid, 'deop', $channel, $who) if $sob;
				Ekg2::command_exec('', Ekg2::session_find($sessid), "/voice $channel $param");
			}
		}
	}

	return 0;
}
##############################################################################
#
# Init
#
{
	if ($custom_SNS eq '') {
		# script name space is script name
		$_[0] =~ /^.+:([^:]+)$/;
		$SNS = "$1:";
	} else {
		$SNS = $custom_SNS;
	}

	#
	# variables
	#
	Ekg2::variable_add_ext($SNS.'welcome',	'1',	'bool_var');
	Ekg2::variable_add_ext($SNS.'debug',	'0', 	'bool_var');
	Ekg2::variable_add_ext($SNS.'revenge',	'0',	'int_var');

	Ekg2::variable_add_ext($SNS.'__op',	'',	'parse_op');
	Ekg2::variable_add_ext($SNS.'__voice',	'',	'parse_voice');

	#
	# commands
	#
	Ekg2::command_bind_ext($SNS.'op',	'pu pu pu pu', '-q', 'command_op');
	Ekg2::command_bind_ext($SNS.'deop',	'pu pu pu pu', '-q', 'command_op');
	Ekg2::command_bind_ext($SNS.'voice',	'pu pu pu pu', '-q', 'command_op');
	Ekg2::command_bind_ext($SNS.'devoice',	'pu pu pu pu', '-q', 'command_op');

	Ekg2::command_bind_ext($SNS.'help',	'p', 'op deop voice devoice list', 'command_help');
	Ekg2::command_bind_ext($SNS.'list',	'p', '-a --all -o --op -v --voice', 'command_list');

	#
	# handlers
	#
	Ekg2::handler_bind('irc-join',		'join_hanler');
	Ekg2::handler_bind('irc-mode',		'mode_hanler');
	Ekg2::handler_bind('variable-changed',	'variable_changed');


	$ekg2_display_crap = Ekg2::variable_find('display_crap')->{value};
	$debug = Ekg2::variable_find($SNS.'debug')->{value};
	$revenge = Ekg2::variable_find($SNS.'revenge')->{value};

	parse_op();
	parse_voice();

	welcome(@_);

}

# Make perl happy
1;
