#!/bin/sh

# historia:
# v1 (7.03.2002) grywalny skrypt, spelniajacy jako tako swoja funkcje
#       powstal w oczekiwaniu na pewna osobe na gg
# v1.1 (8.03.2002) poprawa drukowania nietypowych znakow, np. {}%*#$
#       po wyslaniu ascii-rozyczki przez gg, w logach wyraznie wymagala ona naprawy
# v1.2 (9.03.2002) poprawa wydajnosci. skrypt jest juz w miare szybki
# v1.3 (28.05.2002) obsluga trybow logowania 1 i 2, paprawka dotyczaca ip
#       obsluga wiadomosci wielolinijkowych
# 1.4 (11.2002) kompletnie od nowa napisany skrypt, z kilkoma bajerkami

_ver=1.4

if [ $# -eq 0 ]; then
	echo "co chcesz ze mn± zrobiæ? (u¿yj '$0 -h')";
	exit;
fi

# colors
Cn=$'\x1B[0;38m';
Cr=$'\x1B[0;31m'; # red
Cg=$'\x1B[0;32m'; # green
Ct=$'\x1B[0;33m'; # cyan
Cy=$'\x1B[0;36m'; # yellow

# logs dir
dir="$HOME/.gg/history";
# log file fields regexps
fmr="\([a-z]*recv\)";	# message sent/received. chat/msg ignored
fms="\([a-z]*send\)";	# message sent/received. chat/msg ignored
fuin="\([0-9]*\)"; # uin number
fnick="\([^,]*\)"; # uin number
fdate="\([0-9]\{10\}\)"; # uin number
msg="\"\?\(.*[^\"]\)\"\?"; # message

_getnick () {
	while read n; do
	if [ -d "$dir" ]; then
		buf="`sed -n "/$n/s/;.*//p" "$dir/../userlist"`";
	else
		buf="`sed -n "/${n:-^$}/s/;.*//p" "$(dirname $dir)/userlist"`";
	fi
	echo -e "${n}\t${buf}";
	done < /dev/stdin
}

sessprint=1	# internal, shitty code

# replacing to human readable
# hmr: \1 - sent/recv, \2 - uin, \3 - nick, \4 - send date, \5 - recv date, \6 - message
# hms: no recv date, \5 - message

while [ $# -gt 0 ]; do
	case $1 in
		--location )
			dir="$2";
			shift;
		;;
		-n | --nocolor )
			unset Cn Cr Cg Ct Cy
		;;
		-l | --sessions )
			sess="yes";
			if [ "x${2#?}" = "x${2#+}" ]; then
                                sessnum="${2#+}";
                                shift;
                        fi
			if [ "x${2#?}" = "x${2#+}" ]; then
				sessint="${2#+}";
				shift;
			else
				sessint="60";
			fi
			sessint="`expr $sessint \* 60`";
			sessprint=0;
		;;
		-L | --List )
			list="yes";
		;;
		-h | --help )
cat <<END
Sk³adnia: $0 [OPCJE] [¬RÓD£O]
Wy¶wietla logi ekg

-d 	--dateformat +FORMAT	format wy¶wietlania daty, zobacz date(1)
	--location	miejsce sk³adowania logów, 
			domy¶lnie ~/.gg/history
-h	--help		pomoc
-l	--sessions [+num] [+ods]	wy¶wietl listê sesji
			nieobowi±zkowy parametr num to numer
			sesji, która ma zostaæ wy¶wietlona
			ods to odstêp miêdzy rozmowami, w minutach
			(domy¶lnie: 60)
-L 	--List		lista dostêpnych logów
-n	--nocolor	brak kolorów
-sh 	--short		szybka, uproszczona forma
-v	--version	informacja o wersji
¬RÓD£O			numer gg, nick lub nazwa pliku z logami

