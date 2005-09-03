/*
 *  (C) Copyright 2004-2005 Michal 'GiM' Spadlinski <gim at skrzynka dot pl>
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
	if (type == 1)
		return;

	debug("wykonujê timer %d %s\n", type, (data->chan+4));
	irc_autorejoin(data->s, IRC_REJOIN_KICK, (data->chan)+4);

	xfree(data->nick);
	xfree(data->kickedby);
	xfree(data->chan);
	xfree(data);
}

int irc_autorejoin(session_t *s, int when, char *chan)
{
	irc_private_t *j;
	list_t l;
	string_t st;
	window_t *w;
	char *chanprefix;
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
			st = string_init(NULL);
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
				if (st->len) {
					string_append_c(st, ',');
					if ((w->target)[4] == '!')
					{
						string_append_c(st, '!');
						string_append(st, w->target + 10);
					} else
						string_append(st, w->target + 4);
				} else {
					if ((w->target)[4] == '!') {
						string_append_c(st, '!');
						string_append(st, w->target + 10);
					} else
						string_append(st, w->target + 4);
				}
			}
			if (st->len) 
				irc_write(j, "JOIN %s\r\n", st->str);
			string_free(st, 1);
			break;
		case IRC_REJOIN_KICK:
			irc_write(j, "JOIN %s\r\n", chan);
			break;
		default:
			return -1;
	}
	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
