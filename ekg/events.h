/* $Id$ */

/*
 *  (C) Copyright 2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#ifndef __EKG_EVENTS_H
#define __EKG_EVENTS_H

#include "char.h"
#include "dynstuff.h"
#include "plugins.h"
#include "stuff.h"

typedef struct event {
	unsigned int id; /* identyficator */
	CHAR_T *name;     /* name of the event */
	CHAR_T *target;   /* uid(s), alias(es), group(s) */
	CHAR_T *action;   /* action to do */
	int prio;	/* priority of this event */
} event_t;

extern list_t events;
extern CHAR_T **events_all; /* it may be help for tab complete */

int event_add(const CHAR_T *name, int prio, const CHAR_T *target, const CHAR_T *action, int quiet);
int event_remove(unsigned int id, int quiet);
int events_list(int id, int quiet);
event_t *event_find_id(unsigned int id);
event_t *event_find(const CHAR_T *name, const CHAR_T *target);

void events_add_handler(CHAR_T *name, void *function);
int event_check(const char *session, const char *name, const char *uid, const char *data);
void event_free();
int events_init();

TIMER(ekg_day_timer);

QUERY(event_protocol_message);
QUERY(event_avail);
QUERY(event_away);
QUERY(event_na);
QUERY(event_online);
QUERY(event_descr);

int event_target_check(CHAR_T *buf);

#endif /* __EKG_EVENTS_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
