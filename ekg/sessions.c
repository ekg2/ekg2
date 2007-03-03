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

#include "debug.h"
#include "dynstuff.h"
#include "sessions.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

#include "objects.h"

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include "queries.h"

list_t sessions = NULL;
session_t *session_current = NULL;

/**
 * session_find_ptr()
 *
 * it's search over sessions list and checks if param @a s is in that list.
 * it's useful for all watch handler, and if programmer was too lazy to destroy watches assosiated with that
 * session (in private watch data struct) before it gone.
 *
 * @note It's possible to find another session with the same address as old one.. it's rather not possible.. however.
 *	It's better if you use @a session_find() function.. Yeah, i know it's slower.
 *
 * @param s - session to look for.
 *
 * @return It returns @a s if session was found, otherwise NULL.
 */

session_t *session_find_ptr(session_t *s) {
	list_t l;
	for (l = sessions; l; l = l->next) {
		if (l->data == s)
			return s;

	}
	return NULL;
}

/**
 * session_find()
 *
 * It's search over sessions list and checks if we have session with uid @a uid
 *
 * @param uid - uid of session you look for
 * @sa session_find_ptr() - If you are looking for smth faster ;) but less reliable.
 *
 * @return It returns pointer to session_t struct of found session, or NULL
 *
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

/**
 * session_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 * @note To raczej powinna byc wewnetrzna funkcja w session.c ale nie wiedziec czemu kilka pluginow z tego korzysta...
 * 	W koncu nie mozemy miec dwoch sesji o takich samych uidach... ? to chyba wystarczy porownac adresy?
 * 
 * @param data1 - pierwsza sesja do porownania
 * @param data2 - druga sesja do porownania
 * @sa list_add_sorted
 *
 * @return zwraca wynik xstrcasecmp() na nazwach sesji.
 */
int session_compare(void *data1, void *data2)
{
	session_t *a = data1, *b = data2;
	
	if (!a || !a->uid || !b || !b->uid)
		return 1;

	return xstrcasecmp(a->uid, b->uid);
}

/**
 * session_var_default()
 *
 * sets the default values of session params
 *
 * @param s - session in which we are setting
 * @sa plugin_var_add
 *
 * @return -1 if smth went wrong... 0 on success.
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
		/* debug("\tSetting default var %s at %s\n",  p->params[i]->key, p->params[i]->value); */
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
	session_t *s;
	char *tmp;
	list_t l;

	if (!uid)
		return NULL;

	if (!valid_uid(uid)) {
		debug("\tInvalid UID: %s\n", uid);
		return NULL;
	}
	
	s = xmalloc(sizeof(session_t));
	s->uid = xstrdup(uid);
	s->status = xstrdup(EKG_STATUS_NA);
	
	list_add_sorted(&sessions, s, 0, session_compare);

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (!w->session && !w->floating)	/* remove this? if user want to change session he should press ctrl+x in that window ? */
			w->session = s;
	}
	if (!session_current)
		session_current = s;

	session_var_default(s);

	tmp = xstrdup(uid);
	query_emit_id(NULL, SESSION_ADDED, &tmp);
	xfree(tmp);

	return s;
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
	if (s == session_current)
		session_current = NULL;

	count = list_count(sessions);

	for (l = windows; windows && l; l = l->next) {
		window_t *w = l->data;

		if (w && w->session == s) {
			if (count > 1)
				window_session_cycle(w);
			else
				w->session = NULL;
		} 
	}
	
	if (s->connected) {
		command_exec_format(NULL, s, 1, ("/disconnect %s"), s->uid);
	}
	tmp = xstrdup(uid);
        query_emit_id(NULL, SESSION_CHANGED);
	query_emit_id(NULL, SESSION_REMOVED, &tmp);
	xfree(tmp);

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
	xfree(s->laststatus);
	xfree(s->lastdescr);

	list_remove(&sessions, s, 1);
	return 0;
}

PROPERTY_STRING_GET(session, status)

