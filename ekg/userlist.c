/* $Id$ */

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

#include "ekg2-config.h"
#include "win32.h"

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <sys/types.h>
#include <sys/stat.h>
#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

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

#include "debug.h"
#include "dynstuff_inline.h"
#include "queries.h"

static void userlist_private_free(userlist_t *u);

struct ignore_label ignore_labels[IGNORE_LABELS_MAX] = {
	{ IGNORE_STATUS, "status" },
	{ IGNORE_STATUS_DESCR, "descr" },
	{ IGNORE_NOTIFY, "notify" },
	{ IGNORE_MSG, "msg" },
	{ IGNORE_DCC, "dcc" },
	{ IGNORE_EVENTS, "events" },
	{ IGNORE_XOSD, "xosd" },
	{ IGNORE_LOG, "log" },
	{ 0, NULL }
};

/* groups: */
static LIST_ADD_COMPARE(group_compare, struct ekg_group *) { return xstrcasecmp(data1->name, data2->name); }
static LIST_FREE_ITEM(group_item_free, struct ekg_group *) { xfree(data->name); }
DYNSTUFF_LIST_DECLARE_SORTED(ekg_groups, struct ekg_group, group_compare, group_item_free,
	static __DYNSTUFF_ADD_SORTED,		/* ekg_groups_add() */
	static __DYNSTUFF_REMOVE_ITER,		/* ekg_groups_removei() */
	static __DYNSTUFF_DESTROY)		/* ekg_groups_destroy() */

/* resources: */
static LIST_ADD_COMPARE(userlist_resource_compare, ekg_resource_t *) {
	if (data1->prio != data2->prio)			
		return (data2->prio - data1->prio);	/* sort by prio */

	return xstrcasecmp(data1->name, data2->name);	/* sort by name */
}
static LIST_FREE_ITEM(list_userlist_resource_free, ekg_resource_t *) { xfree(data->name); xfree(data->descr); }
DYNSTUFF_LIST_DECLARE_SORTED(ekg_resources, ekg_resource_t, userlist_resource_compare, list_userlist_resource_free,
	static __DYNSTUFF_ADD_SORTED,		/* ekg_resources_add() */
	static __DYNSTUFF_REMOVE_SAFE,		/* ekg_resources_remove() */
	static __DYNSTUFF_DESTROY)		/* ekg_resources_destroy() */

/* userlist: */
static LIST_ADD_COMPARE(userlist_compare, userlist_t *) { return xstrcasecmp(data1->nickname, data2->nickname); }
static LIST_FREE_ITEM(userlist_free_item, userlist_t *) {
	userlist_private_free(data);
	private_items_destroy(&data->priv_list);
	xfree(data->uid); xfree(data->nickname); xfree(data->descr); xfree(data->foreign); xfree(data->last_descr);
	xfree(data->descr1line);
	ekg_groups_destroy(&(data->groups));
	ekg_resources_destroy(&(data->resources));
}
DYNSTUFF_LIST_DECLARE_SORTED(userlists, userlist_t, userlist_compare, userlist_free_item,
	static __DYNSTUFF_ADD_SORTED,					/* userlists_add() */
	__DYNSTUFF_REMOVE_SAFE,						/* userlists_remove() */
	__DYNSTUFF_DESTROY)						/* userlists_destroy() */

/*
 * userlist_add_entry()
 *
 * dodaje do listy kontaktów pojedyncz± liniê z pliku lub z serwera.
 */
