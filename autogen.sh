#!/bin/sh

if test "$*"; then
	ARGS="$*"
else
	test -f config.log && ARGS=`grep '^  \$ \./configure ' config.log | sed 's/^  \$ \.\/configure //' 2> /dev/null`
fi

echo "Running aclocal..."
aclocal -I m4

echo "Running autoheader..."
autoheader

echo "Running libtoolize..."
libtoolize --ltdl

echo "Running automake..."
automake --foreign --add-missing

echo "Running autoconf..."
autoconf

test x$NOCONFIGURE = x && echo "Running ./configure $ARGS" && ./configure $ARGS


