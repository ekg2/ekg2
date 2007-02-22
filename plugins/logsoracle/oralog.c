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
 
/*
 * All OCI calls should be kept inside this module.
 */

/*
 * NOTE ABOUT MULTITHREADING :
 *  All operations preformed by this module are serialized. If you need
 *  better performance in a multithreaded enviroment then you have to remove
 *  the serialization (see oralog_oper_lock semaphore) and create individual 
 *  session and service handles for each thread.
 *  
 *  OCI is thread safe by default. Additional semaphore (oralog_oper_lock) is
 *  used to ensure that no operation is interrupted by a connect or disconnect.
 *  If you plan to create an individual session object for each thread then you
 *  can convert this to a 'readers & writers' problem.
 */

#include "oralog.h"

#include <string.h>
#include <time.h>

#include <ekg/xmalloc.h>
#include <ekg/debug.h>
#include <ekg/themes.h>

#include <oci.h>

#include <pthread.h>

#ifndef NULL
#define NULL	0
#endif


/* Connection status */
static int logsoracle_connected = 0;

/* Local (private) functions */
static void check_string_len(char *str);
static int  oralog_is_error(OCIError *lhp_error, sword status, int print_messages);
static void set_connection_status(int val);
static void free_global_handles();
static inline size_t ora_strlen(const char *str);

/* Oracle handles */
OCIEnv		*hp_env     = NULL;	
OCIServer	*hp_server  = NULL;
OCISession	*hp_session = NULL;   
OCISvcCtx	*hp_service = NULL; /* SvcCtx -> Service Context */


pthread_mutex_t oralog_oper_lock = PTHREAD_MUTEX_INITIALIZER;

/* 
 * This function should be used to determine if we're connected to the database 
 * (informational purposes only (status command) - do not rely on this)
 *
 * returns: 1 - connected
 *          0 - not connected  
 */
int oralog_is_connected()
{
    return logsoracle_connected;
}

/*
 * Tell module about current connection status
 */
static void set_connection_status(int val)
{
    logsoracle_connected = val;
}


/*
 * Connect to database.
 *  db_login    - database login
 *  db_password - database password
 * return : DB_CONNECT_NEW_CONNECTION if a new connection was estabilished
 *          DB_CONNECT_ALREADY_CONNECTED or
 *          DB_CONNECT_ERROR
 */
int oralog_db_connect(char *db_login, char *db_password, int quiet)
{
	int      print_errors = (quiet) ? 0 : 1;
	int	 errors = 0;
	sword	 retval = 0;
	OCIError *hp_error = NULL;


	pthread_mutex_lock(&oralog_oper_lock);	
	
	if (oralog_is_connected()) {
		debug("[logsoracle] already connected\n");
		
		pthread_mutex_unlock(&oralog_oper_lock);
		return DB_CONNECT_ALREADY_CONNECTED;
	}

	
	debug("[logsoracle] connecting.. ");
	
	
	/* initialize the mode to be the threaded and object environment */
	OCIEnvCreate(&hp_env, OCI_THREADED|OCI_OBJECT, (dvoid *)0, 0, 0, 0, (size_t) 0, (dvoid **)0);

	/* allocate a server handle */
	OCIHandleAlloc((dvoid *)hp_env, (dvoid **)&hp_server, OCI_HTYPE_SERVER, 0, (dvoid **) 0);

	/* allocate an error handle */
	OCIHandleAlloc((dvoid *)hp_env, (dvoid **)&hp_error, OCI_HTYPE_ERROR, 0, (dvoid **) 0);

	/* create a server context */
	/* TODO: dblink can be set here */
	retval = OCIServerAttach(hp_server, hp_error, (text *)0, 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retval, print_errors))
	 errors++;


	/* allocate a service handle */
	OCIHandleAlloc((dvoid *)hp_env, (dvoid **)&hp_service, OCI_HTYPE_SVCCTX, 0, (dvoid **) 0);

	/* associate server handle with service handle*/
	retval = OCIAttrSet((dvoid *)hp_service, OCI_HTYPE_SVCCTX, (dvoid *)hp_server, (ub4) 0, OCI_ATTR_SERVER, hp_error);
	if(oralog_is_error(hp_error, retval, print_errors))
	 errors++;

	/* allocate a session handle */
	OCIHandleAlloc((dvoid *)hp_env, (dvoid **)&hp_session, OCI_HTYPE_SESSION, 0, (dvoid **) 0);

	/* set username in session handle */
	retval = OCIAttrSet((dvoid *)hp_session, OCI_HTYPE_SESSION, (dvoid *)db_login, (ub4)ora_strlen(db_login), OCI_ATTR_USERNAME, hp_error);
	if(oralog_is_error(hp_error, retval, print_errors))
	 errors++;	

	/* set password in session handle */
	retval = OCIAttrSet((dvoid *)hp_session, OCI_HTYPE_SESSION, (dvoid *)db_password, (ub4)ora_strlen(db_password), OCI_ATTR_PASSWORD, hp_error);
	if(oralog_is_error(hp_error, retval, print_errors))
	 errors++;

	retval = OCISessionBegin ((dvoid *)hp_service, hp_error, hp_session, OCI_CRED_RDBMS, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retval, print_errors))
	 errors++;

	/* associate session with service context */
	retval = OCIAttrSet ((dvoid *)hp_service, OCI_HTYPE_SVCCTX, (dvoid *)hp_session, (ub4) 0, OCI_ATTR_SESSION, hp_error);
	if(oralog_is_error(hp_error, retval, print_errors))
	 errors++;

	/* free local handles */
        if(hp_error)
		OCIHandleFree(hp_error, OCI_HTYPE_ERROR);


	if (errors) {
		debug("[logsoracle] errors encounterd - cleaning up connection\n");
		free_global_handles();
		set_connection_status(0);
		
		pthread_mutex_unlock(&oralog_oper_lock);
		return DB_CONNECT_ERROR;
	}
	
	set_connection_status(1);
	debug("connected\n");

	pthread_mutex_unlock(&oralog_oper_lock);
	return DB_CONNECT_NEW_CONNECTION;
}

