/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#include "dynstuff.h"
#include "sessions.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

list_t sessions = NULL;
session_t *session_current;


/*
 * session_find()
 *
 * szuka podanej sesji.
 */
session_t *session_find(const char *uid)
{
	list_t l;

	if (!uid)
		return NULL;

	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;

                if (!xstrcasecmp(s->uid, uid) || (s->alias && !xstrcasecmp(s->alias, uid)))
			return s;
	}

	return NULL;
}

/*
 * session_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 *  - data1, data2 - dwa wpisy userlisty do porównania.
 *
 * zwraca wynik xstrcasecmp() na nazwach sesji.
 */
int session_compare(void *data1, void *data2)
{
	session_t *a = data1, *b = data2;
	
	if (!a || !a->uid || !b || !b->uid)
		return 1;

	return xstrcasecmp(a->uid, b->uid);
}

/*
 * session_var_default()
 *
 * sets the default values 
 * 
 * s - session in which we are setting
 */
int session_var_default(session_t *s)
{
	plugin_t *p;
	int i;
	
	if (!s)
		return -1;	

	p = plugin_find_uid(s->uid);

	if (!p)
		return -1;

	for (i=0; p->params && p->params[i]; i++) {
		debug("\tSetting default var %s at %s\n",  p->params[i]->key, p->params[i]->value);
		session_set(s, p->params[i]->key, p->params[i]->value);
	}

	return 0;
}

/*
 * session_add()
 *
 * dodaje do listy sesji.
 */
session_t *session_add(const char *uid)
{
	session_t s, *sp;
	char *tmp;
	list_t l;

	if (!uid)
		return NULL;

	if (!valid_uid(uid)) {
		debug("\tInvalid UID: %s\n", uid);
		return NULL;
	}
	
	memset(&s, 0, sizeof(s));

	s.uid = xstrdup(uid);
	s.status = xstrdup(EKG_STATUS_NA);
	
	sp = list_add_sorted(&sessions, &s, sizeof(s), session_compare);

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (!w->session)
			w->session = sp;
	}
	if (!session_current)
		session_current = sp;

	session_var_default(sp);

	tmp = xstrdup(uid);
	query_emit(NULL, "session-added", &tmp);
	xfree(tmp);

	return sp;
}

/*
 * session_remove()
 *
 * usuwa sesjê.
 *
 * 0/-1
 */
int session_remove(const char *uid)
{
	session_t *s;
	char *tmp;
	list_t l;
	int count;

	if (!(s = session_find(uid)))
		return -1;
	if (session_current && !xstrcasecmp(s->uid, session_current->uid))
		session_current = NULL;

	count = list_count(sessions);

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (w && w->session && w->session == s) {
			if (count > 1)
				window_session_cycle(w);
			else
				w->session = NULL;
		} 
	}
	
	if(s->connected)
		command_exec(NULL, s, saprintf("/disconnect %s", s->uid), 1);	
	tmp = xstrdup(uid);
	query_emit(NULL, "session-removed", &tmp);
	xfree(tmp);

/*	for (i = 0; s->params && s->params[i]; i++) {
		xfree(s->params[i]->key);
		xfree(s->params[i]->value);
	}

	xfree(s->params); */

        for (l = s->params; l; l = l->next) {
                session_param_t *v = l->data;

                xfree(v->key);
		xfree(v->value);
        }

	list_destroy(s->params, 1);
	xfree(s->alias);
	xfree(s->uid);
	xfree(s->status);
	xfree(s->descr);
	xfree(s->password);

	list_remove(&sessions, s, 1);
	return 0;
}

PROPERTY_STRING_GET(session, status);

int session_status_set(session_t *s, const char *status)
{
	char *__session, *__status;	

	if (!s)
		return -1;

	__session = xstrdup(s->uid);
	__status = xstrdup(status);

	query_emit(NULL, "session-status", &__session, &__status);

	xfree(s->status);

	if (!xstrcmp(__status, EKG_STATUS_AUTOAWAY)) {
		xfree(__status);
		__status = xstrdup(EKG_STATUS_AWAY);
		s->autoaway = 1;
	} else 
		s->autoaway = 0;
	
	s->status = __status;

	xfree(__session);

	return 0;
}

int session_password_set(session_t *s, const char *password)
{
	s->password = (password) ? base64_encode(password) : NULL;
        return 0;
}

const char *session_password_get(session_t *s)
{
	return (s && s->password) ? base64_decode(s->password) : 0; 
}