int session_status_set(session_t *s, const char *status)
{
	int is_xa;

	if (!s)
		return -1;

	{
		char *__session = xstrdup(s->uid);
		char *__status = xstrdup(status);

		query_emit_id(NULL, SESSION_STATUS, &__session, &__status);

		xfree(__session);
		xfree(__status);
	}

/* if it's autoaway or autoxa */
	if ((is_xa = !xstrcmp(status, EKG_STATUS_AUTOXA)) || !xstrcmp(status, EKG_STATUS_AUTOAWAY)) {
		char *tmp = (char*) session_get(s, (is_xa ? "auto_xa_descr" : "auto_away_descr"));

	/* save current status/ descr && turn autoaway on */
		if (!s->autoaway) { /* don't overwrite laststatus, if already on aa */
			xfree(s->laststatus); xfree(s->lastdescr);	/* just in case */
			s->laststatus	= s->status;		s->status = NULL;
			s->lastdescr	= xstrdup(s->descr);
			s->autoaway = 1;
		}
	/* new status */
		xfree(s->status);
		s->status = xstrdup(is_xa ? EKG_STATUS_XA : EKG_STATUS_AWAY);

	/* new descr */
		if (tmp) {
			xfree(s->descr);

			if (xstrchr(tmp, '%')) { /* the New&Better-AutoAway-Markup^TM */
				const char *current_descr = (s->autoaway ? s->lastdescr : s->descr);
				char *c, *xbuf, *xc;
				int xm = 0;

				/* following thing is used to count how large buffer do we need
				 * yep, I know that we can waste some space
				 * yep, I know that this would also count %%$, but I don't think we need to care that much
				 * if user does want to use shitty strings, it's his problem */
				for (c = tmp; (c = xstrstr(c, "%$")); c++, xm++);
				xbuf = xmalloc(xstrlen(tmp) + (xm * xstrlen(current_descr)) + 1);
				xc = xbuf;
				xm = 1; /* previously xm was used as %$ counter, now it says if we should copy or skip */

				for (c = tmp; *c; c++) {
					if (*c == '%') {
						switch (*(++c)) {
							case '?': /* write if descr is set */
								xm = (int) (current_descr);
								break;
							case '!': /* write if descr isn't set */
								xm = !(current_descr);
								break;
							case '/': /* always write */
								xm = 1;
								break;
							/* do we need to employ some never-write (i.e. comments)? */
							case '$': /* insert current descr */
								if (current_descr) {
		/* Here I use memcpy(), 'cause I already need to get strlen(), and the function itself puts final \0 */
									const int xl = xstrlen(current_descr);
									memcpy(xc, current_descr, xl);
									xc += xl;
								}
								break;
							default: /* other chars, i.e. someone's forgotten to escape % */
								if (xm)
									*(xc++) = '%';
							case '%': /* above, or escaped % */
								if (*c == '\0') /* oops, escaped NULL? ( ; */
									c--; /* else for loop won't break */
								else if (xm)
									*(xc++) = *c;
						}
					} else if (xm) /* normal char */
						*(xc++) = *c;
				}

				*xc = '\0'; /* make sure we end with \0 */
				s->descr = xrealloc(xbuf, strlen(xbuf)+1); /* free unused bytes */
			} else /* no markup, just copy */
				s->descr = xstrdup(tmp);
		}
		return 0;
	}

/* if it's autoback */
	if (!xstrcmp(status, EKG_STATUS_AUTOBACK)) {
	/* set status */
		xfree(s->status);
		s->status	= s->laststatus ? s->laststatus : xstrdup(EKG_STATUS_AVAIL);
	/* set descr */
		if (s->lastdescr) {
			xfree(s->descr);
			s->descr = s->lastdescr;
		}

		s->laststatus	= NULL;
		s->lastdescr	= NULL;
		s->autoaway	= 0;
		return 0;
	}

	xfree(s->status);
	s->status = xstrdup(status);

/* if it wasn't neither _autoback nor _autoaway|_autoxa, it should be one of valid status types... */
	if (s->autoaway) {	/* if we're @ away, set previous, set lastdescr status & free data */
		xfree(s->laststatus);	s->laststatus = NULL;
		xfree(s->descr);	s->descr	= s->lastdescr;	s->lastdescr = NULL;
		s->autoaway	= 0;
	}
	return 0;
}

int session_password_set(session_t *s, const char *password)
{
	xfree(s->password);
	s->password = (password) ? base64_encode(password, xstrlen(password)) : NULL;
	return 0;
}

/* static buffer is a better idea - i think (del) */
const char *session_password_get(session_t *s)
{
        static char buf[100];
	char *tmp = base64_decode(s->password);

	if (!tmp)
		return "";
	
	strlcpy(buf, tmp, sizeof(buf));
	xfree(tmp);
	
	return buf;
}


PROPERTY_STRING_GET(session, descr)

