/* $Id$ */

/*
 *  (C) Copyright 2003-2004 Leszek Krupiñski <leafnode@wafel.com>
 *                     2005 Adam Mikuta <adamm@ekg2.org>
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
void logs_status_handler(void *data, va_list ap);
void logs_simple(char *path, char *session, char *uid, char *text, time_t sent, int class, int seq, uint32_t ip, uint16_t port, char *status, char *descr);

void logs_xml();
void logs_gaim();

int config_logs_log;
int config_logs_log_ignored;
int config_logs_log_status;
int config_logs_remind_number = 0;
char * config_logs_path;
char * config_logs_timestamp;

list_t logs_reminded; /* lista z przypomnianymi wiadomosciami - nie logowac */

#endif
