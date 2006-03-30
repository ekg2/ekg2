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

#ifndef __EKG_PLUGINS_IRC_IRC_H
#define __EKG_PLUGINS_IRC_IRC_H

#define DOT(a,x,y,z,error) \
	print_window("__status", z, 0, a, session_name(z), x, y->hostname, y->address, \
			itoa(y->port < 0 ? \
				session_int_get(z, "port") < 0 ? DEFPORT : session_int_get(z, "port") : y->port), \
			itoa(y->family), error ? strerror(error) : "")

#include <ekg/plugins.h>
#include <ekg/sessions.h>

enum { USERMODES=0, CHANMODES, _005_PREFIX, _005_CHANTYPES,
	_005_CHANMODES, _005_MODES, _005_CHANLIMIT, _005_NICKLEN, SERVOPTS };
extern char *sopt_keys[];

typedef struct {
	int fd;				/* connection's fd */
	int connecting;			/* are we connecting _now_ ? */
	int autoreconnecting;		/* are we in reconnecting mode now? */
	int resolving;			/* count of resolver threads. */
	list_t bindlist, bindtmplist;
	list_t connlist, conntmplist;

	watch_t *recv_watch;

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
	char *realname;
	char *host, *ident;
	list_t channels;
} people_t;

/* data for private->channels */
typedef struct {
	char		*name;
	int		syncmode;
	struct timeval	syncstart;
	int		mode;
	char		*topic, *topicby, *mode_str;
	window_t	*window;
	list_t		onchan;
	list_t		banlist;
	/* needed ?
	list_t exclist;
	list_t invlist; */
	list_t          acclist;
} channel_t;

/* data for private->people->channels */
typedef struct {
	int mode; /* bitfield  */
	char sign[2];
	channel_t *chanp;
} people_chan_t;

typedef struct {
	char *mask;
	int frmask;
/*      char frmask[100]; */
} access_t;

/* structure needed by resolver */
typedef struct {
	session_t *session;
	char *hostname;
	char *address;
	int port;
	int family;
} connector_t;

typedef struct {
	char *session;
	list_t *plist;
} irc_resolver_t;

#define irc_private(s) ((irc_private_t*) session_private_get(s))

/* DO NOT TOUCH THIS! */
#define IRC4 "irc:"
#define IRC3 "irc"

plugin_t irc_plugin;

void irc_handle_disconnect(session_t *s, const char *reason, int type);
COMMAND(irc_command_disconnect);

/* checks if name is in format irc:something
 * checkcon is one of:
 *   name is               channel   |  nick 
 *   IRC_GC_CHAN 	-  channame  |  NULL
 *   IRC_GC_NOT_CHAN	-  NULL      | nickname
 *   IRC_GC_ANY		-  name if it's in proper format [irc:something]
 */
enum { IRC_GC_CHAN=0, IRC_GC_NOT_CHAN, IRC_GC_ANY };
char *irc_getchan_int(session_t *s, const char *name, int checkchan);
char *irc_getchan(session_t *s, const char **params, const CHAR_T *name,
      char ***v, int pr, int checkchan);

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
