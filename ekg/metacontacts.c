/* $Id$ */

/*
 *  (C) Copyright 2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include "ekg2-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "dynstuff.h"
#include "sessions.h"
#include "themes.h"
#include "stuff.h"
#include "userlist.h"
#include "xmalloc.h"

#include "dynstuff_inline.h"
#include "metacontacts.h"
#include "queries.h"

metacontact_t *metacontacts = NULL;

/* metacontacts_items: */
static LIST_ADD_COMPARE(metacontact_add_item_compare, metacontact_item_t *) {
        if (!data1 || !data1->name || !data1->s_uid || !data2 || !data2->name || !data2->s_uid)
                return 0;

	if (!xstrcasecmp(data1->s_uid, data2->s_uid))
		return xstrcasecmp(data1->name, data2->name);

        return xstrcasecmp(session_alias_uid_n(data1->s_uid), session_alias_uid_n(data2->s_uid));
}

static LIST_FREE_ITEM(metacontact_item_free, metacontact_item_t *) { xfree(data->name); xfree(data->s_uid); }

__DYNSTUFF_ADD_SORTED(metacontact_items, metacontact_item_t, metacontact_add_item_compare);	/* metacontact_items_add() */
__DYNSTUFF_REMOVE_SAFE(metacontact_items, metacontact_item_t, metacontact_item_free);		/* metacontact_items_remove() */	/* removei() ? */
__DYNSTUFF_DESTROY(metacontact_items, metacontact_item_t, metacontact_item_free);		/* metacontact_items_destroy() */

/* metacontacts: */
static LIST_ADD_COMPARE(metacontact_add_compare, metacontact_t *) { return xstrcasecmp(data1->name, data2->name); }
static LIST_FREE_ITEM(metacontact_list_free, metacontact_t *) { metacontact_items_destroy(&(data->metacontact_items)); xfree(data->name); }

__DYNSTUFF_LIST_ADD_SORTED(metacontacts, metacontact_t, metacontact_add_compare);	/* metacontacts_add() */
__DYNSTUFF_LIST_REMOVE_SAFE(metacontacts, metacontact_t, metacontact_list_free);	/* metacontacts_remove() */	/* removei() ? */
__DYNSTUFF_LIST_DESTROY(metacontacts, metacontact_t, metacontact_list_free);		/* metacontacts_destroy() */

static int metacontact_add_item(metacontact_t *m, const char *session, const char *name, unsigned int prio, int quiet);
static int metacontact_remove_item(metacontact_t *m, const char *session, const char *name, int quiet);
static int metacontact_remove(const char *name);

/* 
 * metacontact function support
 */
