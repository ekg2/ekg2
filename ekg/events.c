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

#include "events.h"
#include "dynstuff.h"
#include "sessions.h"
#include "stuff.h"
#include "themes.h"
#include "plugins.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

list_t events = NULL;
char **events_all = NULL;

/* 
 * on function 
 */
COMMAND(cmd_on)
{
        if(!session) {
                if(session_current)
                        session = session_current;
                else
                        return -1;
        }

	if (match_arg(params[0], 'a', "add", 2)) {
		int prio;

                if (!params[1] || !params[2] || !params[3] || !params[4]) {
                        printq("not_enough_params", name);
                        return -1;
                }

		if (!(prio = atoi(params[2])) || !array_contains(events_all, params[1], 0)) {
			printq("invalid_params", name);
			return -1;
		}
		
		if (event_add(params[1], prio, params[3], params[4], quiet)) {
                        config_changed = 1;
			return 0;
		} else
			return -1;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		int par;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], "*"))
			par = 0;
		else {
			if (!(par = atoi(params[1]))) {
				printq("invalid_params", name);			
				return -1;
			}
		}

		if (!event_remove(par, quiet)) {
			config_changed = 1;
			return 0;
		} else
			return -1;
	}

	if (!params[0] || match_arg(params[0], 'l', "list", 2) || params[0][0] != '-') {
		events_list((params[0] && params[1] && atoi(params[1])) ? atoi(params[1]) : 0, 0);
		return 0;
	}

	printq("invalid_params", name);

	return -1;
}

/*
 * event_add_compare()
 *
 * function that compare two events and returns bigger
 * it helps in list_add() of events
 *
 */
static int event_add_compare(void *data1, void *data2)
{
        event_t *a = data1, *b = data2;

        if (!a || !a->id || !b || !b->id)
                return 0;

        return a->id - b->id;
}


/* 
 * event_add ()
 * 
 * adds event to the events list
 * 
 * it finds id in the same way as in window_new()
 *
 * 0/-1
 */
int event_add(const char *name, int prio, const char *target, const char *action, int quiet)
{
	event_t ev;
	char *tmp;
	int done = 0, id = 1;
	list_t l;

	if (event_find(name, target)) {
		printq("events_exist", name, target);
		return -1;
	}
	
	while (!done) {
                done = 1;

                for (l = events; l; l = l->next) {
                        event_t *ev = l->data;

                        if (ev->id == id) {
                                done = 0;
                                id++;
                                break;
                        }
                }
        }

	ev.id = id;
	ev.name = xstrdup(name);
	ev.prio = prio;
	ev.target = xstrdup(target);
	ev.action = xstrdup(action);
	list_add_sorted(&events, &ev, sizeof(ev), event_add_compare);

	tmp = xstrdup(name);
	query_emit(NULL, "event-added", &tmp);
	xfree(tmp);

	printq("events_add", name);

	return 0;
}

/* 
 * event_remove ()
 * 
 * it removes event from events 
 * 
 * if (id == 0 ) it removes whole list
 * 
 * 0/-1 
 */
int event_remove(unsigned int id, int quiet)
{
        event_t *ev;
	
	if (id == 0) {
		event_free();
		printq("events_del_all");
		goto cleanup;
	}
	
	if (!(ev = event_find_id(id))) {
		printq("events_del_noexist", itoa(id));
		return -1;
	}
	
        xfree(ev->name);
        xfree(ev->action);
        xfree(ev->target);
	
	list_remove(&events, ev, 1);

	printq("events_del", itoa(id));

cleanup:	
        query_emit(NULL, "event-removed", itoa(id));

	return 0;
}

/* 
 * events_list ()
 * 
 * it shows the list of events 
 */
int events_list(int id, int quiet)
{
        list_t l;

	if (!events) {
        	printq("events_list_empty");
		return 0;
	}

	printq("events_list_header");

	for (l = events; l; l = l->next) {
	        event_t *ev = l->data;

		if (!id || id == ev->id)
	                printq("events_list", ev->name, itoa(ev->prio), ev->target, ev->action, itoa(ev->id));
        }

	return 0;
}