void userlist_add_entry(session_t *session, const char *line) {
	char **entry = array_make(line, ";", 8, 0, 0);
	userlist_t *u;
	int count, i;

	if ((count = array_count(entry)) < 7) {
		array_free(entry);
		return;
	}

	u = xmalloc(sizeof(userlist_t)); /* we'd need this here */
	u->uid = entry[6];	entry[6] = NULL;

	{
		int function = EKG_USERLIST_PRIVHANDLER_READING;

		query_emit_id(NULL, USERLIST_PRIVHANDLE, &u, &function, &entry, &count);
	}

	if (valid_plugin_uid(session->plugin, u->uid) != 1) {
		debug_error("userlist_add_entry() wrong uid: %s for session: %s [plugin: 0x%x]\n", u->uid, session->uid, session->plugin);
		array_free_count(entry, count);
		xfree(u->uid);
		xfree(u);
		return;
	}

	u->status = EKG_STATUS_NA;

	for (i = 0; i < 6; i++) {
		if (!xstrcmp(entry[i], "(null)") || !xstrcmp(entry[i], "")) {
			xfree(entry[i]);
			entry[i] = NULL;
		}
	}
			
	u->groups	= group_init(entry[5]);

	if (entry[3]) {
		u->nickname	= !valid_nick(entry[3]) ? 
			saprintf("_%s", entry[3]) :
			xstrdup(entry[3]);
	}

	u->foreign	= entry[7] ? 
		saprintf(";%s", entry[7]) :
		NULL;
	
	array_free_count(entry, count);
	userlists_add(&(session->userlist), u);
}

/**
 * userlist_read()
 *
 * wczytuje listê kontaktów z pliku uid_sesji-userlist w postaci eksportu
 * tekstowego listy kontaktów windzianego klienta.
 *
 * @param session
 * @return 0 on success, -1 file not found
 */
int userlist_read(session_t *session) {
	const char *filename;
	char *buf;
	FILE *f;

	if (!(filename = prepare_pathf("%s-userlist", session->uid)))
		return -1;
	
	if (!(f = fopen(filename, "r")))
		return -1;
			
	while ((buf = read_file(f, 0))) {
		if (buf[0] == '#' || (buf[0] == '/' && buf[1] == '/'))
			continue;
		
		userlist_add_entry(session, buf);
	}

	query_emit_id(NULL, USERLIST_REFRESH);	/* XXX, wywolywac tylko kiedy dodalismy przynajmniej 1 */

	fclose(f);
		
	return 0;
} 

/**
 * userlist_write()
 *
 * It writes @a session userlist to file: <i>session->uid</i>-userlist in ekg2 config directory
 *
 * @todo	Each plugin should've own userlist_write()/ userlist_read()
 *		This format is obsolete.
 *
 * @param session
 *
 * @return	 0 on succees<br>
 *		-1 if smth went wrong<br>
 *		-2 if we fail to create/open userlist file in rw mode
 */

int userlist_write(session_t *session) {
	const char *filename;
	FILE *f;
	userlist_t *ul;

	if (!prepare_path(NULL, 1))	/* try to create ~/.ekg2 dir */
		return -1;

	if (!(filename = prepare_pathf("%s-userlist", session->uid)))
		return -1;
	
	if (!(f = fopen(filename, "w"))) {
		return -2;
	}
	fchmod(fileno(f), 0600);

	/* userlist_dump() */
	for (ul = session->userlist; ul; ul = ul->next) {
		userlist_t *u = ul;
		char **entry = xcalloc(7, sizeof(char *));
		char *line;

		entry[0] = NULL;				/* first name [gg] */
		entry[1] = NULL;				/* last name [gg] */
		entry[2] = xstrdup(u->nickname);		/* display? backwards compatibility? */
		entry[3] = xstrdup(u->nickname);		/* nickname */
		entry[4] = NULL;				/* mobile [gg] */
		entry[5] = group_to_string(u->groups, 1, 0);	/* groups (alloced itself) */
		entry[6] = strdup(u->uid);			/* uid */

		{
			int function = EKG_USERLIST_PRIVHANDLER_WRITING;

			query_emit_id(NULL, USERLIST_PRIVHANDLE, &u, &function, &entry);
		}

		line = array_join_count(entry, ";", 7);

		fprintf(f, "%s%s\n", 
			line,					/* look upper */
			u->foreign ? u->foreign : "");		/* backwards compatibility */

		xfree(line);
		array_free_count(entry, 7);
	}

	fclose(f);
	return 0;
}

