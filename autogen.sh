#!/bin/sh
# Convenience wrapper around autoreconf and optionally configure.
#
# Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
#           2011 Marcin Owsiany <porridge@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License Version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.

set -e

: ${AUTORECONF:=autoreconf}

if test "$*"; then
	ARGS="$*"
else
	if test -f config.log ; then
		ARGS=`grep '^  \$ \./configure ' config.log | sed 's/^  \$ \.\/configure //' 2> /dev/null`
	fi
fi

echo "Running ${AUTORECONF}..."
$AUTORECONF --install --verbose

test x$NOCONFIGURE = x && echo "Running ./configure $ARGS" && ./configure $ARGS
