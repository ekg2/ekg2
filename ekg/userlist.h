/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo�ny <speedy@ziew.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "dynstuff.h"
#include "objects.h"
#include "sessions.h"
#include "plugins.h"

typedef struct {
	char *uid;		/* protok�:identyfikator */
	char *nickname;		/* pseudonim */
	char *first_name;	/* imi� */
	char *last_name;	/* nazwisko */
	char *mobile;		/* kom�rka */
	list_t groups;		/* grupy, do kt�rych nale�y */
	
	char *status;		/* aktualny stan: notavail, avail, away,
				 * invisible, dnd, xa itp. */
	char *descr;		/* opis/pow�d stanu */
	
	uint32_t ip;		/* adres ip */
	uint16_t port;		/* port */

	time_t last_seen;	/* je�li niedost�pny/ukryty to od kiedy */
	
	int protocol;		/* wersja protoko�u */

	char *foreign;		/* dla kompatybilno�ci */

	void *priv;		/* dane pluginu obs�uguj�cego usera */
	
	int blink;		/* czy ma by� zaznaczony jako u�ytkownik, od kt�rego mamy msg */

        uint32_t last_ip;       /* ostatni adres ip */
        uint16_t last_port;     /* ostatni port */

	char *last_status;	/* ostatni stan */
	char *last_descr;	/* ostatni opis */
	time_t status_time;	/* kiedy w��czyli�my aktualny status */
} userlist_t;

#if 0
PROPERTY_STRING(userlist, uid);
PROPERTY_STRING(userlist, nickname);
PROPERTY_STRING(userlist, first_name);
PROPERTY_STRING(userlist, last_name);
PROPERTY_STRING(userlist, mobile);
PROPERTY_STRING(userlist, status);
PROPERTY_STRING(userlist, descr);
PROPERTY_INT(userlist, ip, uint32_t);
PROPERTY_INT(userlist, port, uint16_t);
PROPERTY_INT(userlist, last_ip, uint32_t);
PROPERTY_INT(userlist, last_port, uint16_t);
PROPERTY_INT(userlist, last_seen, time_t);
#endif

#define EKG_STATUS_NA "notavail"
#define EKG_STATUS_AVAIL "avail"
#define EKG_STATUS_AWAY "away"
#define EKG_STATUS_AUTOAWAY "autoaway"	/* tylko dla session_status_set() */
#define EKG_STATUS_INVISIBLE "invisible"
#define EKG_STATUS_XA "xa"
#define EKG_STATUS_DND "dnd"
#define EKG_STATUS_BLOCKED "blocked"
#define EKG_STATUS_UNKNOWN "unknown"

struct group {
	char *name;
};

enum ignore_t {
	IGNORE_STATUS = 1,
	IGNORE_STATUS_DESCR = 2,
	IGNORE_MSG = 4,
	IGNORE_DCC = 8,
	IGNORE_EVENTS = 16,
	IGNORE_NOTIFY = 32,
	
	IGNORE_ALL = 255
};

#define	IGNORE_LABELS_MAX 7
struct ignore_label ignore_labels[IGNORE_LABELS_MAX];

struct ignore_label {
	int level;
	char *name;
};

//list_t userlist;

int userlist_read(session_t* session);
int userlist_write(session_t *session);
#ifdef WITH_WAP
int userlist_write_wap();
#endif
void userlist_write_crash();
void userlist_clear_status(session_t *session, const char *uid);
userlist_t *userlist_add(session_t *session, const char *uid, const char *nickname);
void userlist_add_entry(session_t *session,const char *line);
int userlist_remove(session_t *session, userlist_t *u);
int userlist_replace(session_t *session, userlist_t *u);
userlist_t *userlist_find(session_t *session, const char *uid);
#define userlist_find_n(a, b) userlist_find(session_find(a), b)
char *userlist_dump(session_t *session);
void userlist_free(session_t *session);
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
int check_uid_nick(const char *text);


#endif /* __EKG_USERLIST_H */