COMMAND(cmd_metacontact)
{
        metacontact_t *m;

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2)) {
        	metacontact_t *m;

                for (m = metacontacts; m; m = m->next)
			printq("metacontact_list", m->name);

                if (!metacontacts)
                        printq("metacontact_list_empty");

                return 0;
        }

	if (match_arg(params[0], 'a', ("add"), 2)) {
                if (!params[1]) {
			printq("invalid_params", name);
			return -1;
		}

		if (metacontact_find(params[1])) {
                        printq("metacontact_exists", params[1]);
                        return -1;
		}

		if (metacontact_add(params[1])) {
			char *tmp = xstrdup(params[1]);

	                config_changed = 1;
	                printq("metacontact_added", params[1]);

	                query_emit_id(NULL, METACONTACT_ADDED, &tmp);
			xfree(tmp);
		}
		return 0;	
	}

        if (match_arg(params[0], 'd', ("del"), 2)) {
		if (!params[1]) {
			printq("invalid_params", name);
			return -1;
		}

                if (!metacontact_find(params[1])) {
                        printq("metacontact_doesnt_exist", params[1]);
                        return -1;
                }

                if (metacontact_remove(params[1])) {
			char *tmp = xstrdup(params[1]);

	                config_changed = 1;
	                printq("metacontact_removed", params[1]);
			
	                query_emit_id(NULL, METACONTACT_REMOVED, &tmp);
			xfree(tmp);
		}
                return 0;
	}

	if (match_arg(params[0], 'i', ("add-item"), 2)) {
                if (!params[1] || !params[2] || !params[3] || !params[4]) {
			printq("invalid_params", name);
			return -1;
		}

                if (!(m = metacontact_find(params[1]))) {
                        printq("metacontact_doesnt_exist", params[1]);
                        return -1;
                }

		if (metacontact_add_item(m, params[2], params[3], atoi(params[4]), 0)) {
			char *tmp1 = xstrdup(params[2]), *tmp2 = xstrdup(params[3]), *tmp3 = xstrdup(params[1]);

			printq("metacontact_added_item", session_alias_uid_n(params[2]), params[3], params[1]);

	                query_emit_id(NULL, METACONTACT_ITEM_ADDED, &tmp1, &tmp2, &tmp3);
			xfree(tmp1);
			xfree(tmp2);
			xfree(tmp3);
		}
		return 0;
	}

        if (match_arg(params[0], 'r', ("del-item"), 2)) {
                if (!params[1] || !params[2] || !params[3]) {
			printq("invalid_params", name);
			return -1;
		}

                if (!(m = metacontact_find(params[1]))) {
                        printq("metacontact_doesnt_exist", params[1]);
                        return -1;
                }

                if (metacontact_remove_item(m, params[2], params[3], 0)) {
                        char *tmp1 = xstrdup(params[2]), *tmp2 = xstrdup(params[3]), *tmp3 = xstrdup(params[1]);

                        printq("metacontact_removed_item", session_alias_uid_n(params[2]), params[3], params[1]);

                        query_emit_id(NULL, METACONTACT_ITEM_REMOVED, &tmp1, &tmp2, &tmp3);
                        xfree(tmp1);
                        xfree(tmp2);
                        xfree(tmp3);
                }
                return 0;
        }


	if (params[0] && (m = metacontact_find(params[0]))) {
                metacontact_item_t *i = m->metacontact_items;

                if (!i) {
                        printq("metacontact_item_list_empty");
			return 0;
		}

		printq("metacontact_item_list_header", params[0]);
		
                for (; i; i = i->next) {
			userlist_t *u = userlist_find_n(i->s_uid, i->name);
			char *tmp;

			if (!u) 
				tmp = format_string(format_find("metacontact_info_unknown"));
			else    
				tmp = format_string(format_find(ekg_status_label(u->status, u->descr, "metacontact_info_")), u->nickname, u->descr);

                        printq("metacontact_item_list", session_alias_uid_n(i->s_uid), i->name, tmp, itoa(i->prio));
			xfree(tmp);
                }

		printq("metacontact_item_list_footer", params[0]);

                return 0;
	
	}

	if (params[0] && !params[1]) {
	        printq("metacontact_doesnt_exist", params[0]);
                return -1;
	}

	printq("invalid_params", name);

	return -1;
}

/* 
 * metacontact_find()
 * 
 * it looks for metacontact with given name 
 * it returns pointer to this metacontact 
 */
metacontact_t *metacontact_find(const char *name) 
{
	metacontact_t *m;

	for (m = metacontacts; m; m = m->next) {
        	if (!xstrcasecmp(name, m->name))
			return m;
	}

	return NULL;
}

/*
 * metacontact_add()
 * 
 * it adds metacontact to the list
 * returns pointer to added metacontact
 */
metacontact_t *metacontact_add(const char *name) 
{
	metacontact_t *m;

        if (!name)
                return NULL;

	m = xmalloc(sizeof(metacontact_t));
	m->name = xstrdup(name);

	metacontacts_add(m);
	return m;
}

/*
 * metacontact_find_item()
 * 
 * it looks for metacontact item 
 * returns pointer to this item
 */
