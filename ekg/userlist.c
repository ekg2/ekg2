/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Piotr Domagalski <szalik@szalik.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "dynstuff.h"
#ifndef HAVE_STRLCAT
#  include "compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "plugins.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"
#include "log.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

list_t userlist = NULL;

struct ignore_label ignore_labels[IGNORE_LABELS_MAX] = {
	{ IGNORE_STATUS, "status" },
	{ IGNORE_STATUS_DESCR, "descr" },
	{ IGNORE_NOTIFY, "notify" },
	{ IGNORE_MSG, "msg" },
	{ IGNORE_DCC, "dcc" },
	{ IGNORE_EVENTS, "events" },
	{ 0, NULL }
};

/*
 * userlist_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 *  - data1, data2 - dwa wpisy userlisty do porównania.
 *
 * zwraca wynik xstrcasecmp() na nazwach userów.
 */
static int userlist_compare(void *data1, void *data2)
{
	userlist_t *a = data1, *b = data2;
	
	if (!a || !a->nickname || !b || !b->nickname)
		return 1;

	return xstrcasecmp(a->nickname, b->nickname);
}

/*
 * userlist_add_entry()
 *
 * dodaje do listy kontaktów pojedyncz± liniê z pliku lub z serwera.
 */
void userlist_add_entry(session_t *session, const char *line)
{
	char **entry = array_make(line, ";", 8, 0, 0);
	userlist_t u;
	int count, i;

	if ((count = array_count(entry)) < 7) {
		array_free(entry);
		return;
	}

	memset(&u, 0, sizeof(u)); 

	if (atoi(entry[6])) 
		u.uid = saprintf("gg:%s", entry[6]);
	else
		u.uid = xstrdup(entry[6]);

	for (i = 0; i < 6; i++) {
		if (!xstrcmp(entry[i], "(null)") || !xstrcmp(entry[i], "")) {
			xfree(entry[i]);
			entry[i] = NULL;
		}
	}
			
	u.first_name = xstrdup(entry[0]);
	u.last_name = xstrdup(entry[1]);

	if (entry[3] && !valid_nick(entry[3]))
		u.nickname = saprintf("_%s", entry[3]);
	else
		u.nickname = xstrdup(entry[3]);

	u.mobile = xstrdup(entry[4]);
	u.groups = group_init(entry[5]);
	u.status = xstrdup(EKG_STATUS_NA);
	
	if (entry[7])
		u.foreign = saprintf(";%s", entry[7]);
	else
		u.foreign = xstrdup("");

	for (i = 0; i < count; i++)
		xfree(entry[i]);

	xfree(entry);

	list_add_sorted(&(session->userlist), &u, sizeof(u), userlist_compare);
}

/*
 * userlist_dump()
 *
 * zapisuje listê kontaktów w postaci tekstowej.
 *
 * zwraca zaalokowany bufor, który nale¿y zwolniæ.
 */
char *userlist_dump(session_t *session)
{
	string_t s = string_init(NULL);
	list_t l;

	for (l = session->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		const char *uid;
		char *groups, *line;

		uid = (!strncmp(u->uid, "gg:", 3)) ? u->uid + 3 : u->uid;

		groups = group_to_string(u->groups, 1, 0);
		
		line = saprintf("%s;%s;%s;%s;%s;%s;%s%s\r\n",
			(u->first_name) ? u->first_name : "",
			(u->last_name) ? u->last_name : "",
			(u->nickname) ? u->nickname : "",
			(u->nickname) ? u->nickname : "",
			(u->mobile) ? u->mobile : "",
			groups,
			uid,
			(u->foreign) ? u->foreign : "");
		
		string_append(s, line);

		xfree(line);
		xfree(groups);
	}	

	return string_free(s, 0);
}

/*
 * userlist_write()
 *
 * zapisuje listê kontaktów w pliku ~/.ekg/gg:NUMER-userlist
 */
