/* $Id: userlist.c 4418 2008-08-17 19:54:40Z wiechu $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Piotr Domagalski <szalik@szalik.net>
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

#include "ekg2-remote-config.h"

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <stdlib.h>
#include <string.h>

#include "dynstuff.h"
#include "dynstuff_inline.h"
#include "userlist.h"
#include "xmalloc.h"

struct ignore_label ignore_labels[] = {
	{ IGNORE_STATUS, "status" },
	{ IGNORE_STATUS_DESCR, "descr" },
	{ IGNORE_NOTIFY, "notify" },
	{ IGNORE_MSG, "msg" },
	{ IGNORE_DCC, "dcc" },
	{ IGNORE_EVENTS, "events" },
	{ IGNORE_XOSD, "xosd" },
	{ 0, NULL }
};

/* groups: */
static LIST_ADD_COMPARE(group_compare, struct ekg_group *) { return xstrcasecmp(data1->name, data2->name); }
static LIST_FREE_ITEM(group_item_free, struct ekg_group *) { xfree(data->name); }
DYNSTUFF_LIST_DECLARE_SORTED(ekg_groups, struct ekg_group, group_compare, group_item_free,
	static __DYNSTUFF_ADD_SORTED,		/* ekg_groups_add() */
	__DYNSTUFF_NOREMOVE,
	static __DYNSTUFF_DESTROY)		/* ekg_groups_destroy() */

/* userlist: */
static LIST_ADD_COMPARE(userlist_compare, userlist_t *) { return xstrcasecmp(data1->nickname, data2->nickname); }
static LIST_FREE_ITEM(userlist_free_item, userlist_t *) {
	private_items_destroy(&data->priv_list);
	xfree(data->uid); xfree(data->nickname); xfree(data->descr); xfree(data->last_descr);
	ekg_groups_destroy(&(data->groups));
}
DYNSTUFF_LIST_DECLARE_SORTED(userlists, userlist_t, userlist_compare, userlist_free_item,
	static __DYNSTUFF_ADD_SORTED,					/* userlists_add() */
	EXPORTNOT __DYNSTUFF_REMOVE_SAFE,				/* userlists_remove() */
	EXPORTNOT __DYNSTUFF_DESTROY)					/* userlists_destroy() */

static struct ekg_group *group_init(const char *groups);

EXPORTNOT userlist_t *remote_userlist_add_entry(userlist_t **userlist, char **__entry, int count) {
	userlist_t *u;
	char **entry;
	int i;

	if (count == 0)
		return NULL;

	entry = xcalloc(count, sizeof(char *));

	for (i = 0; i < count; i++) {
		if ((__entry[i][0] == '\0') || !strcmp(__entry[i], "(null)"))
			entry[i] = NULL;
		else
			entry[i] = (char *) __entry[i];
	}

	if (entry[0] == NULL) {
		xfree(entry);
		return NULL;
	}
	/* Note: tutaj bylo valid_plugin_uid() to samo co zwykle, jak u nich jest dobrze, to u nas tez */

	u = xmalloc(sizeof(userlist_t));

	u->uid		= xstrdup(entry[0]);
	u->status 	= (count > 1) ? atoi(entry[1]) : EKG_STATUS_NA;
	u->nickname	= (count > 2) ? xstrdup(entry[2]) : NULL;
	u->groups	= (count > 3) ? group_init(entry[3]) : NULL;
	u->descr	= (count > 4) ? xstrdup(entry[4]) : NULL;

	userlists_add(userlist, u);
	xfree(entry);

	return u;
}

EXPORTNOT void userlist_free(session_t *session) {
	if (!session)
		return;

	userlists_destroy(&(session->userlist));
}

EXPORTNOT int userlist_remove(session_t *session, userlist_t *u) {
	if (!session)
		return -1;
	userlists_remove(&(session->userlist), u);
	return 0;
}

userlist_t *userlist_find(session_t *session, const char *uid) {
	userlist_t *ul;

	if (!uid || !session)
		return NULL;

	for (ul = session->userlist; ul; ul = ul->next) {
		userlist_t *u = ul;
		const char *tmp;
		int len;

		if (!xstrcasecmp(u->uid, uid))
			return u;

		if (u->nickname && !xstrcasecmp(u->nickname, uid))
			return u;

		/* porównujemy resource; if (len > 0) */

		if (!(tmp = strchr(uid, '/')) || (strncmp(uid, "xmpp:", 5)))
			continue;

		len = (int)(tmp - uid);

		if (len > 0 && !xstrncasecmp(uid, u->uid, len))
			return u;
	}

	return NULL;
}

char *get_uid(session_t *session, const char *text) { return NULL; }		/* XXX, z get_uid() korzysta tylko kawalek kodu ncurses, zwiazanego z typing-notify
											wiec i tak nie bedzie dzialac
										 */
int ignored_check(session_t *session, const char *uid) {
	struct ekg_group *gl;
	userlist_t *u;

	if (!(u = userlist_find(session, uid)))
		return 0;

	for (gl = u->groups; gl; gl = gl->next) {
		struct ekg_group *g = gl;

		if (!xstrcasecmp(g->name, "__ignored"))
			return IGNORE_ALL;

		if (!xstrncasecmp(g->name, "__ignored_", 10))
			return atoi(g->name + 10);
	}

	return 0;
}

int ekg_group_member(userlist_t *u, const char *group) {
	struct ekg_group *gl;

	if (!u || !group)
		return 0;

	for (gl = u->groups; gl; gl = gl->next) {
		struct ekg_group *g = gl;

		if (!xstrcasecmp(g->name, group))
			return 1;
	}

	return 0;
}

static char **array_make_groups(const char *string) {
	const char *p, *q;
	char **result = NULL;
	int items = 0;

	for (p = string; ; ) {
		char *token;
		int len;

		while (*p == ',')
			p++;

		if (!*p)
			break;

		for (q = p, len = 0; *q && *q != ','; q++, len++);
		token = xstrndup(p, len);
		p = q;
		
		result = xrealloc(result, (items + 2) * sizeof(char*));
		result[items] = token;
		result[++items] = NULL;

		if (!*p)
			break;

		p++;
	}

	return result;
}

static struct ekg_group *group_init(const char *names) {
	struct ekg_group *gl = NULL;
	char **groups;
	int i;

	if (!names)
		return NULL;

	if ((groups = array_make_groups(names))) {
		for (i = 0; groups[i]; i++) {
			struct ekg_group *g = xmalloc(sizeof(struct ekg_group));

			g->name = groups[i];
			ekg_groups_add(&gl, g);
		}
		/* NOTE: we don't call here array_free() cause we use items of this
		 *	array @ initing groups. We don't use strdup()
		 */
		xfree(groups);
	}
	
	return gl;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */

