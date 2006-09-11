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

#include "dynstuff.h"
#include "plugins.h"
#include "stuff.h"

typedef struct event {
	unsigned int id; /* identyficator */
	char *name;     /* name of the event */
	char *target;   /* uid(s), alias(es), group(s) */
	char *action;   /* action to do */
	int prio;	/* priority of this event */
} event_t;

extern list_t events;
extern char **events_all; /* it may be help for tab complete */

int event_add(const char *name, int prio, const char *target, const char *action, int quiet);

int event_check(const char *session, const char *name, const char *uid, const char *data);
void event_free();
int events_init();

int event_target_check(char *buf);

#endif /* __EKG_EVENTS_H */


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