int session_descr_set(session_t *s, const char *descr)
{
	if (!s)
		return -1;
	
	if (s->autoaway) {
		xfree(s->lastdescr);
		s->lastdescr = xstrdup(descr);
	} else {
		xfree(s->descr);
		s->descr = xstrdup(descr);
	}
	
	return 0;
}

PROPERTY_STRING(session, alias)
PROPERTY_PRIVATE(session)
PROPERTY_INT_GET(session, connected, int)

int session_connected_set(session_t *s, int connected)
{
	if (!s)
		return -1;

	s->connected = connected;

	return 0;
}

PROPERTY_STRING_GET(session, uid)

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
	session_param_t *v;
        plugin_t *p = (s && s->uid) ? plugin_find_uid(s->uid) : NULL;
        plugins_params_t *pa = (p) ? plugin_var_find(p, key) : NULL; 
	int ret = 0;

	if (!s)
		return -1;

	if (!xstrcasecmp(key, "uid"))
		return -1;

	if (!xstrcasecmp(key, "alias")) {
		char *tmp = xstrdup(value);
		ret = session_alias_set(s, value);

		query_emit_id(NULL, SESSION_RENAMED, &tmp);
		xfree(tmp);

		goto notify;
	}

	if (!xstrcasecmp(key, "descr")) {
		ret = session_descr_set(s, value);
		goto notify;
	}

	if (!xstrcasecmp(key, "status")) {
		ret = session_status_set(s, value);
		goto notify;
	}

	if (!xstrcasecmp(key, "password")) {
                ret = session_password_set(s, value);
		goto notify;
	}


	if ((v = session_var_find(s, key))) {
		xfree(v->value);
		v->value = xstrdup(value);
		goto notify;
	}

	v = xmalloc(sizeof(session_param_t));
	v->key = xstrdup(key);
	v->value = xstrdup(value);

	return (list_add_sorted(&s->params, v, 0, session_set_compare) != NULL) ? 0 : -1;

notify:
	if (pa && pa->notify)
		pa->notify(s, key);

	return ret;
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
int session_read(const char *filename) {
	char *line;
	FILE *f;
	session_t *s = NULL;
	list_t l;
	int ret = 0;

	if (!filename) {
		if (!in_autoexec) {
			list_t l;

			for (l = sessions; l; l = l->next) {
				session_t *s = l->data;

				command_exec(NULL, s, ("disconnect"), 1);
			}
			sessions_free();
			debug("	 flushed sessions\n");
		}

		for (l = plugins; l; l = l->next) {
			plugin_t *p = l->data;
			char *tmp;

			if (!p || p->pclass != PLUGIN_PROTOCOL)
				continue;

			tmp = saprintf("sessions-%s", p->name);
			ret = session_read(prepare_path(tmp, 0));
			xfree(tmp);
		}
		return ret;
	}

	if (!(f = fopen(filename, "r"))) {
		debug("Error opening file %s\n", filename);
		return -1;
	}

	while ((line = read_file(f, 0))) {
		char *tmp;

		if (line[0] == '[') {
			tmp = xstrchr(line, ']');

			if (!tmp)
				continue;

			*tmp = 0;
			s = session_add(line + 1);	

			continue;
		}

		if ((tmp = xstrchr(line, '='))) {
			*tmp = 0;
			tmp++;
			if (!session_is_var(s, line)) {
				debug("\tSession variable \"%s\" is not correct\n", line);
				continue;
			}
			xstrtr(tmp, '\002', '\n');
			if(*tmp == '\001') { 
				char *decoded = base64_decode(tmp + 1);
				session_set(s, line, decoded);
				xfree(decoded);
			} else 
				session_set(s, line, tmp);
		}
	}

	fclose(f);
	return ret;
}

/*
 * session_write()
 *
 * writes informations about sessions in files
 */
