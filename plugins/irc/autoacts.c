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
#include <ekg/win32.h>

#include <ekg/debug.h>
#include <ekg/stuff.h>
#include <ekg/sessions.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "irc.h"
#include "autoacts.h"

extern plugin_t irc_plugin; 
static TIMER(irc_autorejoin_timer);

/**
 * irc_onkick_handler()
 *
 * Handler for: <i>IRC_KICK</i><br>
 *
 * Here we check if we were kicked (by checking @a nick) and if yes, than if we want to rejoin on kick (REJOIN && REJOIN_TIME)<br>
 * Than after time specified by REJOIN_TIME we try to rejoin
 *
 * @sa irc_autorejoin()		- for rejoin function
 * @sa irc_autorejoin_timer()	- for rejoin timer.
 *
 * @todo	We don't check if @a nick and @a chan is full uid.. It's I think correct.. However can be faulty.
 *
 * @param	ap 1st param: <i>(char *) </i><b>session</b> 	- session uid
 * 		ap 2nd param: <i>(char *) </i><b>nick</b>	- full uid of kicked person (irc:nickname)
 * 		ap 3rd param: <i>(char *) </i><b>chan</b>	- full uid of channel where kick event happen.
 * 		ap 4th param: <i>(char *) </i><b>kickedby</b>	- full uid who kicked.
 *
 * @return 	1 - If no session, no irc session, or no private struct.
 * 		2 - If we are not interested in autorejoining	[either smb else was kicked (not us) or REJOIN was not set]
 * 		3 - If we'll try to autorejoin
 */

QUERY(irc_onkick_handler) {
	char *session	= *va_arg(ap, char **);
	char *nick	= *va_arg(ap, char **);
	char *chan	= *va_arg(ap, char **);
	char *kickedby	= *va_arg(ap, char **);

	session_t     *s = session_find(session);
	irc_private_t *j;

	if (!s || !(j = s->priv) || s->plugin != &irc_plugin)
		return 1;

	if (!xstrcmp(j->nick, nick+4)) {
		int rejoin = session_int_get(s, "REJOIN");

		if (rejoin < 0)
			return 2;

		if (rejoin&(1<<(IRC_REJOIN_KICK))) {
			int rejoin_time = session_int_get(s, "REJOIN_TIME");

			/* if it's negative value, fix it. cause we set REJOIN... */
			if (rejoin_time < 0) 
				rejoin_time = 0;

			/* if we don't want to wait after kick, we do it here. not in next ekg_loop() */
			if (rejoin_time == 0) {
				irc_autorejoin(s, IRC_REJOIN_KICK, chan+4);
				return 3;
			}

			irc_onkick_handler_t *data = xmalloc(sizeof(irc_onkick_handler_t));

			data->s		= s;
			data->nick 	= xstrdup(nick);
			data->kickedby 	= xstrdup(kickedby);
			data->chan 	= xstrdup(chan);

			timer_add(&irc_plugin, NULL, rejoin_time, 0, irc_autorejoin_timer, data);
			return 3;
		}
	} 
	
	return 2;
}

static TIMER(irc_autorejoin_timer)
{
	irc_onkick_handler_t *d = data;
	if (type == 1) {
		xfree(d->nick);
		xfree(d->kickedby);
		xfree(d->chan);
		xfree(d);
		return 0;
	}

	debug("wykonujê timer %d %s\n", type, (d->chan)+4);
	irc_autorejoin(d->s, IRC_REJOIN_KICK, (d->chan)+4);
	return -1; /* timer tymczasowy */
}

int irc_autorejoin(session_t *s, int when, char *chan)
{
	irc_private_t *j;
	list_t l;
	string_t st;
	window_t *w;
	char *chanprefix;
	int rejoin;

	if (!s || !(j = s->priv) || (s->plugin != &irc_plugin))
    		return -1;

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
				if (!xstrchr(chanprefix, (w->target)[4]))
					continue;

				if (st->len)
					string_append_c(st, ',');
				if ((w->target)[4] == '!') {
					string_append_c(st, '!');
					string_append(st, w->target + 10);
				} else {
					string_append(st, w->target + 4);
				}
			}
			if (st->len) 
				watch_write(j->send_watch, "JOIN %s\r\n", st->str);
			string_free(st, 1);
			break;
		case IRC_REJOIN_KICK:
			watch_write(j->send_watch, "JOIN %s\r\n", chan);
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
