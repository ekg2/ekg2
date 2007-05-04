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

#include <stdlib.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

	/* jogger.c */
session_t *jogger_session_find_uid(session_t *s, const char *uid);

/* Thanks to sparrow, we need to pack big&stinky iconv into this small plugin */

const char *utf_jogger_text[] = {
	"Do Twojego joggera został dodany komentarz",		/* [0] url (#eid[ / Texti*])\n----------------\n */
	"Pojawil sie nowy komentarz do wpisu",			/* [1] as above */

	"Wpis",							/* [2] url + below */
	"został zmodyfikowany",					/* [3] */
	"nie istnieje",						/* n [4] */

	"Dodano wpis:",						/* [5] url */
	"Dodałeś komentarz do wpisu",				/* [6] url */
	
	"Śledzenie wpisu",					/* [7] url + below */
	"zostało wyłączone",					/* [8] */
	"zostało włączone",					/* [9] */

	"Brak uprawnień do śledzenia tego wpisu",		/* [10] */
	"Wpis nie był śledzony",				/* [11] */
};

char *jogger_text[12];

void localize_texts() {
	int i;
	void *p = ekg_convert_string_init("UTF-8", NULL, NULL);

	for (i = 0; i < 12; i++) {
		char *s = ekg_convert_string_p(utf_jogger_text[i], p);

		if (!s)
			s = xstrdup(utf_jogger_text[i]);
		jogger_text[i] = s;
	}
	ekg_convert_string_destroy(p);
}

void free_texts() {
	int i;

	for (i = 0; i < 12; i++)
		xfree(jogger_text[i]);
}

QUERY(jogger_msghandler) {
	const char *suid	= *(va_arg(ap, const char **));
	char **uid		= va_arg(ap, char **);
								{ char ***UNUSED(rcpts)	= va_arg(ap, char ***); }
	char **msg		= va_arg(ap, char **);
								{ uint32_t *UNUSED(format) = va_arg(ap, uint32_t *); }
	const time_t sent	= *(va_arg(ap, const time_t *));
	const int class		= *(va_arg(ap, const int *));

	session_t *s		= session_find(suid);
	session_t *js;

	if (!s || !(js = jogger_session_find_uid(s, *uid)))
		return 0;

	if (class == EKG_MSGCLASS_MESSAGE || class == EKG_MSGCLASS_CHAT) { /* incoming */
			/* (un)subscription acks */
		if (!xstrncmp(*msg, jogger_text[7], xstrlen(jogger_text[7]))) {
			char *tmp;

			if ((tmp = xstrstr(*msg, jogger_text[8]))) {
				*(tmp-1) = '\0';
				print("jogger_unsubscribed", session_name(js), *msg+xstrlen(jogger_text[7])+1);
				*(tmp-1) = ' ';
				return -1; /* cut off */
			} else if ((tmp = xstrstr(*msg, jogger_text[9]))) {
				*(tmp-1) = '\0';
				print("jogger_subscribed", session_name(js), *msg+xstrlen(jogger_text[7])+1);
				*(tmp-1) = ' ';
				return -1; /* cut off */
			}
		}
	} else if (class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) { /* outgoing */
	}

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

COMMAND(jogger_subscribe) {
	const char *uid		= get_uid(session, target);
	int n;

	if (!uid)
		uid = target; /* try also without jogger: prefix */
	else
		uid += 7;

	if (*uid == '#')
		uid++;
	if (!(n = atoi(uid))) {
		printq("invalid_uid");
		return -1;
	}

	return command_exec_format("jogger:", session, 0, "#%c%d", (!xstrcmp(name, "subscribe") ? '+' : '-'), n);
}
