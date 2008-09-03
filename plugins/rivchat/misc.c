/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *		  2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <stdint.h>

#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

uint32_t rivchat_fix32(uint32_t x) {
	return x;
/*
	return (uint32_t)
		(((x & (uint32_t) 0x000000ffU) << 24) |
		((x & (uint32_t) 0x0000ff00U) << 8) |
		((x & (uint32_t) 0x00ff0000U) >> 8) |
		((x & (uint32_t) 0xff000000U) >> 24));
*/
}