/*
 * event_find ()
 *
 * it finds the event and return (if found) descriptor
 * to event
 *
 */
event_t *event_find(const char *name, const char *target)
{
        list_t l;
	event_t *ev_max = NULL;
	int ev_max_prio = 0;
	char **b, **c;

        debug("// event_find (name (%s), target (%s)\n", name, target);
	b = array_make(target, "|,;", 0, 1, 0);
	c = array_make(name, "|,;", 0, 1, 0);
        for (l = events; l; l = l->next) {
                event_t *ev = l->data;
                char **a, **d;
                int i, j, k, m;

                a = array_make(ev->target, "|,;", 0, 1, 0);
		d = array_make(ev->name, "|,;", 0, 1, 0);
                for (i = 0; a[i]; i++) {
			for (j = 0; b[j]; j++) {
				for (k = 0; c[k]; k++) {
					for (m = 0; d[m]; m++) {
				                if (xstrcasecmp(d[m], c[k]) || xstrcasecmp(a[i], b[j]))
        		        	                continue;
                		        	else if (ev->prio > ev_max_prio){
	                        	        	ev_max = ev;
							ev_max_prio = ev->prio;
						}
					}
				}
			}
                }
                array_free(a);
		array_free(d);
        }

	array_free(b);
	array_free(c);

        return (ev_max) ? ev_max : NULL;
}

/*
 * event_find_all ()
 *
 * it finds the event including possibility of * and return (if found) 
 * descriptor to event
 *
 */
event_t *event_find_all(const char *name, const char *uid, const char *target, const char *data)
{
        list_t l;
	event_t *ev_max = NULL;
	int ev_max_prio = 0;
	char **b, **c;

        debug("// event_find_all (name (%s), target (%s)\n", name, target);
	b = array_make(target, "|,;", 0, 1, 0);
	c = array_make(name, "|,;", 0, 1, 0);
        for (l = events; l; l = l->next) {
                event_t *ev = l->data;
                char **a, **d;
                int i, j, k, m;

                a = array_make(ev->target, "|,;", 0, 1, 0);
		d = array_make(ev->name, "|,;", 0, 1, 0);
                for (i = 0; a[i]; i++) {
			for (j = 0; b[j]; j++) {
				for (k = 0; c[k]; k++) {
					for (m = 0; d[m]; m++) {
						char *tmp = format_string(a[i], uid, target, data);
						if ((xstrcasecmp(d[m], c[k]) && xstrcasecmp(d[m], "*")) || (!event_target_check(tmp) && xstrcasecmp(a[i], "*") && xstrcasecmp(a[i], b[j]))) {
							xfree(tmp);
        		        	                continue;
						} else if (ev->prio > ev_max_prio){
	                        	        	ev_max = ev;
							ev_max_prio = ev->prio;
						}
						xfree(tmp);
					}
				}
			}
                }
                array_free(a);
		array_free(d);
        }

	array_free(b);
	array_free(c);

        return (ev_max) ? ev_max : NULL;
}

/*
 * event_find ()
 *
 * it finds the event (by the id) and return (if found) 
 * descriptor to event
 *
 */
event_t *event_find_id(unsigned int id)
{
        list_t l;

        for (l = events; l; l = l->next) {
                event_t *ew = l->data;

                if (ew->id != id)
                        continue;
                else
                        return ew;
        }

        return 0;
}

void events_add_handler(char *name, void *function)
{
        query_connect(NULL, name, function, NULL);
        array_add(&events_all, name);
}

/* 
 * events_init ()
 * 
 * initializing of events and its handlers
 */
int events_init()
{
	events_add_handler("protocol-message", event_protocol_message);
	events_add_handler("event_avail", event_avail);
	events_add_handler("event_away", event_away);
	events_add_handler("event_na", event_na);
	events_add_handler("event_online", event_online);
	events_add_handler("event_descr", event_descr);
	return 0;
}

/* 
 * event_protocol_message()
 * 
 * handler for protocol-message 
 */
