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

#include "ekg2-config.h"

#include <string.h> /* memset() */
#include <stdint.h> /* uint32_t */
#include <time.h>   /* time_t */

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/vars.h>
#include <ekg/protocol.h>
#include <ekg/themes.h>
#include <ekg/xmalloc.h>

#include "main.h"
#include "oralog.h"
#include "commands.h"

/* If not provided then these values will be put into the db */
#define EMPTY_STATUS		""
#define EMPTY_DESCR		""


/* Configuration */
struct logsoracle_conf_struct logsoracle_config;

/* Statistics */
struct logsoracle_stat_struct logsoracle_stat;

/* Private declarations */
int logsoracle_plugin_init(int prio);
static int logsoracle_plugin_destroy();
static int logsoracle_theme_init();
static void logsoracle_stat_inc_message();
static void logsoracle_stat_inc_status();


PLUGIN_DEFINE(logsoracle, PLUGIN_LOG, logsoracle_theme_init);


/*
 * Plugin constructor
 *  returns 0 on success
 *	   -1 on failure
 */
int logsoracle_plugin_init(int prio)
{
	PLUGIN_CHECK_VER("logsoracle");

	plugin_register(&logsoracle_plugin, prio);

	/* connect events with handlers */
	query_connect(&logsoracle_plugin, ("set-vars-default"), logsoracle_handler_setvarsdef, NULL);
	query_connect(&logsoracle_plugin, ("config-postinit"), logsoracle_handler_postinit, NULL);
	query_connect(&logsoracle_plugin, ("session-status"), logsoracle_handler_sestatus, NULL);	
	query_connect(&logsoracle_plugin, ("protocol-status"), logsoracle_handler_prstatus, NULL);
	query_connect(&logsoracle_plugin, ("protocol-message-post"), logsoracle_handler_prmsg, NULL);
		
	/* register variables */
	variable_add(&logsoracle_plugin, ("auto_connect"), VAR_BOOL, 1, &logsoracle_config.auto_connect, NULL, NULL, NULL);
	variable_add(&logsoracle_plugin, ("logging_active"), VAR_BOOL, 1, &logsoracle_config.logging_active, NULL, NULL, NULL);
	variable_add(&logsoracle_plugin, ("log_messages"), VAR_BOOL, 1, &logsoracle_config.log_messages, NULL, NULL, NULL);
	variable_add(&logsoracle_plugin, ("log_status"), VAR_BOOL, 1, &logsoracle_config.log_status, NULL, NULL, NULL);
	variable_add(&logsoracle_plugin, ("db_login"), VAR_STR, 1, &logsoracle_config.db_login, NULL, NULL, NULL);
	variable_add(&logsoracle_plugin, ("db_password"), VAR_STR, 1, &logsoracle_config.db_password, NULL, NULL, NULL);

	/* register commands */
	command_add(&logsoracle_plugin, ("logsoracle:connect"), NULL, logsoracle_cmd_db_connect, 0, NULL);	
	command_add(&logsoracle_plugin, ("logsoracle:disconnect"), NULL, logsoracle_cmd_db_disconnect, 0, NULL);
	command_add(&logsoracle_plugin, ("logsoracle:status"), NULL, logsoracle_cmd_status, 0, NULL);
		
	return 0;
}

/*
 * Plugin destructor 
 */
static int logsoracle_plugin_destroy()
{
	/* debug("[logsoracle] disconnecting from database\n"); */
	oralog_db_disconnect();

	plugin_unregister(&logsoracle_plugin);
	
	return 0;
}

/*
 * Plugin graphical theme constructor
 */
static int logsoracle_theme_init()
{
	debug("[logsoracle] inititalizing theme\n");

	format_add("logsoracle_status", _("%> logsoracle status\n"), 1);
	format_add("logsoracle_status_con", _("%>  connected: %T%1%n\n"), 1);
	format_add("logsoracle_status_sta", _("%>  status changes: %T%1%n\n"), 1);
	format_add("logsoracle_status_msg", _("%>  messages: %T%1%n\n"), 1);
	format_add("logsoracle_error", _("%! oracle error message:\n\n%W%1%n\n\n%! end of message\n"), 1);
	format_add("logsoracle_connected", _("%> connected to Oracle\n"), 1);
	format_add("logsoracle_disconnected", _("%> disconnected from Oracle\n"), 1);
	format_add("logsoracle_disconn_not_needed", _("%> not connected to database\n"), 1);
	format_add("logsoracle_already_connected", _("%> already connected to Oracle. use 'disconnect' first\n"), 1); 

	return 0;
}


/* 
 * This function is called whenever default variable values are desired
 */
QUERY(logsoracle_handler_setvarsdef)
{
	memset((void *)&logsoracle_config, 0, sizeof(logsoracle_config));

	logsoracle_config.auto_connect	 = 0;		    
	logsoracle_config.logging_active = 1;
	logsoracle_config.log_messages	 = 1;
	logsoracle_config.log_status	 = 1;					    
	logsoracle_config.db_login	 = NULL;
	logsoracle_config.db_password	 = NULL;
    
	return 0;
}

/*
 * Post-initialization configuration (plugin is loaded)
 */
QUERY(logsoracle_handler_postinit)
{
	debug("[logsoracle] handler postinit called\n");

	if(logsoracle_config.auto_connect) {
		debug("[logsoracle] Autoconnecting\n");
		command_exec(NULL, session_current, "/logsoracle:connect", 0);
		debug("[logsoracle] Autoconnectiong done\n");
	}
	
	return 0;
}