int userlist_write(session_t *session)
{
	const char *filename;
	char *contacts;
	FILE *f;
	char *tmp=saprintf("%s-userlist", session->uid); 

	if (!(contacts = userlist_dump(session))) {
		xfree(tmp);
		return -1;
	}
	
	if (!(filename = prepare_path(tmp, 1))) {
		xfree(contacts);
		xfree(tmp);
		return -1;
	}
	
	if (!(f = fopen(filename, "w"))) {
		xfree(contacts);
		return -2;
	}
	fchmod(fileno(f), 0600);
	fputs(contacts, f);
	fclose(f);
	
	xfree(contacts);

	return 0;
}

/*
 * userlist_write_crash()
 *
 * zapisuje listê kontaktów w sytuacji kryzysowej jak najmniejszym
 * nak³adem pamiêci i pracy.
 */
void userlist_write_crash()
{
	list_t l;
	char name[32];
	FILE *f;

	chdir(config_dir);
	
	snprintf(name, sizeof(name), "userlist.%d", (int) getpid());
	if (!(f = fopen(name, "w")))
		return;

	chmod(name, 0400);
		
	for (l = userlist; l; l = l->next) {
		userlist_t *u = l->data;
		list_t m;
		
		fprintf(f, "%s;%s;%s;%s;%s;", 
			(u->first_name) ? u->first_name : "",
			(u->last_name) ? u->last_name : "",
			(u->nickname) ? u->nickname : "",
			(u->nickname) ? u->nickname : "",
			(u->mobile) ? u->mobile : "");
		
		for (m = u->groups; m; m = m->next) {
			struct group *g = m->data;

			if (m != u->groups)
				fprintf(f, ",");

			fprintf(f, "%s", g->name);
		}
		
		fprintf(f, ";%s%s\r\n", u->uid, u->foreign);
	}	

	fclose(f);
}

/*
 * userlist_clear_status()
 *
 * czy¶ci stan u¿ytkowników na li¶cie. je¶li uin != 0 to
 * to czy¶ci danego u¿ytkownika.
 *
 *  - uin.
 */
void userlist_clear_status(session_t *session, const char *uid)
{
        list_t l;

        for (l = session->userlist; l; l = l->next) {
                userlist_t *u = l->data;

		if (!uid || !xstrcasecmp(uid, u->uid)) {
			xfree(u->status);
			u->status = xstrdup(EKG_STATUS_NA);
			memset(&u->ip, 0, sizeof(struct in_addr));
			u->port = 0;
			xfree(u->descr);
			u->descr = NULL;
		}
        }
}

/*
 * userlist_free()
 *
 * czy¶ci listê u¿ytkowników i zwalnia pamiêæ.
 */
void userlist_free(session_t *session)
{
	list_t l;

	if (!session->userlist)
		return;

        for (l = session->userlist; l; l = l->next) {
                userlist_t *u = l->data;
		list_t lp;

	        xfree(u->first_name);
	        xfree(u->last_name);
	        xfree(u->nickname);
	        xfree(u->uid);
	        xfree(u->mobile);
	        xfree(u->status);
	        xfree(u->descr);
	        xfree(u->authtype);
	        xfree(u->foreign);
	        xfree(u->last_status);
	        xfree(u->last_descr);
	        xfree(u->resource);
	
	        for (lp = u->groups; lp; lp = lp->next) {
	                struct group *g = lp->data;
	
	                xfree(g->name);
	        }
	
	        list_destroy(u->groups, 1);
        }

        list_destroy(session->userlist, 1);	
}

/*
 * userlist_add()
 *
 * dodaje u¿ytkownika do listy.
 *
 *  - uin,
 *  - display.
 */
userlist_t *userlist_add(session_t *session, const char *uid, const char *nickname)
{
	userlist_t u;

	memset(&u, 0, sizeof(u));

	u.uid = xstrdup(uid);
	u.nickname = xstrdup(nickname);
	u.status = xstrdup(EKG_STATUS_NA);

        u.first_name = NULL;
        u.last_name = NULL;
        u.mobile = NULL;
        u.descr = NULL;
        u.authtype = NULL;
        u.foreign = NULL;
        u.last_status = NULL;
        u.last_descr = NULL;
        u.resource = NULL;

	return list_add_sorted(&(session->userlist), &u, sizeof(u), userlist_compare);
}

