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
 
#include <string.h>

#include <ekg/themes.h> 
#include <ekg/stuff.h> 
#include <ekg/debug.h>

#include "commands.h"
#include "main.h"
#include "oralog.h" 

extern struct logsoracle_conf_struct logsoracle_config;
 
/*
 * Connect to database
 */
COMMAND(logsoracle_cmd_db_connect)
{
	int retval = 0;
	retval = oralog_db_connect(logsoracle_config.db_login, logsoracle_config.db_password, quiet);
	
	switch(retval) {
	    case DB_CONNECT_NEW_CONNECTION:
		logsoracle_stat_clear();
		printq("logsoracle_connected");
	    break;
	    
	    case DB_CONNECT_ALREADY_CONNECTED:
		if(!quiet)
		printq("logsoracle_already_connected");
	    break;
	    
	    case DB_CONNECT_ERROR:
		/* Error messages (including oracle diagnostic messages) are printed in oralog.c */
	    break;
	    
	    default:
		debug("[logsoracle] (!) oralog_db_connect returned an undefined value (!) \n");
	}
	
	return 0;
}

/*
 * Disconnect from database
 */
COMMAND(logsoracle_cmd_db_disconnect)
{
	if(!oralog_db_disconnect())
	    printq("logsoracle_disconnected");
	else
	    printq("logsoracle_disconn_not_needed");
	
	
	return 0;
}

/*
 * Print plugin status
 */
COMMAND(logsoracle_cmd_status)
{
	printq("logsoracle_status");
	printq("logsoracle_status_con", ( oralog_is_connected() ? "yes" : "no" ) );
	printq("logsoracle_status_sta", itoa( (long int)logsoracle_stat_get_status() ) );
	printq("logsoracle_status_msg", itoa( (long int)logsoracle_stat_get_message() ) );
	
	return 0;
}
