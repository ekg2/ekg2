# Audioscrobbler BOT
# Michal 'GiM' Spadlinski
# 12-12-2005
#


our $VERSION = "0.1";
our %EKG2 = (
    authors     => 'Michal Spadlinski',
    name        => 'audioscrobbler',
    description => 'Audioscrobbler BOT',
    license     => 'GPL',
    changed     => 'Mon Dec 12 22:42:15 CET 2005'
);

use vars qw($VERSION %EKG2);
use Ekg2;
use Ekg2::Irc;

require XML::Parser;
require HTTP::Request;
require LWP::UserAgent;

my ($grab, $pid, $watch, $rh, $wh);
my @table;

# Simple audioscrobbler XML parser
sub cogra
{
	$_ = shift;
	$_ =~ s/[\%\\\/\..]//g;
	
	@table = ();
	$grab = 0;

	my $url = "http://ws.audioscrobbler.com/1.0/user/$_/recenttracks.xml";
	my $ua = LWP::UserAgent->new(agent => "GiM_z_hacka_perla_browsera_magiczna/6.66");
	my $resp = $ua->get($url);
	return 0 if (! $resp->is_success);

	my $parser = new XML::Parser(ErrorContext => 2);

	$parser->setHandlers(Char => \&char_handler,
		Start => \&start_handler);

	$parser->parse($resp->content);

	return $table[13]?($table[5]." - ".$table[2]." - ".$table[13]):0;
}

sub start_handler
{
	shift; $_ = shift;
	$grab++ if (/^track/);
}

sub char_handler
{
	shift; $_ = shift;
	push @table,$_ if $grab == 1;
}

sub cmd_ziom {
	my $sess = Ekg2::session_current;
	my $win = Ekg2::window_current;
}

#bindings for ekg2
sub handle_message {
	my ($session, $uid, $rcpt, $text, $format, $send, $class) = @_;

	$tclass = $$class;
	$txt = $$text;
	$tclass -= EKG_NO_THEMEBIT if ($tclass & EKG_NO_THEMEBIT);

	return 1 if ($tclass == EKG_MSGCLASS_SENT || $tclass == EKG_MSGCLASS_SENT_CHAT);

	return 1 if (! ($$session =~ /^irc:/));

	# we bind protocol-message not irc-protocol-message
	# which can contain some ugly characters and cause errors
	# I'm currently disconnected from network so cannot check
	# which characters are allowed.
	if ($txt =~ /cogra ([a-zA-Z0-9_\-^]*)/)
	{
		pipe($rh, $wh);
		$pid = fork();
		if (!defined($pid))
		{
			Ekg2::echo("fork failed");
			close($rh); close($wh);
			return 1;
		}
		if ($pid > 0)
		{
    			close($wh);
    			$watch = Ekg2::watch_add(fileno($rh), WATCH_READ, 0, 'pipe_watcher', $rh);
			return;
		}
		close($rh);
		$z = cogra($1);
		if ($z) {
			print($wh $$uid." $z");
		} else {
			print($wh 0);
		}
		close($wh);
  		exit(1);

	}
	return 1
}

sub pipe_watcher {
  my ($type, $rhfd, $watch, $rh) = @_;
  my ($sess) = Ekg2::session_current;
  my ($wind) = Ekg2::window_current;
  
  return if ($type);

  my $text = <$rh>;
  close($rh);
  
  undef $watch;
  
  if ($text) {
    Ekg2::command("/msg $text");
  } else {
    Ekg2::echo("user's not listening!");
  }
}

Ekg2::handler_bind('protocol-message', 'handle_message');

1;
