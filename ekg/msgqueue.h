/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Piotr Domagalski <szalik@szalik.net>
 *                          Wojtek Kaniewski <wojtekka@irc.pl>
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

#ifndef __EKG_MSGQUEUE_H
#define __EKG_MSGQUEUE_H

#include <sys/types.h>
#include <time.h>

#include "char.h"
#include "dynstuff.h"

typedef struct {
	char *session;		/* do której sesji nale¿y */
	char *rcpts;		/* uidy odbiorców */
	char *message;		/* tre¶æ */
	char *seq;		/* numer sekwencyjny */
	time_t time;		/* czas wys³ania */
	int mark;
} msg_queue_t;

extern list_t msg_queue;

int msg_queue_add(const char *session, const char *rcpts, const char *message, const char *seq);
void msg_queue_remove(msg_queue_t *m);
int msg_queue_remove_uid(const char *uid);
int msg_queue_remove_seq(const char *seq);
void msg_queue_free();
int msg_queue_flush(const char *session);
int msg_queue_count();
int msg_queue_count_uid(const char *uid);
int msg_queue_read();
int msg_queue_write();

#endif /* __EKG_MSGQUEUE_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