/**
 * userlist_write_crash()
 *
 * zapisuje listê kontaktów w sytuacji kryzysowej jak najmniejszym
 * nak³adem pamiêci i pracy.
 *
 * @sa userlist_write
 * @bug It was copied from ekg1 and it doesn't match ekg2 abi.
 *	It's bad so i comment it out... Reimplement it or delete
 */
void userlist_write_crash() {
/*
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
			struct ekg_group *g = m->data;

			if (m != u->groups)
				fprintf(f, ",");

			fprintf(f, "%s", g->name);
		}
		
		fprintf(f, ";%s%s\r\n", u->uid, u->foreign);
	}	

	fclose(f);
 */
}

static void userlist_private_free(userlist_t *u) {
	if (u->priv) {
		int func = EKG_USERLIST_PRIVHANDLER_FREE;

		query_emit_id(NULL, USERLIST_PRIVHANDLE, &u, &func);
	}
}

void *userlist_private_get(plugin_t *plugin, userlist_t *u) {
	int func = EKG_USERLIST_PRIVHANDLER_GET;
	void *up = NULL;

	query_emit_id(plugin, USERLIST_PRIVHANDLE, &u, &func, &up);

	return up;
}

/**
 * userlist_clear_status()
 *
 * If @a uin == NULL then it clears all users <i>presence information</i> in the @a session userlist
 * otherwise it clears only specified user
 * It's useful if user goes notavail, or we goes disconnected..<br>
 * However if that happen you shouldn't use this function but emit query <i>PROTOCOL_STATUS</i> or <i>PROTOCOL_DISCONNECTED</i>
 *
 * @note By <i>presence information</i> I mean:<br>
 *	-> status	- user's status [avail, away, ffc, dnd], it'll be: @a EKG_STATUS_NA ("notavail")<br>
 *	-> descr	- user's description, it'll be: NULL<br>
 *	-> ip		- user's ip, il'll be: 0.0.0.0<br>
 *	-> port		- user's port, it'll be: 0<br>
 *	-> resources	- user's resource, list will be destroyed.
 *
 * @param session
 * @param uid
 */

void userlist_clear_status(session_t *session, const char *uid) {
	userlist_t *ul;

	if (!session)
		return;

	for (ul = session->userlist; ul; ul = ul->next) {
		userlist_t *u = ul;

		if (!uid || !xstrcasecmp(uid, u->uid)) {
			u->status = EKG_STATUS_NA;
			xfree(u->descr);
			u->descr = NULL;

			ekg_resources_destroy(&(u->resources));
		}
	}
}

/*
 * userlist_free()
 *
 * czy¶ci listê u¿ytkowników i zwalnia pamiêæ.
 */
void userlist_free(session_t *session) {
	if (!session)
		return;

	userlists_destroy(&(session->userlist));
}

/**
 * userlist_resource_add()
 *
 * It adds <b>new</b> user resource to resources list, with given data.
 *
 * @note It <b>doesn't</b> check if prio already exists.. So you must remember about
 *	calling userlist_resource_find() if you don't want two (or more) same resources...
 *
 * @param u - user
 * @param name - name of resource
 * @param prio - prio of resource
 *
 * @return It returns inited ekg_resource_t pointer of given data, or NULL if @a u was NULL
 */
ekg_resource_t *userlist_resource_add(userlist_t *u, const char *name, int prio) {
	ekg_resource_t *r;

	if (!u) 
		return NULL;

	r	= xmalloc(sizeof(ekg_resource_t));
	r->name		= xstrdup(name);
	r->prio		= prio;
	r->status	= EKG_STATUS_NA;

	ekg_resources_add(&(u->resources), r);
	return r;
}

/**
 * userlist_resource_find()
 *
 * It search for given resource @a name in user resource list @a u
 *
 * @param u - user
 * @param name - name of resource
 *
 * @return It returns resource with given name if founded, otherwise NULL
 */
