/* $Id: sessions.h 4589 2008-09-01 18:44:20Z peres $ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *		  2004 Piotr Kupisiewicz <deli@rzepaknet.us>
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

typedef enum {
	EKG_STATUS_NULL		= 0x00, /* special value */

	/* These statuses should be considered as no-delivery */
	EKG_STATUS_ERROR,		/* used in Jabber */
	EKG_STATUS_BLOCKED,		/* used in GG */

	/* These statuses should be considered as 'not sure' */
	EKG_STATUS_UNKNOWN,			/* used in Jabber */
	EKG_STATUS_NA,				/* universal */

	/* These should be considered as 'probably available' */
	EKG_STATUS_INVISIBLE,		/* GG; hard to prioritize... */
	EKG_STATUS_DND,			/* Jabber */
	EKG_STATUS_GONE,		/* ICQ */
	EKG_STATUS_XA,			/* Jabber */
	EKG_STATUS_AWAY,		/* universal */

	/* These should be considered as 'sure available' */
	EKG_STATUS_AVAIL,		/* universal */
	EKG_STATUS_FFC			/* Jabber */
} status_t;

typedef struct session_param {
	struct session_param *next;

	char *key;			/* nazwa parametru */
	char *value;			/* warto¶æ parametru */
} session_param_t;

/**
 * session_t contains all information about session
 */
typedef struct ekg_session {
	struct ekg_session	*next;

/* public: */
	void		*plugin;		/* ekg2-remote: OK */
	char		*uid;			/* ekg2-remote: OK */
	char		*alias;			/* ekg2-remote: OK */
	void		*__priv;		/* ekg2-remote: NULL, OK */
	struct userlist	*userlist;

/* private: */
	status_t	status;	
	char		*descr;	
	char		*__password;		/* ekg2-remote: NULL, ok */

	unsigned int	connected	: 1;	/* ekg2-remote: OK */
	unsigned int	connecting	: 2;
	unsigned int	__autoaway	: 1;	/* ekg2-remote: 0, OK */

	time_t		__activity;		/* ekg2-remote: 0, OK */
	time_t		__last_conn;		/* ekg2-remote: 0, OK */

	int		__global_vars_count;	/* ekg2-remote: 0, OK */
	char		**__values;		/* ekg2-remote: NULL, OK */
	session_param_t	*__local_vars;		/* ekg2-remote: NULL, OK */
	
	status_t	__last_status;		/* ekg2-remote: EKG_STATUS_NULL, OK */
	char		*__last_descr;		/* ekg2-remote: NULL, OK */
	int		__flock_fd;		/* ekg2-remote: (0), OK */	/* XXX, could be -1 */
} session_t;

extern session_t *sessions;

extern session_t *session_current;

session_t *session_find(const char *uid);

const char *session_uid_get(session_t *s);

session_t *remote_session_add(const char *uid, const char *plugin);
int remote_session_remove(const char *uid);

void sessions_free();

#endif /* __EKG_SESSIONS_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
