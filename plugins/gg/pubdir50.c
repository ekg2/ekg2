/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
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
#include <ekg/sessions.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "gg.h"
#include "misc.h"

COMMAND(gg_command_find)
{
	gg_private_t *g = session_private_get(session);
	char **argv = NULL;
	char **uargv = NULL;
	gg_pubdir50_t req;
	int i, res = 0, all = 0;

	if (!g->sess || g->sess->state != GG_STATE_CONNECTED) {
		printq("not_connected", session_name(session));
		return -1;
	}

	if (params[0] && match_arg(params[0], 'S', ("stop"), 3)) {
		list_t l;

		for (l = g->searches; l; ) {
			gg_pubdir50_t s = l->data;

			l = l->next;
			gg_pubdir50_free(s);
			list_remove(&g->searches, s, 0);
		}
		
		printq("search_stopped");

		return 0;
	}
	
	argv = (char **) params;

	if (target[0] == '#' && (!argv[0] || !argv[1])) {
		return command_exec_format(target, session, quiet, ("/conference --find %s"), target);
	}

	if (!(req = gg_pubdir50_new(GG_PUBDIR50_SEARCH))) {
		return -1;
	}

	if (target[0] != '-' || !params[0]) {		/* if window_current->target is even --blah use it. it's quite stupid hovewer. */
		const char *uid = get_uid(session, target);

		if (!uid) {
			printq("user_not_found", target);
			return -1;
		}

		if (xstrncasecmp(uid, "gg:", 3)) {
			printq("generic_error", ("Tylko GG"));
			return -1;
		}

		gg_pubdir50_add(req, GG_PUBDIR50_UIN, uid + 3);

		if (!params[0]) goto no_argv;

		argv = &argv[1];	/* skip this param, and go to nextone */
	}
	
	uargv = xcalloc(array_count(argv)+1, sizeof(char **));

	for (i = 0; argv[i]; i++)
		uargv[i] = gg_locale_to_cp(argv[i]);

	for (i = 0; argv[i]; i++) {
		char *arg = argv[i];
				
		if (match_arg(arg, 'f', ("first"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, uargv[++i]);
			continue;
		}

		if (match_arg(arg, 'l', ("last"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, uargv[++i]);
			continue;
		}

		if (match_arg(arg, 'n', ("nickname"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, uargv[++i]);
			continue;
		}
		
		if (match_arg(arg, 'c', ("city"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_CITY, uargv[++i]);
			continue;
		}

		if (match_arg(arg, 'u', ("uin"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_UIN, uargv[++i]);
			continue;
		}
		
		if (match_arg(arg, 's', ("start"), 3) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_START, uargv[++i]);
			continue;
		}
		
		if (match_arg(arg, 'F', ("female"), 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_FEMALE);
			continue;
		}

		if (match_arg(arg, 'M', ("male"), 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_MALE);
			continue;
		}

		if (match_arg(arg, 'a', ("active"), 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_ACTIVE, GG_PUBDIR50_ACTIVE_TRUE);
			continue;
		}

		if (match_arg(arg, 'b', ("born"), 2) && argv[i + 1]) {
			char *foo = xstrchr(uargv[++i], ':');
		
			if (foo)
				*foo = ' ';

			gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, uargv[i]);
			continue;
		}

		if (match_arg(arg, 'A', ("all"), 3)) {
			if (!gg_pubdir50_get(req, 0, GG_PUBDIR50_START))
				gg_pubdir50_add(req, GG_PUBDIR50_START, "0");
			all = 1;
			continue;
		}

		printq("invalid_params", name);
		gg_pubdir50_free(req);

#if (USE_UNICODE || HAVE_GTK)
		if (config_use_unicode) for (i = 0; argv[i]; i++) if (argv[i] != uargv[i]) xfree(uargv[i]);	/* wrong? */
#endif
		xfree(uargv);
		return -1;
	}
#if (USE_UNICODE || HAVE_GTK)
	if (config_use_unicode) for (i = 0; argv[i]; i++) if (argv[i] != uargv[i]) xfree(uargv[i]);		/* wrongx2? */
#endif
	xfree(uargv);

no_argv:
	if (!gg_pubdir50(g->sess, req)) {
		printq("search_failed", ("Nie wiem o co chodzi"));
		res = -1;
	}

	if (all)
		list_add(&g->searches, req);
	else
		gg_pubdir50_free(req);

	return res;
}

COMMAND(gg_command_change)
{
	gg_private_t *g = session_private_get(session);
	int i;
	gg_pubdir50_t req;

	if (!g->sess || g->sess->state != GG_STATE_CONNECTED) {
		printq("not_connected");
		return -1;
	}

	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	if (!(req = gg_pubdir50_new(GG_PUBDIR50_WRITE)))
		return -1;

	if (xstrcmp(params[0], ("-"))) {
		char **argv = array_make(params[0], (" \t"), 0, 1, 1);
		
		for (i = 0; argv[i]; i++)
			argv[i] = gg_locale_to_cp(argv[i]);

		for (i = 0; argv[i]; i++) {
			if (match_arg(argv[i], 'f', ("first"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, argv[++i]);
				continue;
			}

			if (match_arg(argv[i], 'N', ("familyname"), 7) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_FAMILYNAME, argv[++i]);
				continue;
			}
		
			if (match_arg(argv[i], 'l', ("last"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, argv[++i]);
				continue;
			}
		
			if (match_arg(argv[i], 'n', ("nickname"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, argv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'c', ("city"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_CITY, argv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'C', ("familycity"), 7) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_FAMILYCITY, argv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'b', ("born"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, argv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'F', ("female"), 2)) {
				gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_SET_FEMALE);
				continue;
			}

			if (match_arg(argv[i], 'M', ("male"), 2)) {
				gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_SET_MALE);
				continue;
			}

			printq("invalid_params", name);
			array_free(argv);

			gg_pubdir50_free(req);
			return -1;
		}
		array_free(argv);
	}

	if (!gg_pubdir50(g->sess, req)) {
		printq("change_failed", (""));
		gg_pubdir50_free(req);
		return -1;
	}

	gg_pubdir50_free(req);
	g->quiet |= GG_QUIET_CHANGE;

	return 0;
}

/*
 * gg_session_handler_search50()
 *
 * zajmuje siê obs³ug± wyniku przeszukiwania katalogu publicznego.
 *
 *  - s - sesja
 *  - e - opis zdarzenia
 */
void gg_session_handler_search50(session_t *s, struct gg_event *e)
{
	gg_private_t *g = session_private_get(s);
	gg_pubdir50_t res = e->event.pubdir50;
	int i, count, all = 0;
	list_t l;
	uin_t last_uin = 0;

	if (!g)
		return;

	if ((count = gg_pubdir50_count(res)) < 1) {
		print("search_not_found");
		return;
	}

	debug_function("gg_session_handler_search50() handle_search50, count = %d\n", gg_pubdir50_count(res));

	for (l = g->searches; l; l = l->next) {
		gg_pubdir50_t req = l->data;

		if (gg_pubdir50_seq(req) == gg_pubdir50_seq(res)) {
			all = 1;
			break;
		}
	}

	for (i = 0; i < count; i++) {
		const char *uin		= gg_pubdir50_get(res, i, "fmnumber");
		const char *__firstname = gg_pubdir50_get(res, i, "firstname");
		const char *__lastname	= gg_pubdir50_get(res, i, "lastname");
		const char *__nickname	= gg_pubdir50_get(res, i, "nickname");
		const char *__fmstatus	= gg_pubdir50_get(res, i, "fmstatus");
		const char *__birthyear = gg_pubdir50_get(res, i, "birthyear");
		const char *__city	= gg_pubdir50_get(res, i, "city");

		char *firstname		= gg_cp_to_locale(xstrdup(__firstname));
		char *lastname		= gg_cp_to_locale(xstrdup(__lastname));
		char *nickname		= gg_cp_to_locale(xstrdup(__nickname));
		char *city		= gg_cp_to_locale(xstrdup(__city));
		int status		= (__fmstatus)	? atoi(__fmstatus) : GG_STATUS_NOT_AVAIL;
		const char *birthyear	= (__birthyear && xstrcmp(__birthyear, "0")) ? __birthyear : NULL;

		char *name, *active, *gender;
		const char *target = NULL;

		if (count == 1 && !all) {
			xfree(last_search_first_name);
			xfree(last_search_last_name);
			xfree(last_search_nickname);
			xfree(last_search_uid);
			last_search_first_name	= xstrdup(firstname);
			last_search_last_name	= xstrdup(lastname);
			last_search_nickname	= xstrdup(nickname);
			last_search_uid		= saprintf("gg:%s", uin);
		}

		name = saprintf(
			("%s %s"),
					firstname ? firstname : (""), 
					lastname ? lastname : (""));

#define __format(x) ((count == 1 && !all) ? "search_results_single" x : "search_results_multi" x)
		{
			const char *fvalue;
			switch (status) {
				case GG_STATUS_AVAIL:
				case GG_STATUS_AVAIL_DESCR:
					fvalue = format_find(__format("_avail"));
					break;
				case GG_STATUS_BUSY:
				case GG_STATUS_BUSY_DESCR:
					fvalue = format_find(__format("_away"));
					break;
				case GG_STATUS_INVISIBLE:
				case GG_STATUS_INVISIBLE_DESCR:
					fvalue = format_find(__format("_invisible"));
					break;
				default:
					fvalue = format_find(__format("_notavail"));
			}
			active = format_string(fvalue, (__firstname) ? __firstname : nickname);
		}
		gender = format_string(format_find(__format("_unknown")), "");

			/* XXX: why do we _exactly_ use it here? can't we just always
			 *	define target and thus display result in right conversation window? */
		for (l = autofinds; l; l = l->next) {
			char *d = (char *) l->data;
		
			if (!xstrcasecmp(d + 3, uin)) {
				target = d;
				break;
			}
		}
		
		print_info(target, s, __format(""), 
			uin		? uin : ("?"), name, 
			nickname	? nickname : (""), 
			city		? city : (""), 
			birthyear	? birthyear : ("-"),
			gender, active);

#undef __format

		xfree(name);
		xfree(active);
		xfree(gender);

		xfree(firstname);
		xfree(lastname);
		xfree(nickname);
		xfree(city);

		last_uin = atoi(uin);
	}

	/* je¶li mieli¶my ,,/find --all'', szukamy dalej */
	for (l = g->searches; l; l = l->next) {
		gg_pubdir50_t req = l->data;
		uin_t next;

		if (gg_pubdir50_seq(req) != gg_pubdir50_seq(res))
			continue;

		/* nie ma dalszych? to dziêkujemy */
		if (!(next = gg_pubdir50_next(res)) || !g->sess || next <= last_uin) {
			list_remove(&g->searches, req, 0);
			gg_pubdir50_free(req);
			break;
		}

		gg_pubdir50_add(req, GG_PUBDIR50_START, itoa(next));
		gg_pubdir50(g->sess, req);

		break;
	}

}

/*
 * handle_change50()
 *
 * zajmuje siê obs³ug± zmiany danych w katalogu publicznym.
 *
 *  - s - sesja
 *  - e - opis zdarzenia
 */
void gg_session_handler_change50(session_t *s, struct gg_event *e)
{
	gg_private_t *g = session_private_get(s);
	int quiet;

	if (!g)
		return;

	quiet = (g->quiet & GG_QUIET_CHANGE);
	printq("change");
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