int session_write()
{
	list_t l, ls;
	FILE *f = NULL;
	int ret = 0;

	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;
		char *tmp;

		if (p->pclass != PLUGIN_PROTOCOL) continue; /* skip no protocol plugins */
		
		tmp = saprintf("sessions-%s", p->name);

                if (!(f = fopen(prepare_path(tmp, 1), "w"))) {
                        debug("Error opening file %s\n", tmp);
                        xfree(tmp);
			ret = -1;
			continue;
                }

                xfree(tmp);

		for (ls = sessions; ls; ls = ls->next) {
			session_t *s = ls->data;
			list_t lp;
			plugin_t *ps = plugin_find_uid(s->uid);

			if (ps != p)
				continue;

			userlist_write(s);
			fprintf(f, "[%s]\n", s->uid);
			if (s->alias)
				fprintf(f, "alias=%s\n", s->alias);
			if (s->status && config_keep_reason != 2)
				fprintf(f, "status=%s\n", (s->autoaway ? s->laststatus : s->status));
			if (s->descr && config_keep_reason) {
				char *myvar = (s->autoaway ? s->lastdescr : s->descr);
				xstrtr(myvar, '\n', '\002');
				fprintf(f, "descr=%s\n", myvar);
				xstrtr(myvar, '\002', '\n');
			}
        	        if (s->password && config_save_password)
	                        fprintf(f, "password=\001%s\n", s->password);
                
			for (lp = s->params; lp; lp = lp->next) {
		                session_param_t *v = lp->data;
        
				if (v->value)
					fprintf(f, "%s=%s\n", v->key, v->value);
	                }
		}

		fclose(f);
	}

	return ret;
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
/*
	if (xstrchr(uid, ':'))
		uid = xstrchr(uid, ':') + 1;
 */

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
 * session_name()
 * 
 * returns static buffer with formated session name
 */