static metacontact_item_t *metacontact_find_item(metacontact_t *m, const char *name, const char *uid)
{
        metacontact_item_t *i;

	if (!m)
		return NULL;

        for (i = m->metacontact_items; i; i = i->next) {
                if (!xstrcasecmp(name, i->name) && !xstrcasecmp(uid, i->s_uid))
                        return i;
        }

        return NULL;
}

/* 
 * metacontact_add_item()
 * 
 * it adds item to the metacontact
 * 
 * m - metacontact
 * session - session UID (!)
 * name - name of the contact in userlist
 * prio - prio
 * quiet - be quiet ? 
 */
static int metacontact_add_item(metacontact_t *m, const char *session, const char *name, unsigned int prio, int quiet)
{
	metacontact_item_t *i;
	session_t *s;
	char *uid;

	if (!m || !name || !session) {
		debug("! metacontact_add_item: NULL data on input\n");
		return 0;
	}

	if (!(s = session_find(session))) {
		printq("session_doesnt_exist", session);
		return 0;
	}
		/* XXX, bad session */
        if (!(uid = get_uid(s, name))) {
		printq("user_not_found", name);
                debug("! metacontact_add_item: UID is not on our contact lists: %s\n", name);
                return 0;
        }

	i = xmalloc(sizeof(metacontact_item_t));
	
	i->name		= xstrdup(uid);
	i->s_uid	= xstrdup(s->uid);
	i->prio		= prio;

	metacontact_items_add(&m->metacontact_items, i);

	return 1;
}

/* 
 * metacotact_remove_item()
 * 
 * it removes item from a list
 * returns 1 if success, if errors found 0 is returned
 */
static int metacontact_remove_item(metacontact_t *m, const char *session, const char *name, int quiet)
{
	metacontact_item_t *i;
	session_t *s;

        if (!m || !name || !session) {
                debug("! metacontact_add_item: NULL data on input\n");
                return 0;
        }

        if (!(s = session_find(session))) {
                printq("session_doesnt_exist", session);
                return 0;
        }


	if (!(i = metacontact_find_item(m, name, s->uid))) {
		printq("metacontact_item_doesnt_exist", session, name);	
		return 0;
	}	

	metacontact_items_remove(&m->metacontact_items, i);
	
	return 1;
}

/* 
 * metacontact_remove()
 * 
 * it removes metacontact from a list 
 * returns 0 if errors, 1 if successed 
 */
static int metacontact_remove(const char *name)
{
	metacontact_t *m = metacontact_find(name);
	metacontact_item_t *i;

        for (i = m->metacontact_items; i; ) {
		metacontact_item_t *next = i->next;

		metacontact_remove_item(m, i->s_uid, i->name, 1);
		i = next;
	}

	metacontacts_remove(m);

	return 1;
}

/* 
 * metacontact_session_renamed_handler()
 * 
 * when session alias is changed it had to be resorted in metacontact items 
 * list
 */
static int metacontact_session_renamed_handler(void *data, va_list ap)
{
	/* We don't change alias so frequently,
	 * so we can resort whole list, not only items w/ that session */

	/* XXX: exactly, why we do it? we don't even look at session alias
	 * 	in sorting function */
	LIST_RESORT2(&metacontacts, metacontact_add_item_compare);
#if 0
	char **tmp = va_arg(ap, char**);
	session_t *s = session_find(*tmp);
        char *session;
	metacontact_t *m;

	if (!s)
		return 0;

	session = s->uid;

	for (m = metacontacts; m; m = m->next) {
		metacontact_item_t *i;

		for (i = m->metacontact_items; i;) {
			metacontact_item_t *next = i->next;

			if (!xstrcasecmp(session, i->s_uid)) {
				char *s_uid, *name;
				int prio;
				
				s_uid = xstrdup(i->s_uid);
				name = xstrdup(i->name);
				prio = i->prio;
				
				metacontact_remove_item(m, s_uid, name, 1);
				metacontact_add_item(m, s_uid, name, prio, 1);
			
				xfree(s_uid);
				xfree(name);

					/* list has changed, so we need to start from beginning */
				m = metacontacts;
				break;
			}

			i = next;
		}
	}
#endif
	
	return 1;
}