/*
 * userlist_remove()
 *
 * usuwa danego u¿ytkownika z listy kontaktów.
 *
 *  - u.
 */
int userlist_remove(session_t *session, userlist_t *u)
{
	list_t l;

	if (!u)
		return -1;


	
	xfree(u->first_name);
	xfree(u->last_name);
	xfree(u->nickname);
	xfree(u->uid);
	xfree(u->mobile);
	xfree(u->status);
	xfree(u->descr);
	xfree(u->authtype);
	xfree(u->foreign);
	xfree(u->last_status);
	xfree(u->last_descr);
	xfree(u->resource);

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		xfree(g->name);
	}

	list_destroy(u->groups, 1);
	list_remove(&(session->userlist), u, 1);

	return 0;
}

/*
 * userlist_replace()
 *
 * usuwa i dodaje na nowo u¿ytkownika, ¿eby zosta³ umieszczony na odpowiednim
 * (pod wzglêdem kolejno¶ci alfabetycznej) miejscu. g³upie to trochê, ale
 * przy listach jednokierunkowych nie za bardzo jest sens komplikowaæ sprawê
 * z przesuwaniem elementów listy.
 * 
 *  - u.
 *
 * 0/-1
 */
int userlist_replace(session_t *session, userlist_t *u)
{
	if (!u)
		return -1;
	if (list_remove(&(session->userlist), u, 0))
		return -1;
	if (!list_add_sorted(&(session->userlist), u, 0, userlist_compare))
		return -1;

	return 0;
}

/*
 * userlist_find()
 *
 * znajduje odpowiedni± strukturê `userlist' odpowiadaj±c± danemu
 * identyfikatorowi lub jego opisowi.
 *
 *  - uid,
 */
userlist_t *userlist_find(session_t *session, const char *uid)
{
	list_t l;

	if (!uid || !session)
		return NULL;
	
	for (l = session->userlist; l; l = l->next) {
		userlist_t *u = l->data;
		const char *tmp;
		int len;

                if (!xstrcasecmp(u->uid, uid))
			return u;

		if (u->nickname && !xstrcasecmp(u->nickname, uid))
			return u;

		/* porównujemy resource */
		
		if (!(tmp = xstrchr(uid, '/')))
			continue;

		len = (int)(tmp - uid);

		if (!xstrncasecmp(uid, u->uid, len))
			return u;
		
        }

        return NULL;
}

int userlist_set(session_t *session, const char *contacts)
{
	char **entries = array_make(contacts, "\r\n", 0, 1, 0);
	int i;

	if (!session)
		return -1;

	userlist_free(session);

	for (i = 0; entries[i]; i++)
		userlist_add_entry(session, entries[i]);

	array_free(entries);

	return 0;
}

/*
 * valid_nick()
 *
 * sprawdza, czy nick nie zawiera znaków specjalnych,
 * które mog³yby powodowaæ problemy.
 *
 * zwraca 1 je¶li nick jest w porz±dku, w przeciwnym razie 0.
 */
int valid_nick(const char *nick)
{
	int i;
	const char *wrong[] = { "(null)", "__debug", "__status",
				 "__current", "__contacts", "*", "$", NULL };

	if (!nick)
		return 0;

	for (i = 0; wrong[i]; i++) {
		if (!xstrcmp(nick, wrong[i]))
			return 0;
	}

	if (nick[0] == '@' || nick[0] == '#' || xstrchr(nick, ','))
		return 0;

	return 1;
}

/*
 * valid_uid()
 *
 * sprawdza, czy uid jest obs³ugiwany przez jaki¶ plugin i czy jest
 * poprawny.
 *
 * zwraca 1 je¶li nick jest w porz±dku, w przeciwnym razie 0.
 */
int valid_uid(const char *uid)
{
	int valid = 0;
	char *tmp;
	tmp = xstrdup(uid);

	query_emit(NULL, "protocol-validate-uid", &tmp, &valid);
	xfree(tmp);

	return (valid > 0);
}


