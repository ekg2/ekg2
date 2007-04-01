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
 * @todo It's too heavy, we really <b>need</b> to move some plugin specified data [like mobile, protocol, authtype] to private. sizeof(userlist_t)==96
 * @todo Move 'mobile' to gg_private_userlist_t, and make this work with 'sms' plugin somehow
 * @bug There are two private fields [u->private and u->priv] one need to be removed.
 */

typedef struct {
	char *uid;		/**< uin in form protocol:id */
	char *nickname;		/**< nickname */
	list_t groups;		/**< list_t with ekg_group<br>
				 * 	Groups to which this user belongs like: work, friends, family..<br>
				 *	It's also used internally by ekg2, for example when user is ignore he has group with name: __ignore */
	
	int status;		/**< current status */
	char *descr;		/**< description of status. */
	list_t resources;	/**< list_t with ekg_resource_t<br>It's used to handle Jabber resources, and also by irc friendlist. */

	uint32_t ip;		/**< ipv4 address of user, use for example inet_ntoa() to get it in format: 111.222.333.444 [:)]<br>
				 *	It's used mainly for DCC communications. */
	uint16_t port;		/**< port of user<br> 
				 *	It's used mainly for DCC communications. */

	time_t last_seen;	/**< Last time when user was available [when u->status was > notavail] */
	
	int protocol;		/**< Protocol version [only used by gg plugin] */

	char *foreign;		/**< For compatilibity with ekg1 userlist. */

	void *priv;		/**< Private data for protocol plugin. */
	
	int xstate;		/**< Extended userlist element state, for example blinking or typing notify */

        uint32_t last_ip;       /**< Lastseen ipv4 address */
        uint16_t last_port;     /**< Lastseen port */

	int last_status;	/**< Lastseen status */
	char *last_descr;	/**< Lastseen description */
	time_t status_time;	/**< From when we have this status, description */
	void *private;          /**< Alternate private data, used by ncurses plugin */
} userlist_t;

enum xstate_t {
	EKG_XSTATE_BLINK	= 1,
	EKG_XSTATE_TYPING	= 2
};

/**
 * userlist_privhandler_funcn_t - here we declare possible options for 'function' arg in USERLIST_PRIVHANDLE
 *
 * All of them, excluding EKG_USERLIST_PRIVHANDLER_FREE, should alloc&init priv if needed
 */
enum userlist_privhandler_funcn_t {
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
							 *	and char*** for value ptr (not duplicated) */
};

/**
 * status_t - user's current status, as prioritized enum
 */

enum status_t {
	EKG_STATUS_NULL		= 0x00, /* special value */
	/* These statuses should be considered as no-delivery */
	EKG_STATUS_ERROR,		/* used in Jabber */
	EKG_STATUS_BLOCKED,		/* used in GG */
	/* These statuses should be considered as 'not sure' */
	EKG_STATUS_UNKNOWN	= 0x10,	/* will be used in Jabber */
	EKG_STATUS_NA		= 0x20,	/* universal */
	/* These should be considered as 'probably available' */
	EKG_STATUS_INVISIBLE,		/* will be used in GG; hard to prioritize... */
	EKG_STATUS_DND,			/* Jabber */
	EKG_STATUS_XA,			/* Jabber */
	EKG_STATUS_AWAY,		/* universal */
	/* These should be considered as 'sure available' */
	EKG_STATUS_AVAIL	= 0x40,	/* universal */
	EKG_STATUS_FFC,			/* Jabber; FREE_FOR_CHAT was too long */
	/* These are special statuses, which can be used only with special functions */
	EKG_STATUS_AUTOAWAY	= 0x80,	/* putting in auto-away */
	EKG_STATUS_AUTOXA,		/* putting in auto-xa */
	EKG_STATUS_AUTOBACK		/* returning to previous status */
};