PROPERTY_STRING(session, descr);
PROPERTY_STRING(session, alias);
PROPERTY_PRIVATE(session);
PROPERTY_INT_GET(session, connected, int);

int session_connected_set(session_t *s, int connected)
{
	if (!s)
		return -1;

	s->connected = connected;

	return 0;
}

PROPERTY_STRING_GET(session, uid);

/* 
 * session_var_find()
 * 
 * it looks for given var in given session
 */
session_param_t *session_var_find(session_t *s, const char *key)
{
	if (!s)
		return NULL;

        if (s->params) {
                list_t l;
		for (l = s->params; l; l = l->next) {
                session_param_t *v = l->data;
			if (!xstrcasecmp(v->key, key)) 
				return v;
		}	
        }

	return NULL;
}

/*
 * session_get()
 *
 * pobiera parametr sesji.
 */
const char *session_get(session_t *s, const char *key)
{
	variable_t *v;
	session_param_t *sp;

	if (!s)
		return NULL;
	
	if (!xstrcasecmp(key, "uid"))
		return session_uid_get(s);

	if (!xstrcasecmp(key, "alias"))
		return session_alias_get(s);

	if (!xstrcasecmp(key, "descr"))
		return session_descr_get(s);

	if (!xstrcasecmp(key, "status"))
		return session_status_get(s);
	
	if (!xstrcasecmp(key, "password"))
                return session_password_get(s);
	
	if ((sp = session_var_find(s, key)))
		return sp->value;

	if (!(v = variable_find(key)) || (v->type != VAR_INT && v->type != VAR_BOOL))
		return NULL;
	
	return itoa(*(int*)(v->ptr));
}

/*
 * session_int_get()
 *
 * pobiera parametr sesji jako liczbê.
 */
int session_int_get(session_t *s, const char *key)
{
	const char *tmp = session_get(s, key);

	if (!tmp)
		return -1;

	return strtol(tmp, NULL, 0);
}
/* 
 * session_is_var()
 *
 * checks if given variable is correct for given session
 */
int session_is_var(session_t *s, const char *key)
{
	if (!s)
		return -1;

        if (!xstrcasecmp(key, "alias"))
                return 1;

        if (!xstrcasecmp(key, "descr"))
                return 1;

        if (!xstrcasecmp(key, "status"))
                return 1;

        if (!xstrcasecmp(key, "password"))
                return 1;
	
	if (session_var_find(s, key))
		return 1;

	return 0;
}

static int session_set_compare(void *data1, void *data2)
{
        session_param_t *a = data1, *b = data2;

        if (!a || !a->key || !b || !b->key)
                return 0;

        return xstrcasecmp(a->key, b->key);
}


/*
 * session_set()
 *
 * ustawia parametr sesji.
 */
int session_set(session_t *s, const char *key, const char *value)
{
        session_param_t *sp;
	session_param_t v;
	
	if (!s)
		return -1;

	if (!xstrcasecmp(key, "uid"))
		return -1;

	if (!xstrcasecmp(key, "alias")) {
		int ret = session_alias_set(s, value);
		char *tmp = xstrdup(value);

		query_emit(NULL, "session-renamed", &tmp);
		xfree(tmp);

		return ret;
	}

	if (!xstrcasecmp(key, "descr"))
		return session_descr_set(s, value);

	if (!xstrcasecmp(key, "status"))
		return session_status_set(s, value);

	if (!xstrcasecmp(key, "password"))
                return session_password_set(s, value);


	if ((sp = session_var_find(s, key))) {
		xfree(sp->value);
		sp->value = xstrdup(value);
		return 1;
	}

	memset(&v, 0, sizeof(v));
	v.key = xstrdup(key);
	v.value = xstrdup(value);

	return (list_add_sorted(&s->params, &v, sizeof(v), session_set_compare) != NULL) ? 0 : -1;
}

/*
 * session_int_set()
 *
 * ustawia parametr sesji jako liczbê.
 */
int session_int_set(session_t *s, const char *key, int value)
{
	return session_set(s, key, itoa(value));
}

/*
 * session_read()
 *
 * czyta informacje o sesjach z pliku.
 */
