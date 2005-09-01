#!/bin/sh
#
# $Id$

make clean

rm -rf aclocal.m4 ekg2-config.h ekg2-config.h.in config.log config.status configure configure.lineno 
rm -rf install-sh autom4te.cache config.guess config.sub libtool ltmain.sh stamp-h1 depcomp missing mkinstalldirs
rm -rf compile config.rpath libltdl po

find . \( -name Makefile -o -name Makefile.in -o -name \*.la -o -name \*.a -o -name \*.so -o -name \*.o -type f \) -exec rm -f {} \;
find . \( -name \.deps -o -name \.libs -type d \) -exec rm -rf {} \;
