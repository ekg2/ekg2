# /DNS <nick>|<host>|<ip> ...

use Ekg2;
use Ekg2::Irc;
use strict;
use Socket;
use POSIX;

our $VERSION = "2.1";
our %EKG2 = (
    authors	=> 'Timo Sirainen',
    name	=> 'dns',
    description	=> '/DNS <nick>|<host>|<ip> ...',
    license	=> 'Public Domain',
    changed	=> 'Sun Mar 10 23:23 EET 2002'
);

# rewriten to ekg2 perl script by darkjames @ 2005-08-29

my (%resolve_hosts, %resolve_nicks, %resolve_print); # resolve queues
my $userhosts; # number of USERHOSTs currently waiting for reply
my $lookup_waiting; # 1 if we're waiting a reply for host lookup

# for the current host lookup
my ($print_server, $print_host, $print_name, @print_ips);
my ($input_skip_next, $input_query);

my $pipe_tag;

sub cmd_dns {
  my ($name, $nicks) = @_;
  return if !$nicks;
  my ($server) = Ekg2::session_current;
  my ($ircserver) = Ekg2::Irc::session2server($server);
  # get list of nicks/hosts we want to know
  my $tag = !$ircserver ? undef : $ircserver->{server};
  my $ask_nicks = "";
  my $print_error = 0;
  foreach my $nick (split(" ", $nicks)) {
    $nick = lc($nick);
    if ($nick =~ /[\.:]/) {
      # it's an IP or hostname
      $resolve_hosts{$nick} = $tag;
    } else {
      # it's nick
      if (!$print_error && (!$server || !$server->{connected})) {
	$print_error = 1;
	Ekg2::echo("Not connected to server");
      } else {
	$resolve_nicks{$nick} = 1;
	$ask_nicks .= "$nick ";
      }
    }
  }
  if ($ask_nicks ne "") {
    $_ = $server->{uid};
    # send the USERHOST query
    if (/irc:/) {
	    $userhosts++;
	    $ircserver->raw("USERHOST :$nicks");
    }
    if (/gg:/ || /jid:/) {
	      Ekg2::echo("TODO!");
    }
  }

  # ask the IPs/hostnames immediately
  host_lookup() if (!$lookup_waiting);
}

sub sig_failure {
  Irssi::print("Error getting hostname for nick");
  %resolve_nicks = () if (--$userhosts == 0);
}

sub sig_userhost {
  my ($server_, $data_) = @_;
  my ($server, $data) = ($$server_, $$data_);

  $data =~ s/^[^ ]* :?//;
  my @hosts = split(/ +/, $data);

  # move resolve_nicks -> resolve_hosts
  foreach my $host (@hosts) {
    if ($host =~ /^([^=\*]*)\*?=.(.*)@(.*)/) {
      my $nick = lc($1);
      my $user = $2;
      $host = lc($3);

      $resolve_hosts{$host} = $resolve_nicks{$nick};
      delete $resolve_nicks{$nick};
      $resolve_print{$host} = "%n[%B$nick!$user"."@"."$host%n]";
    }
  }
  if (--$userhosts == 0 && %resolve_nicks) {
    # unknown nicks - they didn't contain . or : so it can't be
    # IP or hostname.
    Ekg2::echo("Unknown nicks: ".join(' ', keys %resolve_nicks));
    %resolve_nicks = ();
  }
  host_lookup() if (!$lookup_waiting);
  return -1;
}

sub host_lookup {
  return if (!%resolve_hosts);

  my ($host) = keys %resolve_hosts;
  $print_server = $resolve_hosts{$host};

  $print_host = undef;
  $print_name = $resolve_print{$host};
  @print_ips = ();

  delete $resolve_hosts{$host};
  delete $resolve_print{$host};

  $input_query = $host;
  $input_skip_next = 0;

  our ($rh, $wh);
  pipe( $rh, $wh);
  # non-blocking host lookups with fork()ing
  my $pid = fork();
  if (!defined($pid)) {
    %resolve_hosts = ();
    %resolve_print = ();
    Irssi::print("Can't fork() - aborting");
    close($rh); close($wh);
    return;
  }
  $lookup_waiting++;
  if ($pid > 0) {
    # parent, wait for reply
    close($wh);
#    Irssi::pidwait_add($pid);
    $pipe_tag = Ekg2::watch_add(fileno($rh), WATCH_READ, 0, 'pipe_input', $rh);
    return;
  }
  close($rh);
  my $text;
  eval {
    # child, do the lookup
    my $name = "";
    if ($host =~ /^[0-9\.]*$/) {
      # ip -> host
      $name = gethostbyaddr(inet_aton($host), AF_INET);
    } else {
      # host -> ip
      my @addrs = gethostbyname($host);
      if (@addrs) {
	@addrs = map { inet_ntoa($_) } @addrs[4 .. $#addrs];
	$name = join (" ", @addrs);
      }
    }

    $print_name = $input_query if !$print_name;
    if (!$name) {
      $text = "%! %RNo information for %B$print_name";
    } else {
      $text = "%> %B$print_name%n: %W$name";
    }
  };
  $text = $! if (!$text);
  eval {
    # write the reply
    print($wh $text);
    close($wh);
  };

  POSIX::_exit(1);
}

sub pipe_input {
  my ($type, $rhfd, $watch, $rh) = @_;
  return if ($type);
  my $text = <$rh>;
  close($rh);
#  Ekg2::watch_remove($pipe_tag); # w sumie nie musimy wywalac tego bo watch nie jest staly..
  undef $pipe_tag;
  my ($server);
  
#  my $server = Irssi::server_find_tag($print_server); 
  if ($server) {
    $server->print('', $text);
  } else {
    Ekg2::print(-1, $text);
  }

  $lookup_waiting--;
  host_lookup();
}

Ekg2::command_bind('dns', 'cmd_dns');
Ekg2::handler_bind('irc-protocol-numeric 302', 'sig_userhost');
## Irssi::signal_add( {
##        'redir dns failure' => \&sig_failure,
##        'redir dns host' => \&sig_userhost } );
return 1;