int session_read()
{
	char *line;
	FILE *f;
	session_t *s = NULL;

	if (!(f = fopen(prepare_path("sessions", 0), "r")))
		return -1;

	while ((line = read_file(f))) {
		char *tmp;

		if (line[0] == '[') {
			tmp = xstrchr(line, ']');

			if (!tmp)
				goto next;

			*tmp = 0;
			s = session_add(line + 1);	

			goto next;
		}

		if ((tmp = xstrchr(line, '='))) {
			*tmp = 0;
			tmp++;
			if (!session_is_var(s, line)) {
				debug("\tSession variable \"%s\" is not correct\n", line);
				goto next;
			}
			if(*tmp == '\001')  
				session_set(s, line, base64_decode(tmp + 1));
			else 
				session_set(s, line, tmp);
			goto next;
		}

next:
		xfree(line);
	}

	fclose(f);

	return 0;
}

/*
 * session_write()
 *
 * zapisuje informacje o sesjach w pliku.
 */
int session_write()
{
	list_t l;
	FILE *f = NULL;

	f = fopen(prepare_path("sessions", 1), "w");

	if (!f)
		return -1;

	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;
		list_t lp;
		
		userlist_write(s);
		fprintf(f, "[%s]\n", s->uid);
		if (s->alias)
			fprintf(f, "alias=%s\n", s->alias);
		if (s->status && config_keep_reason != 2)
			fprintf(f, "status=%s\n", s->status);
		if (s->descr && config_keep_reason)
			fprintf(f, "descr=%s\n", s->descr);
                if (s->password && config_save_password)
                        fprintf(f, "password=\001%s\n", s->password);
                
		for (lp = s->params; lp; lp = lp->next) {
	                session_param_t *v = lp->data;
        
			if (v->value)
				fprintf(f, "%s=%s\n", v->key, v->value);
                }
	}
	fclose(f);

	return 0;
}

/*
 * session_format()
 *
 * formatuje ³adnie nazwê sesji zgodnie z wybranym tematem.
 *
 *  - s - sesja.
 */
const char *session_format(session_t *s)
{
	static char buf[256];
	const char *uid;
	char *tmp;

	if (!s)
		return "";

	uid = s->uid;

//	if (xstrchr(uid, ':'))
//		uid = xstrchr(uid, ':') + 1;

	if (!s->alias)
		tmp = format_string(format_find("session_format"), uid, uid);
	else
		tmp = format_string(format_find("session_format_alias"), s->alias, uid);
	
	strlcpy(buf, tmp, sizeof(buf));
	
	xfree(tmp);

	return buf;
}

/*
 * session_check()
 *
 * sprawdza, czy dana sesja zawiera prywatne dane pluginu i jest danego
 * protoko³u.
 *
 * 0/1
 */
int session_check(session_t *s, int need_private, const char *protocol)
{
	if (!s)
		return 0;

	if (need_private && !s->priv)
		return 0;

	if (protocol) {
		int plen = xstrlen(protocol);

		if (xstrlen(s->uid) < plen + 1)
			return 0;

		if (strncmp(s->uid, protocol, plen) || s->uid[plen] != ':')
			return 0;
	}

	return 1;
}

/*
 * session_unidle()
 *
 * funkcja wywo³ywana, gdy w danej sesji u¿ytkownik podj±³ jakie¶ dzia³ania.
 * je¶li dla danej sesji zostanie wywo³ana choæ raz ta funkcja, bêdzie ona
 * brana pod uwagê przy autoawayu. po przekroczeniu okre¶lonego czasu,
 * zostanie wywo³ana komenda /_autoaway dla tej sesji, a potem /_autoback.
 * nale¿y je obs³ugiwaæ, inaczej bêd± ¶mieci na ekranie.
 *
 * 0/-1
 */
int session_unidle(session_t *s)
{
	if (!s)
		return -1;

	s->activity = time(NULL);

	if (s->autoaway)
		command_exec(NULL, s, "/_autoback", 0);

	return 0;
}

/*
 * session_command()
 *
 * obs³uga komendy /session
 */
