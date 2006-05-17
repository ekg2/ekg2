# YDP Dictionary bot based on socket, Michal 'GiM' Spadlinski
# requires sockydpdict :)
# 
# SSS - short, small and simple 
#

use IO::Socket;

our $VERSION = "0.1";
our %EKG2 = (
    authors     => 'Michal Spadlinski',
    name        => 'slownikobot',
    description => 'YDP slownik BOT',
    license     => 'GPL',
    changed     => 'Wed May 17 20:49:47 CEST 2006'
);


sub cotozaslowo
{
	my $fname = "/tmp/ydpsock";
	my $answer;
	my $flags = 0;
	my $maxlen = 4096;
	$_ = shift;

	socket (CLIENT, PF_UNIX, SOCK_STREAM, 0);
	connect (CLIENT, sockaddr_un($fname))
    		or return 0;


	defined (send(CLIENT, "$_\n", $flags))
		or return 0;

	recv (CLIENT, $answer, $maxlen, $flags)
		or return 0;
	return $answer;
}

#bindings for ekg2
sub handle_message {
	#query_emit(NULL, "irc-protocol-message", &(sid), &(j->nick), &__msg, &isour, &xosd_to_us, &xosd_is_priv, &uid_full);
	my ($session, $uid, $msg, $our, $n1, $n2, $n3) = @_;

	$txt = $$msg;
	return 1 if (! ($$session =~ /^irc:/));
	# we don't respond to our own queries
	return 1 if ($$our);

	if ($txt =~ /(!ang) ([a-zA-Z ]*)/)
	{
		$z = cotozaslowo($2);
		$z =~ s/\n{2,}/\n/g;
		Ekg2::command("/msg $$uid $z");
		return 0;
	}
	return 1
}


Ekg2::handler_bind('irc-protocol-message', 'handle_message');

1;