/*
 * Session status change
 * Called after commands: 'away', 'back', ...
 */
QUERY(logsoracle_handler_sestatus)
{
	char *session_uid = *(va_arg(ap, char **));	/* eg. jid:romeo@verona.net */
	int status	  = *(va_arg(ap, int *));	/* eg. (away|back|...) */

	session_t *session = (session_t *)session_find(session_uid);
	

	if(!logsoracle_config.log_status)
		return 0;


	if(session_uid == NULL) {
		debug("[logsoracle] session status : RECIEVED NULL in session_uid !\n");
		return 0;
	}

	if(session == NULL) {
		debug("[logsoracle] session status : couldn't find specified session (NULL)!");
		return 0;
	}	
	
	/*	
	debug("[logsoracle] session status (session %s :: status %s :: descr '%s')\n", session_uid, ekg_status_string(status, 0), session->descr);
	*/
    
	if(!oralog_db_new_status(session_uid, session_uid, (status) ? ekg_status_string(status, 2) : EMPTY_STATUS, (session->descr) ? session->descr : EMPTY_DESCR, time(NULL), 0))
		logsoracle_stat_inc_status();

    
	return 0;
}


/*
 * Protocol status change
 * Called when someone from the userlist changes his/her status.
 */
QUERY(logsoracle_handler_prstatus)
{
	char *session_uid  = *(va_arg(ap, char**));	/* eg. jid:romeo@verona.net */
	char *uid	   = *(va_arg(ap, char**));	/* eg. jid:julia@verona.net */
	int status_n	   = *(va_arg(ap, int*));	/* eg. "away" (as enum) */
	char *descr	   = *(va_arg(ap, char**));	/* eg. "i'm not here" */
	char *status	   = 0;				/* status as string; written into db */
	
	int  descr_alloc  = 0;

	if(!logsoracle_config.log_status)
	    return 0;
		

	if(session_uid == NULL) {
		debug("[logsoracle] protocol status : RECIEVED NULL in session_uid !\n");
		return 0;
	}

	if(uid == NULL) {
		debug("[logsoracle] protocol status : RECIEVED NULL in uid!");
		return 0;
	}	
	
	if(status_n) {
		status = xstrdup(ekg_status_string(status, 0));
	} else {
		status = xstrdup(EMPTY_STATUS);
	}
		
	if(descr == NULL) {
		descr = xstrdup(EMPTY_DESCR);
		descr_alloc = 1;
	}
	
		
	debug("[logsoracle] protocol status (session %s :: uid %s :: status %s :: descr '%s'\n", session_uid, uid, status, descr);

	if(!oralog_db_new_status(session_uid, uid, status, descr, time(NULL), 0))
		logsoracle_stat_inc_status();
	  
    
	xfree(status);

	if(descr_alloc) {
		xfree(descr);
	}

	
	return 0;
}


/*
 * Protocol message post
 * Sent/Received a message 
 */		  
QUERY(logsoracle_handler_prmsg)
{
	char *session_uid = *(va_arg(ap, char**));	/* session uid */
	char *uid	  = *(va_arg(ap, char**));	/* sender uid */
	char **rcpts	  = *(va_arg(ap, char***));	/* list of reciepients (uids)*/
	char *text	  = *(va_arg(ap, char**));	/* message content */
	uint32_t *format  = *(va_arg(ap, uint32_t**));	/* ? */
	time_t	 sent	  = *(va_arg(ap, time_t*));	/* timestamp */
	int  class	  = *(va_arg(ap, int*));	/* check msgclass_t in protocol.h */
	char *seq	  = *(va_arg(ap, char**));	/* sequence number */

	int i;
	int rcpts_alloc=0;
				
	debug("[logsoracle] protocol message :\n");
	debug("[logsoracle] session: %s uid: %s\n", session_uid, uid);
	debug("[logsoracle] recipietns:\n");
	if(rcpts != NULL) {
		for(i=0; rcpts[i] != NULL; i++) {
			debug("[logsoracle]  %d. %s\n", i, rcpts[0]); 
		}
	}
	debug("[logsoracle] message content:\n");
	debug("[logsoracle]  %s\n", text);
	debug("[logsoracle] format: %d sent: %d class: %d\n", format, sent, class);
	debug("[logsoracle] seq: %s\n", seq);

	/* BUG (?)  
	 * When someone sends us a message rcpts is NULL
	 */
	if(rcpts == NULL) {
		array_add(&rcpts, xstrdup(session_uid));
		rcpts_alloc=1;
	}
	/* BUG */
	
	if(!oralog_db_new_msg(session_uid, uid, rcpts, text, time(NULL), 0))
		logsoracle_stat_inc_message();
	
	/* BUG (?) */
	if(rcpts_alloc) {
		/* This will also attemp to free allocated strings */
		array_free(rcpts);
		rcpts=NULL;
	}
	/* BUG */
	
	return 0;
}

/*
 * Functions for maintaining statistics
 */
void logsoracle_stat_clear()
{
	memset(&logsoracle_stat, 0, sizeof(logsoracle_stat));
}

static void logsoracle_stat_inc_message()
{
	logsoracle_stat.session_message_insert++;
}

static void logsoracle_stat_inc_status()
{
	logsoracle_stat.session_status_insert++;
}

int logsoracle_stat_get_message()
{
	return logsoracle_stat.session_message_insert;
}

int logsoracle_stat_get_status()
{
	return logsoracle_stat.session_status_insert;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