/*
 * Disconnect from database
 *  returns 0 if disconnection was needed
 */
int oralog_db_disconnect()
{
	pthread_mutex_lock(&oralog_oper_lock);

	if (!oralog_is_connected()) {
		debug("[logsoracle] attemping to disconnect a dead connection\n");
		
		pthread_mutex_unlock(&oralog_oper_lock);
		return 1;
	}

	debug("[logsoracle] disconnecting.. ");
	
	free_global_handles();
	set_connection_status(0);
	
	debug("disconnected\n");

	pthread_mutex_unlock(&oralog_oper_lock);
	return 0;
}

/*
 * This private function is responsible for freeing 
 * globally allocated OCI handles.
 */
static void free_global_handles()
{
	if(hp_env) 
		OCIHandleFree(hp_env, OCI_HTYPE_ENV);
	if(hp_server) 
		OCIHandleFree(hp_server, OCI_HTYPE_SERVER);
	if(hp_session)
		OCIHandleFree(hp_session, OCI_HTYPE_SESSION);
	if(hp_service) 
		OCIHandleFree(hp_service, OCI_HTYPE_SVCCTX);

	/* keep it clean */
	hp_env     = NULL;
	hp_server  = NULL;
	hp_session = NULL;
	hp_service = NULL;				
}


/*
 * Add a new status change to the database
 * Params:
 *  session     - user session name
 *  uid         - uid of person changing status
 *  status      - type of new status (away|back|...)
 *  descr       - new description
 *  change_time - time of status change (no. of seconds since 1st Jan 1970)
 *  quiet       - print error messages or not
 * Returns 0 on success.
 */
