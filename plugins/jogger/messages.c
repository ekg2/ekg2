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
#include <string.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/queries.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

	/* jogger.c */
session_t *jogger_session_find_uid(session_t *s, const char *uid);

/* Thanks to sparrow, we need to pack big&stinky iconv into this small plugin */

const char *utf_jogger_text[] = {
	"Do Twojego joggera został dodany komentarz",		/* [0] url (#eid[ / Texti*])\n----------------\n */
	"Pojawil sie nowy komentarz do wpisu",			/* [1] as above */

	"Wpis",							/* [2] */
	"został zmodyfikowany",					/* url [3] */
	"nie istnieje",						/* n [4] */

	"Dodano wpis:",						/* [5] url */
	"Dodałeś komentarz do wpisu",				/* [6] url */
	
	"Śledzenie wpisu",					/* [7] url + below */
	"zostało wyłączone",					/* [8] */
	"zostało włączone",					/* [9] */

	"Brak uprawnień do śledzenia tego wpisu",		/* [10] */
	"Wpis nie był śledzony",				/* [11] */
	"Do śledzonego joggera został dodany nowy wpis:",	/* [12] url (#eid[ / Texti*]) */
};

#define JOGGER_TEXT_MAX 13

char *jogger_text[JOGGER_TEXT_MAX];

void localize_texts() {
	int i;
	void *p = ekg_convert_string_init("UTF-8", NULL, NULL);

	for (i = 0; i < JOGGER_TEXT_MAX; i++) {
		char *s = ekg_convert_string_p(utf_jogger_text[i], p);

		if (!s)
			s = xstrdup(utf_jogger_text[i]);
		jogger_text[i] = s;
	}
	ekg_convert_string_destroy(p);
}

void free_texts() {
	int i;

	for (i = 0; i < JOGGER_TEXT_MAX; i++)
		xfree(jogger_text[i]);
}

