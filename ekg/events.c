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
#include <time.h>

#include "commands.h"
#include "debug.h"
#include "events.h"
#include "plugins.h"
#include "sessions.h"
#include "userlist.h"
#include "stuff.h"
#include "xmalloc.h"

#include "themes.h"
#include "windows.h"

#include "queries.h"

event_t *events = NULL;
char **events_all = NULL;

int config_display_day_changed = 1;

static QUERY(event_protocol_message);
static QUERY(event_avail);
static QUERY(event_online);
static QUERY(event_away);
static QUERY(event_na);
static QUERY(event_descr);
static QUERY(event_misc);

static TIMER(ekg_day_timer);

static void events_add_handler(char *name, void *function);
static event_t *event_find(const char *name, const char *target);
static event_t *event_find_id(unsigned int id);
static int event_remove(unsigned int id, int quiet);
static int events_list(int id, int quiet);

static int event_target_check(char *buf);
static int event_check(const char *session, const char *name, const char *uid, const char *data);

/* 
 * on function 
 */
COMMAND(cmd_on) {
	if (match_arg(params[0], 'a', ("add"), 2)) {
		int prio;

                if (!params[1] || !params[2] || !params[3] || !params[4]) {
                        printq("not_enough_params", name);
                        return -1;
                }

		if (!(prio = atoi(params[2]))) {
			printq("invalid_params", name);
			return -1;
		}

/* first we add event, because event_add() can fail. */
		
		if (event_add(params[1], prio, params[3], params[4], quiet)) 
			return -1;

/* after it we do query_connect() if we don't know it in events_all */

		if (!array_contains(events_all, params[1], 0)) {
			query_t *q;

			debug("cmd_on, array_contains(events_all, \"%s\", 0) failed. Binding new query: %s\n", params[1], params[1]);

			q = query_connect(NULL, params[1], event_misc, NULL);
			q->data = (char *) query_name(q->id);		/* hack */
		}

		config_changed = 1;
		return 0;
	}

	if (match_arg(params[0], 'd', ("del"), 2)) {
		int par;

		if (!params[1]) {
			printq("not_enough_params", name);
			return -1;
		}

		if (!xstrcmp(params[1], ("*")))
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

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2) || params[0][0] != '-') {
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
static LIST_ADD_COMPARE(event_add_compare, event_t *) { return data1->id - data2->id; }

/* 
 * event_add ()
 * 
 * adds event to the events list
 * 
 * it finds id in the same way as in window_new()
 *
 * 0/-1
 */
int event_add(const char *name, int prio, const char *target, const char *action, int quiet) {
	event_t *ev;
	char *tmp;
	int done = 0, id = 1;

	if (event_find(name, target)) {
		printq("events_exist", name, target);
		return -1;
	}
	
	while (!done) {
                done = 1;

                for (ev = events; ev; ev = ev->next) {
                        if (ev->id == id) {
                                done = 0;
                                id++;
                                break;
                        }
                }
        }
	ev		= xmalloc(sizeof(event_t));
	ev->id		= id;
	ev->name 	= xstrdup(name);
	ev->prio 	= prio;
	ev->target 	= xstrdup(target);
	ev->action 	= xstrdup(action);
	LIST_ADD_SORTED2(&events, ev, event_add_compare);

	tmp = xstrdup(name);
	query_emit_id(NULL, EVENT_ADDED, &tmp);
	xfree(tmp);

	printq("events_add", name);

	return 0;
}

static LIST_FREE_ITEM(list_event_free, struct event *) {
	xfree(data->name);
	xfree(data->action);
	xfree(data->target);
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
static int event_remove(unsigned int id, int quiet) {
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

	LIST_REMOVE2(&events, ev, list_event_free);

	printq("events_del", itoa(id));

cleanup:	
/*        query_emit_id(NULL, EVENT_REMOVED, itoa(id)); */	/* XXX, incorrect. */

	return 0;
}

/* 
 * event_free ()
 *
 * it frees whole list 
 */
void event_free() {
	xfree(events_all);
	events_all = NULL;

	if (!events)
		return;

	LIST_DESTROY2(events, list_event_free);
	events = NULL;
}

/* 
 * events_list ()
 * 
 * it shows the list of events 
 */
static int events_list(int id, int quiet) {
        event_t *ev;

	if (!events) {
        	printq("events_list_empty");
		return 0;
	}

	printq("events_list_header");

	for (ev = events; ev; ev = ev->next) {
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
event_t *event_find(const char *name, const char *target) {
	event_t *ev, *ev_max = NULL;
	int ev_max_prio = 0;
	char **b, **c;

	debug("// event_find (name (%s), target (%s)\n", name, target);
	b = array_make(target, ("|,;"), 0, 1, 0);
	c = array_make(name, ("|,;"), 0, 1, 0);
	for (ev = events; ev; ev = ev->next) {
		char **a, **d;
		int i, j, k, m;

		a = array_make(ev->target, ("|,;"), 0, 1, 0);
		d = array_make(ev->name, ("|,;"), 0, 1, 0);
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
static event_t *event_find_all(const char *name, const char *session, const char *uid, const char *target, const char *data) {
	event_t *ev, *ev_max = NULL;
	int ev_max_prio = 0;
	char **b, **c;

//	debug("// event_find_all (session %s) (name (%s), target (%s)\n", session, name, target);
	b = array_make(target, ("|,;"), 0, 1, 0);
	c = array_make(name, ("|,;"), 0, 1, 0);
	for (ev = events; ev; ev = ev->next) {
		char **a, **d;
		int i, j, k, m;

		a = array_make(ev->target, ("|,;"), 0, 1, 0);
		d = array_make(ev->name, ("|,;"), 0, 1, 0);
		for (i = 0; a[i]; i++) {
			for (j = 0; b[j]; j++) {
				for (k = 0; c[k]; k++) {
					for (m = 0; d[m]; m++) {
						char *tmp = format_string(a[i], uid, target, data, session);
						if ((xstrcasecmp(d[m], c[k]) && xstrcasecmp(d[m], ("*"))) || 
								(!event_target_check(tmp) && xstrcasecmp(a[i], ("*")) && 
								 xstrcasecmp(a[i], b[j]))) {
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
static event_t *event_find_id(unsigned int id) {
	event_t *ew;

        for (ew = events; ew; ew = ew->next) {
                if (ew->id != id)
                        continue;
                else
                        return ew;
        }

        return 0;
}

static void events_add_handler(char *name, void *function) {
        query_connect(NULL, name, function, NULL);
        array_add(&events_all, name);
}

/* 
 * events_init ()
 * 
 * initializing of events and its handlers
 */
int events_init() {
	timer_add(NULL, "daytimer", 1, 1, ekg_day_timer, NULL);

	events_add_handler(("protocol-message"), event_protocol_message);
	events_add_handler(("event_avail"), event_avail);
	events_add_handler(("event_away"), event_away);
	events_add_handler(("event_na"), event_na);
	events_add_handler(("event_online"), event_online);
	events_add_handler(("event_descr"), event_descr);
	return 0;
}

static TIMER(ekg_day_timer) {
	static struct tm *oldtm = NULL;
	struct tm *tm;
	time_t now = time(NULL);

	if (type) {
		xfree(oldtm);
		return 0;
	}
	tm = localtime(&now);
#define dayischanged(x) (oldtm->tm_##x != tm->tm_##x)
	if (oldtm && (dayischanged(mday) /* day */ || dayischanged(mon) /* month */ || dayischanged(year)) /* year */)  {
		if (config_display_day_changed) {
			list_t l;
			char *ts = xstrdup(timestamp("%d %b %Y"));

			for (l = windows; l; l = l->next) {
				window_t *w = l->data;
				int oldact;
				
				if (!w || w->id == 0 || w->floating) continue; /* skip __debug && (floatings windows [__lastlog, __contacts, ...]) */

				oldact = w->act;	/* save old act */
				w->lock++;		/* lock window */
				print_window_w(w, 0, "day_changed", ts);
				w->lock--;		/* unlock window */
				w->act = oldact;	/* restore old act */
			}
			xfree(ts);

			query_emit_id(NULL, UI_WINDOW_REFRESH);
		}
		debug("[EKG2] day changed to %.2d.%.2d.%.4d\n", tm->tm_mday, tm->tm_mon+1, tm->tm_year+1900);
		query_emit_id(NULL, DAY_CHANGED, &tm, &oldtm);
#undef dayischanged
	} else if (!oldtm) {
		oldtm = xmalloc(sizeof(struct tm));
	} else return 0;

	memcpy(oldtm, tm, sizeof(struct tm));
	return 0;
}

/* 
 * event_protocol_message()
 * 
 * handler for protocol-message 
 */
static QUERY(event_protocol_message) {
        char *session	= *(va_arg(ap, char**));
        char *uid	= *(va_arg(ap, char**));
        	char ***UNUSED(rcpts)	= va_arg(ap, char***);
        char *text	= *(va_arg(ap, char**));

	event_check(session, "protocol-message", uid, text);
	return 0;
}

/*
 * event_avail ()
 *
 * handler for changing status on available
 */
static QUERY(event_avail) {
        char *session	= *(va_arg(ap, char**));
        char *uid	= *(va_arg(ap, char**));

	event_check(session, "event_avail", uid, NULL);
	return 0;
}

/*
 * event_away ()
 *
 * handler for changing status on away
 */
static QUERY(event_away) {
        char *session	= *(va_arg(ap, char**));
        char *uid	= *(va_arg(ap, char**));

	event_check(session, "event_away", uid, NULL);
        return 0;
}

/*
 * event_na ()
 *
 * handler for changing status on NA
 */
static QUERY(event_na) {
        char *session	= *(va_arg(ap, char**));
        char *uid	= *(va_arg(ap, char**));

	event_check(session, "event_na", uid, NULL);
        return 0;
}

/*
 * event_online ()
 *
 * handler for changing status from NA to avail
 */
static QUERY(event_online) {
        char *session	= *(va_arg(ap, char**));
        char *uid	= *(va_arg(ap, char**));

	event_check(session, "event_online", uid, NULL);
        return 0;
}

/*
 * event_descr ()
 *
 * handler for changing description
 */
static QUERY(event_descr) {
        char *session	= *(va_arg(ap, char**));
        char *uid	= *(va_arg(ap, char**));
	char *descr	= *(va_arg(ap, char**));
	
	event_check(session, "event_descr", uid, descr);
        return 0;
}

static QUERY(event_misc) {
	event_check(NULL, data, "*", NULL);
	return 0;
}


/* event_check ()
 * 
 * it looks if the given event has a handler
 * if yes it runs it
 * it also check target and if possible uid taken from target
 *
 */
static int event_check(const char *session, const char *name, const char *uid, const char *data) {
	session_t *__session;
	userlist_t *userlist;
	event_t *ev;
	const char *target;
        char *action, **actions;
	char *edata = NULL;
	int i;

	if (!events)
		return 1;
	
        if (!(__session = session_find(session)))
		__session = session_current;

	if (uid && ignored_check(__session, uid) & IGNORE_EVENTS) {
		return -1;
	}

	userlist = userlist_find(__session, uid);
	target = (userlist && userlist->nickname) ? userlist->nickname : uid;

	if (!(ev = event_find_all(name, session, uid, target, data)))
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

	actions = array_make(action, (";"), 0, 0, 1);

	for (i = 0; actions && actions[i]; i++) {
	        char *tmp = format_string(strip_spaces(actions[i]), (uid) ? uid : target, target, ((data) ? data : ""), ((edata) ? edata : ""), session_uid_get(__session));

		debug("// event_check() calling \"%s\"\n", tmp);
		command_exec(NULL, NULL, tmp, 0); /* BUG? CHECK: hm, we've got specified session, not current one... target too.. so is it correct ? */
		xfree(tmp);
	}

	array_free(actions);
	xfree(edata);

        return 0;
}

/*
 * event_target_check_compare()
 *
 * it only compares given as an argument values
 *
 * returns logical value
 */
static int event_target_check_compare(char *buf) {
	string_t s;

	s = string_init(NULL);

	while(*buf) {
		if (*buf == '/') {
			buf++;
			if (!*buf)
				break;
			buf++;
			if (!*buf)
				break;
			continue;
		}

		if (*buf == '=') {
			buf++;

			if (!*buf)
				break;

			if (*buf == '=') /* '==' */ {
				buf++;
				if (!*buf)
					break;

				return (!xstrcmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
			}

			/* '=' */
			return (!xstrcasecmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
		}

		if (*buf == '!') {
			buf++;

			if (!*buf)
				break;

			if (*buf == '=') {
				buf++;

				if (!*buf)
					break;

				if (*buf == '=') { /* '!==' */
					buf++;
					if (!*buf)
						break;

					return (xstrcmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
				}

				/* '!=' */
				return (xstrcasecmp(s->str, buf) && !string_free(s, 1)) ? 1 : 0;
			}

			if (*buf == '+') {
				buf++;

				if (!*buf)
					break;

				if (*buf == '+') { /* '!++' */
					buf++;
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
			buf++;

			if (!*buf)
				break;

			if (*buf == '+') { /* '++' */
				buf++;

				if (!*buf)
					break;

				return (xstrstr(buf, s->str) && !string_free(s, 1)) ? 1 : 0;
			}

			/* '+' */
			return (xstrcasestr(buf, s->str) && !string_free(s, 1)) ? 1 : 0;
		}

		string_append_c(s, *buf);
		buf++;
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

static int event_target_check(char *buf) {
	char **params = array_make(buf, ("&|"), 0, 1, 1);
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
		buf++;
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


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