/*
 * metacontact_userlist_removed_handler()
 *
 * when some user from userlist is removed we have to remove it
 * also from metacontact
 */
static int metacontact_userlist_removed_handler(void *data, va_list ap)
{
	/* XXX: disabled it to allow jabber metacontacts
	 * 	but think about it */
#if 0
        char **name = va_arg(ap, char**);
	metacontact_t *m;

        for (m = metacontacts; m; m = m->next) {
		metacontact_item_t *i;

                for (i = m->metacontact_items; i; i = i->next) {
			userlist_t *u;
			
			if (!i) 
				break;
			
			u = userlist_find_n(i->s_uid, i->name);
		
			if (!u)
				continue;
	
			if (u->nickname && !xstrcasecmp(*name, u->nickname)) {
				metacontact_remove_item(m, i->s_uid, i->name, 1);
				break;
			}

			if (!xstrcasecmp(*name, u->uid)) {
                                metacontact_remove_item(m, i->s_uid, i->name, 1);
				break;
			}
		}		
	}
#endif

	return 0;
}

/* 
 * metacontact_find_prio()
 * 
 * it finds and return metacontact_item with the 'most available'
 * status; if more than uids have the same one, it also looks
 * at the priority
 */
metacontact_item_t *metacontact_find_prio(metacontact_t *m)
{
        metacontact_item_t *i;
	metacontact_item_t *ret = NULL;
	userlist_t *last = NULL;

        if (!m)
                return NULL;

        for (i = m->metacontact_items; i; i = i->next) {
		userlist_t *u = userlist_find_n(i->s_uid, i->name);
		
		if (!u)
			continue;
		if (!ret) {
			ret = i;
			last = u;
			continue;
		}

		if (u->status > last->status || (u->status == last->status && i->prio > ret->prio)) {
			ret = i;
			last = u;
			continue;
		}
        }

        return ret;
}

/* 
 * metacontact_init()
 * 
 * initialization of the metacontacts 
 */
void metacontact_init()
{
	query_connect_id(NULL, SESSION_RENAMED, metacontact_session_renamed_handler, NULL);
	query_connect_id(NULL, USERLIST_REMOVED, metacontact_userlist_removed_handler, NULL);
}

/*
 * metacontact_write()
 * 
 * it writes info about metacontacts to file 
 */
int metacontact_write()
{
        metacontact_t *m;
        FILE *f = NULL;

        f = fopen(prepare_path("metacontacts", 1), "w");

        if (!f)
                return -1;

        for (m = metacontacts; m; m = m->next) {
		metacontact_item_t *i;
                fprintf(f, "[%s]\n", m->name);

                for (i = m->metacontact_items; i; i = i->next)
                        fprintf(f, "%s %s %d\n", i->s_uid, i->name, i->prio);
        }
        fclose(f);

        return 0;
}

/* 
 * metacontact_read()
 * 
 * it reads info about metacontacts from file 
 */
int metacontact_read()
{
        char *line;
        FILE *f;
        metacontact_t *m = NULL;

        if (!(f = fopen(prepare_path("metacontacts", 0), "r")))
                return -1;

        while ((line = read_file(f, 0))) {
                char *tmp;
		char **array = NULL;

                if (line[0] == '[') {
                        tmp = xstrchr(line, ']');

                        if (!tmp)
                                continue;

                        *tmp = 0;
                        m = metacontact_add(line + 1);

			continue;
                } 

		array = array_make(line, " ", 3, 1, 1);
		
		if (array_count(array) != 3) {
			debug("		metacontact_read: wrong number of forms\n");
			goto next;
		}

		metacontact_add_item(m, array[0], array[1], atoi(array[2]), 1);
next:
		array_free(array);
        }

        fclose(f);

        return 0;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
