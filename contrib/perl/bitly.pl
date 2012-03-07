use Ekg2;
use LWP::UserAgent;
use URI::Escape;
use strict;
use vars qw($VERSION %EKG2);

our $VERSION = "1.10";
our %EKG2    = (
    authors     => 'Jakub Łaszczyński',
    contact     => 'jakub.laszczynski@gmail.com',
    name        => 'bitly for ekg2',
    description => 'shortens urls for incoming messages',
    license     => 'GNU GPL',
    changed     => '2012-02-24'
);

Ekg2::variable_add( 'bitly_login',   '' );
Ekg2::variable_add( 'bitly_apikey',  '' );
Ekg2::variable_add( 'bitly_timeout', '5' );
Ekg2::variable_add( 'bitly_length',  '30' );
Ekg2::variable_add( 'bitly_pagesizelimit',  '102400' );
Ekg2::variable_add( 'bitly_debug',  '0' );

my $bitly_login  = Ekg2::variable_find('bitly_login')->{value};
my $bitly_apikey = Ekg2::variable_find('bitly_apikey')->{value};
my $length       = Ekg2::variable_find('bitly_length')->{value};
my $timeout      = Ekg2::variable_find('bitly_timeout')->{value};
my $pagesizelimit= Ekg2::variable_find('bitly_pagesizelimit')->{value};
my $debug        = Ekg2::variable_find('bitly_debug')->{value};

Ekg2::handler_bind( 'protocol-message-received', 'shortenline' );
Ekg2::handler_bind( 'variable-changed',          'variable_changed' );

welcome(@_);

sub bitly() {
    my $lwp = LWP::UserAgent->new;
    $lwp->timeout($timeout);
    $lwp->max_size($pagesizelimit);

    my $url    = shift;
    my $window = shift;

    $url = uri_escape($url);

    my $api_src  = 'https://api-ssl.bit.ly/v3/shorten?login=' . $bitly_login . '&apiKey=' . $bitly_apikey . '&format=txt&longUrl=' . $url;
    my $response = $lwp->get($api_src);

    Ekg2::debug("BITLY API->$api_src\n") if ($debug);

    if ( $response->is_success ) {
        my $url_bitly = $response->decoded_content;
        $url_bitly =~ s/\n//g;
        Ekg2::debug("BITLY SHORTENED->$url_bitly\n") if ($debug);
        my $get_title    = $lwp->get($url_bitly);
        if ( $get_title->is_success ) {
            $get_title = $get_title->decoded_content;
            $get_title =~ s/(\s+|\n)/ /g;
            $get_title =~ /<title>(.*)<\/title>/;
            Ekg2::Window::print( $window, "Page title: $1" ) if $1;
        }
        Ekg2::Window::print( $window, "Shortened url: $url_bitly" );
    }
    else {
        my $errstr = $response->status_line;
        Ekg2::debug("An error occurred while making the HTTP Request: $errstr\n") if ($debug);
        Ekg2::Window::print( $window, "An error occurred while making the HTTP Request: $errstr\n" );
    }
}

sub shortenline() {
    my ( $session, $uid, $rcpts, $ptext ) = map $$_, @_;

    Ekg2::debug("SESSION-> $session\n")     if ($debug);
    Ekg2::debug("UID-> $uid\n")             if ($debug);
    Ekg2::debug("RCPTS-> $rcpts\n")         if ($debug);
    Ekg2::debug("PTEXT-> $ptext\n")         if ($debug);
    Ekg2::debug("APIKEY-> $bitly_apikey\n") if ($debug);
    Ekg2::debug("LOGIN-> $bitly_login\n")   if ($debug);
    Ekg2::debug("LENGTH-> $length\n")       if ($debug);
    Ekg2::debug("TIMEOUT-> $timeout\n")     if ($debug);

    Ekg2::command_exec( '', Ekg2::session_find($session), "/query $uid" ) unless Ekg2::window_find($uid);
    my $window = Ekg2::window_find($uid);

    while ( $ptext =~ m|(http.*?://([^\s)\"](?!ttp:))+)|g ) {
        my $url = $&;
        Ekg2::debug("DETECTED URL-> $url\n") if ($debug);

        Ekg2::debug( "URL-> URLLENGTH: " . length($url) . " LENGTH: $length\n" ) if ($debug);
        if ( length($url) > $length ) {
            Ekg2::debug("URL-> LONGER\n") if ($debug);
            &bitly( $url, $window );
        }
    }
}

sub variable_changed() {
    my ($name) = ${ $_[0] };

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
    elsif ( $name eq 'bitly_pagesizelimit' ) {
        $pagesizelimit = Ekg2::variable_find('bitly_pagesizelimit')->{value};
        Ekg2::debug("BITLY PAGESIZELIMIT CHANGED-> $pagesizelimit\n") if ($debug);
    }
    elsif ( $name eq 'bitly_debug' || $name eq 'debug' ) {
        $debug = Ekg2::variable_find('debug')->{value} && Ekg2::variable_find('bitly_debug')->{value};
        Ekg2::debug("DEBUG VALUE CHANGED-> $debug\n");
    }
}

sub welcome(@) {
    return if Ekg2::variable_find('bitly_login')->{value};
    Ekg2::print( 1, "bitly.pl for EKG2.\n" );
    Ekg2::print( 1, "This is automatic URL shortener using bitly API.\n" );
    Ekg2::print( 1, "To use it obtain API key by registering on bit.ly website,\n" );
    Ekg2::print( 1, "and then set proper bitly_ variables in ekg2 config.\n\n" );
    Ekg2::print( 1, "After such setup all received URLs which are longer then.\n" );
    Ekg2::print( 1, "bitly_length will be shortened." );
}

return 1;
