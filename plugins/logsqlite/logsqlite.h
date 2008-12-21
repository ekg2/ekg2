/* $Id$ */

/*
 *  (C) Copyright 2005 Leszek Krupiñski <leafnode@wafel.com>
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


#ifndef __LOGSQLITE_H__

#define __LOGSQLITE_H__


#ifdef HAVE_SQLITE3
# include <sqlite3.h>
# define sqlite_t sqlite3
#else
# include <sqlite.h>
# define sqlite_t sqlite
#endif

extern char *logsqlite_prepare_path();
extern QUERY(logsqlite_msg_handler);
extern QUERY(logsqlite_status_handler);
extern int logsqlite_theme_init();
extern sqlite_t * logsqlite_prepare_db(session_t * session, time_t sent, int mode);
extern sqlite_t * logsqlite_open_db(session_t * session, time_t sent, char * path);
extern void logsqlite_close_db(sqlite_t * db);
extern void logsqlite_setvar_default();

extern char *config_logsqlite_path;
extern int config_logsqlite_last_in_window;
extern int config_logsqlite_last_open_window;
extern int config_logsqlite_last_limit_msg;
extern int config_logsqlite_last_limit_status;
extern int config_logsqlite_last_print_on_open;
extern int config_logsqlite_log;
extern int config_logsqlite_log_ignored;
extern int config_logsqlite_log_status;

#endif
