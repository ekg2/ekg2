#!/usr/bin/perl -w

########
## Autor: Mateusz Greszta (theKnight) <thedarkknight@o2.pl>
## Opis : uruchom w katalogu z logami,
##        na wejscie skieruj userlist (mozna tez podac nazwe jako parametr);
##        na wyjsciu masz spis polecen tworzacych powiazania symboliczne;
## Cel  : utworzenie powiazan symbolicznych:
##        nick (bez dziwnych znakow) -> plik z logami (nazwa == uin)
##            (w nicku wszystko co nie jest litera/cyfra jest zastepowane '_'
##             ale bez powtorzen -- "xx ! yy" zamienia sie na "xx_yy")
## Uwaga: dostaniesz lacza tylko do istniejacych plikow.
##        polecenie 'ln -sf ...' nadpisze istniejacy plik z nazwa nicka
##            (o ile taki bedzie istnial).
##        nic innego nie bedzie kasowane.
##        aby skrypt automatycznie uruchamal polecenia 'ln ...':
##            znajdz i usun "#exec#" ponizej.
##        a jak nie chcesz nic wyswietlac to zakomentuj "print;".
## PS   : aha -- trzeba oczywiscie miec w systemie 'ln' ;)
########

# hash [ uin -> nick ]
my %hash;

while (<>) {
    # ominiecie 'pustych lini'
    next if /^\s*$/;
    @_ = split /;/;
    my ($nick, $uin) = ($_[3], $_[6]);
    # usuniecie \r\n (czy jakos tak)
    $uin =~ s/\s//g;
    # wszysko co nie jest /a-zA-Z0-9/
    # jest zamieniane na _ (bez powtorzen)
    $nick =~ tr/a-zA-Z0-9_/_/cs;
    $hash{$uin} = $nick;
    $hash{"$uin.gz"} = $nick;
}


opendir DIR, '.';

foreach $file (readdir DIR) {
    if ($hash{$file}) {
        $_ = "ln -sf $file $hash{$file}\n" ;  # tu sie laczy (:
        print;  # tu sie pisze  :
#exec#        `$_`;  # tu sie robi  :)
    }
}
