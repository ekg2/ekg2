/*
 *  Copyright (c) 2004 Maciej Pietrzak <maciej@pietrzak.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>

char* strfry(char *s)
{
	if (s) {
		int i,l,o;
		char c;
		l = strlen(s);
		for(i = 0; i < l; i++) {
			o = rand() % l;
			c = s[i];
			s[i] = s[o];
			s[o] = c;
		}
	}
	return s;
}

