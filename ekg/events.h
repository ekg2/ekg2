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

#include "dynstuff.h"
#include "plugins.h"

typedef struct event {
	unsigned int id; /* identyficator */
        char *name;     /* name of the event */
        char *target;   /* uid(s), alias(es), group(s) */
        char *action;   /* action to do */
	int prio;	/* priority of this event */
}event_t;

list_t events;
char **events_all; /* it may be help for tab complete */

int event_add(const char *name, int prio, const char *target, const char *action, int quiet);
int event_remove(unsigned int id, int quiet);
int events_list(int id, int quiet);
event_t *event_find_id(unsigned int id);
event_t *event_find(const char *name, const char *target);

void events_add_handler(char *name, void *function);
int event_check(const char *session, const char *name, const char *target, const char *data);
void event_free();
int events_init();

int event_protocol_message(void *data, va_list ap);
int event_avail(void *data, va_list ap);
int event_away(void *data, va_list ap);
int event_na(void *data, va_list ap);
int event_online(void *data, va_list ap);
int event_descr(void *data, va_list ap);

int event_target_check (char *buf);

