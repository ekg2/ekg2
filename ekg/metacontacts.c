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

#include "metacontacts.h"

list_t metacontacts = NULL;

/* 
 * metacontact function support
 */
COMMAND(cmd_metacontact)
{
        metacontact_t *m;

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2)) {
                list_t l;

                for (l = metacontacts; l; l = l->next) {
                        metacontact_t *m = l->data;
                        
			printq("metacontact_list", m->name);
                }

                if (!metacontacts)
                        printq("metacontact_list_empty");

                return 0;
        }

	if (match_arg(params[0], 'a', ("add"), 2)) {
                if (!params[1])
                        goto invalid_params;

		if (metacontact_find(params[1])) {
                        printq("metacontact_exists", params[1]);
                        return -1;
		}

		if (metacontact_add(params[1])) {
			char *tmp = xstrdup(params[1]);

	                config_changed = 1;
	                printq("metacontact_added", params[1]);

	                query_emit(NULL, ("metacontact-added"), &tmp);
			xfree(tmp);
		}
		return 0;	
	}

        if (match_arg(params[0], 'd', ("del"), 2)) {
		if (!params[1])
			goto invalid_params;

                if (!metacontact_find(params[1])) {
                        printq("metacontact_doesnt_exist", params[1]);
                        return -1;
                }

                if (metacontact_remove(params[1])) {
			char *tmp = xstrdup(params[1]);

	                config_changed = 1;
	                printq("metacontact_removed", params[1]);
			
	                query_emit(NULL, ("metacontact-removed"), &tmp);
			xfree(tmp);
		}
                return 0;
	}

	if (match_arg(params[0], 'i', ("add-item"), 2)) {
                if (!params[1] || !params[2] || !params[3] || !params[4])
                        goto invalid_params;

                if (!(m = metacontact_find(params[1]))) {
                        printq("metacontact_doesnt_exist", params[1]);
                        return -1;
                }

		if (metacontact_add_item(m, params[2], params[3], atoi(params[4]), 0)) {
			char *tmp1 = xstrdup(params[2]), *tmp2 = xstrdup(params[3]), *tmp3 = xstrdup(params[1]);

			printq("metacontact_added_item", session_alias_uid_n(params[2]), params[3], params[1]);

	                query_emit(NULL, ("metacontact-item-added"), &tmp1, &tmp2, &tmp3);
			xfree(tmp1);
			xfree(tmp2);
			xfree(tmp3);
		}
		return 0;
	}

        if (match_arg(params[0], 'r', ("del-item"), 2)) {
                if (!params[1] || !params[2] || !params[3])
                        goto invalid_params;

                if (!(m = metacontact_find(params[1]))) {
                        printq("metacontact_doesnt_exist", params[1]);
                        return -1;
                }

                if (metacontact_remove_item(m, params[2], params[3], 0)) {
                        char *tmp1 = xstrdup(params[2]), *tmp2 = xstrdup(params[3]), *tmp3 = xstrdup(params[1]);

                        printq("metacontact_removed_item", session_alias_uid_n(params[2]), params[3], params[1]);

                        query_emit(NULL, ("metacontact-item-removed"), &tmp1, &tmp2, &tmp3);
                        xfree(tmp1);
                        xfree(tmp2);
                        xfree(tmp3);
                }
                return 0;
        }


	if (params[0] && (m = metacontact_find(params[0]))) {
                list_t l;

                if (!m->metacontact_items) {
                        printq("metacontact_item_list_empty");
			return 0;
		}

		printq("metacontact_item_list_header", params[0]);
		
                for (l = m->metacontact_items; l; l = l->next) {
                        metacontact_item_t *i = l->data;
			userlist_t *u = userlist_find_n(i->s_uid, i->name);
			char *tmp;

			if (!u) 
				tmp = format_string(format_find("metacontact_info_unknown"));
			else    
				tmp = format_string(format_find(ekg_status_label(u->status, u->descr, "metacontact_info_")), (u->first_name) ? u->first_name : u->nickname, u->descr);

                        printq("metacontact_item_list", session_alias_uid_n(i->s_uid), i->name, tmp, itoa(i->prio));
			xfree(tmp);
                }

		printq("metacontact_item_list_footer", params[0]);

                return 0;
	
	}

	if (params[0] && !params[1]) {
	        printq("metacontact_doesnt_exist", params[0]);
                return 0;
	}