const char *session_name(session_t *s)
{
	static char buf[150];
	char *tmp = format_string(format_find("session_name"), s ? (s->alias) ? s->alias : s->uid : "?");

	strlcpy(buf, tmp, sizeof(buf));

	xfree(tmp);
	return buf;
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
		command_exec(NULL, s, ("/_autoback"), 0);

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

	if (!params[0] || match_arg(params[0], 'l', ("list"), 2)) {
		list_t l;

		for (l = sessions; l; l = l->next) {
			session_t *s = l->data;
			const char *status, *descr;
			char *tmp;

			status = (!s->connected) ? EKG_STATUS_NA : s->status;
			descr = (s->connected) ? s->descr : NULL;

			tmp = format_string(format_find(ekg_status_label(status, descr, "user_info_")), "foobar", descr);

			if (!s->alias)
				printq("session_list", s->uid, s->uid, tmp);
			else
				printq("session_list_alias", s->uid, s->alias, tmp);

			xfree(tmp);
		}

		if (!sessions)
			wcs_printq("session_list_empty");

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
				debug("status=%s\n", (s->autoaway ? s->laststatus : s->status));
			if (s->descr)
				debug("descr=%s\n", (s->autoaway ? s->lastdescr : s->descr));
                
			for (lp = s->params; lp; lp = lp->next) {
		                session_param_t *v = lp->data;

				if (v->value)
					debug("%s=%s\n", v->key, v->value);
                	}
		}
		return 0;
	}

	if (match_arg(params[0], 'a', ("add"), 2)) {
		if (!valid_uid(params[1])) {
			printq("invalid_uid", params[1]);
			return -1;
		}

		if (session_find(params[1])) {
			printq("session_exists", params[1]);
			return -1;
		}

		if (!window_current->session)
			window_session_set(window_current, session_add(params[1]));
		else
			session_add(params[1]);
		
		config_changed = 1;
		
		printq("session_added", params[1]);

		return 0;
	}

	if (match_arg(params[0], 'd', ("del"), 2)) {
		if (!session_find(params[1])) {
			printq("session_doesnt_exist", params[1]);
			return -1;
		}

		session_remove(params[1]);
		
		config_changed = 1;
		printq("session_removed", params[1]);

		return 0;
	}

	if (match_arg(params[0], 'w', ("sw"), 2)) {
		session_t *s;
		
		if (!params[1]) {
			printq("invalid_params", name);
			return -1;		
		}
		if (!(s = session_find(params[1]))) {
			printq("session_doesnt_exist", params[1]);
			return -1;
		}
		if (window_current->session == s)
			return 0; /* we don't need to switch to the same session */
		if (window_current->target && (window_current->id != 0))
			command_exec(NULL, NULL, "/window switch 1", 2);

		window_current->session = s;
		session_current = s;

		query_emit_id(NULL, SESSION_CHANGED);

		return 0;
	}
	
	if (match_arg(params[0], 'g', ("get"), 2)) {
		const char *var;
		
		if (!params[1]) {
			wcs_printq("invalid_params", name);
			return -1;
		}	
		
		if (!(s = session_find(params[1]))) {
			if (window_current->session) {
				return command_exec_format(NULL, s, 0, ("%s --get %s %s"), name, session->uid, params[1]);
			} else
				wcs_printq("invalid_session");
			return -1;
		}
		 
		if (params[2] && session_is_var(s, params[2])) {
			plugin_t *p = plugin_find_uid(s->uid);
			plugins_params_t *pa;
			char *tmp = NULL;

                        if ((pa = plugin_var_find(p, params[2]))) {
         	        	var = session_get_n(s->uid, params[2]);

                 		if (pa->secret)
                         		printq("session_variable", session_name(s), params[2], (pa->type == VAR_STR && !var) ? (tmp = format_string(format_find("value_none"))) : "(...)");
                                else
                                        printq("session_variable", session_name(s), params[2], (pa->type == VAR_STR && !var) ? (tmp = format_string(format_find("value_none"))) : var);
				xfree(tmp);	
                        	return 0;
			}

			var = session_get_n(s->uid, params[2]);
                        if (!xstrcasecmp(params[2], "password"))
                		printq("session_variable", session_name(session), params[2], (var) ? "(...)" : (tmp = format_string(format_find("value_none"))));
                        else
                                printq("session_variable", session_name(session), params[2], (var) ? var : (tmp = format_string(format_find("value_none"))));
			xfree(tmp);
                        return 0;
		}
		
		if (params[2]) {
	    		printq("session_variable_doesnt_exist", session_name_n(params[1]), params[2]);
			return -1;
		}

		wcs_printq("invalid_params", name);
		return -1;
	}

	if (match_arg(params[0], 's', ("set"), 2)) {
		
		if (!params[1]) {
			wcs_printq("invalid_params", name);
			return -1;
		}	
		
		if (!(s = session_find(params[1]))) {
			if (params[1][0] == '-') {
				if (session) {
					if (!session_is_var(session, params[1] + 1)) {
						printq("session_variable_doesnt_exist", session_name(session), params[1] + 1);
						return -1;
					}
					session_set(window_current->session, params[1] + 1, NULL);
					config_changed = 1;
					printq("session_variable_removed", session_name(session), params[1] + 1);
					return 0;
				} else {
					printq("invalid_session");
					return -1;
				}
			}
		
			if(params[2] && !params[3]) {
				if (session) {
					if (!session_is_var(session, params[1])) {
                                                printq("session_variable_doesnt_exist", session_name(session), params[1]);
                                                return -1;
					}
					session_set_n(session->uid, params[1], params[2]);
					config_changed = 1;
					command_exec_format(NULL, s, 0, ("%s --get %s %s"), name, session->uid, params[1]);
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
			
    		    	wcs_printq("invalid_params", name);
			return -1;
		}
		
		if (params[2] && params[2][0] == '-') {
			if (!session_is_var(s, params[2] + 1)) {
                        	printq("session_variable_doesnt_exist", session_name(session), params[2] + 1);
                        	return -1;
                        }

			session_set_n(s->uid, params[2] + 1, NULL);
			config_changed = 1;
			printq("session_variable_removed", session_name_n(s->uid), params[2] + 1);
			return 0;
		}
		
		if(params[2] && params[3]) {
                        if (!session_is_var(s, params[2])) {
                                printq("session_variable_doesnt_exist", session_name(session), params[2]);
                                return -1;
                        }

			session_set_n(s->uid, params[2], params[3]);
			config_changed = 1;
			command_exec_format(NULL, s, 0, ("%s --get %s %s"), name, s->uid, params[2]);
			return 0;
		}
		
		wcs_printq("invalid_params", name);
		return -1;
	}

	if ((s = session_find(params[0]))) {
	    	
		const char *status;
		char *tmp;
		int i;
		plugin_t *p = plugin_find_uid(s->uid);

		if (params[1] && params[1][0] == '-') { 
			config_changed = 1;
			command_exec_format(NULL, s, 0, ("%s --set %s %s"), name, params[0], params[1]);
			return 0;
		}

		if(params[1] && params[2]) {
			command_exec_format(NULL, s, 0, ("%s --set %s %s %s"), name, params[0], params[1], params[2]);
			config_changed = 1;
			return 0;
		}
		
		if(params[1]) {
			command_exec_format(NULL, s, 0, ("%s --get %s %s"), name, params[0], params[1]);
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
				printq("session_info_param", sp->key, (session_get(s, sp->key)) ? "(...)" : format_string(format_find("value_none")));
			else
				printq("session_info_param", sp->key, (session_get(s, sp->key)) ? session_get(s, sp->key) : format_string(format_find("value_none")));
		}	

		printq("session_info_footer", s->uid);
		
		xfree(tmp);
		return 0;	
	}
	
	if (params[0] && params[0][0] != '-' && params[1] && session && session->uid) {
		command_exec_format(NULL, s, 0, ("%s --set %s %s %s"), name, session_alias_uid(session), params[0], params[1]);
		return 0;
        }
	
	if (params[0] && params[0][0] != '-' && session && session->uid) {
		command_exec_format(NULL, s, 0, ("%s --get %s %s"), name, session_alias_uid(session), params[0]);
		return 0;
	}
	
	if (params[0] && params[0][0] == '-' && session && session->uid) {
		command_exec_format(NULL, s, 0, ("%s --set %s %s"), name, session_alias_uid(session), params[0]);
		return 0;
	}

	wcs_printq("invalid_params", name);
	
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
		list_t lp;

		if (!s)
			continue;

	        for (lp = s->params; lp; lp = lp->next) {
        	        session_param_t *v = lp->data;
	
	                xfree(v->key);
	                xfree(v->value);
	        }
	
	        list_destroy(s->params, 1);
	        xfree(s->alias);
	        xfree(s->uid);
	        xfree(s->status);
        	xfree(s->descr);
	        xfree(s->password);
		xfree(s->laststatus);
		xfree(s->lastdescr);
		userlist_free(s);
        }

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;

		if (!w)
			continue;

		w->session = NULL;
	}

        list_destroy(sessions, 1);
        sessions = NULL;
	session_current = NULL;
	window_current->session = NULL;
}

