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

#include <ekg/stuff.h>
#include <ekg/sessions.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "irc.h"
#include "misc.h"
#include "autoacts.h"

extern plugin_t irc_plugin; 

int irc_onkick_handler(session_t *s, irc_onkick_handler_t *data)
{
	irc_private_t *j = irc_private(s);
	int rejoin;

	if (!xstrcmp(j->nick, (data->nick)+4))
	{
		rejoin = session_int_get(s, "REJOIN");
		if (rejoin&(1<<(IRC_REJOIN_KICK))) {
			timer_add(&irc_plugin, NULL, 
					session_int_get(s, "REJOIN_TIME"),
					0, irc_autorejoin_timer, data);
			return 0;
		}
	} 
	
	xfree(data->nick);
	xfree(data->kickedby);
	xfree(data->chan);
	xfree(data);
	
	return 0;
}

void irc_autorejoin_timer(int type, void *d)
{
	irc_onkick_handler_t *data = d;

	debug("wykonujê timer\n");
	irc_autorejoin(data->s, IRC_REJOIN_KICK, (data->chan)+4);
/*
	xfree(data->nick);
	xfree(data->kickedby);
	xfree(data->chan);
	xfree(data);*/
}

int irc_autorejoin(session_t *s, int when, char *chan)
{
	irc_private_t *j;
	list_t l;
	window_t *w;
	char *chanprefix, *str,*tmp;
	int rejoin;

	if (!s) return -1;
	j = irc_private(s);
	if (!j) return -1;

	chanprefix = SOP(_005_CHANTYPES);
	rejoin = session_int_get(s, "REJOIN");

	if (!(rejoin&(1<<(when))))
		return 0;

	switch (when)
	{
		case IRC_REJOIN_CONNECT:
			tmp = str = NULL;
			for (l = windows; l; l = l->next) {
				w = l->data;
				if (!(w->target))
					continue;
				if (session_compare(w->session, s)) 
					continue;
				if (xstrncasecmp(IRC4, w->target, 4)) 
					continue;
				if (!xstrchr(chanprefix, (w->target)[4]))
					continue;
				if (str) {
					tmp = str;
					str = saprintf("%s,%s", tmp,
							(w->target)+4);
					xfree(tmp);
				} else str = xstrdup((w->target)+4);
			}
			if (str) {
				irc_write(j, "JOIN %s\r\n", str);
				xfree(str);
			}
			break;
		case IRC_REJOIN_KICK:
			irc_write(j, "JOIN %s\r\n", chan);
			break;
		default:
			return -1;
	}
	return 0;
}
