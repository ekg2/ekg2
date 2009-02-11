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

#ifndef __EKG_PLUGINS_IRC_INPUT_H
#define __EKG_PLUGINS_IRC_INPUT_H

#include <ekg/sessions.h>
#include "irc.h"

char *irc_ircoldcolstr_juststrip(session_t *sess, char *inp);
char *irc_ircoldcolstr_to_ekgcolstr(session_t *s, char *str, int strip);
char *irc_ircoldcolstr_to_ekgcolstr_nf(session_t *sess, char *str, int strip);
char *ctcp_parser(session_t *sess, int ispriv, char *sender, char *recp, char *s);


#define CTCP_COMMAND(x) static int x(session_t *s, irc_private_t *j, int number, \
		char *ctcp, char *sender, char*idhost, char *targ)
typedef int (*CTCP_Cmd) (session_t *s, irc_private_t *j, int number,
		char *ctcp, char *sender, char *idhost, char *targ);

typedef struct {
	char *name;
	int handled;
} ctcp_t;

enum { CTCP_ACTION=1, CTCP_DCC, CTCP_SED, CTCP_FINGER, CTCP_VERSION, CTCP_SOURCE,
	CTCP_USERINFO, CTCP_CLIENTINFO, CTCP_PING, CTCP_TIME, CTCP_ERRMSG };

static const ctcp_t ctcps[] = {
	{ "ACTION",	1 },
	{ "DCC",	0 },
	{ "SED",	0 },

	{ "FINGER",	1 },
	{ "VERSION",	1 },
	{ "SOURCE",	1 },
	{ "USERINFO",	1 },
	{ "CLIENTINFO", 1 },
	{ "PING",	1 },
	{ "TIME",	1 },
	{ "ERRMSG",	1 },
	{ NULL,		0 }
};

#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
