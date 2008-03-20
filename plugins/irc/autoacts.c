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
 * Here we check if we were kicked (by checking @a nick) and if yes, than if we want to rejoin on kick (<i>REJOIN</i> && <i>REJOIN_TIME</i>)<br>
 * Than after time specified by <i>REJOIN_TIME</i> we try to rejoin
 *
 * @sa irc_autorejoin()		- for rejoin function
 * @sa irc_autorejoin_timer()	- for rejoin timer.
 *
 * @todo	We don't check if @a nick and @a chan is full uid.. It's I think correct.. However can be faulty.
 *
 * @param ap 1st param: <i>(char *) </i><b>session</b> 	- session uid
 * 	  ap 2nd param: <i>(char *) </i><b>nick</b>	- full uid of kicked person (irc:nickname)
 * 	  ap 3rd param: <i>(char *) </i><b>chan</b>	- full uid of channel where kick event happen.
 * 	  ap 4th param: <i>(char *) </i><b>kickedby</b>	- full uid who kicked.
 * @param data NULL
 *
 * @return 	1 - If no session, no irc session, or no private struct.<br>
 * 		2 - If we are not interested in autorejoining	[either smb else was kicked (not us) or <i>REJOIN</i> was not set]<br>
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

/**
 * irc_autorejoin_timer()
 *
 * Timer handler for auto-rejoining<br>
 * Added by irc_autorejoin()
 * It just execute irc_autorejoin() with params...
 *
 * @todo Remove some struct info from irc_onkick_handler_t? Here we use only channelname and session..
 * @todo Check session with session_find_ptr() ?
 *
 * @param data - irc_onkick_handler_t * struct with data inited by irc_autorejoin()
 *
 * @return -1 [TEMPORARY HANDLER]
 */

static TIMER(irc_autorejoin_timer) {
	irc_onkick_handler_t *d = data;
	if (type == 1) {
		xfree(d->nick);
		xfree(d->kickedby);
		xfree(d->chan);
		xfree(d);
		return 0;
	}

	debug("irc_autorejoin_timer() rejoining to: %s\n", d->chan);
	irc_autorejoin(d->s, IRC_REJOIN_KICK, (d->chan)+4);
	return -1;
}

/**
 * irc_autorejoin()
 *
 * Try to rejoin.
 *
 * @todo We double check "REJOIN" if IRC_REJOIN_KICK
 *
 * @param s	- session
 * @param when	- type of rejoining:<br>
 * 			- <i>IRC_REJOIN_CONNECT</i> 	When we want to rejoin to all channels we had opened for example after <i>/reconnect</i>
 * 			- <i>IRC_REJOIN_KICK</i>	When we want to rejoin to given channel (@a chan)
 * @param chan	- if @a when == IRC_REJOIN_KICK than it specify to which channel we want to rejoin after kick.
 *
 * @return 	 0 - if we send JOIN commands to ircd...<br>
 * 		-1 - If smth went wrong.
 */

int irc_autorejoin(session_t *s, int when, char *chan) {
	irc_private_t *j;
	string_t st;
	window_t *w;
	char *chanprefix;
	int rejoin;

#if 1	/* there's no need of doing it, already checked by irc_onkick_handler() or if it goes through irc_c_init() it's even better. */
	if (!s || !(j = s->priv) || (s->plugin != &irc_plugin))
    		return -1;
#endif
	chanprefix = SOP(_005_CHANTYPES);
	rejoin = session_int_get(s, "REJOIN");

	if (!(rejoin&(1<<(when))))
		return -1;

	switch (when) {
		case IRC_REJOIN_CONNECT:
			st = string_init(NULL);
			for (w = windows; w; w = w->next) {
				if (!w->target || w->session != s)			/* check if sessions match and has w->target */
					continue;

				if (valid_plugin_uid(s->plugin, w->target) != 1)	/* check if window is correct for irc: */
					continue;
				
				if (!xstrchr(chanprefix, (w->target)[4]))		/* check if this is channel.. */
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
