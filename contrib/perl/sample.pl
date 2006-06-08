use Ekg2;

sub ekg2_handle_status {
	my ($session, $uid, $status, $desc) = @_;
	my $sesja = Ekg2::session_find($session);
        if ($sesja->{connected}) {
    	        Ekg2::echo("sesja ". $session . "po³±czona");
		Ekg2::echo("status: ". $sesja->{status});
		Ekg2::echo("ustawiam t± sesjê jako domy¶ln±");
#		sesja['default'] = "1"
	}
	else {
		ekg.echo("sesja ".$session . " nie jest po³±czona");
	}
	Ekg2::echo("Dosta³em status!");
	Ekg2::echo("Sesja : ".$session);
	Ekg2::echo("UID   : ".$uid);
	Ekg2::echo("Status: ".$status);

#    if desc:
#	ekg.echo("Opis  : "+desc)
#    sesja = ekg.getSession(session)
#    ekg.echo('Lista userów sesji: '+", ".join(sesja.users()))
#    user = sesja.getUser(uid)
#    if user.last_status:
#	ekg.echo(str(user.last_status))
#	stat, des = user.last_status
#	ekg.echo("Ostatni status: "+stat)
#	if user.last_status[1]:
#	    ekg.echo("Ostatni opis  : "+des)
#    else:
#	ekg.echo("Nie ma poprzedniego stanu - pewnie dopiero siê ³±czymy...")
#    ekg.echo("IP: "+user.ip)
#    ekg.echo("Grupy: "+", ".join(user.groups()))
#    if status == ekg.STATUS_AWAY:
#	ekg.echo("Chyba go nie ma...")
#    if status == ekg.STATUS_XA:
#	ekg.echo("Chyba bardzo go nie ma, to na grzyb mi taki status?. Po³ykam. *¶lurp*")
#	return -1;
#    return 1
}

sub handle_message {
	my ($session, $uid, $type, $text, $sent_time, $ign_level) = @_;
	
	Ekg2::debug("[test script] some debug\n");
	Ekg2::echo("Dosta³em wiadomo¶æ!");
	Ekg2::echo("Sesja : " . $session);
	Ekg2::echo("UID   : " . $uid);
        if ($class == MSGCLASS_MESSAGE) { Ekg2::echo("Typ   : msg"); }
	else { if ($class == MSGCLASS_CHAT ) { Ekg2::echo("Typ   : chat"); } }
#     Ekg2::echo("Czas  : "+time.strftime("%a, %d %b %Y %H:%M:%S %Z", time.gmtime(sent_time)))
     Ekg2::echo("Ign   : " . $ignore_level);
     Ekg2::echo("TxtLen: " . length($text));
     if (length($text) == 13) {
  	Ekg2::echo("Oj, ale pechowa liczba, nie odbieram");
	return 0;
     }
     return 1

}

sub handle_message_own {
    my ($session, $target, $text) = @_;
    Ekg2::debug("[test script] some debug\n");
    Ekg2::echo("Wysy³am wiadomo¶æ!");
    Ekg2::echo("Sesja : ". $session);
    Ekg2::echo("Target: ". $target);
    Ekg2::echo("TxtLen: ". length($text));
    return -(-1);
}

sub ekg2_message {
	my ($session, $uid, $rcpt, $text, $format, $send, $class) = @_;
	
	$tclass = $$class;
	
#	$$text   = "dupa4";
#	Ekg2::echo($$text);
#	$$class -= EKG_NO_THEMEBIT;

	if ($tclass & EKG_NO_THEMEBIT) { $tclass -= EKG_NO_THEMEBIT; };

# jesli moje to handle_msg_own();
	if ($tclass == EKG_MSGCLASS_SENT || $tclass == EKG_MSGCLASS_SENT_CHAT) { return handle_message_own($$session, $$rcpt, $$text); }
# jesli nie to  handle_msg();
	else                                                                   { return handle_message($$session, $$uid, $$tclass, $$text, $$send, 0); }
}

sub ekg2_handle_connect { 
	my ($session) = @_;
	
        Ekg2::echo("Po³±czono! Ale super! Mo¿na gadaæ!");
        Ekg2::echo("Sesja : ". $session);
#    if session[:3] == 'irc':
#	struct = time.gmtime()
#	if struct[3] >= 8 and struct[3] < 17:
#	    ekg.echo('£adnie to tak ircowaæ w pracy? ;)')
	
	my $sesja = Ekg2::session_find($session);
        if ($sesja->{connected}) {
		Ekg2::echo('Po³±czony!');
	}
        else {
		Ekg2::echo('W tym miejscu jeszcze nie po³±czony');
	}
#        Ekg2::echo('Lista userów sesji: '+", ".join(sesja.users()))
}

sub ekg2_handle_disconnect {
	my ($session) = @_;

        Ekg2::echo("£o, sesja " . $session . " nam pad³a");
        Ekg2::echo("Wysy³amy smsa ¿e nam cu¶ pad³o...");
}
sub ekg2_handle_keypress {
        my ($ch) = @_;
	if ($$ch == -1)  { return -1 }
	Ekg2::echo("Nacisnales #" .$$ch);
	$$ch = 41 if ($$ch == 40); # zamiana '(' na ')'
	
	return 0;
}

# Ekg2::handler_bind('protocol-disconnected', 'ekg2_handle_disconnect');
# Ekg2::handler_bind('protocol-status',       'ekg2_handle_status');
# Ekg2::handler_bind('protocol-message',      'ekg2_message');
# Ekg2::handler_bind('ui-keypress',           'ekg2_handle_keypress');

return 1;
