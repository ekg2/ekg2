/* $Id$ */

/*
 *  (C) Copyright 2003-2004 Leszek Krupiñski <leafnode@wafel.com>
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


#ifndef __logs_h__

#define __logs_h__

char *logs_prepare_path(session_t *session, char *uid, char **rcpts, char *text, time_t sent, int class);
FILE* logs_open_file(char *path, char *ext, int makedir);
char * prepare_timestamp(time_t ts);
void logs_handler(void *data, va_list ap);
void logs_handler_newwin(void *data, va_list ap);
void logs_simple();
void logs_xml();
void logs_gaim();

int logs_log;
int logs_log_ignored;
int logs_log_status;
char * logs_path;
char * logs_timestamp;

#endif
