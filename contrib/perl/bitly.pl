use Ekg2;
use LWP::UserAgent;
use URI::Escape;
use strict;
use vars qw($VERSION %EKG2);

our $VERSION = "1.5";
our %EKG2    = (
    authors     => 'Jakub Łaszczyński',
    contact     => 'jakub.laszczynski@gmail.com',
    name        => 'bitly for ekg2',
    description => 'shortens urls for incoming messages',
    license     => 'GNU GPL',
    changed     => '2012-01-31'
);

Ekg2::variable_add( 'bitly_login',   '' );
Ekg2::variable_add( 'bitly_apikey',  '' );
Ekg2::variable_add( 'bitly_timeout', '5' );
Ekg2::variable_add( 'bitly_length',  '30' );

my $bitly_login  = Ekg2::variable_find('bitly_login')->{value};
my $bitly_apikey = Ekg2::variable_find('bitly_apikey')->{value};
my $length       = Ekg2::variable_find('bitly_length')->{value};
my $timeout      = Ekg2::variable_find('bitly_timeout')->{value};
my $debug        = Ekg2::variable_find('debug')->{value};

Ekg2::handler_bind( 'protocol-message-received', 'shortenline' );
Ekg2::handler_bind( 'variable-changed',          'variable_changed' );

sub bitly() {
    my $lwp = LWP::UserAgent->new;
    $lwp->timeout($timeout);

    my $url    = shift;
    my $window = shift;

    $url = uri_escape($url);

    my $api_src =
        'http://api.bit.ly/v3/shorten?login='
      . $bitly_login
      . '&apiKey='
      . $bitly_apikey
      . '&format=txt&longUrl='
      . $url;
    my $response = $lwp->get($api_src);

    Ekg2::debug("BITLY API->$api_src\n") if ($debug);

    if ( $response->is_success ) {
        my $url_bitly = $response->decoded_content;
        $url_bitly =~ s/\n//g;
        Ekg2::debug("BITLY SHORTENED->$url_bitly\n") if ($debug);
        Ekg2::Window::print( $window, "Shortened url: $url_bitly" );
    }
    else {
        my $errstr = $response->status_line;
        Ekg2::debug(
            "An error occurred while making the HTTP Request: $errstr\n")
          if ($debug);
        Ekg2::Window::print( $window,
            "An error occurred while making the HTTP Request: $errstr\n" );
    }
}

sub shortenline() {
    my ( $session, $uid, $msg, $our, $n1, $n2 ) = map $$_, @_;

    Ekg2::debug("SESSION-> $session\n")     if ($debug);
    Ekg2::debug("UUID-> $uid\n")            if ($debug);
    Ekg2::debug("MSG-> $msg\n")             if ($debug);
    Ekg2::debug("OUR-> $our\n")             if ($debug);
    Ekg2::debug("N1-> $n1\n")               if ($debug);
    Ekg2::debug("N2-> $n2\n")               if ($debug);
    Ekg2::debug("APIKEY-> $bitly_apikey\n") if ($debug);
    Ekg2::debug("LOGIN-> $bitly_login\n")   if ($debug);
    Ekg2::debug("LENGTH-> $length\n")       if ($debug);
    Ekg2::debug("TIMEOUT-> $timeout\n")     if ($debug);

    if ( $our =~ m!https?://[^ ]+! ) {
        Ekg2::debug("DETECTED URL-> $our\n") if ($debug);

        my $url    = $our;
        my $window = Ekg2::window_find($uid);
        Ekg2::debug( "URL-> URLLENGTH: " . length($url) . " LENGTH: $length\n" )
          if ($debug);
        if ( length($url) > $length ) {
            Ekg2::debug("URL-> LONGER\n") if ($debug);
            &bitly( $url, $window );
        }
    }
}

sub variable_changed() {
    my ($name) = ${ $_[0] };
    my $val;

    if ( $name eq 'bitly_login' ) {
        $bitly_login = Ekg2::variable_find('bitly_login')->{value};
        Ekg2::debug("BITLY LOGIN CHANGED-> $bitly_login\n") if ($debug);
    }
    elsif ( $name eq 'bitly_apikey' ) {
        $bitly_apikey = Ekg2::variable_find('bitly_apikey')->{value};
        Ekg2::debug("BITLY APIKEY CHANGED-> $bitly_apikey\n") if ($debug);
    }
    elsif ( $name eq 'bitly_length' ) {
        $length = Ekg2::variable_find('bitly_length')->{value};
        Ekg2::debug("BITLY LENGTH CHANGED-> $length\n") if ($debug);
    }
    elsif ( $name eq 'bitly_timeout' ) {
        $timeout = Ekg2::variable_find('bitly_timeout')->{value};
        Ekg2::debug("BITLY TIMEOUT CHANGED-> $timeout\n") if ($debug);
    }
}

return 1;
