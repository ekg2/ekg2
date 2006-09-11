/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *                2004 Piotr Kupisiewicz <deletek@ekg2.org>
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
	CHAR_T **argv = NULL, *user;
	char **uargv = NULL;
	gg_pubdir50_t req;
	int i, res = 0, all = 0;

	if (!g->sess || g->sess->state != GG_STATE_CONNECTED) {
		wcs_printq("not_connected", session_name(session));
		return -1;
	}

	if (params[0] && match_arg(params[0], 'S', TEXT("stop"), 3)) {
		list_t l;

		for (l = g->searches; l; ) {
			gg_pubdir50_t s = l->data;

			l = l->next;
			gg_pubdir50_free(s);
			list_remove(&g->searches, s, 0);
		}
		
		wcs_printq("search_stopped");

		return 0;
	}
	
	if (!params[0]) {
		if (!(params[0] = xwcsdup(window_current->target))) {
			wcs_printq("not_enough_params", name);
			return -1;
		}
		params[1] = NULL;
	}

	argv = (CHAR_T **) params;

	if (argv[0] && !argv[1] && argv[0][0] == '#') {
		return command_exec_format(target, session, quiet, TEXT("/conference --find %s"), argv[0]);
	}

	if (!(req = gg_pubdir50_new(GG_PUBDIR50_SEARCH))) {
		return -1;
	}

	uargv = xcalloc(array_count(argv)+1, sizeof(CHAR_T **));

	user = xwcsdup(argv[0]);
	
	if (argv[0] && argv[0][0] != '-') {
		const char *uid = get_uid(session, argv[0]);

		if (!uid) {
			printq("user_not_found", user);
			xfree(user);
			return -1;
		}

		if (xstrncasecmp(uid, "gg:", 3)) {
			wcs_printq("generic_error", TEXT("Tylko GG"));
			xfree(user);
			return -1;
		}

		gg_pubdir50_add(req, GG_PUBDIR50_UIN, uid + 3);

		for (i = 1; argv[i]; i++)
			uargv[i] = gg_locale_to_cp(argv[i]);

		i = 1;
	} else {
		for (i = 0; argv[i]; i++)
			uargv[i] = gg_locale_to_cp(argv[i]);
		
		i = 0;
	}

	xfree(user);

	for (; argv[i]; i++) {
		CHAR_T *arg = argv[i];
				
		if (match_arg(arg, 'f', TEXT("first"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, uargv[++i]);
			continue;
		}

		if (match_arg(arg, 'l', TEXT("last"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, uargv[++i]);
			continue;
		}

		if (match_arg(arg, 'n', TEXT("nickname"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, uargv[++i]);
			continue;
		}
		
		if (match_arg(arg, 'c', TEXT("city"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_CITY, uargv[++i]);
			continue;
		}

		if (match_arg(arg, 'u', TEXT("uin"), 2) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_UIN, uargv[++i]);
			continue;
		}
		
		if (match_arg(arg, 's', TEXT("start"), 3) && argv[i + 1]) {
			gg_pubdir50_add(req, GG_PUBDIR50_START, uargv[++i]);
			continue;
		}
		
		if (match_arg(arg, 'F', TEXT("female"), 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_FEMALE);
			continue;
		}

		if (match_arg(arg, 'M', TEXT("male"), 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_MALE);
			continue;
		}

		if (match_arg(arg, 'a', TEXT("active"), 2)) {
			gg_pubdir50_add(req, GG_PUBDIR50_ACTIVE, GG_PUBDIR50_ACTIVE_TRUE);
			continue;
		}

		if (match_arg(arg, 'b', TEXT("born"), 2) && argv[i + 1]) {
			char *foo = xstrchr(uargv[++i], ':');
		
			if (foo)
				*foo = ' ';

			gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, uargv[i]);
			continue;
		}

		if (match_arg(arg, 'A', TEXT("all"), 3)) {
			if (!gg_pubdir50_get(req, 0, GG_PUBDIR50_START))
				gg_pubdir50_add(req, GG_PUBDIR50_START, "0");
			all = 1;
			continue;
		}

		wcs_printq("invalid_params", name);
		gg_pubdir50_free(req);
		xfree(uargv);
		return -1;
	}
		xfree(uargv);

	if (!gg_pubdir50(g->sess, req)) {
		wcs_printq("search_failed", TEXT("Nie wiem o co chodzi"));
		res = -1;
	}

	if (all)
		list_add(&g->searches, req, 0);
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
		wcs_printq("not_connected");
		return -1;
	}

	if (!params[0]) {
		wcs_printq("not_enough_params", name);
		return -1;
	}

	if (!(req = gg_pubdir50_new(GG_PUBDIR50_WRITE)))
		return -1;

	if (xwcscmp(params[0], TEXT("-"))) {
		CHAR_T **argv = array_make(params[0], TEXT(" \t"), 0, 1, 1);
		char **uargv = xcalloc(array_count(argv)+1, sizeof(char *));
		
		for (i = 0; argv[i]; i++) {
			uargv[i] = gg_locale_to_cp(argv[i]);
		}

		for (i = 0; argv[i]; i++) {
			if (match_arg(argv[i], 'f', TEXT("first"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_FIRSTNAME, uargv[++i]);
				continue;
			}

			if (match_arg(argv[i], 'N', TEXT("familyname"), 7) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_FAMILYNAME, uargv[++i]);
				continue;
			}
		
			if (match_arg(argv[i], 'l', TEXT("last"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_LASTNAME, uargv[++i]);
				continue;
			}
		
			if (match_arg(argv[i], 'n', TEXT("nickname"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_NICKNAME, uargv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'c', TEXT("city"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_CITY, uargv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'C', TEXT("familycity"), 7) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_FAMILYCITY, uargv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'b', TEXT("born"), 2) && argv[i + 1]) {
				gg_pubdir50_add(req, GG_PUBDIR50_BIRTHYEAR, uargv[++i]);
				continue;
			}
			
			if (match_arg(argv[i], 'F', TEXT("female"), 2)) {
				gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_SET_FEMALE);
				continue;
			}

			if (match_arg(argv[i], 'M', TEXT("male"), 2)) {
				gg_pubdir50_add(req, GG_PUBDIR50_GENDER, GG_PUBDIR50_GENDER_SET_MALE);
				continue;
			}

			wcs_printq("invalid_params", name);
			gg_pubdir50_free(req);
			array_free(argv);
			return -1;
		}

		array_free(argv);
		xfree(uargv);
	}

	if (!gg_pubdir50(g->sess, req)) {
		wcs_printq("change_failed", TEXT(""));
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

	gg_debug(GG_DEBUG_MISC, "handle_search50, count = %d\n", gg_pubdir50_count(res));

	for (l = g->searches; l; l = l->next) {
		gg_pubdir50_t req = l->data;

		if (gg_pubdir50_seq(req) == gg_pubdir50_seq(res)) {
			all = 1;
			break;
		}
	}

	for (i = 0; i < count; i++) {
		char   *cpfirstname, *cplastname, *cpnickname, *cpcity;
		CHAR_T *firstname, *lastname, *nickname, *city;
		const char *birthyear;

		const char *uin		= gg_pubdir50_get(res, i, "fmnumber");
		const char *__firstname = gg_pubdir50_get(res, i, "firstname");
		const char *__lastname	= gg_pubdir50_get(res, i, "lastname");
		const char *__nickname	= gg_pubdir50_get(res, i, "nickname");
		const char *__fmstatus	= gg_pubdir50_get(res, i, "fmstatus");
		const char *__birthyear = gg_pubdir50_get(res, i, "birthyear");
		const char *__city	= gg_pubdir50_get(res, i, "city");

		CHAR_T *name, *active, *gender;
		const char *target = NULL;

		int status;

		cpfirstname 	= (__firstname) ? xstrdup(__firstname) : NULL;
		cplastname 	= (__lastname)  ? xstrdup(__lastname) : NULL;
		cpnickname 	= (__nickname)	? xstrdup(__nickname) : NULL;
		cpcity 		= (__city)	? xstrdup(__city) : NULL;

		status 		= (__fmstatus)	? atoi(__fmstatus) : GG_STATUS_NOT_AVAIL;
		birthyear 	= (__birthyear && xstrcmp(__birthyear, "0")) ? __birthyear : NULL;

		firstname 	= gg_cp_to_locale(cpfirstname);
		lastname 	= gg_cp_to_locale(cplastname);
		nickname 	= gg_cp_to_locale(cpnickname);
		city		= gg_cp_to_locale(cpcity);

		if (count == 1 && !all) {
			xfree(last_search_first_name);
			xfree(last_search_last_name);
			xfree(last_search_nickname);
			xfree(last_search_uid);
			last_search_first_name	= xwcsdup(firstname);
			last_search_last_name	= xwcsdup(lastname);
			last_search_nickname	= xwcsdup(nickname);
			last_search_uid 	= saprintf("gg:%s", uin);
		}

		name = saprintf(
			TEXT("%s %s"),
					firstname ? firstname : TEXT(""), 
					lastname ? lastname : TEXT(""));

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

		for (l = autofinds; l; l = l->next) {
			char *d = (char *) l->data;
		
			if (!xstrcasecmp(d + 3, uin)) {
				target = d;
				break;
			}
		}
		
		print_window(target, s, 0, __format(""), 
			uin 		? uin : TEXT("?"), name, 
			nickname 	? nickname : TEXT(""), 
			city		? city : TEXT(""), 
			birthyear	? birthyear : TEXT("-"),
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
	wcs_printq("change");
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
