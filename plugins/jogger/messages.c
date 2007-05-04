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

#include <ekg/plugins.h>
#include <ekg/protocol.h>
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

