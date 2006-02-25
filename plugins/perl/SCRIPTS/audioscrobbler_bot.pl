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

my $ignore_my_own = 0;

########################################################
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


### BEGIN code stoling from: Ewelinker [ewelirssi.pl] by Maciek 'japhy' Pasternacki
our %shifts = ( '`'=>'~', '~'=>'`',
            '1'=>'!', '!'=>'1', '2'=>'@', '@'=>'2',
            '3'=>'#', '#'=>'3', '4'=>'$', '$'=>'4',
            '5'=>'%', '%'=>'5', '6'=>'^', '^'=>'6',
            '7'=>'&', '&'=>'7', '8'=>'*', '*'=>'8',
            '9'=>'(', '('=>'9', '0'=>')', ')'=>'0',
            '-'=>'_', '_'=>'-', '='=>'+', '+'=>'=',
            '['=>'{', '{'=>'[', ']'=>'}', '}'=>']',
            ';'=>':', ':'=>';', "'"=>'"', '"'=>"'",
            ','=>'<', '<'=>',', '.'=>'>', '>'=>'.',
            '/'=>'?', '?'=>'/' );

srand ($$ ^ time());
sub maybe { my ( $prob, $rx, $subs, $rand, $const );
            if ( $#_==2 ) {
                ( $prob, $rx, $subs ) = @_;
                ( $rand, $const ) = ( 0, 1 );
            } elsif ( $#_==4 ) {
                ( $prob, $rx, $subs, $rand, $const ) = @_;
            } else { die "maybe: dupa\n"; }
            s/$rx/rand()<$prob?($subs)x(rand($rand)+$const):$&/ge; }

sub ewelize {
    $_ = lc shift;

    maybe .75, qr/n[±a][³l]em/, 'uem';
    maybe .75, qr/n[êe][³l]am/, 'uam';
    maybe .66, qr/±\B/, 'on';
    maybe .66, qr/±\b/, 'om';
    maybe .66, qr/sz/, 'sh';
    maybe .66, qr/cz/, 'ch';
    maybe .66, qr/ii\b/, 'ji';
    maybe .50, qr/o\b/, 'om';
    maybe .66, qr/(?<=\b[cdnt])o/, 'io';
    maybe .10, qr/(?<=[cdnt])o/, 'io';
    maybe .33, qr/u/, 'oo';
    maybe .10, qr/u/, 'o';
    maybe .60, qr/³/, 'u';
    maybe .50, qr/ê\B/, 'en';
    maybe .50, qr/ê\b/, 'em';
    maybe .50, qr/ó/, 'oo';
    maybe .50, qr/¿/, 'rz';
    maybe .33, qr/c(?=[^h])/, 's';
    maybe .33, qr/w/, 'ff';
    maybe .20, qr/ch/, 'f';
    maybe .10, qr/ch/, 'ff';

    maybe .66, qr/\!/, '!', 10, 2;
    maybe .50, qr/\?/, '?', 5, 2;

    maybe .75, qr/,/, "<K\x55\x52\x57A>";
    maybe .50, qr/(?<=\w{4})\b\.\B/, "<I\x43\x48\x55J>";
    maybe .50, qr/(?<=\w{4})\b\.\B/, "<W\x50\x49\x5a\x44U>";
    maybe .50, qr/(?<=\w{4})\b\.\B/, "<I\x44\x55\x50A>";
    maybe .05, qr/(?<=\w{4})\b\.\B/, ", w p\x69\x7a\x64e palec.";
    maybe .05, qr/(?<=\w{4})\b\.\B/, ', w zombek czesany.';
    maybe .05, qr/(?<=\w{4})\b\.\B/, ", w \x63\x68uja wafla.";
    maybe .05, qr/(?<=\w{4})\b\.\B/, ", w morde za\x6a\x65\x62\x61\x6eego je¿a.";
    maybe .33, qr/K\x55\x52\x57A/, 'HY', 5, 3;

    s/\<((HY)+)\>/lc ", $1,"/eg;
    s/\<K\x55\x52\x57A\>/, \x6b\x75\x72wa,/g;
    s/\<I\x43\x48\x55J\>/, i \x63\x68uj./g;
    s/\<W\x50\x49\x5a\x44U\>/, w p\x69\x7a\x64u./g;
    s/\<I\x44\x55\x50A\>/, i dupa./g;

    maybe .10, qr/\x6b\x75\x72wa/, 'kuffa';
    maybe .10, qr/\x6b\x75\x72wa/, 'kuwa';
    maybe .33, qr/\x63\x68uj/, '\x68uj';
    maybe .15, qr/c?\x68uj/, 'ciul';

    s/\W/(defined($shifts{"$&"})&&(rand()<.10))?$shifts{$&}:$&/eg;

    s/(\w)(\w)/rand()<.05?"$2$1":$&/eg;
    s/./rand()<.66?lc$&:uc$&/eg;

    return $_;
}
### END code stoling from: Ewelinker [ewelirssi.pl] by Maciek 'japhy' Pasternacki

#bindings for ekg2
sub handle_message {
	my ($session, $uid, $rcpt, $text, $format, $send, $class) = @_;

	$tclass = $$class;
	$txt = $$text;
	$tclass -= EKG_NO_THEMEBIT if ($tclass & EKG_NO_THEMEBIT);

	return 1 if ($ignore_my_own && ($tclass | EKG_MSGCLASS_SENT || $tclass | EKG_MSGCLASS_SENT_CHAT));

	return 1 if (! ($$session =~ /^irc:/));

	# we bind protocol-message not irc-protocol-message
	# which can contain some ugly characters and cause errors
	# I'm currently disconnected from network so cannot check
	# which characters are allowed.
	if ($txt =~ /(cogra|cogr4|c0gr4) ([a-zA-Z0-9_\-^]*)/)
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
		$juzer = $2;
		$z = cogra($2);
		if ($z) {
			srand ($$ ^ time());
			if ($1 =~ /(cogr4|c0gr4)/) { $z = ewelize($z); }
			if ($1 =~ /c0gr4/) {
        	               $z =~ s/./sprintf "\003%02d$&\003", rand(15)/eg;
                	       $z =~ s/,/,,/g;
			}
			print($wh $$uid." $juzer playz: $z");
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
