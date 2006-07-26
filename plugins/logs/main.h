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

#include "ekg2-config.h"
#include <stdio.h>
#include <ekg/sessions.h>
#include <ekg/plugins.h>

struct {
	int   logformat; 
			/* 19:55:24 <@zdzichuBG> wtedy trzeba by jescze jakis callback na zmiane zmiennej logs_format 
			 * callback zmiennych sesyjnych w ekg2 niet. jest cos takiego.
			 */
	char *path;	/* path don't free it ! .... */
	FILE *file; 	/* file don't close it! it will be closed at unloading plugin. */
} typedef log_window_t;

struct {
	char *session;	/* session name */
	char *uid;	/* uid of user */
	time_t t;	/* time when we create (lw->path || just lw) */
	log_window_t *lw;
} typedef logs_log_t;

struct {
	char *chname;	/* channel name, (null if priv) */
	char *uid;	/* user name kto do nas pisal */
	char *msg;	/* msg */
	time_t t;	/* czas o ktorej dostalismy wiadomosc */
} typedef log_session_away_t;

struct {
	char	*sname;		/* session name */
	list_t	messages; 	/* lista z log_session_away_t */
} typedef log_away_t;

char *logs_prepare_path(session_t *session, const char *logs_path, const char *uid, time_t sent);
const char *prepare_timestamp_format(const char *format, time_t t);

logs_log_t *logs_log_find(const char *session, const char *uid, int create);
logs_log_t *logs_log_new(logs_log_t *l, const char *session, const char *uid);

FILE *logs_open_file(char *path, int ff);
QUERY(logs_handler);
QUERY(logs_handler_newwin);
QUERY(logs_status_handler);
QUERY(logs_handler_irc);
QUERY(logs_handler_raw);

void logs_simple(FILE *file, const char *session, const char *uid, const char *text, time_t sent, int class, uint32_t ip, uint16_t port, const char *status);
void logs_xml	(FILE *file, const char *session, const char *uid, const char *text, time_t sent, int class);
void logs_irssi	(FILE *file, const char *session, const char *uid, const char *text, time_t sent, int type, const char *ip);
void logs_gaim();

list_t log_logs = NULL; 
list_t log_awaylog = NULL;

int config_away_log = 0;
int config_logs_log;
int config_logs_log_raw;
int config_logs_log_ignored;
int config_logs_log_status;
int config_logs_remind_number = 0;
int config_logs_max_files = 7;
char *config_logs_path;
char *config_logs_timestamp;

#endif