COMMAND(session_command)
{
	session_t *s;

	if (!params[0] || match_arg(params[0], 'l', "list", 2)) {
		list_t l;

		for (l = sessions; l; l = l->next) {
			session_t *s = l->data;
			const char *status;
			char *tmp;

			status = (!s->connected) ? EKG_STATUS_NA : s->status;

			tmp = format_string(format_find(ekg_status_label(status, s->descr, "user_info_")), "foobar", s->descr);

			if (!s->alias)
				printq("session_list", s->uid, s->uid, tmp);
			else
				printq("session_list_alias", s->uid, s->alias, tmp);

			xfree(tmp);
		}

		if (!sessions)
			printq("session_list_empty");

		return 0;
	}
	
	if (!xstrcasecmp(params[0], "--dump")) {
		list_t l;
		
		for (l = sessions; l; l = l->next) {
			session_t *s = l->data;
			list_t lp;

			debug("[%s]\n", s->uid);
			if (s->alias)
				debug("alias=%s\n", s->alias);
			if (s->status)
				debug("status=%s\n", s->status);
			if (s->descr)
				debug("descr=%s\n", s->descr);
                
			for (lp = s->params; lp; lp = lp->next) {
		                session_param_t *v = lp->data;

				if (v->value)
					debug("%s=%s\n", v->key, v->value);
                	}
		}
		return 0;
	}

	if (match_arg(params[0], 'a', "add", 2)) {
		if (!valid_uid(params[1])) {
			printq("invalid_uid", params[1]);
			return -1;
		}

		if (session_find(params[1])) {
			printq("session_exists", params[1]);
			return -1;
		}

		window_session_set(window_current, session_add(params[1]));
		
		config_changed = 1;
		
		printq("session_added", params[1]);

		return 0;
	}

	if (match_arg(params[0], 'd', "del", 2)) {
		if (!session_find(params[1])) {
			printq("session_doesnt_exist", params[1]);
			return -1;
		}

		session_remove(params[1]);
		
		config_changed = 1;
		printq("session_removed", params[1]);

		return 0;
	}

	if (match_arg(params[0], 'w', "sw", 2)) {
		session_t *s;
		
		if (!params[1]) {
			printq("invalid_params", name);
			return -1;		
		}
		if (!(s = session_find(params[1]))) {
			printq("session_doesnt_exist", params[1]);
			return -1;
		}
		if (window_current->target && window_current->id != 0) {
			printq("sesssion_cannot_change");
			return -1;			
		}

		window_current->session = s;
		session_current = s;

		query_emit(NULL, "session-changed");

		return 0;
	}
	
	if (match_arg(params[0], 'g', "get", 2)) {
		const char *var;
		
		if (!params[1]) {
			printq("invalid_params", name);
			return -1;
		}	
		
		if (!(s = session_find(params[1]))) {
			if (window_current->session) {
                        	char *tmp = saprintf("%s --get %s %s", name, window_current->session->uid, params[1]);
                        	return command_exec(NULL, s, tmp, 0);
			} else
				printq("invalid_session");
			return -1;
		}
		 
		if (params[2] && session_is_var(s, params[2])) {
			plugin_t *p = plugin_find_uid(s->uid);
                        plugins_params_t *pa;

                        if ((pa = plugin_var_find(p, params[2]))) {
         	        	var = session_get_n(s->uid, params[2]);

                 		if (pa->secret)
                         		printq("session_variable", session_name(s), params[2], (pa->type == VAR_STR && !var) ? "(none)" : "(...)");
                                else
                                        printq("session_variable", session_name(s), params[2], (pa->type == VAR_STR && !var) ? "(none)" : var);

                        	return 0;
			}

			var = session_get_n(s->uid, params[2]);
                        if (!xstrcasecmp(params[2], "password"))
                		printq("session_variable", session_name(window_current->session), params[2], (var) ? "(...)" : "(none)");
                        else
                                printq("session_variable", session_name(window_current->session), params[2], (var) ? var : "(none)");
                        return 0;
		}
		
		if (params[2]) {
	    		printq("session_variable_doesnt_exist", session_name_n(params[1]), params[2]);
			return -1;
		}

		printq("invalid_params", name);
		return -1;
	}

	if (match_arg(params[0], 's', "set", 2)) {
		
		if (!params[1]) {
			printq("invalid_params", name);
			return -1;
		}	
		
		if (!(s = session_find(params[1]))) {
			if (params[1][0] == '-') {
				if (window_current->session) {
					if (!session_get(window_current->session, params[1] + 1)) {
						printq("session_variable_doesnt_exist", session_name(window_current->session), params[1] + 1);
						return -1;
					}
					session_set(window_current->session, params[1] + 1, NULL);
					config_changed = 1;
					printq("session_variable_removed", session_name(window_current->session), params[1] + 1);
					return 0;
				} else {
					printq("invalid_session");
					return -1;
				}
			}
		
			if(params[2] && !params[3]) {
				if (window_current->session) {
					char *tmp = saprintf("%s --get %s %s", name, window_current->session->uid, params[1]);
					if (!session_get(window_current->session, params[1])) {
                                                printq("session_variable_doesnt_exist", session_name(window_current->session), params[1]);
                                                return -1;
					}
					session_set_n(window_current->session->uid, params[1], params[2]);
					config_changed = 1;
					command_exec(NULL, s, tmp, 0);
					return 0;
				} else {
					printq("invalid_session");
					return -1;
				}
			}
			
			if(params[2] && params[3]) {
				printq("session_doesnt_exist", params[1]);
				return -1;
			}
			
    		    	printq("invalid_params", name);
			return -1;
		}
		
		if (params[2] && params[2][0] == '-') {
			if (!session_get(s, params[2] + 1)) {
                        	printq("session_variable_doesnt_exist", session_name(window_current->session), params[2] + 1);
                        	return -1;
                        }

			session_set_n(s->uid, params[2] + 1, NULL);
			config_changed = 1;
			printq("session_variable_removed", session_name_n(params[1]), params[2] + 1);
			return 0;
		}
		
		if(params[2] && params[3]) {
			char *tmp = saprintf("%s --get %s %s", name, s->uid, params[2]);
			
                        if (!session_get(s, params[2])) {
                                printq("session_variable_doesnt_exist", session_name(window_current->session), params[2]);
                                return -1;
                        }

			session_set_n(s->uid, params[2], params[3]);
			config_changed = 1;
			command_exec(NULL, s, tmp, 0);
			return 0;
		}
		
		printq("invalid_params", name);
		return -1;
	}

	if ((s = session_find(params[0]))) {
	    	
		const char *status;
		char *tmp;
		int i;
		plugin_t *p = plugin_find_uid(s->uid);

		if (params[1] && params[1][0] == '-') { 
			tmp = saprintf("%s --set %s %s", name, params[0], params[1]);
			config_changed = 1;
			command_exec(NULL, s, tmp, 0);
			return 0;
		}

		if(params[1] && params[2]) {
			tmp = saprintf("%s --set %s %s %s", name, params[0], params[1], params[2]);
			command_exec(NULL, s, tmp, 0);
			config_changed = 1;
			return 0;
		}
		
		if(params[1]) {
			tmp = saprintf("%s --get %s %s", name, params[0], params[1]);
			command_exec(NULL, s, tmp, 0);
			config_changed = 1;
			return 0;		
		}
		
		status = (!s->connected) ? EKG_STATUS_NA : s->status;
	
		tmp = format_string(format_find(ekg_status_label(status, s->descr, "user_info_")), (s->alias) ? s->alias : "x", s->descr);

		if (!s->alias)
			printq("session_info_header", s->uid, s->uid, tmp);
		else
			printq("session_info_header_alias", s->uid, s->alias, tmp);

		for (i = 0; p->params[i]; i++) {
			plugins_params_t *sp = p->params[i];
			if (sp->secret)
				printq("session_info_param", sp->key, (session_get(s, sp->key)) ? "(...)" : "(none)");
			else
				printq("session_info_param", sp->key, (session_get(s, sp->key)) ? session_get(s, sp->key) : "(none)");
		}	

		printq("session_info_footer", s->uid);

		return 0;	
	}
	
	if (params[0] && params[0][0] != '-' && params[1] && window_current->session && window_current->session->uid) {
		char *tmp = saprintf("%s --set %s %s %s", name, session_alias_uid(window_current->session), params[0], params[1]);
		command_exec(NULL, s, tmp, 0);
		return 0;
        }
	
	if (params[0] && params[0][0] != '-' && window_current->session && window_current->session->uid) {
		char *tmp = saprintf("%s --get %s %s", name, session_alias_uid(window_current->session), params[0]);
		command_exec(NULL, s, tmp, 0);
		return 0;
	}
	
	if (params[0] && params[0][0] == '-' && window_current->session && window_current->session->uid) {
		char *tmp = saprintf("%s --set %s %s", name, session_alias_uid(window_current->session), params[0]);
		command_exec(NULL, s, tmp, 0);
		return 0;
	}

	printq("invalid_params", name);
	
	return -1;
}

/* sessions_free()
 *
 * zwalnia wszystkie dostêpne sesje
 */
void sessions_free()
{
        list_t l;

        if (!sessions)
                return;

        for (l = sessions; l; l = l->next) {
                session_t *s = l->data;

                if (s && s->uid)
                        session_remove_s(s);
        }

        list_destroy(sessions, 1);
        sessions = NULL;
}