/* Few words about statuses:
 *
 * All of the enum statuses are proritity-sorted. I mean, if we want to determine, which of the two given statuses is more
 * important, we just do standard arithmetic comparation (e.g. (status1 > status2)). The statuses are also divided into few
 * functional groups.
 *
 * EKG_STATUS_NULL is just declared for fun. It can be used locally (e.g. in functions, where status can be set conditionally,
 * to see if some condition was true), but it can't be passed to core. None of the core functions recognizes it, so it will be
 * probably treated like unknown status. I even don't think anyone would use that long name, instead of putting 0.
 *
 * The next two statuses, blocked and error, represent situations, in which messages sent to user probably won't be delivered.
 * They both aren't currently treated specially by core, but this may change in future. If You want to check, if given status
 * belongs to that group, you should use EKG_STATUS_IS_NODELIVERY.
 *
 * Then, we've got two kinds of N/A. Both of them mean the user may be unavailable at the moment, but the messages will be
 * delivered or queued. EKG_STATUS_UNKNOWN would probably be the lowest prioritized of these statuses, so it is used as a mark
 * for above group, and EKG_STATUS_NA would be the highest one, so it is used as a mark for all N/A statuses. This group
 * (combined with above) is identified by macro EKG_STATUS_IS_NA.
 *
 * Next status, EKG_STATUS_INVISIBLE, is very problematic. It means that user has sent us an N/A status, but some magic says
 * it is available although. It's hard to say, if it's an N/A status, or more 'deep kind of away' (often invisible is used
 * when someone goes AFK for a long time). I don't think it should be used as some kind of mark, and also shouldn't be 'less
 * available' than EKG_STATUS_NA, so it's put after it. But this _can change_.
 *
 * Status described above starts the third group of statuses, aways. These are statuses, which say that user is connected with
 * server, and messages are delivered directly to him/her, but he/she is probably AFK, busy or like that. All those statuses
 * are grouped by macro EKG_STATUS_IS_AWAY.
 *
 * And the last formal group is available-statuses. The first of them, most traditional 'available', is a mark for this
 * and above group. The macro is EKG_STATUS_IS_AVAIL.
 *
 * The real last group is designed for special use only. Currently, there are only statuses for setting and disabling auto-away
 * mode in EKG2. These three can be passed only to session_status_set(), and aren't recognized by everything else.
 */

#define EKG_STATUS_IS_NODELIVERY(x)	(x < EKG_STATUS_UNKNOWN)
#define EKG_STATUS_IS_NA(x)		(x <= EKG_STATUS_NA)
#define EKG_STATUS_IS_AWAY(x)		((x > EKG_STATUS_NA) && (x < EKG_STATUS_AVAIL))
#define EKG_STATUS_IS_AVAIL(x)		(x >= EKG_STATUS_AVAIL)

/** 
 * ekg_resource_t is used to manage userlist_t resources.<br>
 * For example jabber resources, or irc friendlist
 */

typedef struct {
	char *name;		/**< name of resource */
	int status;		/**< status, like u->status 	[status of resource]		*/
	char *descr;		/**< descr, like u->descr	[description of resource]	*/
	int prio;		/**< prio of resource 		[priority of this resource] 	*/
	void *private;		/**< priv, like u->private 	[private data info/struct]	*/
} ekg_resource_t;

/**
 * struct ekg_group is used to manage userlist_t groups.
 */

struct ekg_group {
	char *name;		/**< name of group */
};

enum ignore_t {
	IGNORE_STATUS		= 0x01,
	IGNORE_STATUS_DESCR	= 0x02,
	IGNORE_MSG		= 0x04,
	IGNORE_DCC		= 0x08,
	IGNORE_EVENTS		= 0x10,
	IGNORE_NOTIFY		= 0x20,
	IGNORE_XOSD		= 0x40,
	
	IGNORE_ALL		= 0xFF
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
int valid_plugin_uid(plugin_t *plugin, const char *uid);
const char *format_user(session_t *session, const char *uid);
char *get_uid(session_t *session, const char *text);
char *get_uid_any(session_t *session, const char *text);
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
