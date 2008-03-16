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

/**
 * status_t - user's current status, as prioritized enum
 */

typedef enum {
	EKG_STATUS_NULL		= 0x00, /* special value */

	/* These statuses should be considered as no-delivery */
	EKG_STATUS_ERROR,		/* used in Jabber */
	EKG_STATUS_BLOCKED,		/* used in GG */

	/* These statuses should be considered as 'not sure' */
	EKG_STATUS_UNKNOWN	= 0x10,	/* used in Jabber */
	EKG_STATUS_NA		= 0x20,	/* universal */

	/* These should be considered as 'probably available' */
	EKG_STATUS_INVISIBLE,		/* GG; hard to prioritize... */
	EKG_STATUS_DND,			/* Jabber */
	EKG_STATUS_XA,			/* Jabber */
	EKG_STATUS_AWAY,		/* universal */

	/* These should be considered as 'sure available' */
	EKG_STATUS_AVAIL	= 0x40,	/* universal */
	EKG_STATUS_FFC,			/* Jabber */

	/* These are special statuses, which are to be used only with dedicated functions */
	EKG_STATUS_AUTOAWAY	= 0x80,	/* putting in auto-away */
	EKG_STATUS_AUTOXA,		/* putting in auto-xa */
	EKG_STATUS_AUTOBACK		/* returning to previous status */
} status_t;

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

typedef struct {
	char *key;			/* nazwa parametru */
	char *value;			/* warto¶æ parametru */
} session_param_t;

/**
 * session_t contains all information about session
 */
typedef struct session {
	struct session	*next;

/* public: */
	void		*plugin;	/**< protocol plugin owing session */
	char		*uid;		/**< user ID */
	char		*alias;		/**< short name */
	void		*priv;		/**< protocol plugin's private data */
	list_t		userlist;	/**< session's userlist */

/* private: */
	status_t	status;		/**< session's user status */
	char		*descr;		/**< session's user description */
	char		*password;	/**< session's account password */
	int		connected;	/**< whether session is connected */
	time_t		activity;	/**< timestamp of last activity */
	int		autoaway;	/**< whether we're in autoaway */
	int		scroll_last;
	int		scroll_pos;
	int		scroll_op;
	time_t		last_conn;	/**< timestamp of connecting */

	int		global_vars_count;
	char		**values;
	list_t		local_vars;
	
/* new auto-away */
	status_t	last_status;	/**< user's status before going into autoaway */
	char		*last_descr;	/**< user's description before going into autoaway */

#ifdef HAVE_FLOCK /* XXX: -D for docs? */
	int		lock_fd;	/**< fd used for session locking */
#endif
} session_t;

#ifndef EKG2_WIN32_NOFUNCTION
extern session_t *sessions;

extern session_t *session_current;

session_t *session_find(const char *uid);
session_t *session_find_ptr(session_t *s);

int session_is_var(session_t *s, const char *key);

const char *session_uid_get(session_t *s);

const char *session_alias_get(session_t *s);
int session_alias_set(session_t *s, const char *alias);

int session_status_get(session_t *s);
#define session_status_get_n(a) session_status_get(session_find(a))
int session_status_set(session_t *s, status_t status);

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