int oralog_db_new_status(char *session, char *uid, char *status, char *descr, time_t change_time, int quiet)
{
	OCIStmt	  *hp_stmt = NULL;     /* SQL statement handle */
	OCIBind	  *bind[5] = { NULL }; /* Needed for C-var to SQL-var binding */	
        sword     retstat  = 0;        /* Return value of function */
	int       errors   = 0;        /* Error counter */
	uword     invalid  = 0;        /* for DateCheck() */
        OCIDate   oci_date;            /* for 'change_time' - oracle internal representation */
	struct tm *tm_chtime = NULL;   /* for 'change_time' - standard C representation */
	
	int       print_errors = (quiet) ? 0 : 1; /* if nonzero then print errors into status window */
	OCIError  *hp_error   = NULL;             /* oci error handle */
	
	/* SQL statement to execute */
	text	*sqlstmt = (text *) "INSERT INTO status_changes VALUES( status_changes_seq.nextval, :session_uid, :changes_uid, :status, :descr, :change_time)";


	pthread_mutex_lock(&oralog_oper_lock);

        if (!oralog_is_connected()) {
		debug("[logsoracle] can't log status - not connected\n");
		
		pthread_mutex_unlock(&oralog_oper_lock);
		return 1;
	}


	check_string_len(session);
	check_string_len(uid);
	check_string_len(status);
	check_string_len(descr);


	/* Create local handles (OCIBind's are created automaticly) */
	OCIHandleAlloc( (dvoid *)hp_env, (dvoid **)&hp_stmt, OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
	OCIHandleAlloc( (dvoid *)hp_env, (dvoid **)&hp_error, OCI_HTYPE_ERROR, 0, (dvoid **) 0);

	/* Create DATE */
	tm_chtime = localtime(&change_time);
	
	memset(&oci_date, 0, sizeof(OCIDate));
	OCIDateSetTime( &oci_date, (ub1)tm_chtime->tm_hour, (ub1)tm_chtime->tm_min, (ub1)tm_chtime->tm_sec ); 
	OCIDateSetDate( &oci_date, (sb2)tm_chtime->tm_year+1900, (ub1)tm_chtime->tm_mon+1, (ub1)tm_chtime->tm_mday );


	/* check if provided 'change_time' was mapped correctly  */
        retstat = OCIDateCheck(hp_error, &oci_date, &invalid);
        if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;
		 	

	/* prepare SQL statement */	
	debug("[logsoracle] preparing new status statement\n");
	retstat = OCIStmtPrepare(hp_stmt, hp_error, (text *)sqlstmt, (ub4)ora_strlen((char *)sqlstmt), (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;


        /* bind variables */
	debug("[logsoracle] binding..\n");

	retstat = OCIBindByName(hp_stmt, &bind[0], hp_error, (text *) ":session_uid", -1, (dvoid *)session,
				ora_strlen(session)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;	

				
	retstat = OCIBindByName(hp_stmt, &bind[1], hp_error, (text *) ":changes_uid",
				-1, (dvoid *) uid,
				ora_strlen(uid)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;				


	retstat = OCIBindByName(hp_stmt, &bind[2], hp_error, (text *) ":status",
				-1, (dvoid *) status,
				ora_strlen(status)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;

				
	retstat = OCIBindByName(hp_stmt, &bind[3], hp_error, (text *) ":descr",
    				-1, (dvoid *) descr,
				ora_strlen(descr)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;


	/* Oracle type DATE can be easily bind by using OCIDate struct and SQLT_ODT datatype */					   
	retstat = OCIBindByName(hp_stmt, &bind[4], hp_error, (text *) ":change_time",
				-1, (dvoid *) &oci_date,
				(sword) sizeof(OCIDate), SQLT_ODT, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;
			 			


	/* Statement ready - execute */
	debug("[logsoracle] executing\n");
	retstat = OCIStmtExecute(hp_service, hp_stmt, hp_error, (ub4) 1, (ub4) 0,
	    			(CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;


	/* Commit transaction */
	if(!errors) {
		debug("[logsoracle] commit\n");
		OCITransCommit(hp_service, hp_error, (ub4) 0);
	}
	else {
		debug("[logsoracle] errors present - aborting transaction\n");
		OCITransRollback(hp_service, hp_error, (ub4) OCI_DEFAULT);
	}
					
	/* Cleanup (bind handles should be removed as a part of statement) */	
	if(hp_stmt)
	    OCIHandleFree(hp_stmt, OCI_HTYPE_STMT);	
        if(hp_error)
	    OCIHandleFree(hp_error, OCI_HTYPE_ERROR);
	
	pthread_mutex_unlock(&oralog_oper_lock);
	return 0;
}


/*
 * Add a new message to the database.
 *  session    - session name
 *  sender_uid -
 *  rcpts      -
 *  content    -
 *  recv_time  - time of arrival
 *  quiet      - print error messages?
 * Returns 0 on success
 */
int oralog_db_new_msg(char *session, char *sender_uid, char **rcpts, char *content, time_t recv_time, int quiet)
{
	int i;

	/* There will be at least 2 INSERTs - 1 into 'messages' table and 1 (at least)
	   into the 'recipients' table. */
	text    *sqlstmt_msg  = (text *)"INSERT INTO messages VALUES(messages_seq.nextval, :session_uid, :sender_uid, :content, :recv_time) RETURNING id INTO :msg_id";
	text    *sqlstmt_rcp  = (text *)"INSERT INTO recipients VALUES(recipients_seq.nextval, :recipient_uid, :msg_id)";

	/* Statement & bind handles */
	OCIStmt *hp_stmt_msg = NULL;
	OCIStmt *hp_stmt_rcp = NULL;
	OCIBind *bind_msg[5] = { NULL };
	OCIBind *bind_rcp[2] = { NULL };

	/* Control vars */
	sword    retstat  = 0;
	int      errors   = 0;
	int      print_errors = (quiet) ? 0 : 1;
	OCIError *hp_error = NULL;

	/* Date handling */
	uword     invalid = 0;
	OCIDate   oci_recvtime;
	struct tm *tm_recvtime = NULL;		
	
	/* ID of inserted message will be put here */
	OCINumber msg_id;
	int       init_val = 0;	/* this variable is needed for initialization only */
	

	pthread_mutex_lock(&oralog_oper_lock);
	
	if (!oralog_is_connected()) {
		debug("[logsoracle] can't log msg - not connected\n");
		
		pthread_mutex_unlock(&oralog_oper_lock);
		return 1;
	}


	check_string_len(session);
	check_string_len(sender_uid);
	check_string_len(content);

	/* Create local handles (OCIBind's are created automaticly) */
	OCIHandleAlloc( (dvoid *)hp_env, (dvoid **)&hp_stmt_msg, OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
        OCIHandleAlloc( (dvoid *)hp_env, (dvoid **)&hp_stmt_rcp, OCI_HTYPE_STMT, (size_t) 0, (dvoid **) 0);
	OCIHandleAlloc( (dvoid *)hp_env, (dvoid **)&hp_error, OCI_HTYPE_ERROR, 0, (dvoid **) 0);	
	
        /* Create DATE in Oracle format */
	tm_recvtime = localtime(&recv_time);
		
	memset(&oci_recvtime, 0, sizeof(OCIDate));
	OCIDateSetTime( &oci_recvtime, (ub1)tm_recvtime->tm_hour, (ub1)tm_recvtime->tm_min, (ub1)tm_recvtime->tm_sec );
	OCIDateSetDate( &oci_recvtime, (sb2)tm_recvtime->tm_year+1900, (ub1)tm_recvtime->tm_mon+1, (ub1)tm_recvtime->tm_mday );

	/* check if provided 'recv_time' was mapped correctly  */
	retstat = OCIDateCheck(hp_error, &oci_recvtime, &invalid);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;

        /* prepare SQL statements */
	debug("[logsoracle] preparing new messages insert statement\n");
	retstat = OCIStmtPrepare(hp_stmt_msg, hp_error, (text *)sqlstmt_msg, (ub4)ora_strlen((char *)sqlstmt_msg), (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;	 

        debug("[logsoracle] preparing new recipients insert statement\n");
	retstat = OCIStmtPrepare(hp_stmt_rcp, hp_error, (text *)sqlstmt_rcp, (ub4)ora_strlen((char *)sqlstmt_rcp), (ub4)OCI_NTV_SYNTAX, (ub4)OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;
 

	/* bind the placeholders */
        debug("[logsoracle] binding messages insert..\n");
	   
	retstat = OCIBindByName(hp_stmt_msg, &bind_msg[0], hp_error, (text *) ":session_uid", -1, (dvoid *)session,
				ora_strlen(session)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;

        retstat = OCIBindByName(hp_stmt_msg, &bind_msg[1], hp_error, (text *) ":sender_uid", -1, (dvoid *)sender_uid,
				ora_strlen(sender_uid)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;									    

        retstat = OCIBindByName(hp_stmt_msg, &bind_msg[2], hp_error, (text *) ":content", -1, (dvoid *)content,
				ora_strlen(content)+1, SQLT_STR, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;
	 
        retstat = OCIBindByName(hp_stmt_msg, &bind_msg[3], hp_error, (text *) ":recv_time",
				-1, (dvoid *) &oci_recvtime,
				(sword) sizeof(OCIDate), SQLT_ODT, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;	 
	

	/* Create a new OCINumber - will be under msg_id */
	retstat = OCINumberFromInt(hp_error, &init_val, sizeof(init_val), OCI_NUMBER_SIGNED, &msg_id);
        if(oralog_is_error(hp_error, retstat, print_errors))
         errors++;
	      	
	
	/* bind the OCINumber */
        retstat = OCIBindByName(hp_stmt_msg, &bind_msg[4], hp_error, (text *) ":msg_id",
	                        -1, (dvoid *) &msg_id,
				(sword) sizeof(msg_id), SQLT_VNU, (dvoid *) 0,
				(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;
															 

        /* Statement ready - execute */
	debug("[logsoracle] executing insert on messages\n");
	retstat = OCIStmtExecute(hp_service, hp_stmt_msg, hp_error, (ub4) 1, (ub4) 0,
			        (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
	if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;

	/* You can check recieved message ID like this:
	retstat = OCINumberToInt(hp_error, &msg_id, sizeof(int), OCI_NUMBER_SIGNED, &tmpval);
        if(oralog_is_error(hp_error, retstat, print_errors))
	 errors++;	
	debug("[logsoracle] recieved message id: %d\n", tmpval);
	*/
	
	/* Insert into recipients table */
	if(rcpts) {
	    for(i=0; rcpts[i] != NULL; i++) {
		
		check_string_len(rcpts[i]);
		
		debug("[logsoracle] binding recipients\n");
		retstat = OCIBindByName(hp_stmt_rcp, &bind_rcp[0], hp_error, (text *) ":recipient_uid", -1, (dvoid *)rcpts[i],
	                                ora_strlen(rcpts[i])+1, SQLT_STR, (dvoid *) 0,
					(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
		if(oralog_is_error(hp_error, retstat, print_errors))
		 errors++;	    
	    
	    
		retstat = OCIBindByName(hp_stmt_rcp, &bind_rcp[1], hp_error, (text *) ":msg_id",
	                                -1, (dvoid *) &msg_id,
					(sword) sizeof(msg_id), SQLT_VNU, (dvoid *) 0,
					(ub2 *) 0, (ub2 *) 0, (ub4) 0, (ub4 *) 0, OCI_DEFAULT);
		if(oralog_is_error(hp_error, retstat, print_errors))
		 errors++;
		
    		debug("[logsoracle] executing insert on recipients\n");
	        retstat = OCIStmtExecute(hp_service, hp_stmt_rcp, hp_error, (ub4) 1, (ub4) 0,
		                        (CONST OCISnapshot *) NULL, (OCISnapshot *) NULL, OCI_DEFAULT);
		if(oralog_is_error(hp_error, retstat, print_errors))
		 errors++;
	    }
	}
				    

        /* Commit transaction */
	if(!errors) {
	    debug("[logsoracle] commit\n");
	    OCITransCommit(hp_service, hp_error, (ub4) 0);
	}
	else {
	    debug("[logsoracle] errors present - aborting transaction\n");
	    OCITransRollback(hp_service, hp_error, (ub4) OCI_DEFAULT);
	}


        /* Cleanup (bind handles should be removed as a part of statement) */
	if(hp_stmt_msg)
	    OCIHandleFree(hp_stmt_msg, OCI_HTYPE_STMT);
        if(hp_stmt_rcp)
	    OCIHandleFree(hp_stmt_rcp, OCI_HTYPE_STMT);
        if(hp_error)
	    OCIHandleFree(hp_error, OCI_HTYPE_ERROR);

	pthread_mutex_unlock(&oralog_oper_lock);
	return 0;
}


/*
 * Check if 'str' is ok to be but into the database. Correct if not (shorten)
 */
static void check_string_len(char *str)
{
	if (ora_strlen(str) >= ORACLE_MAX_STRING_LEN ) {
	    debug("[logsoracle] Warning: string to long - clipping\n");
	    str[ORACLE_MAX_STRING_LEN - 1] = '\0';
	}
    
	return;
}

/* 
 * Function checks provided handle and some OCI return value for errors
 *  lhp_error      - error handle
 *  status         - return value of function
 *  print_messages - if nonzero then print messages into the status/current window. 
 *
 * Returns '0' on success/warning
 *         '1' on failure (critical error)
 */
static int oralog_is_error(OCIError *lhp_error, sword status, int print_messages)
{
	text buff[1024];
	sb4  error_code;

	switch (status)
	{
	    case OCI_SUCCESS:
		return 0;
	    case OCI_SUCCESS_WITH_INFO:
		debug("[logsoracle] function returned OCI_SUCCESS_WITH_INFO\n");
		return 0;
	    case OCI_NEED_DATA:
		debug("[logsoracle] function returned OCI_NEED_DATA\n");
		break;
	    case OCI_NO_DATA:
		debug("[logsoracle] function returned  OCI_NODATA\n");
		break;
	    case OCI_ERROR:
		OCIErrorGet((dvoid *)lhp_error, (ub4) 1, (text *) NULL, &error_code, buff, (ub4) sizeof(buff), OCI_HTYPE_ERROR);
		debug("[logsoracle] function returned OCI_ERROR\n");
		debug("[logsoracle] message is : \n%s\n", buff);
		if(print_messages) {
		    print_window("__status", session_current, 0, "logsoracle_error", buff ); 
		}
		break;
	    case OCI_INVALID_HANDLE:
		debug("[logsoracle] function returned OCI_INVALID_HANDLE\n");
		break;
	    case OCI_STILL_EXECUTING:
		debug("[logsoracle] function returned OCI_STILL_EXECUTE\n");
		break;
	    case OCI_CONTINUE:
		debug("[logsoracle] function returned OCI_CONTINUE\n");
		break;
	    default:
		break;
	}
	
	return 1;
}

/*
 * For portability
 */
static inline size_t ora_strlen(const char *str) 
{
	return xstrlen(str);
}