ekg_resource_t *userlist_resource_find(userlist_t *u, const char *name) {
	ekg_resource_t *rl;
	if (!u) return NULL;

	for (rl = u->resources; rl; rl = rl->next) {
		ekg_resource_t *r = rl;

		if (!xstrcmp(r->name, name))
			return r;
	}
	return NULL;
}

/**
 * userlist_resource_remove()
 *
 * Remove given resource @a r from user resource list @a u<br>
 * Free allocated memory.
 *
 * @param u - user
 * @param r - resource
 */
void userlist_resource_remove(userlist_t *u, ekg_resource_t *r) {		/* XXX, remove */
	if (!u || !r) return;
	ekg_resources_remove(&(u->resources), r);
}

/*
 * userlist_add()
 *
 * dodaje u¿ytkownika do listy.
 *
 *  - uin,
 *  - display.
 */
userlist_t *userlist_add(session_t *session, const char *uid, const char *nickname) {
	if (!session)
		return NULL;

	if (valid_plugin_uid(session->plugin, uid) != 1) {
		debug_error("userlist_add() wrong uid: %s for session: %s [plugin: 0x%x]\n", uid, session->uid, session->plugin);
		return NULL;
	}

	return userlist_add_u(&(session->userlist), uid, nickname);
}

/*
 * userlist_add_u()
 *
 * adds user to window's userlist
 * uid - uid,
 * nickname - display.
 */
userlist_t *userlist_add_u(userlist_t **userlist, const char *uid, const char *nickname) {
	userlist_t *u = xmalloc(sizeof(userlist_t));

	u->uid = xstrdup(uid);
	u->nickname = xstrdup(nickname);
	u->status = EKG_STATUS_NA;

	userlists_add(userlist, u);
	return u;
}

/*
 * userlist_remove()
 *
 * usuwa danego u¿ytkownika z listy kontaktów.
 *
 *  - u.
 */
int userlist_remove(session_t *session, userlist_t *u) {
	return userlist_remove_u(&(session->userlist), u);
}

/*
 * userlist_remove_u()
 *
 * removes given user from window's userlist
 *
 *  - u.
 */
