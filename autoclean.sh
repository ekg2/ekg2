#!/bin/sh
make clean
rm -rf aclocal.m4 config.h* config.log config.status configure stamp-h1 ekg/Makefile ekg/Makefile.in ekg/.deps depcomp missing mkinstalldirs install-sh autom4te.cache config.guess config.sub libtool ltmain.sh
find \( -name Makefile -o -name Makefile.in -o -name '*.la' -o -name '*.a' -o -name '*.so' -o -name '*.o' \) -exec rm -f {} \;
find \( -name .deps -o -name .libs \) -type d -exec rm -rf {} \;

