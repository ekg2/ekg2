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

#ifndef __IRC_PIPL_H
#define __IRC_PIPL_H

#include "irc.h"

people_t *irc_find_person(list_t p, char *nick);
channel_t *irc_find_channel(list_t p, char *channame);
people_chan_t *irc_find_person_chan(list_t p, char *channame);

/* person joins channel */
int irc_add_person(session_t *s, irc_private_t *j, char *nick, char *channame);
/* we join channel */
int irc_add_people(session_t *s, irc_private_t *j, char *names, char *channame);

/* someone made /part */
int irc_del_person_channel(session_t *s, irc_private_t *j, char *nick, char *chan);
/* someone made /quit */
int irc_del_person(session_t *s, irc_private_t *j, char *nick,
		char *wholenick, char *reason, int doprint);
/* we've made /part */
int irc_del_channel(session_t *s, irc_private_t *j, char *name);

/* add channel to our list of channels */
channel_t *irc_add_channel(session_t *s, irc_private_t *j, char *name,
		window_t *win);

int irc_nick_change(session_t *s, irc_private_t *j, char *old, char *new);
int irc_nick_prefix(irc_private_t *j, people_chan_t *ch, int irc_color);
int irc_color_in_contacts(char *modes, int mode, userlist_t *ul);

/* clean up */
int irc_free_people(session_t *s, irc_private_t *j);

#endif