Raporty o b³êdach: ekg-users@list.ziew.org lub tri10o@bsod.org
END
			exit;
		;;
		-d | --dateformat ) # date format
			dateformat="+$2";
			shift;
		;;
		-sh | --short ) # short mode
			hmr="${Ct}\3${Cn} \6";
			hms="${Cy}-->${Cn} \5";
			mode=0;
		;;
		-v | --version )
cat <<END
ekl2.sh $_ver
triteno <tri10o@bsod.org>
END
			exit;
;;
		* ) # file name
			fle="$1";
		;;
	esac
	shift
done

# logs location, setting fle variable
if [ ! -d "$dir" ]; then
	if [ -f "$dir" -a "`basename $dir`" != "history" ]; then
		fle="$dir"; # if someone will set exact file
	else
		if [ -f "$dir" -a "`basename $dir`" = "history" ]; then # occurs, when log variable is set to 1
			printonlynick="$fle";
			fle="$dir/history";
		else
			echo "katalog $dir nie istnieje, u¿yj parametru --location aby bezpo¶rednio okre¶liæ miejsce przechowywania logów."; 
			exit 1; 
		fi
	fi
fi

if [ ! -f "$fle" -a -z "$list" ]; then
	list="hidden_yes";
fi

_userslist() {
if [ -d "$dir" ]; then
	# dir jest katalogiem
	ls "$dir" | _getnick
else 
	if [ -f "$dir" ]; then
		# dir jest plikiem zbiorczym
		sed -n "/^\([^,]*\),${fuin},${fnick},${fdate},/s/^\([^,]*\),${fuin},.*/\2/p" "$dir" | sort | uniq | _getnick;
	fi
fi
exit;
}

if [ "${list}" = "yes" ]; then
	_userslist;
fi

if [ "${list}" = "hidden_yes" ]; then
	if [ -d "$dir" ]; then
		fle="$dir/`_userslist | grep "$fle" | cut -f 1`";
	else
		fle="$dir";
	fi
	if [ ! -f "$fle" ]; then
		echo -e "Nie mogê okre¶liæ pliku z logami. Upewnij siê,\n¿e podajesz w³a¶ciw± nazwê pliku/nick/numerek osoby";
		exit 1;
	fi
fi

	# default mode
if [ -z ${hmr} ]; then
	hmr="\4 \3 ${Ct} \6";
	hms="\4 - ${Cy} \5";
	dateformat="+%H:%M %d.%m.%Y"; # see date(1)
	mode=1;
fi

disccount=0;

_outputclass() {
case $1 in
	0 )
	cat -
	;;
	1 )
	if [ -z "${Cn}" ]; then # check if we can display colors
		flds="date nick msg";
	else 
		flds="date nick clr msg";
	fi
	while read $flds; do
		if [ "$sess" = "yes" ]; then
			[ -z "$lastdate" ] && lastdate="$date";
			if [ "`expr $date - $lastdate`" -gt "$sessint" ]; then
				let disccount++;
				if [ ! -z "$sessnum" -a "$sessnum" -eq "$disccount" ]||[ -z "$sessnum" ]; then
					echo -e "$disccount.\tRozmowa z `date --date "01/01/1970 ${date}sec" "${dateformat}"`. Zaczêta przez $nick.";
				fi
				[ "$sessnum" = "$disccount" ] &&  sessprint=1 || sessprint=0
			fi 2>/dev/null
			lastdate="$date";
		fi
		if [ "$sessprint" -eq 1 ]; then
	     		echo "${clr}.--`date --date "01/01/1970 ${date}sec" "${dateformat}"`- ${nick} ---- - -   - ";
			echo "|${Cn} $msg";
			echo "${clr}'--------${Cn}";
		fi
	done < /dev/stdin
	;;
esac
}

( sed -n "/,${printonlynick:-.*},/{
       /^$fmr/ s/^${fmr},${fuin},${fnick},${fdate},${fdate},${msg}$/${hmr}/;
       /^$fms/ s/^${fms},${fuin},${fnick},${fdate},${msg}$/${hms}/;p;}" "$fle" ) | _outputclass $mode