/*
 * session_help()
 *
 * it shows help about variable from ${datadir}/ekg/plugins/{plugin_name}/
 * session.txt
 *
 * s - session
 * name - name of the variable
 */
void session_help(session_t *s, const char *name)
{
	FILE *f;
	char *line, *type = NULL, *def = NULL, *tmp;
	char *plugin_name;

	string_t str;
	int found = 0;
	int sessfilnf = 0;

	if (!s)
		return;
	{
		if (!session_is_var(s, name)) {
			wcs_print("session_variable_doesnt_exist", session_name(s), name);
			return;
		}
	}

	plugin_name = plugin_find_uid(s->uid)->name;

	do {
		/* first try to find the variable in plugins' session file */
		if (!(f = help_path("session", plugin_name))) {
			sessfilnf = 1;
			break;
		}

		while ((line = read_file(f, 0))) {
			if (!xstrcasecmp(line, name)) {
				found = 1;
				break;
			}
		}
	} while(0);

	if (!found) {
		do {
			/* then look for them inside global session file */
			if (!sessfilnf)
				fclose(f);
			
			if (!(f = help_path("session", NULL)))
				break;
			
			while ((line = read_file(f, 0))) {
				if (!xstrcasecmp(line, name)) {
					found = 1;
					break;
				}
			}
		} while (0);
	}


	if (!found) {
		if (f)
			fclose(f);
		if (sessfilnf)
			print("help_session_file_not_found", plugin_name);
		else
			print("help_session_var_not_found", name);
		return;
	}

	line = read_file(f, 0);

	if ((tmp = xstrstr(line, (": "))))
		type = xstrdup(tmp + 2);
	else
		type = xstrdup(("?"));

	line = read_file(f, 0);
	if ((tmp = xstrstr(line, (": "))))
		def = xstrdup(tmp + 2);
	else
		def = xstrdup(("?"));

	wcs_print("help_session_header", session_name(s), name, type, def);

	xfree(type);
	xfree(def);

	if (tmp)		/* je¶li nie jest to ukryta zmienna... */
		read_file(f, 0);	/* ... pomijamy liniê */

	str = string_init(NULL);
	while ((line = read_file(f, 0))) {
		if (line[0] != '\t')
			break;

		if (!xstrncmp(line, ("\t- "), 3) && xstrcmp(str->str, (""))) {
			wcs_print("help_session_body", str->str);
			string_clear(str);
		}

		if (!xstrncmp(line, ("\t"), 1) && xstrlen(line) == 1) {
			string_append(str, ("\n\r"));
			continue;
		}

		string_append(str, line + 1);

		if (line[xstrlen(line) - 1] != ' ')
			string_append_c(str, ' ');
	}

	if (xstrcmp(str->str, ("")))
		wcs_print("help_session_body", str->str);

	string_free(str, 1);

	if (xstrcmp(format_find("help_session_footer"), ""))
		wcs_print("help_session_footer", name);

	fclose(f);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
