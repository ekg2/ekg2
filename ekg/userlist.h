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

/** 
 * userlist_t is used to manage all info about user.<br>
 * It's used not only to manage contacts in roster, but also to manage people in chat or conference
 *
 * @bug There are two private fields [u->private and u->priv] one need to be removed.
 */

typedef struct userlist {
	struct userlist	*next;

	char		*uid;		/**< uin in form protocol:id */
	char		*nickname;	/**< nickname */
	struct ekg_group *groups;	/**< list_t with ekg_group<br>
					 * 	Groups to which this user belongs like: work, friends, family..<br>
					 *	It's also used internally by ekg2, for example when user is ignore he has group with name: __ignore */
	
	status_t	status;		/**< current status */
	char		*descr;		/**< description of status. */
	struct ekg_resource *resources;	/**< list_t with ekg_resource_t<br>It's used to handle Jabber resources, and also by irc friendlist. */

	time_t		last_seen;	/**< Last time when user was available [when u->status was > notavail] */
	
	char		*foreign;	/**< For compatilibity with ekg1 userlist. */

	void		*priv;		/**< Private data for protocol plugin. */
	
	unsigned int	blink	: 1;	/**< Blink userlist entry (message) */
	unsigned int	typing	: 1;	/**< User is composing */

	status_t	last_status;	/**< Lastseen status */
	char		*last_descr;	/**< Lastseen description */
	time_t		status_time;	/**< From when we have this status, description */
	void		*private;          /**< Alternate private data, used by ncurses plugin */
	private_data_t	*new_private;	/* New user private data
					 * 2do:
					 *	1. migrate data from private to new_private
					 *	2. remove private
					 *	3. rename new_private to private
					 */
} userlist_t;

typedef enum {
	EKG_XSTATE_BLINK	= 1,
	EKG_XSTATE_TYPING	= 2
} xstate_t;

/**
 * userlist_privhandler_func_t - here we declare possible options for 'function' arg in USERLIST_PRIVHANDLE
 *
 * All of them, excluding EKG_USERLIST_PRIVHANDLER_FREE, should alloc&init priv if needed
 */
typedef enum {
	EKG_USERLIST_PRIVHANDLER_FREE		= 0,	/**< Free private data (called when freeing userlist_t) */
	EKG_USERLIST_PRIVHANDLER_GET,			/**< Return private data ptr, arg is void** for ptr */
	EKG_USERLIST_PRIVHANDLER_READING,		/**< Called when reading userlist file,<br>
							 *	1st arg is char*** with data array,<br>
							 *	2nd arg is int* with array element count
							 *		you can assume it's always at least 7<br>
							 *	Please bear in mind that this query is called
							 *		at the very beginning of userlist_add_entry() */
	EKG_USERLIST_PRIVHANDLER_WRITING,		/**< Called when writing userlist file, arg is char*** with data array */

	EKG_USERLIST_PRIVHANDLER_GETVAR_BYNAME	= 0x80,	/**< Get private 'variable' by name, args are char** with var name
							 *	and char** for value ptr (not duplicated) */
	EKG_USERLIST_PRIVHANDLER_SETVAR_BYNAME	= 0xC0,	/**< Set private 'variable' by name, args care char** with var name
							 *	and char** with value (will be duplicated) */
} userlist_privhandler_func_t;

/** 
 * ekg_resource_t is used to manage userlist_t resources.<br>
 * For example jabber resources, or irc friendlist
 */

typedef struct ekg_resource {
	struct ekg_resource *next;

	char		*name;		/**< name of resource */
	status_t	status;		/**< status, like u->status 	[status of resource]		*/
	char		*descr;		/**< descr, like u->descr	[description of resource]	*/
	int		prio;		/**< prio of resource 		[priority of this resource] 	*/
	void		*private;	/**< priv, like u->private 	[private data info/struct]	*/
} ekg_resource_t;

/**
 * struct ekg_group is used to manage userlist_t groups.
 */

struct ekg_group {
	struct ekg_group *next;

	char *name;		/**< name of group */
};

typedef enum {
	IGNORE_STATUS		= 0x01,
	IGNORE_STATUS_DESCR	= 0x02,
	IGNORE_MSG		= 0x04,
	IGNORE_DCC		= 0x08,
	IGNORE_EVENTS		= 0x10,
	IGNORE_NOTIFY		= 0x20,
	IGNORE_XOSD		= 0x40,
	
	IGNORE_ALL		= 0xFF
} ignore_t;

struct ignore_label {
	ignore_t	level;
	char		*name;
};

#define	IGNORE_LABELS_MAX 8
extern struct ignore_label ignore_labels[IGNORE_LABELS_MAX];

#ifndef EKG2_WIN32_NOFUNCTION

int userlist_read(session_t* session);
int userlist_write(session_t *session);
void userlist_write_crash();
void userlist_clear_status(session_t *session, const char *uid);
userlist_t *userlist_add(session_t *session, const char *uid, const char *nickname);
userlist_t *userlist_add_u(userlist_t **userlist, const char *uid, const char *nickname);
void userlist_add_entry(session_t *session,const char *line);
int userlist_remove(session_t *session, userlist_t *u);
int userlist_remove_u(userlist_t **userlist, userlist_t *u);
int userlist_replace(session_t *session, userlist_t *u);
userlist_t *userlist_find(session_t *session, const char *uid);
userlist_t *userlist_find_u(userlist_t **userlist, const char *uid);
#define userlist_find_n(a, b) userlist_find(session_find(a), b)
void userlist_free(session_t *session);
void userlists_destroy(userlist_t **userlist);

void *userlist_private_get(plugin_t *plugin, userlist_t *u);
int userlist_private_item_get_safe(userlist_t *u, const char *item_name, char **result);
const char *userlist_private_item_get(userlist_t *u, const char *item_name);

/* u->resource */
ekg_resource_t *userlist_resource_add(userlist_t *u, const char *name, int prio);
ekg_resource_t *userlist_resource_find(userlist_t *u, const char *name);
void userlist_resource_remove(userlist_t *u, ekg_resource_t *r);

int ignored_add(session_t *session, const char *uid, ignore_t level);
int ignored_remove(session_t *session, const char *uid);
int ignored_check(session_t *session, const char *uid);
int ignore_flags(const char *str);
const char *ignore_format(int level);

int ekg_group_add(userlist_t *u, const char *group);
int ekg_group_remove(userlist_t *u, const char *group);
int ekg_group_member(userlist_t *u, const char *group);
char *group_to_string(struct ekg_group *l, int meta, int sep);
struct ekg_group *group_init(const char *groups);

int valid_nick(const char *nick);
int valid_plugin_uid(plugin_t *plugin, const char *uid);
const char *format_user(session_t *session, const char *uid);
char *get_uid(session_t *session, const char *text);
char *get_uid_any(session_t *session, const char *text);
char *get_nickname(session_t *session, const char *text);

#endif

#define user_private_get_safe(user, name, result) \
	private_item_get_safe(&(user)->new_private, name, result)
#define user_private_get(user, name) \
	private_item_get(&(user)->new_private, name)
#define user_private_get_int_safe(user, name, result) \
	private_item_get_int_safe(&(user)->new_private), name, int *result)
#define user_private_get_int(user, name) \
	private_item_get_int(&(user)->new_private, name)
#define user_private_set(user, name, value) \
	private_item_set(&(user)->new_private, name, value)
#define user_private_set_int(user, name, value) \
	private_item_set_int(&(user)->new_private, name, value)

#endif /* __EKG_USERLIST_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