/*
 * valid_plugin_uid()
 *
 * sprawdza, czy uid jest obs³ugiwany przez podany plugin i czy jest
 * poprawny.
 *
 * zwraca 1 je¶li nick jest w porz±dku, w przeciwnym razie 0.
 * natoamiast zwraca -1 gdy pogadany plugin jest pusty 
 */

int valid_plugin_uid(plugin_t *plugin, const char *uid)
{
        int valid = 0;
        char *tmp;

	if (!plugin)
		return -1;

        tmp = xstrdup(uid);

        query_emit(plugin, "protocol-validate-uid", &tmp, &valid);
        xfree(tmp);

        return (valid > 0);

}

/*
 * get_uid()
 *
 * je¶li podany tekst jest uid (ale nie jednocze¶nie nazw± u¿ytkownika),
 * zwraca jego warto¶æ. je¶li jest nazw± u¿ytkownika w naszej li¶cie kontaktów,
 * zwraca jego uid. je¶li tekstem jestem znak ,,$'', zwraca uid aktualnego
 * rozmówcy. inaczej zwraca NULL.
 *
 *  - text.
 */
char *get_uid(session_t *session, const char *text)
{
	userlist_t *u;

	if (text && !xstrcmp(text, "$"))
		text = window_current->target;
	
	u = userlist_find(session, text);

	if (u && u->uid)
		return u->uid;

	if (valid_uid(text))
		return (char *)text;

	return NULL;
}

/*
 * get_uid_all()
 * 
 * the same as get_uid(), but searches in all sessions
 */
char *get_uid_all(const char *text)
{
	list_t l;
	for (l = sessions; l; l = l->next) {
		session_t *session = l->data;
	        userlist_t *u;

	        if (text && !xstrcmp(text, "$"))
	                text = window_current->target;
	
	        u = userlist_find(session, text);
	
	        if (u && u->uid)
	                return u->uid;
	
	        if (valid_uid(text))
	                return (char *)text;
	}

        return NULL;
}

/* 
 * get_nickname()
 *
 * if given text is nickname it returns the same, if it is 
 * an uid it returns its nickname (if exists), if there is 
 * no nickname it returns uid, else if contacts doesnt exist
 * it returns text if it is a correct uid, else NULL
 */
char *get_nickname(session_t *session, const char *text)
{
        userlist_t *u;

        u = userlist_find(session, text);

        if (u && u->nickname)
                return u->nickname;

	if (u && u->uid)
		return u->uid;

	if (valid_uid(text))
	        return (char *)text;

        return NULL;
}


/* 
 * check_uid_nick()
 *
 * checks, if given uid/nick is on our contacts list
 * it checks every session
 */

int check_uid_nick(const char *text)
{
        list_t l;

        if (!text)
                return -1;

        for (l = sessions; l; l = l->next) {
                session_t *s = l->data;

		if (userlist_find(s, text))
			return 1;
        }

        return 0;

}

/*
 * format_user()
 *
 * zwraca ³adny (ew. kolorowy) tekst opisuj±cy dany numerek. je¶li jest
 * w naszej li¶cie kontaktów, formatuje u¿ywaj±c `known_user', w przeciwnym
 * wypadku u¿ywa `unknown_user'. wynik jest w statycznym buforze.
 *
 *  - uin - numerek danej osoby.
 */
const char *format_user(session_t *session, const char *uid)
{
	userlist_t *u = userlist_find(session, uid);
	static char buf[256], *tmp;
	
//	if (uid && xstrchr(uid, ':'))
//		uid = xstrchr(uid, ':') + 1;

	if (!u || !u->nickname)
		tmp = format_string(format_find("unknown_user"), uid, uid);
	else
		tmp = format_string(format_find("known_user"), u->nickname, uid);
	
	strlcpy(buf, tmp, sizeof(buf));
	
	xfree(tmp);

	return buf;
}

/*
 * ignored_remove()
 *
 * usuwa z listy ignorowanych numerków.
 *
 *  - uin.
 */
