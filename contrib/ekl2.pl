#!/usr/bin/perl -w

use POSIX;

while (<ARGV>) {
  chop;
  if (/^(chat|msg)send,/) {
    ($event, $uin, $nick, $time, $rest) = split /,/, $_, 5;
    if ($event eq "chatsend") {
      print "Rozmowa do " . $nick . "/" . $uin . " (" . strftime("%c", gmtime($time)) . ")\n";
    } else {
      print "Wiadomo¶æ do " . $nick . "/" . $uin . " (" . strftime("%c", gmtime($time)) . ")\n";
    }
    print $rest . "\n";
  } else {
      if (/^(chat|msg)recv,/) {
      ($event, $uin, $nick, $time, $timesent, $rest) = split /,/, $_, 6;
      if ($event eq "chatrecv") {
        print "Rozmowa od " . $nick . "/" . $uin . " (" . strftime("%c", gmtime($time)) .
                  "; wys³ana " . strftime("%c", gmtime($timesent)) . ")\n";
      } else {
        print "Wiadomosc od " . $nick . "/" . $uin . " (" . strftime("%c", gmtime($time)) .
                  "; wys³ana " . strftime("%c", gmtime($timesent)) . ")\n";
      }
      print $rest . "\n";
      } else {
         print; print "\n";
      }
  }
}
