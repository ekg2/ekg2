.TH EKL2 1 "2 Marca 2003" 
.SH NAZWA
ekl2 \- Podgl±d historii Eksperymentalnego Klienta Gadu-Gadu.
.SH SK£ADNIA
.B ekl2 [
.BI OPCJE
.B ] [
.BI ¬RÓD£O
.B ]
.SH OPIS
.B ekl2
Jest to aplikacja umo¿liwiaj±ca podgl±d historii rozmów, tworzonej przez
Eksperymentalnego Klienta Gadu-Gadu.
.SH OPCJE
.TP
.BI \-d\ +format\ \-\-date\ +format\ 
Ustala format wy¶wietlanej daty (zobacz date(1)).
.TP
.BI \-\-location\ 
Ustala miejsce przechowywania plików dziennika (domy¶lnie ~/.gg/history).
.TP
.BI \-h\ \-\-help\ 
Wy¶wietla pomoc.
.TP
.BI \-L\ \-\-List\ 
Pokazuje listê dostêpnych logow. (z katalogu ~/.gg/history).
.TP
.BI \-l\ \-\-sessions\ [+num]\ [+ods]
Pokazuje listê sesji w podanym logu.
Nieobowi±zkowy parametr
.BI num\ 
to numer sesji, która ma zostaæ wy¶wietlona, a 
.BI ods\ 
to odstêp miêdzy rozmowami, w minutach (domy¶lnie: 60)
.TP
.BI \-n\ \-\-nocolor\ 
Bez kolorków.
.BI \-sh\ \-\-short\ 
simplified form.
.TP
.BI \-v\ \-\-version\ 
Prints script version.
.TP
.BI ¬RÓD£O\ 
numer gg, nick lub nazwa pliku z logami.
.SH "PATRZ TE¯"
.BR ekg (1).