int event_protocol_message(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        char ***__rcpts = va_arg(ap, char***), **rcpts = *__rcpts;
        char **__text = va_arg(ap, char**), *text = *__text;
        session_t *session_class = session_find(session);
        userlist_t *userlist = userlist_find(session_class, uid);

	rcpts = NULL;
	if (userlist && userlist->nickname)
		event_check(session, "protocol-message", userlist->nickname, text);
	else
		event_check(session, "protocol-message", uid, text);

	return 0;
}

/*
 * event_avail ()
 *
 * handler for changing status on available
 */
int event_avail(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        session_t *session_class = session_find(session);
        userlist_t *userlist = userlist_find(session_class, uid);

	if (userlist && userlist->nickname)
		event_check(session, "event_avail", userlist->nickname, NULL);
	else
		event_check(session, "event_avail", uid, NULL);

	return 0;
}

/*
 * event_away ()
 *
 * handler for changing status on away
 */
int event_away(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        session_t *session_class = session_find(session);
        userlist_t *userlist = userlist_find(session_class, uid);

        if (userlist && userlist->nickname)
                event_check(session, "event_away", userlist->nickname, NULL);
        else
                event_check(session, "event_away", uid, NULL);

        return 0;
}

/*
 * event_na ()
 *
 * handler for changing status on NA
 */
int event_na(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        session_t *session_class = session_find(session);
        userlist_t *userlist = userlist_find(session_class, uid);

        if (userlist && userlist->nickname)
                event_check(session, "event_na", userlist->nickname, NULL);
        else
                event_check(session, "event_na", uid, NULL);

        return 0;
}

/*
 * event_online ()
 *
 * handler for changing status from NA to avail
 */
int event_online(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
        session_t *session_class = session_find(session);
        userlist_t *userlist = userlist_find(session_class, uid);

        if (userlist && userlist->nickname)
                event_check(session, "event_online", userlist->nickname, NULL);
        else
                event_check(session, "event_online", uid, NULL);

        return 0;
}

/*
 * event_descr ()
 *
 * handler for changing description
 */
int event_descr(void *data, va_list ap)
{
        char **__session = va_arg(ap, char**), *session = *__session;
        char **__uid = va_arg(ap, char**), *uid = *__uid;
	char **__descr = va_arg(ap, char**), *descr = *__descr;
        session_t *session_class = session_find(session);
        userlist_t *userlist = userlist_find(session_class, uid);

        if (userlist && userlist->nickname)
                event_check(session, "event_descr", userlist->nickname, descr);
        else
                event_check(session, "event_descr", uid, descr);

        return 0;
}


/* event_check ()
 * 
 * it looks if the given event has a handler
 * if yes it runs it
 * it also check target and if possible uid taken from target
 *
 */
int event_check(const char *session, const char *name, const char *target, const char *data)
{
	session_t *__session;
        char *action = NULL, **actions, *edata = NULL;
	char *uid;
	int i;
	event_t *ev = NULL;

	if (!events)
		return 1;

        if (!(__session = session_find(session)))
		__session = session_current;

	if (__session)
                uid = get_uid(__session, target);
        else
                uid = NULL;

	if (uid && ignored_check(__session, uid) & IGNORE_EVENTS) {
		return -1;
	}

	if (!(ev = event_find_all(name, uid, target, data)))
		return -1;

	action = ev->action;

        if (!action)
                return -1;

        if (data) {
                int size = 1;
                const char *p;
                char *q;

                for (p = data; *p; p++) {
                        if (strchr("`!#$&*?|\\\'\"{}[]<>()\n\r", *p))
                                size += 2;
                        else
                                size++;
                }

                edata = xmalloc(size);

                for (p = data, q = edata; *p; p++, q++) {
                        if (strchr("`!#$&*?|\\\'\"{}[]<>()", *p))
                                *q++ = '\\';
                        if (*p == '\n') {
                                *q++ = '\\';
                                *q = 'n';
                                continue;
                        }
                        if (*p == '\r') {
                                *q++ = '\\';
                                *q = 'r';
                                continue;
                        }
                        *q = *p;
                }

                *q = 0;
        }

	actions = array_make(action, ";", 0, 0, 1);

	for (i = 0; actions && actions[i]; i++) {
	        char *tmp = format_string(strip_spaces(actions[i]), (uid) ? uid : target, target, ((data) ? data : ""), ((edata) ? edata : ""), session_uid_get(__session));

		debug("// event_check() calling \"%s\"\n", tmp);		

		command_exec(NULL, NULL, tmp, 0);
		xfree(tmp);
	}

	array_free(actions);

        return 0;
}

