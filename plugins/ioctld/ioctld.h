/* $Id$ */

/*
 *  (C) Copyright 2002  Pawel Maziarz <drg@go2.pl>
 *			Wojtek Kaniewski <wojtekka@irc.pl>
 *			Robert J. Wozny <speedy@ziew.org>
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

#ifndef __EKG_IOCTLD_IOCTLD_H
#define __EKG_IOCTLD_IOCTLD_H

#define IOCTLD_MAX_ITEMS 	50
#define IOCTLD_MAX_DELAY 	2000000
#define IOCTLD_DEFAULT_DELAY 	100000

struct action_data {
	int act;
	int value[IOCTLD_MAX_ITEMS];
	int delay[IOCTLD_MAX_ITEMS];
};

enum action_type {
	ACT_BLINK_LEDS = 1,
	ACT_BEEPS_SPK = 2
};

int blink_leds(int *flag, int *delay);
int beeps_spk(int *tone, int *delay);

#endif /* __EKG_IOCTLD_IOCTLD_H */
