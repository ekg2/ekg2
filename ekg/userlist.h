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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "dynstuff.h"
#include "objects.h"
#include "sessions.h"

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
	
	uint32_t ip;		/* adres ip */
	uint16_t port;		/* port */

	time_t last_seen;	/* je¶li niedostêpny/ukryty to od kiedy */
	
	int protocol;		/* wersja protoko³u */

	char *foreign;		/* dla kompatybilno¶ci */

	void *priv;		/* dane pluginu obs³uguj±cego usera */
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
int userlist_remove(session_t *session, userlist_t *u);
int userlist_replace(session_t *session, userlist_t *u);
userlist_t *userlist_find(session_t *session, const char *uid);
char *userlist_dump(session_t *session);
void userlist_free(session_t *session);
int userlist_set(session_t *session, const char *contacts);

int ignored_add(session_t *session, const char *uid, int level);
int ignored_remove(session_t *session, const char *uid);
int ignored_check(session_t *session, const char *uid);
int ignore_flags(const char *str);
const char *ignore_format(int level);

int group_add(userlist_t *u, const char *group);
int group_remove(userlist_t *u, const char *group);
int group_member(userlist_t *u, const char *group);
char *group_to_string(list_t l, int meta, int sep);
list_t group_init(const char *groups);

int valid_nick(const char *nick);
int valid_uid(const char *uid);
int same_protocol(char **uids);
const char *format_user(session_t *session, const char *uid);
const char *get_uid(session_t *session, const char *text);


#endif /* __EKG_USERLIST_H */
