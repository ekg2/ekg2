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

#ifndef __EKG_PLUGINS_IRC_IRC_H
#define __EKG_PLUGINS_IRC_IRC_H

#include <ekg/plugins.h>
#include <ekg/sessions.h>

enum { USERMODES=0, CHANMODES, _005_PREFIX, _005_CHANTYPES,
	_005_CHANMODES, _005_MODES, SERVOPTS };
extern char *sopt_keys[];

typedef struct {
	int fd;				/* connection's fd */
	char *server;			/* server name */
	int port;			/* complicated, huh ? ;> */
	int connecting;			/* are we connecting _now_ ? */

	char *nick;			/* guess again ? ;> */
	char *host_ident;		/* ident+host */
	char *obuf;			/* output buffer */
	int obuf_len;			/* size of above */

	list_t people;			/* list of people_t */
	list_t channels;		/* list of people_chan_t */
	list_t hilights;

	char *sopt[SERVOPTS];		/* just a few options from
					 * www.irc.org/tech_docs/005.html
					 * server's response */
} irc_private_t;

#define SOP(x) (j->sopt[x])

/* data for private->people */
typedef struct {
	char *nick;
	list_t channels;
} people_t;

/* data for private->channels */
typedef struct {
	char *name;
	int mode;
	char *topic;
	window_t *window;
	list_t onchan;
} channel_t;

/* data for private->people->channels */
typedef struct {
	int mode; /* +v +o */
	char sign[2];
	channel_t *chanp;
} people_chan_t;

#define irc_private(s) ((irc_private_t*) session_private_get(s))

/* DO NOT TOUCH THIS! */
#define IRC4 "irc:"
#define IRC3 "irc"

plugin_t irc_plugin;

typedef struct {
	session_t *session;
} irc_handler_data_t;

void irc_handle_reconnect(int type, void *data);
static void irc_handle_disconnect(session_t *s);

#ifdef __GNU__
int irc_write(irc_private_t *j, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
#else
int irc_write(irc_private_t *j, const char *format, ...);
#endif

#endif /* __EKG_PLUGINS_IRC_IRC_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
