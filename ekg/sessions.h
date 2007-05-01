/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 * 		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

#ifndef __EKG_SESSIONS_H
#define __EKG_SESSIONS_H

#include <time.h>
#include "dynstuff.h"

typedef struct {
	char *key;			/* nazwa parametru */
	char *value;			/* warto¶æ parametru */
} session_param_t;

/**
 * session_t contains all information about session
 */
typedef struct {
	/* public: */
	void *plugin;			/**< protocol plugin owing session */
	char *uid;			/**< user ID */
	char *alias;			/**< short name */
	void *priv;			/**< protocol plugin's private data */
	list_t userlist;		/**< session's userlist */

	/* private: */
	int status;			/**< session's user status */
	char *descr;			/**< session's user description */
	char *password;			/**< session's account password */
	int connected;			/**< whether session is connected */
	int activity;			/**< timestamp of last activity */
	int autoaway;			/**< whether we're in autoaway */
	int scroll_last;
	int scroll_pos;
	int scroll_op;
	time_t last_conn;               /**< timestamp of connecting */

	int global_vars_count;
	char **values;
	list_t local_vars;
	
	/* new auto-away */
	int laststatus;			/**< user's status before going into autoaway */
	char *lastdescr;		/**< user's description before going into autoaway */
} session_t;

#ifndef EKG2_WIN32_NOFUNCTION
extern list_t sessions;

extern session_t *session_current;

session_t *session_find(const char *uid);
session_t *session_find_ptr(session_t *s);

int session_is_var(session_t *s, const char *key);

const char *session_uid_get(session_t *s);

const char *session_alias_get(session_t *s);
int session_alias_set(session_t *s, const char *alias);

int session_status_get(session_t *s);
#define session_status_get_n(a) session_status_get(session_find(a))
int session_status_set(session_t *s, int status);

const char *session_descr_get(session_t *s);
int session_descr_set(session_t *s, const char *descr);

const char *session_password_get(session_t *s);
int session_password_set(session_t *s, const char *password);

void *session_private_get(session_t *s);
int session_private_set(session_t *s, void *priv);

int session_connected_get(session_t *s);
int session_connected_set(session_t *s, int connected);

const char *session_get(session_t *s, const char *key);
int session_int_get(session_t *s, const char *key);
int session_set(session_t *s, const char *key, const char *value);
int session_int_set(session_t *s, const char *key, int value);

const char *session_format(session_t *s);
#define session_format_n(a) session_format(session_find(a))

/* alias or uid - formatted */
const char *session_name(session_t *s);

/* alias or uid - not formatted */
#define session_alias_uid(a) (a->alias) ? a->alias : a->uid
#define session_alias_uid_n(a) session_alias_uid(session_find(a))

int session_check(session_t *s, int need_private, const char *protocol);

int session_unidle(session_t *s);

int session_compare(void *data1, void *data2);
session_t *session_add(const char *uid);
int session_remove(const char *uid);

int session_read(const char *filename);
int session_write();

void sessions_free();

void session_help(session_t *s, const char *name);
#endif

#endif /* __EKG_SESSIONS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
