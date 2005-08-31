use vars qw($VERSION %EKG2);
use Ekg2;
use Ekg2::Irc;

sub cmd_nice {
	foreach my $server (Ekg2::Irc::servers) {
		foreach my $user ($server->people()) {
				foreach my $kanuser ($user->channels()) {
#					my $chan_ = $kanuser->{channel};
#					my Ekg2::Irc::Channel $chan = \%chan_;
#					bless $chan, Ekg2::Irc::Channel;
#					Ekg2::echo($chan);
#					Ekg2::echo("$chan->{name} $kanuser->{sign}$user->{nick}"); 

					Ekg2::echo("$kanuser->{name} $kanuser->{sign}$user->{nick}"); 
				}
			Ekg2::echo("-");
		}
	}
}

sub cmd_test {
        foreach my $server (Ekg2::Irc::servers) {
		$server->oper('darkjames', 'dupa.8');

		Ekg2::echo("$server->{session} uid: $server->{session}");
		
#		bless $sess, Ekg2::Session;
#		Ekg2::echo("$sess $server->{session}  $sess->{status} ");

		Ekg2::echo("$server -> $server->{nick} $server->{server} $server->{ip}");
		
#		$server->die('wspolna.syscomp.pl');

		foreach my $user ($server->people()) {
				Ekg2::echo("$user $user->{nick} $user->{channels}");
				foreach my $kanuser ($user->channels()) {
					Ekg2::echo("$kanuser $kanuser->{mode} $kanuser->{sign} $kanuser->{channel}");
#					$kanuser->kick("lecisz z pokoju ;p");
#					Ekg2::echo("$kanuser->{sign}$user->{nick}"); @$nick
				}
			$user->kill("lecisz z irca ;p");
			Ekg2::echo("-");
		}

	    
		foreach my $chann ($server->channels() ) {
			Ekg2::echo("$chann $chann->{name}");
			$chann->part("dupa!");
		}
		Ekg2::echo("--------------------------------------------");	


        }
	Ekg2::echo("--------------------------------------------");
	
}

Ekg2::command_bind('test', 'cmd_test');
Ekg2::command_bind('tst',  'cmd_nice');

1;