int userlist_remove_u(userlist_t **userlist, userlist_t *u) {
	if (!u)
		return -1;

	userlists_remove(userlist, u);

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
int userlist_replace(session_t *session, userlist_t *u) {
	if (!u)
		return -1;
	if (!LIST_UNLINK2(&(session->userlist), u) && (errno == ENOENT))
		return -1;
	userlists_add(&(session->userlist), u);

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
userlist_t *userlist_find(session_t *session, const char *uid) {
	if (!uid || !session)
		return NULL;

	return userlist_find_u(&(session->userlist), uid);
}

/* 
 * userlist_find_u()
 *
 * finds and returns pointer to userlist_t which includes given
 * uid
 */
userlist_t *userlist_find_u(userlist_t **userlist, const char *uid) {
	userlist_t *ul;

	if (!uid || !userlist)
		return NULL;

	for (ul = *userlist; ul; ul = ul->next) {
		userlist_t *u = ul;
		const char *tmp;
		int len;

		if (!xstrcasecmp(u->uid, uid))
			return u;

		if (u->nickname && !xstrcasecmp(u->nickname, uid))
			return u;

		/* porównujemy resource; if (len > 0) */

		if (!(tmp = xstrchr(uid, '/')) || (xstrncmp(uid, "tlen:", 5) && xstrncmp(uid, "xmpp:", 5)) )
			continue;

		len = (int)(tmp - uid);

		if (len > 0 && !xstrncasecmp(uid, u->uid, len))
			return u;
	}

	return NULL;
}

/*
 * valid_nick()
 *
 * sprawdza, czy nick nie zawiera znaków specjalnych,
 * które mog³yby powodowaæ problemy.
 *
 * zwraca 1 je¶li nick jest w porz±dku, w przeciwnym razie 0.
 */
int valid_nick(const char *nick) {
	const char *wrong[] = { "(null)", "__debug", "__status",
				 "__current", "__contacts", "*", "$", NULL };
	int i;

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

/**
 * valid_uid()
 *
 * Check if @a uid can be handled by any plugin
 *
 * @param uid	- uid to check for
 *
 * @sa valid_plugin_uid()	- You can specify plugin
 *
 * @return	1 - if uid can be handled by ekg2<br>
 *		0 - if not
 */

int valid_uid(const char *uid) {
	int valid = 0;
	char *tmp;
	tmp = xstrdup(uid);

	query_emit_id(NULL, PROTOCOL_VALIDATE_UID, &tmp, &valid);
	xfree(tmp);

	return (valid > 0);
}

/**
 * valid_plugin_uid()
 *
 * Check if @a uid can be handled by given @a plugin
 * 
 * @param plugin	- plugin to check for
 * @param uid		- uid to check for
 *
 * @sa valid_uid()	- if we want to know if this @a uid can be handled by ekg2 [when it doesn't matter which plugin]
 *
 * @return	 1 - if @a uid can be handled by @a plugin<br>
 *		 0 - if not<br>
 *		-1 - if @a plugin == NULL
 */

int valid_plugin_uid(plugin_t *plugin, const char *uid) {
	int valid = 0;
	char *tmp;

	if (!plugin) {
		debug_error("valid_plugin_uid() no plugin passed. In current api, it means than something really bad happen.\n");
		return -1;
	}

	tmp = xstrdup(uid);

	query_emit_id(plugin, PROTOCOL_VALIDATE_UID, &tmp, &valid);
	xfree(tmp);

	return (valid > 0);
}

/**
 * get_uid_any()
 *
 * Return and checks if uid passed @a text is proper for at least one session, or it's nickname of smb on @a session userlist
 *
 * @param session	- session to search for item on userlist
 * @param text		- uid to check for, if '$' then check current window.
 *
 * @sa get_uid()	- to search only specified session.
 *
 * @return If we found proper uid for @a text, than return it. Otherwise NULL
 */

char *get_uid_any(session_t *session, const char *text) {
	char *uid = get_uid(session, text);

	if (!session) 
		return uid;

	if (!uid && !xstrcmp(text, "$"))
		text = window_current->target;

	return uid ? uid : valid_uid(text) ? (char *) text : NULL;
}

/**
 * get_uid()
 *
 * Return and checks if uid passed @a text is proper for @a session or it's nickname of smb on @a session userlist.
 *
 * @note It also work with userlist_find() and if @a text is nickname of smb in session userlist.. 
 *	 Than it return uid of this user. 
 *	 So you shouldn't call userlist_find() with get_uid() as param, cause it's senseless
 *	 userlist_find() don't check for "$" target, so you must do it by hand. Rest is the same.
 *	 If there are such user:
 *	 <code>userlist_find(s, get_uid(s, target))</code> return the same as <code>userlist_find(s, target)</code><br>
 *	 If not, even <code>userlist_find(s, get_uid(s, get_uid(s, get_uid(s, target))))</code> won't help 
 *
 * @param session - session to check for, if NULL check all sessions (it doesn't look at userlists, in this mode)
 * @param text - uid to check for, if '$' than check current window.
 *
 * @sa userlist_find()
 * @sa get_nickname()	- to look for nickname..
 * @sa get_uid_any()	- to do all session searching+specified session userlist search..
 *				This function does only all session searching if @a session is NULL... and than
 *				it doesn't look at userlist. Do you feel difference?
 *
 * @return If we found proper uid for @a text, than return it. Otherwise NULL
 */

char *get_uid(session_t *session, const char *text) {
	userlist_t *u;

	if (text && !xstrcmp(text, "$"))
		text = window_current->target;

	if (!session)
		return valid_uid(text) ? (char *) text : NULL;

	u = userlist_find(session, text);

	if (u && u->uid)
		return u->uid;

	if (valid_plugin_uid(session->plugin, text) == 1)
		return (char *)text;

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
char *get_nickname(session_t *session, const char *text) {
	userlist_t *u;

	if (text && !xstrcmp(text, "$"))
		text = window_current->target;

	if (!session)
		return valid_uid(text) ? (char *) text : NULL;

	u = userlist_find(session, text);

	if (u && u->nickname)
		return u->nickname;

	if (u && u->uid)
		return u->uid;

	if (valid_plugin_uid(session->plugin, text) == 1)
		return (char *)text;

	return NULL;
}
/*
 * get_user_name()
 *
 * returns users first name or nick name or NULL
 *
 */
char *get_user_name(userlist_t *u) {
	char *name;
	if (!u)
		return NULL;
	name = user_private_item_get(u, "first_name");
	return (name && *name) ? name : u->nickname;
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
const char *format_user(session_t *session, const char *uid) {
	userlist_t *u = userlist_find(session, uid);
	static char buf[256], *tmp;

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
int ignored_remove(session_t *session, const char *uid) {
	userlist_t *u = userlist_find(session, uid);
	char *tmps, *tmp;
	struct ekg_group *gl;
	int level, tmp2 = 0;

	if (!u)
		return -1;

	if (!(level = ignored_check(session,uid)))
		return -1;

	for (gl = u->groups; gl; gl = gl->next) {
		struct ekg_group *g = gl;

		if (xstrncasecmp(g->name, "__ignored", 9))
			continue;

		gl = ekg_groups_removei(&u->groups, g);
	}

	if (!u->nickname && !u->groups) {
		userlist_remove(session, u);
		return 0;
	}

	tmps	= xstrdup(session->uid);
	tmp	= xstrdup(u->uid);
	query_emit_id(NULL, PROTOCOL_IGNORE, &tmps, &tmp, &level, &tmp2);
	xfree(tmps);
	xfree(tmp);

	if ((level & IGNORE_STATUS || level & IGNORE_STATUS_DESCR)) {
		query_emit_id(NULL, PROTOCOL_UNIGNORE, &u, &session);
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
int ignored_add(session_t *session, const char *uid, ignore_t level) {
	userlist_t *u;
	char *tmps, *tmp;
	int oldlevel = 0;

	if (ignored_check(session, uid))
		return -1;
	
	if (!(u = userlist_find(session, uid)))
		u = userlist_add(session, uid, NULL);

	tmp = saprintf("__ignored_%d", level);
	ekg_group_add(u, tmp);
	xfree(tmp);

	if (level & IGNORE_STATUS)
		u->status = EKG_STATUS_NA; /* maybe EKG_STATUS_UNKNOWN would be better? */

	if (level & IGNORE_STATUS_DESCR) {
		xfree(u->descr);
		u->descr = NULL;
	}

	tmps	= xstrdup(session->uid);
	tmp	= xstrdup(u->uid);
	query_emit_id(NULL, PROTOCOL_IGNORE, &tmps, &tmp, &oldlevel, &level);
	xfree(tmps);
	xfree(tmp);
	
	return 0;
}

/**
 * ignored_check()
 *
 * czy dany numerek znajduje siê na li¶cie ignorowanych.
 *
 * @param session - sesja w ktorej mamy szukac uzytkownika
 * @param uid - uid uzytkownika
 *
 */
int ignored_check(session_t *session, const char *uid) {
	userlist_t *u = userlist_find(session, uid);
	struct ekg_group *gl;

	if (!u)
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

/**
 * ignore_flags()
 *
 * zamienia ³añcuch znaków na odpowiedni
 * poziom ignorowania w postaci liczby.
 *
 * @param str
 * @sa ignore_format
 * @sa ignore_t
 * @sa ignore_labels
 *
 * @return zwraca bitmaske opisana przez str
 */
int ignore_flags(const char *str) {
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

/**
 * ignore_format()
 *
 * zwraca statyczny ³añcuch znaków reprezentuj±cy
 * dany poziom ignorowania.
 *
 * @param level - poziom ignorowania, bitmaska z `enum ignore_t'
 * @sa ignore_flags
 * @sa ignore_t
 * @sa ignore_labels
 *
 * @return zwraca <b>statyczny</b> bufor opisujacy bitmaske za pomoca `ignore_labels`
 */
const char *ignore_format(int level) {
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

/**
 * ekg_group_add()
 *
 * dodaje u¿ytkownika do podanej grupy.
 *
 * @param u - wpis usera,
 * @param group - nazwa grupy.
 *
 * @return -1 jesli juz user jest w tej grupie, lub zle parametry. 0 gdy dodano.
 */
int ekg_group_add(userlist_t *u, const char *group) {
	struct ekg_group *g, *gl;

	if (!u || !group)
		return -1;

	for (gl = u->groups; gl; gl = gl->next) {
		g = gl;

		if (!xstrcasecmp(g->name, group))
			return -1;
	}
	g = xmalloc(sizeof(struct ekg_group));
	g->name = xstrdup(group);

	ekg_groups_add(&u->groups, g);

	return 0;
}

/**
 * ekg_group_remove()
 *
 * usuwa u¿ytkownika z podanej grupy.
 *
 * @param u - wpis usera,
 * @param group - nazwa grupy.
 *
 * @return 0 je¶li siê uda³o, inaczej -1.
 */
int ekg_group_remove(userlist_t *u, const char *group) {
	struct ekg_group *gl;

	if (!u || !group)
		return -1;
	
	for (gl = u->groups; gl; gl = gl->next) {
		struct ekg_group *g = gl;

		if (!xstrcasecmp(g->name, group)) {
			(void) ekg_groups_removei(&u->groups, g);
			
			return 0;
		}
	}
	
	return -1;
}

/**
 * ekg_group_member()
 *
 * sprawdza czy u¿ytkownik jest cz³onkiem danej grupy.
 *
 * @param u - uzytkownik, ktorego chcemy sprawdzic
 * @param group - grupa ktora chcemy sprawdzic
 *
 * @return 1 je¶li tak, 0 je¶li nie.
 */
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

/**
 * group_init()
 *
 * inicjuje listê grup u¿ytkownika na podstawie danego ci±gu znaków,
 * w którym kolejne nazwy grup s± rozdzielone przecinkiem.
 * 
 *  @param names - nazwy grup.
 *
 *  @return zwraca listê `struct group' je¶li siê uda³o, inaczej NULL.
 */
struct ekg_group *group_init(const char *names) {
	struct ekg_group *gl = NULL;
	char **groups;
	int i;

	if (!names)
		return NULL;

	groups = array_make(names, ",", 0, 1, 0);

	for (i = 0; groups[i]; i++) {
		struct ekg_group *g = xmalloc(sizeof(struct ekg_group));

		g->name = groups[i];
		ekg_groups_add(&gl, g);
	}
	/* NOTE: we don't call here array_free() cause we use items of this
	 *	array @ initing groups. We don't use strdup()
	 */
	xfree(groups);
	
	return gl;
}

/**
 * group_to_string()
 *
 * zmienia listê grup na ci±g znaków rodzielony przecinkami.
 *
 *  @param groups - lista grup.
 *  @param meta - czy do³±czyæ ,,meta-grupy''?
 *  @param sep - czy oddzielaæ przecinkiem _i_ spacj±?
 *
 *  @return zwraca zaalokowany ci±g znaków lub NULL w przypadku b³êdu.
 */
char *group_to_string(struct ekg_group *groups, int meta, int sep) {
	string_t foo = string_init(NULL);
	struct ekg_group *gl;
	int comma = 0;

	for (gl = groups; gl; gl = gl->next) {
		struct ekg_group *g = gl;

		if (!meta && !xstrncmp(g->name, "__", 2)) {
			comma = 0; /* mg: why? */
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
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */

