/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@o2.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#ifndef __EKG_LOG_H
#define __EKG_LOG_H

#include <sys/types.h>
#include <time.h>

#include "char.h"
#include "dynstuff.h"

struct last {
	int type;		/* 0 - przychodz±ca, 1 - wychodz±ca */
	char *uid;		/* od kogo, lub do kogo przy wysy³anych */
	time_t time;		/* czas */
	time_t sent_time;	/* czas wys³ania wiadomo¶ci przychodz±cej */
	unsigned char *message;	/* wiadomo¶æ */
};

#ifndef EKG2_WIN32_NOFUNCTION
extern list_t lasts;

void last_add(int type, const char *uid, time_t t, time_t st, const char *msg);
void last_del(const char *uid);
int last_count(const char *uid);
void last_free();

CHAR_T *xml_uescape(const CHAR_T *text);
CHAR_T *xml_escape(const char *text);
char *log_escape(const char *str);

#endif

#endif /* __EKG_LOG_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