QUERY(jogger_msghandler) {
	const char *suid	= *(va_arg(ap, const char **));
	const char *uid		= *(va_arg(ap, char **));
	char **rcpts		= *(va_arg(ap, char ***));
	const char *msg		= *(va_arg(ap, char **));
								{ uint32_t **UNUSED(format) = va_arg(ap, uint32_t **); }
	time_t sent		= *(va_arg(ap, const time_t *));
	const int class		= *(va_arg(ap, const int *));
	const char *seq		= *(va_arg(ap, char**));
	const int dobeep	= *(va_arg(ap, int*));
	const int secure	= *(va_arg(ap, int*));

	session_t *s		= session_find(suid);
	session_t *js;

	if (class == EKG_MSGCLASS_MESSAGE || class == EKG_MSGCLASS_CHAT) { /* incoming */
		if (!s || !(js = jogger_session_find_uid(s, uid)))
			return 0;

		const char *owncf = session_get(js, "own_commentformat");
		int found = 0;

		if (!xstrncmp(msg, jogger_text[0], xstrlen(jogger_text[0])))
			found = 1; /* own jogger comment */
		else if (owncf && !xstrncmp(msg, owncf, xstrlen(owncf)))
			found = 2; /* as above, but with custom text */
		else if (!xstrncmp(msg, jogger_text[1], xstrlen(jogger_text[1])))
			found = 3; /* other jogger comment */
		else if (!xstrncmp(msg, jogger_text[12], xstrlen(jogger_text[12])))
			found = 4; /* new jogger entry */

		if (!xstrncmp(msg, jogger_text[2], xstrlen(jogger_text[2]))) {
			char *tmp;

			if ((tmp = xstrstr(msg, jogger_text[3]))) {
					/* XXX: msg instead? */
				*(tmp-1) = '\0';
				print("jogger_modified", session_name(js), msg+xstrlen(jogger_text[2])+1);
				*(tmp-1) = ' ';
				return -1;
			}
			if ((tmp = xstrstr(msg, jogger_text[4]))) {
				*(tmp-1) = '\0';
				print("jogger_noentry", session_name(js), msg+xstrlen(jogger_text[2])+1);
				*(tmp-1) = ' ';
				return -1;
			}
		}
		if (!xstrncmp(msg, jogger_text[5], xstrlen(jogger_text[5]))) {
			print("jogger_published", session_name(js), msg+xstrlen(jogger_text[5])+1);
			return -1;
		}
		if (!xstrncmp(msg, jogger_text[6], xstrlen(jogger_text[6]))) {
			print("jogger_comment_added", session_name(js), msg+xstrlen(jogger_text[6])+1);
			return -1;
		}
		if (!xstrncmp(msg, jogger_text[7], xstrlen(jogger_text[7]))) {
			char *tmp;

			if ((tmp = xstrstr(msg, jogger_text[8]))) {
				*(tmp-1) = '\0';
				print("jogger_unsubscribed", session_name(js), msg+xstrlen(jogger_text[7])+1);
				*(tmp-1) = ' ';
				return -1;
			}
			if ((tmp = xstrstr(msg, jogger_text[9]))) {
				*(tmp-1) = '\0';
				print("jogger_subscribed", session_name(js), msg+xstrlen(jogger_text[7])+1);
				*(tmp-1) = ' ';
				return -1;
			}
		}
		if (!xstrncmp(msg, jogger_text[10], xstrlen(jogger_text[10]))) {
			print("jogger_subscription_denied", session_name(js));
			return -1;
		}
		if (!xstrncmp(msg, jogger_text[11], xstrlen(jogger_text[11]))) {
			print("jogger_unsubscribed_earlier", session_name(js));
			return -1;
		}

		if (found == 0)
			return 0;

		if (found <= 4) { /* we get id here */
			const char *tmp = xstrstr(msg, " (#");

			if (tmp) {
				const int oq	= (session_int_get(js, "newentry_open_query") || (found < 4));
				char *suid, *uid, *msg;
				char **rcpts	= NULL;
				uint32_t *fmt	= NULL;
				
				if (found == 4)
					msg	= xstrdup(msg+xstrlen(jogger_text[12])+1);
				else {
						/* XXX: store URL somewhere (nickname?) */
					if (!(msg = xstrchr(tmp, '\n')) || !(msg = xstrchr(msg+1, '\n')))
						return 0;
					msg	= xstrdup(msg+1);
				}

				if (oq)
					uid	= saprintf("jogger:%d", atoi(tmp+3));
				else
					uid	= xstrdup("jogger:");
				suid		= xstrdup(session_uid_get(js));

				query_emit_id(NULL, PROTOCOL_MESSAGE, &suid, &uid, &rcpts, &msg, &fmt, &sent, &class, &seq, &dobeep, &secure);

				xfree(suid);
				xfree(uid);
				xfree(msg);
				return -1;
			}
		}
	} else if (class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) { /* outgoing */
		if (!s || !rcpts || !rcpts[0] || !(js = jogger_session_find_uid(s, rcpts[0])))
			return 0;

		char *suid, *uid;
		char *lmsg	= (char*) msg;
		char *rcpts[2]	= { NULL, NULL };
		char **rcptsb	= &rcpts;
		uint32_t *fmt	= NULL;

		if (*lmsg == '#') {
			int n;
			char *tmp;

			if ((*(++lmsg) == '-') || (*lmsg == '+')) /* throw away subscriptions */
				return -1;
			if ((n = atoi(lmsg)) && (tmp = xstrchr(lmsg, ' '))) { /* comment */
				lmsg		= tmp+1;
				rcpts[0]	= saprintf("jogger:%d", n);
			}
		}
		
		lmsg		= xstrdup(lmsg);
		suid		= xstrdup(session_uid_get(js));
		uid		= xstrdup(suid);
		if (!rcpts[0])
			rcpts[0]	= xstrdup("jogger:");

		query_emit_id(NULL, PROTOCOL_MESSAGE, &suid, &uid, &rcptsb, &lmsg, &fmt, &sent, &class, &seq, &dobeep, &secure);
	
		xfree(uid);
		xfree(suid);
		xfree(lmsg);
		return -1;
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