invalid_params:
	printq("invalid_params", name);

	return 0;
}

/* 
 * metacontact_find()
 * 
 * it looks for metacontact with given name 
 * it returns pointer to this metacontact 
 */
metacontact_t *metacontact_find(const char *name) 
{
	list_t l;

	for (l = metacontacts; l; l = l->next) {
        	metacontact_t *m = l->data;

        	if (!xstrcasecmp(name, m->name))
			return m;
	}

	return NULL;
}

/* 
 * metacontact_add_compare()
 *
 * helpfull function when adding to the metacontacts list 
 */
static int metacontact_add_compare(void *data1, void *data2)
{
        metacontact_t *a = data1, *b = data2;

        if (!a || !a->name || !b || !b->name)
                return 0;

        return xstrcasecmp(a->name, b->name);
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

	return list_add_sorted(&metacontacts, m, 0, metacontact_add_compare);
}

/*
 * metacontact_add_item_compare()
 * 
 * heplfull when adding to items list
 */
static int metacontact_add_item_compare(void *data1, void *data2)
{
        metacontact_item_t *a = data1, *b = data2;

        if (!a || !a->name || !a->s_uid || !b || !b->name || !b->s_uid)
                return 0;

	if (!xstrcasecmp(a->s_uid, b->s_uid))
		return xstrcasecmp(a->name, b->name);

        return xstrcasecmp(session_alias_uid_n(a->s_uid), session_alias_uid_n(b->s_uid));
}

/*
 * metacontact_find_item()
 * 
 * it looks for metacontact item 
 * returns pointer to this item
 */
metacontact_item_t *metacontact_find_item(metacontact_t *m, const char *name, const char *uid)
{
        list_t l;

	if (!m)
		return NULL;

        for (l = m->metacontact_items; l; l = l->next) {
                metacontact_item_t *i = l->data;

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
int metacontact_add_item(metacontact_t *m, const char *session, const char *name, unsigned int prio, int quiet)
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

        if (!(uid = get_uid(s, name))) {
		printq("user_not_found", name);
                debug("! metacontact_add_item: UID is not on our contact lists: %s\n", name);
                return 0;
        }

	i = xmalloc(sizeof(metacontact_item_t));
	
	i->name		= xstrdup(uid);
	i->s_uid	= xstrdup(s->uid);
	i->prio		= prio;

	list_add_sorted(&m->metacontact_items, i, 0, metacontact_add_item_compare);

	return 1;
}

/* 
 * metacotact_remove_item()
 * 
 * it removes item from a list
 * returns 1 if success, if errors found 0 is returned
 */
int metacontact_remove_item(metacontact_t *m, const char *session, const char *name, int quiet)
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

	xfree(i->name);
	xfree(i->s_uid);
	list_remove(&m->metacontact_items, i, 1);
	
	return 1;
}

/* 
 * metacontact_remove()
 * 
 * it removes metacontact from a list 
 * returns 0 if errors, 1 if successed 
 */
int metacontact_remove(const char *name)
{
	metacontact_t *m = metacontact_find(name);
	list_t l;

        for (l = m->metacontact_items; l; ) {
		metacontact_item_t *i = l->data;

		l = l->next;

		metacontact_remove_item(m, i->s_uid, i->name, 1);
	}

        list_destroy(m->metacontact_items, 1);
        xfree(m->name);

        list_remove(&metacontacts, m, 1);

	return 1;
}

/* 
 * metacontact_session_renamed_handler()
 * 
 * when session alias is changed it had to be resorted in metacontact items 
 * list
 */
