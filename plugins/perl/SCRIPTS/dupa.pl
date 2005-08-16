# use strict;
use vars qw($VERSION %EKG2);
use Ekg2;

$VERSION = "0.1";
%EKG2 = (
    authors     => 'Jakub Zawadzki',
    name        => 'dupa',
    description => '...',
    license     => 'Public Domain',
    changed     => 'Mon Jul 11 18:07 CET 2005'
);

my $i;

sub window_switcher {
	my ($type, $stimer, $timer) = @_;
	if ($type) { return; }
	Ekg2::Window::switch_id($i);
	
	Ekg2::timer_unbind($stimer) if (Ekg2::window_current->{id} != $i);
	$i++;
}


sub iwil_wait {
	my ($type, $stimer, $timer) = @_;
	if ($type) { return; }
	
#	$timer->destroy();

#	$iwil_wind = Ekg2::window_current;
	$i = 0;
	
	Ekg2::timer_bind(1, 'window_switcher');

	Ekg2::timer_unbind($stimer);

}

sub cmd_evil {
	$i = 5;
	Ekg2::echo("iwil mode on");
	Ekg2::echo("Take a look at Your windows, It may be the last time You see them?");
	
	Ekg2::timer_bind(0, 'iwil_wait');
	
	return 666;
	
	Ekg2::plugin_find("python")->unload(); 	# nie lubimy pythona! ;p 
#	Ekg2::plugin_find("gg")->unload(); 	#  gg tez nie ;p

	foreach my $var (Ekg2::variables) {
		$var->set("123");
	}

        foreach my $sess (Ekg2::sessions) {
		foreach my $sess_var ($sess->params() ) {
			$sess_var->set("123");
#			Ekg2::echo($sess_var  . " -> " . $sess_var->{key} . "  =  " .  $sess_var->{value});
		}
        }


}

sub cmd_perl_list {
	my ($params)   = @_;

	foreach my $comm (Ekg2::commands) {
		Ekg2::echo("$comm -> $comm->{name} = $comm->{params}");
	}
	Ekg2::echo("--------------------------------------------");

	foreach my $wind (Ekg2::windows) {
		Ekg2::echo("$wind -> $wind->{id} = $wind->{target}");
		Ekg2::echo("WINDOW->USERSLIST:  ($wind->{userlist})");
			foreach my $user2  ( Ekg2::Userlist::users( $wind->{userlist} ) ) {
				Ekg2::echo("$user2 -> $user2->{uid} $user2->{status}");
			}
	}
	Ekg2::echo("--------------------------------------------");

        foreach my $sess (Ekg2::sessions) {
                 Ekg2::echo("$sess -> $sess->{uid} $sess->{connected} ($sess->{status})");
		 Ekg2::echo("--------------USERSLIST--------------------");
		 foreach my $user  (Ekg2::Userlist::users( $sess->{userlist} ) ) {
			Ekg2::echo("$user -> $user->{uid} $user->{status}");
		}
		Ekg2::echo("--------------VARIABLES---------------------");
		foreach my $sess_var ($sess->params() ) {
			Ekg2::echo("$sess_var -> $sess_var->{key} = $sess_var->{value}");
		}
		Ekg2::echo("--------------------------------------------");
        }

        foreach my $plug (Ekg2::plugins) {
                Ekg2::echo("$plug -> $plug->{name} $plug->{prio}");
	}
	Ekg2::echo("--------------------------------------------");

	foreach my $var (Ekg2::variables) {
		Ekg2::echo("$var -> $var->{name} = $var->{value}");
#		$var->help();
	}
	
	foreach my $timer (Ekg2::timers) {
		Ekg2::echo("$timer -> $timer->{name} = $timer->{freq}");
#		$var->help();
	}
	
#	Ekg2::echo("--------------------------------------------");
}

sub cmd_connect_if_disconnected_disconnect_if_connected {
        foreach my $sess (Ekg2::sessions) {
                Ekg2::echo($sess  . " -> " . $sess->{uid} . " conn  = " . $sess->{connected} . " (" . $sess->{status} . ")" );

		if ($sess->{connected}) {
			$sess->disconnect();
		}
		else {
			$sess->connect();
		}
	}
}

sub cmd_test {
	$var =  Ekg2::var_find("dupus");
	Ekg2::echo("$var $var->{value}");
	
	
	return; 
        foreach my $sess (Ekg2::sessions) {
                Ekg2::echo("$sess -> $sess->{uid} $sess->{connected} ($sess->{status})");
		Ekg2::echo("--------------VARIABLES---------------------");
		foreach my $sess_var ($sess->params() ) {
#			Ekg2::echo("$sess_var -> $sess_var->{key} = $sess_var->{value}");
#			$sess_var->set($sess, "assa");
			
		}
		
        }

	return;

	my $sess = Ekg2::session_current;
	Ekg2::echo("$sess $sess->{uid} $sess->{userlist} ");

	foreach my $user  (Ekg2::Userlist::users( $sess->{userlist} ) ) {
		Ekg2::echo("$user -> $user->{uid} $user->{status}");
	}
#		Ekg2::User::remove_u($sess->{userlist}, $user);


}

sub cmd_timer_test { # testowanie obslugi timerow. powinno byc na przemian 0 i 1 z podobnymi adresami...
	my ($type, $stimer, $timer) = @_;
	
	Ekg2::echo("$type $stimer $timer");
	if ($type) {
		Ekg2::timer_bind(0.001, 'cmd_timer_test');
		return;
	}
	Ekg2::timer_unbind($stimer);
	return 0;
}

sub cmd_timer {
	my ($type, $stimer, $timer) = @_;
	if ($type) {
		Ekg2::echo("Timer ($timer) zniknal");
		return;
	}
	
	Ekg2::echo("Timer ($timer) zaraz zniknie");
	Ekg2::timer_unbind($stimer);
}
sub as {
	my ($varname, $value) = @_;
	Ekg2::echo("Zmieniles $varname na $value i ja o tym wiem!");
	
	
}

sub ekg2_autoreconnect { # autoreconnect. perl-side
        my ($sesja) = @_;
        Ekg2::echo("rozlaczono! $sesja, laczenie.");
#       $sess = Ekg2::session_find($sesja);
#       $sess->connect();
}

sub ekg2_dupa {
        my ($sesja) = @_;
        Ekg2::echo("rozlaczono! $sesja");
}


Ekg2::command_bind('perl_list', 'cmd_perl_list');
return 1;


Ekg2::debug("(perl) Hello ekg2!\n");
Ekg2::command_bind('test', 'cmd_test');
Ekg2::command_bind('iwil', 'cmd_evil');

Ekg2::timer_bind(1, 'cmd_timer');

Ekg2::handler_bind('protocol-disconnected', 'ekg2_autoreconnect');
Ekg2::handler_bind('protocol-disconnected', 'ekg2_dupa');


Ekg2::var_add_handler('dupa', 'temp', 'as');
Ekg2::var_add('dupus', 'as');


# cmd_timer_test(1, 0, 0); # testowanie obslugi timerow. 

return 1;
