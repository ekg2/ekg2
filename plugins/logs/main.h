/* $Id$ */

/*
 *  (C) Copyright 2003-2004 Leszek Krupiñski <leafnode@wafel.com>
 *		       2005 Adam Mikuta <adamm@ekg2.org>
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
#include <ekg/protocol.h>

struct {
	int   logformat; 
			/* 19:55:24 <@zdzichuBG> wtedy trzeba by jescze jakis callback na zmiane zmiennej logs_format 
			 * callback zmiennych sesyjnych w ekg2 niet. jest cos takiego.
			 */
	char *path;	/* path don't free it ! .... */
	FILE *file;	/* file don't close it! it will be closed at unloading plugin. */
} typedef log_window_t;

struct {
	char *session;	/* session name */
	char *uid;	/* uid of user */
	time_t t;	/* time when we create (lw->path || just lw) */
	log_window_t *lw;
} typedef logs_log_t;

/* log ff types... */
typedef enum {
	LOG_FORMAT_NONE = 0,
	LOG_FORMAT_SIMPLE,
	LOG_FORMAT_XML,
	LOG_FORMAT_IRSSI,
	LOG_FORMAT_RAW, 
} log_format_t;

	/* irssi style info messages */
#define IRSSI_LOG_EKG2_OPENED	"--- Log opened %a %b %d %H:%M:%S %Y"	/* defaultowy log_open_string irssi , jak cos to dodac zmienna... */
#define IRSSI_LOG_EKG2_CLOSED	"--- Log closed %a %b %d %H:%M:%S %Y"	/* defaultowy log_close_string irssi, jak cos to dodac zmienna... */
#define IRSSI_LOG_DAY_CHANGED	"--- Day changed %a %b %d %Y"		/* defaultowy log_day_changed irssi , jak cos to dodac zmienna... */

static char *logs_prepare_path(session_t *session, const char *logs_path, const char *uid, time_t sent);
static const char *prepare_timestamp_format(const char *format, time_t t);

static logs_log_t *logs_log_find(const char *session, const char *uid, int create);
static logs_log_t *logs_log_new(logs_log_t *l, const char *session, const char *uid);

static FILE *logs_open_file(char *path, int ff);

static void logs_simple(FILE *file, const char *session, const char *uid, const char *text, time_t sent, msgclass_t class, const char *status);
static void logs_xml	(FILE *file, const char *session, const char *uid, const char *text, time_t sent, msgclass_t class);
static void logs_irssi(FILE *file, const char *session, const char *uid, const char *text, time_t sent, msgclass_t class);
#if 0 /* never started? */
static void logs_gaim();
#endif

static list_t log_logs = NULL; 

static int config_logs_log;
static int config_logs_log_raw;
static int config_logs_log_ignored;
static int config_logs_log_status;
static int config_logs_remind_number = 0;
static int config_logs_max_files = 7;
static char *config_logs_path;
static char *config_logs_timestamp;

#endif