int metacontact_session_renamed_handler(void *data, va_list ap)
{
	char **tmp = va_arg(ap, char**);
	session_t *s = session_find(*tmp);
        char *session;
	list_t l;

	if (!s)
		return 0;

	session = s->uid;

	for (l = metacontacts; l; l = l->next) {
        	metacontact_t *m = l->data;
		list_t l2;

		for (l2 = m->metacontact_items; l2;) {
			metacontact_item_t *i = (l2 && l2->data) ? l2->data : NULL;
			
			if (!i)
				break;

			l2 = l2->next;

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
			}
		}
	}
	
	return 1;
}

/*
 * metacontact_userlist_removed_handler()
 *
 * when some user from userlist is removed we have to remove it
 * also from metacontact
 */
int metacontact_userlist_removed_handler(void *data, va_list ap)
{
        char **name = va_arg(ap, char**);
        list_t l;

        for (l = metacontacts; l; l = l->next) {
                metacontact_t *m = l->data;
                list_t l2;

                for (l2 = m->metacontact_items; l2; l2 = l2->next) {
			metacontact_item_t *i = (l2 && l2->data) ? l2->data : NULL;
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

	return 0;
}

/* 
 * metacontact_find_prio()
 * 
 * it finds and return metacontact_item with the higher priority 
 * first it looks at the status, then if all are notavail the one 
 * with higher priority is returned 
 */
metacontact_item_t *metacontact_find_prio(metacontact_t *m)
{
        list_t l;
	metacontact_item_t *ret = NULL;
	userlist_t *last = NULL;

        if (!m)
                return NULL;

        for (l = m->metacontact_items; l; l = l->next) {
                metacontact_item_t *i = l->data;
		userlist_t *u = userlist_find_n(i->s_uid, i->name);
		
		if (!u)
			continue;
		if (!ret) {
			ret = i;
			last = u;
			continue;
		}

		if (xstrcasecmp(last->status, EKG_STATUS_NA) && xstrcasecmp(u->status, EKG_STATUS_NA) && ret->prio < i->prio) {
			ret = i;
			last = u;
			continue;
		}

		if (!xstrcasecmp(last->status, EKG_STATUS_NA) && xstrcasecmp(u->status, EKG_STATUS_NA)) {
			ret = i;
			last = u;
			continue;
		}
		
		if (!xstrcasecmp(last->status, EKG_STATUS_NA) && !xstrcasecmp(u->status, EKG_STATUS_NA) && ret->prio < i->prio) {
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
        query_connect(NULL, ("session-renamed"), metacontact_session_renamed_handler, NULL);
	query_connect(NULL, ("userlist-removed"), metacontact_userlist_removed_handler, NULL);
}

/* 
 * metacontact_free()
 * 
 * it should free all memory user by metacontacts
 */
void metacontact_free()
{
	list_t l;

        for (l = metacontacts; l;) {
                metacontact_t *m = l->data;
		
		l = l->next;
		metacontact_remove(m->name);		
	}

        list_destroy(metacontacts, 1);
}

/*
 * metacontact_write()
 * 
 * it writes info about metacontacts to file 
 */
int metacontact_write()
{
        list_t l;
        FILE *f = NULL;

        f = fopen(prepare_path("metacontacts", 1), "w");

        if (!f)
                return -1;

        for (l = metacontacts; l; l = l->next) {
                metacontact_t *m = l->data;
                list_t lp;

                fprintf(f, "[%s]\n", m->name);

                for (lp = m->metacontact_items; lp; lp = lp->next) {
                        metacontact_item_t *i = lp->data;

                        fprintf(f, "%s %s %d\n", i->s_uid, i->name, i->prio);
                }
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

        while ((line = read_file(f))) {
                char *tmp;
		char **array = NULL;

                if (line[0] == '[') {
                        tmp = xstrchr(line, ']');

                        if (!tmp)
                                goto next;

                        *tmp = 0;
                        m = metacontact_add(line + 1);

                        goto next;
                } 

		array = array_make(line, " ", 3, 1, 1);
		
		if (array_count(array) != 3) {
			debug("		metacontact_read: wrong number of forms\n");
			goto next;
		}
			
		metacontact_add_item(m, array[0], array[1], atoi(array[2]), 1);
next:
		array_free(array);
                xfree(line);
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
