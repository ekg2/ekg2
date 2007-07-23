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

#define JOGGER_TEXT_MAX 14

	/* jogger.c */
session_t *jogger_session_find_uid(session_t *s, const char *uid);

const char *utf_jogger_text[JOGGER_TEXT_MAX] = {
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
	"Brak uprawnień do komentowania tego wpisu!"		/* [13] */
};

char *jogger_text[JOGGER_TEXT_MAX];

void jogger_free_texts(int real_free) {
	int i;

	for (i = 0; i < JOGGER_TEXT_MAX; i++) {
		if (real_free)
			xfree(jogger_text[i]);
		jogger_text[i] = NULL;
	}
}

void jogger_localize_texts(void *p) {
	int i;

	jogger_free_texts(1);
	for (i = 0; i < JOGGER_TEXT_MAX; i++) {
		char *s = ekg_convert_string_p(utf_jogger_text[i], p);

		if (!s)
			s = xstrdup(utf_jogger_text[i]);
		jogger_text[i] = s;
	}
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
		char *tmp;

		if (!xstrncmp(msg, jogger_text[0], xstrlen(jogger_text[0])))
			found = 1; /* own jogger comment */
		else if (!xstrncmp(msg, jogger_text[1], xstrlen(jogger_text[1])))
			found = 2; /* other jogger comment */
		else if (owncf && !xstrncmp(msg, owncf, xstrlen(owncf)))
			found = 3; /* own jogger comment with custom text */
		else if (!xstrncmp(msg, jogger_text[12], xstrlen(jogger_text[12])))
			found = 4; /* new jogger entry */
		else if (!xstrncmp(msg, jogger_text[2], xstrlen(jogger_text[2]))) {
			if ((tmp = xstrstr(msg, jogger_text[3])))
				found = 5;
			else if ((tmp = xstrstr(msg, jogger_text[4])))
				found = 6;
		} else if (!xstrncmp(msg, jogger_text[7], xstrlen(jogger_text[7]))) {
			if ((tmp = xstrstr(msg, jogger_text[8])))
				found = 7;
			else if ((tmp = xstrstr(msg, jogger_text[9])))
				found = 8;
		} else if (!xstrncmp(msg, jogger_text[5], xstrlen(jogger_text[5])))
			found = 9;
		else if (!xstrncmp(msg, jogger_text[6], xstrlen(jogger_text[6])))
			found = 10;
		else if (!xstrncmp(msg, jogger_text[10], xstrlen(jogger_text[10])))
			found = 11;
		else if (!xstrncmp(msg, jogger_text[11], xstrlen(jogger_text[11])))
			found = 12;
		else if (!xstrncmp(msg, jogger_text[13], xstrlen(jogger_text[13])))
			found = 13;

		if (found <= 4) { /* we get id here */
			const char *tmp = xstrstr(msg, " (#");

			if (!tmp)
				found	= 0;

			const int oq	= ((found > 0) && (session_int_get(js, "newentry_open_query") || (found < 4)));
			char *suid, *uid, *lmsg, *url;
			char **rcpts	= NULL;
			uint32_t *fmt	= NULL;


			if (found == 4) {
				lmsg	= xstrdup(msg+xstrlen(jogger_text[12])+1);
				url	= xstrndup(lmsg, tmp-msg-xstrlen(jogger_text[12])-1);
			} else if (found == 0) {
				lmsg	= xstrdup(msg);
				url	= NULL;
			} else {
				url	= msg+xstrlen(found == 3 ? owncf : jogger_text[found-1])+1;

				if (!(lmsg = xstrchr(tmp, '\n')) || !(lmsg = xstrchr(lmsg+1, '\n')))
					return 0;

				lmsg	= xstrdup(lmsg+1);
				url	= xstrndup(url, tmp-url);
			}

			if (url && xstrncmp(url, "http://", 7)) { /* if URL is invalid, we probably have partial match */
				xfree(url);
				xfree(lmsg);

				lmsg	= xstrdup(msg);
				url	= NULL;
			}

			if (oq && url)
				uid	= saprintf("jogger:%d", atoi(tmp+3));
			else
				uid	= xstrdup("jogger:");
			suid		= xstrdup(session_uid_get(js));

			if (url) {
				userlist_t *u = userlist_find(js, uid);
				
				if (!u)
					userlist_add(js, uid, url);
				else if (xstrcmp(u->nickname, url)) {
					xfree(u->nickname);
					u->nickname = url;
				} else
					xfree(url);
			}

			query_emit_id(NULL, PROTOCOL_MESSAGE, &suid, &uid, &rcpts, &lmsg, &fmt, &sent, &class, &seq, &dobeep, &secure);

			xfree(suid);
			xfree(uid);
			xfree(lmsg);
		} else if (found <= 8) {
			const char *formats[]	= { "jogger_modified", "jogger_noentry", 
					"jogger_unsubscribed", "jogger_subscribed" };
			const char *url		= msg+xstrlen(found > 6 ? jogger_text[7] : jogger_text[2])+1;
			char *id		= (found == 6 ? saprintf("jogger:%d", atoi(url+1)) : NULL);

			*(tmp-1) = '\0';
			print_window((id ? id : url), js, 0, formats[found-5], session_name(js), url);
			*(tmp-1) = ' ';
			xfree(id);
		} else if (found <= 10) {
			const char *formats[]	= { "jogger_published", "jogger_comment_added" };
			const char *url		= msg+xstrlen(jogger_text[found-4])+1;

			print_window(url, js, 0, formats[found-9], session_name(js), url);
		} else if (found <= 13) {
			const char *formats[]	= { "jogger_subscription_denied", "jogger_unsubscribed_earlier",
					"jogger_posting_denied" };

			print(formats[found-12], session_name(js));
		}
		return -1;
	} else if (class == EKG_MSGCLASS_SENT || class == EKG_MSGCLASS_SENT_CHAT) { /* outgoing */
		if (!s || !rcpts || !rcpts[0] || !(js = jogger_session_find_uid(s, rcpts[0])))
			return 0;

		char *suid, *uid;
		char *lmsg	= (char*) msg;
		char *rcpts[2]	= { NULL, NULL };
		char **rcptsb	= &rcpts; /* this is REALLY necessary, please do not remove or else we get segv */
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
		} else if (session_int_get(js, "ignore_outgoing_entries"))
			return -1;
		
		lmsg		= xstrdup(lmsg);
		suid		= xstrdup(session_uid_get(js));
		uid		= xstrdup(suid);
		if (!rcpts[0])
			rcpts[0]	= xstrdup("jogger:");

		query_emit_id(NULL, PROTOCOL_MESSAGE, &suid, &uid, &rcptsb, &lmsg, &fmt, &sent, &class, &seq, &dobeep, &secure);
	
		xfree(rcpts[0]);
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
	const char *msg		= (is_inline ? params[0] : params[1]);
	int n;

	if (!params[0]) /* we don't print anything, because it is only possible with inline_msg */
		return 0;

	if (!uid || !js || !juid) {
		printq("invalid_session");	/* XXX, unprepared session? */
		return -1;
	}
	uid += 7; /* skip jogger: */

	if (*uid == '\0') { /* redirect message to jogger uid */
		if (is_inline)
			return command_exec(juid, js, msg, 0);
		else
			return command_exec_format(NULL, js, 0, "/%s \"%s\" %s", name, juid, msg);
	}
	if (*uid == '#')
		uid++;

	if (!(n = atoi(uid))) {
		printq("invalid_uid");
		return -1;
	}

	{		/* skip #eid if (unnecessarily) added */
		char *tmp = saprintf("#%d ", n);

		if (!xstrncmp(msg, tmp, xstrlen(tmp)))
			msg += xstrlen(tmp);
		xfree(tmp);
	}

		/* post as comment-reply */
	if (is_inline)
		return command_exec_format(juid, js, 0, "#%d %s", n, msg);
	else
		return command_exec_format(NULL, js, 0, "/%s \"%s\" #%d %s", name, juid, n, msg);
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
