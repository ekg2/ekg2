#!/bin/bash
# getekg.sh - skrypt ¶ci±gaj±cy zawsze najnowsz± wersjê EKG.
# Autor: Arim.
# Ostatnie modyfikacja: 28/05/2002 14:28

if [ "$1" == "" ]; then 
	echo "Podaj zmienne (\"--prefix=/usr\" itp.) dla configure'a!"; 
	exit 1; 
fi
echo "Wchodzê do katalogu tymczasowego."
cd /tmp
echo -n "¦ci±gam najnowsz± wersjê."
 if [ ! -x "`which wget`" ]; then \
    if [ ! -x "`which curl`" ]; then \
        if [ ! -x "`which lynx`" ]; then \
            echo "Brak mo¿liwo¶ci ¶ci±gniêcia pliku (chyba, ¿e pod Xami)";
            exit 1;
        else 
	    lynx -dump http://dev.null.pl/ekg/ekg-current.tar.gz | tar zxvf -
	fi # lynx
    else
	curl -s http://dev.null.pl/ekg/ekg-current.tar.gz | tar xzf -
    fi #curl
 else
    wget -q -O - http://dev.null.pl/ekg/ekg-current.tar.gz | tar xzf -
 fi #wget
echo -n " Rozpakowane.";
cd `ls -1 | grep ekg-`
echo -n " Konfigurujê..."
./configure $* > /dev/null
echo " Instalujê..."
make install > /dev/null
cd ..
rm -r `ls -1 | grep ekg-`
echo "Zrobione."
