/*
 *  Copyright (c) 2003 Piotr Domagalski <szalik@szalik.net>
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

#include <sys/types.h>

size_t strlcat(char *dst, const char *src, size_t size)
{
	register size_t i, j;
	size_t left, dlen;

	for (i = 0; i < size && dst[i]; i++)
		continue;

	dlen = i;
	left = size - i;

	for (j = 0; left > j + 1 && src[j]; j++, i++)
		dst[i] = src[j];

	if (left)
		dst[i] = 0;

	while (src[j])
		j++;

	return dlen + j;
}
