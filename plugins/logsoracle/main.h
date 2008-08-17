/* $Id$ */

/*
 *  (C) Copyright 2006 Szymon Bilinski <ecimon(at)babel.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __logsoracle_h__
#define __logsoracle_h__

#define	LOGSORACLE_PLUGIN_VERSION	"0.1"

/*
 * Module Configuration
 * [These variables are visible via the 'set' command int EKG2] 
 */
struct logsoracle_conf_struct {
    int auto_connect;		/* VAR_BOOL */
    int	logging_active;		/* VAR_BOOL */
    int log_messages;		/* VAR_BOOL : log sent/incoming messages */
    int	log_status;		/* VAR_BOOL : log status changes */
    char *db_login;		/* VAR_STR */
    char *db_password;		/* VAR_STR */
};

/*
 * Statistics
 */
struct logsoracle_stat_struct {
    int session_message_insert;	/* How many messages were added during this logsoracle session (since connect) */
    int session_status_insert;	/* Same thing for status changes */
};

/*
 * Declare functions that will process incoming events
 */
QUERY(logsoracle_handler_setvarsdef);
QUERY(logsoracle_handler_postinit);
QUERY(logsoracle_handler_newwin);
QUERY(logsoracle_handler_killwin);
QUERY(logsoracle_handler_sestatus);
QUERY(logsoracle_handler_prstatus);
QUERY(logsoracle_handler_prmsg);

/*
 * Statistics
 */
void logsoracle_stat_clear();
int  logsoracle_stat_get_message();
int  logsoracle_stat_get_status();

#endif