/* 
 * event_free ()
 *
 * it frees whole list 
 */
void event_free()
{
	list_t l;

	if (!events)
		return;

	for (l = events; l; l = l->next) {
		struct event *e = l->data;

		xfree(e->action);
	}

	list_destroy(events, 1);
	xfree(events_all);
	events_all = NULL;
	events = NULL;
}

/*
 * event_target_check_compare()
 *
 * it only compares given as an argument values
 *
 * returns logical value
 */
int event_target_check_compare(char *buf)
{
        string_t s;

        s = string_init("");

        while(*buf) {
                if (*buf == '/') {
                        *buf++;
                        if (!*buf)
                                break;
                        *buf++;
                        if (!*buf)
                                break;
                        continue;
                }

                if (*buf == '=') {
                        *buf++;

                        if (!*buf)
                                break;

                        if (*buf == '=') /* '==' */ {
                                *buf++;
				if (!*buf)
					break;

				return (!xstrcmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
			}

                        /* '=' */
                	return (!xstrcasecmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
		}

                if (*buf == '!') {
                        *buf++;

                        if (!*buf)
                                break;

                        if (*buf == '=') {
                                *buf++;

                                if (!*buf)
                                        break;

                                if (*buf == '=') { /* '!==' */
        	                        *buf++;
	                                if (!*buf)
                	                        break;

					return (xstrcmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
                                }

                                /* '!=' */
				return (xstrcasecmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
                        }

                        if (*buf == '+') {
                                *buf++;

                                if (!*buf)
                                        break;

                                if (*buf == '+') { /* '!++' */
                                        *buf++;
                                        if (!*buf)
                                                break;

                	                return (!xstrstr(buf, s->str) && !string_free(s, 1)) ? 1 : 0;
				}

			
				/* '!+' */
	                        return (!xstrcasestr(buf, s->str) && !string_free(s, 1)) ? 1 : 0;
			}

                        continue;
                }

		if (*buf == '+') {
			*buf++;

                        if (!*buf)
                                break;
			
			if (*buf == '+') { /* '++' */
	                        *buf++;
        
	                        if (!*buf)
        	                        break;

				return (xstrstr(buf, s->str) && !string_free(s, 1)) ? 1 : 0;
 			}

			/* '+' */
			return (xstrcasestr(buf, s->str) && !string_free(s, 1)) ? 1 : 0;
		}

                string_append_c(s, *buf);
                *buf++;
        }
	
	string_free(s, 1);
	return 0;
}

/* 
 * event_target_check()
 *
 * it has to get as an argument parametr to check
 *
 * returns logical value of given expression
 */

int event_target_check(char *buf)
{
	char **params = array_make(buf, "&|", 0, 1, 1);
	int i = 1;
	char *separators;
	char last_returned = 0;
	int first = 1;
	
#define s separators[i]

	if (!params)
		return -1;
	
	separators = xmalloc(array_count(params) * sizeof(char) + 1);
	
	while (*buf) {
		if (*buf == '&' || *buf == '|') {
			s = *buf;
			i++;	
		}
		*buf++;
	}
	for (i = 0; params[i]; i++) {
		int returned_now;

		returned_now = event_target_check_compare(params[i]);
		if (s && s == '&') {
			if (returned_now && last_returned)
				last_returned = 1;
			else
				last_returned = 0;	
		} else if (s && s == '|') {
			if (returned_now || last_returned)
				last_returned = 1;
			else
				last_returned = 0;
		}

		if (first) {
			last_returned = returned_now;
			first = 0;
		}

	}
#undef s

	xfree(separators);
	array_free(params);

	return last_returned;
}

