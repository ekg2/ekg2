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

#include "char.h"
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
	char *resource;         /* jabberowy resource */

	uint32_t ip;		/* adres ip */
	uint16_t port;		/* port */

	time_t last_seen;	/* je¶li niedostêpny/ukryty to od kiedy */
	
	int protocol;		/* wersja protoko³u */

	char *foreign;		/* dla kompatybilno¶ci */

	void *priv;		/* dane pluginu obs³uguj±cego usera */
	
	int blink;		/* czy ma byæ zaznaczony jako u¿ytkownik, od którego mamy msg */

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
#define EKG_STATUS_AUTOAWAY "autoaway"	/* tylko dla session_status_set() */
#define EKG_STATUS_INVISIBLE "invisible"
#define EKG_STATUS_XA "xa"
#define EKG_STATUS_DND "dnd"
#define EKG_STATUS_FREE_FOR_CHAT "chat"
#define EKG_STATUS_BLOCKED "blocked"
#define EKG_STATUS_UNKNOWN "unknown"
#define EKG_STATUS_ERROR "error"

/* WCS VERSION */
#define WCS_EKG_STATUS_NA TEXT("notavail")
#define WCS_EKG_STATUS_AVAIL TEXT("avail")
#define WCS_EKG_STATUS_AWAY TEXT("away")
#define WCS_EKG_STATUS_AUTOAWAY TEXT("autoaway")	/* tylko dla session_status_set() */
#define WCS_EKG_STATUS_INVISIBLE TEXT("invisible")
#define WCS_EKG_STATUS_XA TEXT("xa")
#define WCS_EKG_STATUS_DND TEXT("dnd")
#define WCS_EKG_STATUS_FREE_FOR_CHAT TEXT("chat")
#define WCS_EKG_STATUS_BLOCKED TEXT("blocked")
#define WCS_EKG_STATUS_UNKNOWN TEXT("unknown")
#define WCS_EKG_STATUS_ERROR TEXT("error")

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
CHAR_T *userlist_dump(session_t *session);
void userlist_free(session_t *session);
void userlist_free_u(list_t *userlist);
int userlist_set(session_t *session, const char *contacts);

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
int same_protocol(char **uids);
const char *format_user(session_t *session, const char *uid);
char *get_uid(session_t *session, const char *text);
char *get_uid_all(const char *text);
char *get_nickname(session_t *session, const char *text);
int check_uid_nick(const char *text);

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
