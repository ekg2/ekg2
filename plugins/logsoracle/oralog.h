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

#ifndef __oralog_h__
#define __oralog_h__

#include <time.h>

/* VARCHAR2 length is maximum 4000 chars - if you want more then you probably
   need LOBs (Large OBjects) or something */
#define ORACLE_MAX_STRING_LEN	4000

int oralog_db_connect(char *db_login, char *db_password, int quiet);
int oralog_db_disconnect();
int oralog_is_connected();

int oralog_db_new_status(char *session, char *uid, char *status, char *descr, time_t time, int quiet);
int oralog_db_new_msg(char *session, char *sedner_uid, char **rcpts, char *content, time_t recv_time, int quiet);

#define DB_CONNECT_NEW_CONNECTION	0
#define DB_CONNECT_ALREADY_CONNECTED	1
#define DB_CONNECT_ERROR		2

#endif