int ignored_remove(session_t *session, const char *uid)
{
	userlist_t *u = userlist_find(session, uid);
	char *tmp;
	list_t l;
	int level, tmp2 = 0;

	if (!u)
		return -1;

	if (!(level = ignored_check(session,uid)))
		return -1;

	for (l = u->groups; l; ) {
		struct group *g = l->data;

		l = l->next;

		if (xstrncasecmp(g->name, "__ignored", 9))
			continue;

		xfree(g->name);
		list_remove(&u->groups, g, 1);
	}

	if (!u->nickname && !u->groups) {
		userlist_remove(session, u);
		return 0;
	}

	tmp = xstrdup(u->uid);
	query_emit(NULL, "protocol-ignore", &tmp, &level, &tmp2);
	xfree(tmp);
	
	if ((level & IGNORE_STATUS || level & IGNORE_STATUS_DESCR)) {
		query_emit(NULL, "protocol-unignore", &u, &session);
	}

	return 0;
}

/*
 * ignored_add()
 *
 * dopisuje do listy ignorowanych numerków.
 *
 *  - uin.
 *  - level.
 */
int ignored_add(session_t *session, const char *uid, int level)
{
	userlist_t *u;
	char *tmp;
	int oldlevel = 0;

	if (ignored_check(session, uid))
		return -1;
	
	if (!(u = userlist_find(session, uid)))
		u = userlist_add(session, uid, NULL);

	tmp = saprintf("__ignored_%d", level);
	ekg_group_add(u, tmp);
	xfree(tmp);

	if (level & IGNORE_STATUS) {
		xfree(u->status);
		u->status = xstrdup(EKG_STATUS_NA);
	}

	if (level & IGNORE_STATUS_DESCR) {
		xfree(u->descr);
		u->descr = NULL;
	}

	tmp = xstrdup(u->uid);
	query_emit(NULL, "protocol-ignore", &tmp, &oldlevel, &level);
	xfree(tmp);
	
	return 0;
}

/*
 * ignored_check()
 *
 * czy dany numerek znajduje siê na li¶cie ignorowanych.
 *
 *  - uin.
 */
int ignored_check(session_t *session, const char *uid)
{
	userlist_t *u = userlist_find(session, uid);
	list_t l;

	if (!u)
		return 0;

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!xstrcasecmp(g->name, "__ignored"))
			return IGNORE_ALL;

		if (!xstrncasecmp(g->name, "__ignored_", 10))
			return atoi(g->name + 10);
	}

	return 0;
}

/*
 * ignore_flags()
 *
 * zamienia ³añcuch znaków na odpowiedni
 * poziom ignorowania w postaci liczby.
 */
int ignore_flags(const char *str)
{
	int x, y, ret = 0;
	char **arr;

	if (!str)
		return ret;

	arr = array_make(str, "|,:", 0, 1, 0);

	for (x = 0; arr[x]; x++) {
		if (!xstrcmp(arr[x], "*")) {
			ret = IGNORE_ALL;
			break;
		}

		for (y = 0; ignore_labels[y].name; y++)
			if (!xstrcasecmp(arr[x], ignore_labels[y].name))
				ret |= ignore_labels[y].level;
	}

	array_free(arr);

	return ret;
}

/*
 * ignore_format()
 *
 * zwraca statyczny ³añcuch znaków reprezentuj±cy
 * dany poziom ignorowania.
 */
const char *ignore_format(int level)
{
	static char buf[200];
	int i, comma = 0;

	buf[0] = 0;

	if (level == IGNORE_ALL)
		return "*";

	for (i = 0; ignore_labels[i].name; i++) {
		if (level & ignore_labels[i].level) {
			if (comma++)
				strlcat(buf, ",", sizeof(buf));

			strlcat(buf, ignore_labels[i].name, sizeof(buf));
		}
	}

	return buf;
}

/*
 * group_compare()
 *
 * funkcja pomocna przy list_add_sorted().
 *
 *  - data1, data2 - dwa wpisy grup do porównania.
 *
 * zwraca wynik xstrcasecmp() na nazwach grup.
 */
