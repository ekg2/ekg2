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

#include "ekg2.h"

guint32 rivchat_fix32(guint32 x) {
	return x;
/*
	return (guint32)
		(((x & (guint32) 0x000000ffU) << 24) |
		((x & (guint32) 0x0000ff00U) << 8) |
		((x & (guint32) 0x00ff0000U) >> 8) |
		((x & (guint32) 0xff000000U) >> 24));
*/
}

