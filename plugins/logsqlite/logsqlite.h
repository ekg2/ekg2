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

#include <sqlite.h>

char *logsqlite_prepare_path();
int logsqlite_msg_handler(void *data, va_list ap);
int logsqlite_status_handler(void *data, va_list ap);
int logsqlite_theme_init();
sqlite * logsqlite_open_db();
void logsqlite_close_db(sqlite * db);
void logsqlite_setvar_default();

int config_logsqlite_last_in_window = 0;
int config_logsqlite_last_open_window = 0;
int config_logsqlite_last_limit = 10;
int config_logsqlite_log = 0;
int config_logsqlite_log_ignored = 0;
int config_logsqlite_log_status = 0;
int config_logsqlite_remind_number = 0;
char * config_logsqlite_path;

#endif
