/*
 *  (C) Copyright 2004 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
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


#ifndef __EKG_PLUGINS_IRC_AUTOACTS_H
#define __EKG_PLUGINS_IRC_AUTOACTS_H

#include <ekg/sessions.h>

enum { IRC_REJOIN_KICK=0, IRC_REJOIN_CONNECT };

typedef struct {
	session_t *s;
	char *nick;
	char *kickedby;
	char *chan;
} irc_onkick_handler_t;

int irc_autorejoin(session_t *s, int when, char *chan);
void irc_autorejoin_timer(int type, void *data);
int irc_onkick_handler(session_t *s, irc_onkick_handler_t *data);


#endif
