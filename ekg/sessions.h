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

#include "dynstuff.h"

list_t sessions;

typedef struct {
	char *key;			/* nazwa parametru */
	char *value;			/* warto¶æ parametru */
	int secret;			/* czy warto¶æ ma byæ ukryta? */
} session_param_t;

typedef struct {
	/* public: */
	char *uid;			/* id u¿ytkownika */
	char *alias;			/* alias sesji */
	void *priv;			/* dla plugina obs³uguj±cego sesjê */
	list_t userlist;		/* userlista */

	/* private: */
	char *status;			/* stan sesji */
	char *descr;			/* opis stanu sesji */
	char *password;
	int connected;			/* czy sesja jest po³±czona? */
	int activity;			/* kiedy ostatnio co¶ siê dzia³o? */
	int autoaway;			/* jeste¶my w autoawayu? */
	time_t last_conn;               /* kiedy siê po³±czyli¶my */
	session_param_t **params;	/* parametry sesji */
} session_t;

session_t *session_current;

session_t *session_find(const char *uid);

const char *session_uid_get(session_t *s);
#define session_uid_get_n(a) session_uid_get(session_find(a))

const char *session_alias_get(session_t *s);
#define session_alias_get_n(a) session_alias_get(session_find(a))
int session_alias_set(session_t *s, const char *alias);
#define session_alias_set_n(a,b) session_alias_set(session_find(a),b)

const char *session_status_get(session_t *s);
#define session_status_get_n(a) session_status_get(session_find(a))
int session_status_set(session_t *s, const char *status);
#define session_status_set_n(a,b) session_status_set(session_find(a),b)

const char *session_descr_get(session_t *s);
#define session_descr_get_n(a) session_descr_get(session_find(a))
int session_descr_set(session_t *s, const char *descr);
#define session_descr_set_n(a,b) session_descr_set(session_find(a),b)

const char *session_password_get(session_t *s);
#define session_password_get_n(a) session_descr_get(session_find(a))
int session_password_set(session_t *s, const char *password);
#define session_password_set_n(a,b) session_descr_set(session_find(a),b)

void *session_private_get(session_t *s);
#define session_private_get_n(a) session_private_get(session_find(a))
int session_private_set(session_t *s, void *priv);
#define session_private_set_n(a,b) session_private_set(session_find(a),b)

int session_connected_get(session_t *s);
#define session_connected_get_n(a) session_connected_get(session_find(a))
int session_connected_set(session_t *s, int connected);
#define session_connected_set_n(a,b) session_connected_set(session_find(a),b)

const char *session_get(session_t *s, const char *key);
#define session_get_n(a,b) session_get(session_find(a),b)
int session_int_get(session_t *s, const char *key);
#define session_int_get_n(a,b) session_int_get(session_find(a),b)
int session_set(session_t *s, const char *key, const char *value);
#define session_set_n(a,b,c) session_set(session_find(a),b,c)
int session_int_set(session_t *s, const char *key, int value);
#define session_int_set_n(a,b,c) session_int_set(session_find(a),b,c)

const char *session_format(session_t *s);
#define session_format_n(a) session_format(session_find(a))

int session_check(session_t *s, int need_private, const char *protocol);
#define session_check_n(a,b,c) session_check(session_find(a),b,c)

int session_unidle(session_t *s);
#define session_unidle_n(a) session_unidle(session_find(a))

int session_compare(void *data1, void *data2);
session_t *session_add(const char *uid);
int session_remove(const char *uid);
int session_remove_s(session_t *s);

int session_read();
int session_write();

void sessions_free();
#endif /* __EKG_SESSIONS_H */
