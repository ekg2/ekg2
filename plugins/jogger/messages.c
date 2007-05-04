/*
 *  (C) Copyright 2007	Michał Górny & EKG2 authors
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

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

	/* jogger.c */
session_t *jogger_session_find_uid(session_t *s, const char *uid);


QUERY(jogger_msghandler) {
	const char *suid	= *(va_arg(ap, const char **));
	const char *uid		= *(va_arg(ap, const char **));
								{ char ***UNUSED(rcpts)	= va_arg(ap, char ***); }
	const char *msg		= *(va_arg(ap, const char **));
								{ uint32_t *UNUSED(format) = va_arg(ap, uint32_t *); }
	const time_t sent	= *(va_arg(ap, const time_t *));
	const int class		= *(va_arg(ap, const int *));

	session_t *s		= session_find(suid);
	session_t *js;

	if (!s || !(js = jogger_session_find_uid(s, uid)))
		return 0;

		/* 1) comment match */
		/* 2) own jogger comment match */
		/* 3) outgoing comment match */
		/* 4) ack match */

	return 0;
}

COMMAND(jogger_msg) {
	const int is_inline	= (*name == '\0');
	const char *uid 	= get_uid(session, target);
	session_t *js		= session_find(session_get(session, "used_session"));
	const char *juid	= session_get(session, "used_uid");
	int n;

	if (!uid || !js || !juid) {
		printq("invalid_session");	/* XXX, unprepared session? */
		return -1;
	}
	uid += 7; /* skip jogger: */

	if (*uid == '\0') { /* redirect message to jogger uid */
		if (is_inline)
			return command_exec(juid, js, params[0], 0);
		else
			return command_exec_format(NULL, js, 0, "/%s \"%s\" %s", name, juid, params[1]);
	}
	if (*uid == '#')
		uid++;

	if (!(n = atoi(uid))) {
		printq("invalid_uid");
		return -1;
	}

		/* post as comment-reply */
	if (is_inline)
		return command_exec_format(juid, js, 0, "#%d %s", n, params[0]);
	else
		return command_exec_format(NULL, js, 0, "/%s \"%s\" #%d %s", name, juid, n, params[1]);
}

