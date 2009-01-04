/* $Id: userlist.h 4412 2008-08-17 12:28:15Z peres $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
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

#include <time.h>

#include "dynstuff.h"
#include "plugins.h"
#include "sessions.h"
#include "windows.h"

typedef struct userlist {
	struct userlist	*next;

	char		*uid;		/**< uin in form protocol:id */
	char		*nickname;	/**< nickname */
	struct ekg_group *groups;	/**< list_t with ekg_group<br>
					 *	Groups to which this user belongs like: work, friends, family..<br>
					 *	It's also used internally by ekg2, for example when user is ignore he has group with name: __ignore */
	
	status_t	status;		/**< current status */
	char		*descr;		/* */

	void *__resources;		/* ekg2-remote: OK, NULL */	/* XXX, to w ogole by sie przydalo dorobic w ui-pluginach */
	time_t		__last_seen;	/* ekg2-remote: OK, 0 */
	char		*__foreign;	/* ekg2-remote: OK, NULL */

	void		*priv;		/**< Private data for protocol plugin. */
	
	unsigned int	blink	: 1;	/**< Blink userlist entry (message) */
	unsigned int	typing	: 1;	/**< User is composing */

	status_t	last_status;	/**< Lastseen status */
	char		*last_descr;	/**< Lastseen description */
	time_t		status_time;	/**< From when we have this status, description */
	void		*private;	   /**< Alternate private data, used by ncurses plugin */
	private_data_t	*priv_list;	/* New user private data */
} userlist_t;

typedef enum {
	EKG_XSTATE_BLINK	= 1,
	EKG_XSTATE_TYPING	= 2
} xstate_t;

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

extern struct ignore_label ignore_labels[];

userlist_t *remote_userlist_add_entry(userlist_t **userlist, char **__entry, int count);
int userlist_remove(session_t *session, userlist_t *u);
userlist_t *userlist_find(session_t *session, const char *uid);
#define userlist_find_n(a, b) userlist_find(session_find(a), b)
void userlist_free(session_t *session);
void userlists_destroy(userlist_t **userlist);

int ignored_check(session_t *session, const char *uid);

int ekg_group_member(userlist_t *u, const char *group);

char *get_uid(session_t *session, const char *text);

#define user_private_item_get_int(user, name) \
	private_item_get_int(&(user)->priv_list, name)
#define user_private_item_set(user, name, value) \
	private_item_set(&(user)->priv_list, name, value)

#endif /* __EKG_USERLIST_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