static int group_compare(void *data1, void *data2)
{
	struct group *a = data1, *b = data2;
	
	if (!a || !a->name || !b || !b->name)
		return 0;

	return xstrcasecmp(a->name, b->name);
}

/*
 * ekg_group_add()
 *
 * dodaje u¿ytkownika do podanej grupy.
 *
 *  - u - wpis usera,
 *  - group - nazwa grupy.
 */
int ekg_group_add(userlist_t *u, const char *group)
{
	struct group g;
	list_t l;

	if (!u || !group)
		return -1;

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!xstrcasecmp(g->name, group))
			return -1;
	}
	
	g.name = xstrdup(group);

	list_add_sorted(&u->groups, &g, sizeof(g), group_compare);

	return 0;
}

/*
 * ekg_group_remove()
 *
 * usuwa u¿ytkownika z podanej grupy.
 *
 *  - u - wpis usera,
 *  - group - nazwa grupy.
 *
 * zwraca 0 je¶li siê uda³o, inaczej -1.
 */
int ekg_group_remove(userlist_t *u, const char *group)
{
	list_t l;

	if (!u || !group)
		return -1;
	
	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!xstrcasecmp(g->name, group)) {
			xfree(g->name);
			list_remove(&u->groups, g, 1);
			
			return 0;
		}
	}
	
	return -1;
}

/*
 * ekg_group_member()
 *
 * sprawdza czy u¿ytkownik jest cz³onkiem danej grupy.
 *
 * zwraca 1 je¶li tak, 0 je¶li nie.
 */
int ekg_group_member(userlist_t *u, const char *group)
{
	list_t l;

	if (!u || !group)
		return 0;

	for (l = u->groups; l; l = l->next) {
		struct group *g = l->data;

		if (!xstrcasecmp(g->name, group))
			return 1;
	}

	return 0;
}

/*
 * group_init()
 *
 * inicjuje listê grup u¿ytkownika na podstawie danego ci±gu znaków,
 * w którym kolejne nazwy grup s± rozdzielone przecinkiem.
 * 
 *  - names - nazwy grup.
 *
 * zwraca listê `struct group' je¶li siê uda³o, inaczej NULL.
 */
list_t group_init(const char *names)
{
	list_t l = NULL;
	char **groups;
	int i;

	if (!names)
		return NULL;

	groups = array_make(names, ",", 0, 1, 0);

	for (i = 0; groups[i]; i++) {
		struct group g;

		g.name = xstrdup(groups[i]);
		list_add_sorted(&l, &g, sizeof(g), group_compare);
	}
	
	array_free(groups);
	
	return l;
}

/*
 * group_to_string()
 *
 * zmienia listê grup na ci±g znaków rodzielony przecinkami.
 *
 *  - groups - lista grup.
 *  - meta - czy do³±czyæ ,,meta-grupy''?
 *  - sep - czy oddzielaæ przecinkiem _i_ spacj±?
 *
 * zwraca zaalokowany ci±g znaków lub NULL w przypadku b³êdu.
 */
char *group_to_string(list_t groups, int meta, int sep)
{
	string_t foo = string_init(NULL);
	list_t l;
	int comma = 0;

	for (l = groups; l; l = l->next) {
		struct group *g = l->data;

		if (!meta && !strncmp(g->name, "__", 2)) {
			comma = 0;
			continue;
		}

		if (comma)
			string_append(foo, (sep) ? ", " : ",");

		comma = 1;

		string_append(foo, g->name);
	}

	return string_free(foo, 0);
}

/*
 * same_protocol()
 *
 * sprawdza, czy wszystkie uidy s± tego samego protoko³u.
 */
int same_protocol(char **uids)
{
	const char *colon;
	int len, i;

	if (!uids || !uids[0] || !(colon = xstrchr(uids[0], ':')))
		return 0;

	len = (int) (colon - uids[0]) + 1;

	for (i = 0; uids[i]; i++)
		if (strncmp(uids[0], uids[i], len))
			return 0;

	return 1;
}

