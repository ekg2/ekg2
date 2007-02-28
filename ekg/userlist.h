/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
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

#ifndef __EKG_USERLIST_H
#define __EKG_USERLIST_H

#include "ekg2-config.h"
#include "win32.h"

#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "dynstuff.h"
#include "sessions.h"
#include "plugins.h"
#include "windows.h"

typedef struct {
	char *uid;		/* protokó³:identyfikator */
	char *nickname;		/* pseudonim */
	char *first_name;	/* imiê */
	char *last_name;	/* nazwisko */
	char *mobile;		/* komórka */
	list_t groups;		/* grupy, do których nale¿y */
	
	char *status;		/* aktualny stan: notavail, avail, away,
				 * invisible, dnd, xa itp. */
	char *descr;		/* opis/powód stanu */
	char *authtype;		/* to/from/both itp */	
	char *resource;		/* for leafnode, always NULL */
	list_t resources;	/* jabberowe resources */

	uint32_t ip;		/* adres ip */
	uint16_t port;		/* port */

	time_t last_seen;	/* je¶li niedostêpny/ukryty to od kiedy */
	
	int protocol;		/* wersja protoko³u */

	char *foreign;		/* dla kompatybilno¶ci */

	void *priv;		/* dane pluginu obs³uguj±cego usera */
	
	int xstate;		/* formerly called blink */

        uint32_t last_ip;       /* ostatni adres ip */
        uint16_t last_port;     /* ostatni port */

	char *last_status;	/* ostatni stan */
	char *last_descr;	/* ostatni opis */
	time_t status_time;	/* kiedy w³±czyli¶my aktualny status */
	void *private;          /* sometimes can be helpfull */
} userlist_t;

#define EKG_STATUS_NA "notavail"
#define EKG_STATUS_AVAIL "avail"
#define EKG_STATUS_AWAY "away"
#define EKG_STATUS_INVISIBLE "invisible"
#define EKG_STATUS_XA "xa"
#define EKG_STATUS_DND "dnd"
#define EKG_STATUS_FREE_FOR_CHAT "chat"
#define EKG_STATUS_BLOCKED "blocked"
#define EKG_STATUS_UNKNOWN "unknown"
#define EKG_STATUS_ERROR "error"
/* only for session_status_set() */
#define EKG_STATUS_AUTOAWAY "autoaway"
#define EKG_STATUS_AUTOXA "autoxa"
#define EKG_STATUS_AUTOBACK "autoback"

#define EKG_XSTATE_BLINK	01
#define EKG_XSTATE_TYPING	02

typedef struct {
	char *name;		/* name of resource */
	char *status;		/* status, like u->status */
	char *descr;		/* descr, like u->descr */
	int prio;		/* prio of resource */
	void *private;		/* priv, like u->private */
} ekg_resource_t;

struct ekg_group {
	char *name;
};

enum ignore_t {
	IGNORE_STATUS = 1,
	IGNORE_STATUS_DESCR = 2,
	IGNORE_MSG = 4,
	IGNORE_DCC = 8,
	IGNORE_EVENTS = 16,
	IGNORE_NOTIFY = 32,
	IGNORE_XOSD = 64,
	
	IGNORE_ALL = 255
};

struct ignore_label {
	int level;
	char *name;
};

#define	IGNORE_LABELS_MAX 8
extern struct ignore_label ignore_labels[IGNORE_LABELS_MAX];

#ifndef EKG2_WIN32_NOFUNCTION

int userlist_read(session_t* session);
int userlist_write(session_t *session);
#ifdef WITH_WAP
int userlist_write_wap();
#endif
void userlist_write_crash();
void userlist_clear_status(session_t *session, const char *uid);
userlist_t *userlist_add(session_t *session, const char *uid, const char *nickname);
userlist_t *userlist_add_u(list_t *userlist, const char *uid, const char *nickname);
void userlist_add_entry(session_t *session,const char *line);
int userlist_remove(session_t *session, userlist_t *u);
int userlist_remove_u(list_t *userlist, userlist_t *u);
int userlist_replace(session_t *session, userlist_t *u);
userlist_t *userlist_find(session_t *session, const char *uid);
userlist_t *userlist_find_u(list_t *userlist, const char *uid);
#define userlist_find_n(a, b) userlist_find(session_find(a), b)
char *userlist_dump(session_t *session);
void userlist_free(session_t *session);
void userlist_free_u(list_t *userlist);
int userlist_set(session_t *session, const char *contacts);

/* u->resource */
ekg_resource_t *userlist_resource_add(userlist_t *u, const char *name, int prio);
ekg_resource_t *userlist_resource_find(userlist_t *u, const char *name);
void userlist_resource_remove(userlist_t *u, ekg_resource_t *r);
void userlist_resource_free(userlist_t *u);

int ignored_add(session_t *session, const char *uid, int level);
int ignored_remove(session_t *session, const char *uid);
int ignored_check(session_t *session, const char *uid);
int ignore_flags(const char *str);
const char *ignore_format(int level);

int ekg_group_add(userlist_t *u, const char *group);
int ekg_group_remove(userlist_t *u, const char *group);
int ekg_group_member(userlist_t *u, const char *group);
char *group_to_string(list_t l, int meta, int sep);
list_t group_init(const char *groups);

int valid_nick(const char *nick);
int valid_uid(const char *uid);
int valid_plugin_uid(plugin_t *plugin, const char *uid);
const char *format_user(session_t *session, const char *uid);
char *get_uid(session_t *session, const char *text);
char *get_nickname(session_t *session, const char *text);

#endif

#endif /* __EKG_USERLIST_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
