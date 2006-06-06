# Michal 'GiM' Spadlinski
#
# GG is fuj und bebe,
# simple autoresponder for ekg2
#
use Ekg2;

# your's gg uid
my ($myggnum) = "gg:XXXXXX";
# if delay between messages is greater then this, we send text again
my ($time) = 60*60*3; # 3 hours
my ($jabba) = "gim\@znajdzwgooglach.pl";
# message :)
my ($mesg) = "Wiadomo¶æ t± otrzymujesz automatycznie\n".
		"GG zmierza ku upadkowi, przeczytaj proszê dwa linki poni¿ej\n".
		"i skontaktuj siê ze mn± na jabberze pod: $jabba\n".
		"http://www.gadawski.pl/gg/ ORAZ bezbolesne przej¶cie na jabbera:\n".
		"http://www.nogui.yoyo.pl/tuty/GG2Jab/";

############## CODE FOLLOWS #########
my %persony;

sub ekg2_message {
  my ($tsession, $tuid, $trcpt, $ttext, $tformat, $tsend, $tclass) = @_;

  $session = $$tsession;
  $uid = $$tuid;
  $text = $$ttext;
  if ($session =~ $myggnum && !($uid =~ $myggnum))
  {
    if (time() - $persony{$uid} > $time)
    {
      $persony{$uid} = time();
      my ($wind) = Ekg2::window_current;
      my ($newwind) = Ekg2::window_findid(1);
      $newwind->switch() if ($newwind);

      my ($sc) = Ekg2::session_current;
      my ($gs) = Ekg2::session_find($session);
      $gs->set() if($gs);
      Ekg2::command("/msg $uid $mesg");
      $sc->set();
      $wind->switch();
    }
  }
  return 1;
}

Ekg2::handler_bind('protocol-message',      'ekg2_message');

return 1;

