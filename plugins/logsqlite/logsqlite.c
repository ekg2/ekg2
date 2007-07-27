/* $Id$ */

/*
 *  (C) Copyright 2005 Leszek Krupiñski <leafnode@wafel.com>
 *                     Marcin Jurkowski <marcin@vilo.eu.org>
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

#include <stdint.h>
#include <stdlib.h>

#include "ekg2-config.h"

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/log.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/queries.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>


#ifdef HAVE_SQLITE3
# include <sqlite3.h>
# define sqlite_n_exec(db, q, a, b, c) sqlite3_exec(db, q, a, b, c)
# define sqlite_n_close(db) sqlite3_close(db)
#else
# include <sqlite.h>
# define sqlite_n_exec(db, q, a, b, c) sqlite_exec(db, q, a, b, c)
# define sqlite_n_close(db) sqlite_close(db)
#endif

#include "logsqlite.h"


PLUGIN_DEFINE(logsqlite, PLUGIN_LOG, logsqlite_theme_init);

char *config_logsqlite_path = NULL;
int config_logsqlite_last_in_window = 0;
int config_logsqlite_last_open_window = 0;
int config_logsqlite_last_limit = 10;
int config_logsqlite_last_print_on_open = 0;
int config_logsqlite_log = 0;
int config_logsqlite_log_ignored = 0;
int config_logsqlite_log_status = 0;

static sqlite_t * logsqlite_current_db = NULL;
static char * logsqlite_current_db_path = NULL;
static int logsqlite_in_transaction = 0;


/*
 * last log
 */
COMMAND(logsqlite_cmd_last)
{
	sqlite_t * db;
	char buf[100];
	const char *last_direction;
#ifdef HAVE_SQLITE3
	sqlite3_stmt *stmt;
#else
	char *sql;
	const char ** results;
	const char ** fields;
	sqlite_vm * vm;

	char *errors;
	int count;
#endif
	
	struct tm *tm;
	time_t ts;
	int count2 = 0;
	char * gotten_uid = NULL;
	char * nick = NULL;
	char * keep_nick = NULL;
	int limit = config_logsqlite_last_limit;
	int i = 0;
	char * target_window = "__current";
	char *sql_search = NULL;

	if (!session) {
		if (session_current) {
			session = session_current;
		} else {
			return -1;
		}
	}
	
	
	for (i = 0; params[i]; i++) {
		
		if (match_arg(params[i], 'n', "number", 2) && params[i + 1]) {
			limit = atoi(params[++i]);

			if (limit <= 0) {
				printq("invalid_params", "logsqlite:last");
				return 0;
			}
			continue;
		}

		if (match_arg(params[i], 's', "search", 2) && params[i + 1]) {
			sql_search = (char *) params[++i];
			continue;
		}
				
		keep_nick = (char *) params[i];
	}

	if (! (db = logsqlite_prepare_db(session, time(0), 0)))
		return -1;

	keep_nick = xstrdup(keep_nick);
	sql_search = sql_search ? sql_search : "";	/* XXX: use fix() */
#ifdef HAVE_SQLITE3
	sql_search = sqlite3_mprintf("%%%s%%", sql_search);
#endif


	if (keep_nick) {
		nick = strip_quotes(keep_nick);
		gotten_uid = get_uid(session, nick);
		if (! gotten_uid) 
			gotten_uid = nick;
		if (config_logsqlite_last_in_window)
			target_window = gotten_uid;

#ifdef HAVE_SQLITE3
		sqlite3_prepare(db, "SELECT * FROM (SELECT uid, nick, ts, body, sent FROM log_msg WHERE uid = ?1 AND body LIKE ?3 ORDER BY ts DESC LIMIT ?2) ORDER BY ts ASC", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, gotten_uid, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 3, sql_search, -1, SQLITE_STATIC);
#else
		sql = sqlite_mprintf("SELECT * FROM (SELECT uid, nick, ts, body, sent FROM log_msg WHERE uid = '%q' AND body LIKE '%%%q%%' ORDER BY ts DESC LIMIT %i) ORDER BY ts ASC", gotten_uid, sql_search, limit);
#endif
	} else {
		if (config_logsqlite_last_in_window)
			target_window = "__status";

#ifdef HAVE_SQLITE3
		sqlite3_prepare(db, "SELECT * FROM (SELECT uid, nick, ts, body, sent FROM log_msg WHERE body LIKE ?3 ORDER BY ts DESC LIMIT ?2) ORDER BY ts ASC", -1, &stmt, NULL);
		sqlite3_bind_text(stmt, 3, sql_search, -1, SQLITE_STATIC);
#else
		sql = sqlite_mprintf("SELECT * FROM (SELECT uid, nick, ts, body, sent FROM log_msg WHERE body LIKE '%%%q%%' ORDER BY ts DESC LIMIT %i) ORDER BY ts ASC", sql_search, limit);
#endif
	}

#ifdef HAVE_SQLITE3
	sqlite3_bind_int(stmt, 2, limit);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ts = (time_t) sqlite3_column_int(stmt, 2);
#else
	sqlite_compile(db, sql, NULL, &vm, &errors);
	while (sqlite_step(vm, &count, &results, &fields) == SQLITE_ROW) {
		ts = (time_t) atoi(results[2]);
#endif
		if (count2 == 0) {
			if (gotten_uid)
				print_window(target_window, session, config_logsqlite_last_open_window, "last_begin_uin", gotten_uid);
			else
				print_window(target_window, session, config_logsqlite_last_open_window, "last_begin");
		}
		count2++;
		tm = localtime(&ts);
		strftime(buf, sizeof(buf), format_find("last_list_timestamp"), tm);
#ifdef HAVE_SQLITE3
		if (sqlite3_column_int(stmt, 4) == 0)
#else
		if (!xstrcmp(results[4], "0"))
#endif
			last_direction = "last_list_in";
		else
			last_direction = "last_list_out";

		print_window(target_window, session, config_logsqlite_last_open_window, last_direction, buf,
#ifdef HAVE_SQLITE3
			sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 3));
#else
			results[1], results[3]);
#endif
	}
	if (count2 == 0) {
		if (nick) {
			print_window(target_window, session, config_logsqlite_last_open_window, "last_list_empty_nick", nick);
		} else {
			print_window(target_window, session, config_logsqlite_last_open_window, "last_list_empty");
		}
	} else {
		print_window(target_window, session, config_logsqlite_last_open_window, "last_end");
	}

	xfree(keep_nick);

#ifdef HAVE_SQLITE3
	xfree(sql_search);
	sqlite3_finalize(stmt);
#else
	xfree(sql);
	sqlite_finalize(vm, &errors);
#endif
	return 0;
}

/*
 * set default configuration options
 */
void logsqlite_setvar_default()
{
	xfree(config_logsqlite_path);
	config_logsqlite_path = xstrdup("~/.ekg2/logsqlite.db");
}

/*
 * prepare path to database
 */
char *logsqlite_prepare_path(session_t *session, time_t sent)
{
	char *path, *tmp, datetime[5];
	struct tm *tm = localtime(&sent);
	string_t buf;

	if (!(tmp = config_logsqlite_path)) {
		return NULL;
	}

	buf = string_init(NULL);

	while (*tmp) {
		if ((char)*tmp == '%' && (tmp+1) != NULL) {
			switch (*(tmp+1)) {
				case 'S':	string_append_n(buf, session->uid, -1);
						break;
				case 'Y':	snprintf(datetime, 5, "%4d", tm->tm_year+1900);
						string_append_n(buf, datetime, 4);
						break;
				case 'M':	snprintf(datetime, 3, "%02d", tm->tm_mon+1);
						string_append_n(buf, datetime, 2);
						break;
				case 'D':       snprintf(datetime, 3, "%02d", tm->tm_mday);
						string_append_n(buf, datetime, 2);
						break;
				default:	string_append_c(buf, *(tmp+1));
			};

			tmp++;
		} else if (*tmp == '~' && (*(tmp+1) == '/' || *(tmp+1) == '\0')) {
			const char *home = getenv("HOME");
			string_append_n(buf, (home ? home : "."), -1);
		} else
			string_append_c(buf, *tmp);
		tmp++;
	};


	xstrtr(buf->str, ' ', '_');

	path = string_free(buf, 0);

	return path;
}


/*
 * prepare db handler
 *
 * 'mode': 0 = read, 1 = write (determines whether transaction should be used)
 */
sqlite_t * logsqlite_prepare_db(session_t * session, time_t sent, int mode)
{
	char * path;
	sqlite_t * db;

	if (!(path = logsqlite_prepare_path(session, sent)))
		return 0;
	if (!logsqlite_current_db_path || !logsqlite_current_db) {
		if (! (db = logsqlite_open_db(session, sent, path)))
			return 0;
                xfree(logsqlite_current_db_path);
		logsqlite_current_db_path = xstrdup(path);
		logsqlite_current_db = db;

		if (mode)
			sqlite_n_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
		logsqlite_in_transaction = mode;
	} else if (!xstrcmp(path, logsqlite_current_db_path) && logsqlite_current_db) {
		db = logsqlite_current_db;
		debug("[logsqlite] keeping old db\n");

		if (mode && !logsqlite_in_transaction)
			sqlite_n_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
		else if (!mode && logsqlite_in_transaction)
			sqlite_n_exec(db, "COMMIT", NULL, NULL, NULL);
		logsqlite_in_transaction = mode;
	} else {
		logsqlite_close_db(logsqlite_current_db);
		db = logsqlite_open_db(session, sent, path);
		logsqlite_current_db = db;
		xfree(logsqlite_current_db_path);
		logsqlite_current_db_path = xstrdup(path);

		if (mode)
			sqlite_n_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
		logsqlite_in_transaction = mode;
	}
	xfree(path);
	return db;
}

/*
 * open db
 */
sqlite_t * logsqlite_open_db(session_t * session, time_t sent, char * path)
{
	FILE * testFile;
	struct stat statbuf;
	char * slash, * dir;
	sqlite_t * db = NULL;
#ifdef HAVE_SQLITE3	
	const 
#endif 	
		char * errormsg = NULL;
	int makedir = 1, slash_pos = 0;



	while (makedir) {
		if (!(slash = xstrchr(path + slash_pos, '/'))) {
			makedir = 0;
			continue;
		};

		slash_pos = slash - path + 1;
		dir = xstrndup(path, slash_pos);

		if (stat(dir, &statbuf) != 0 && mkdir(dir, 0700) == -1) {
			char *bo = saprintf("nie mo¿na %s bo %s", dir, strerror(errno));
			print("generic",bo);
			xfree(bo);
			xfree(dir);
			return NULL;
		}
		xfree(dir);
	} // while mkdir

	debug("[logsqlite] opening database %s\n", path);

	testFile = fopen(path, "r");
	if (!testFile) {
		debug("[logsqlite] creating database %s\n", path);
#ifdef HAVE_SQLITE3		
		sqlite3_open(path, &db);
#else
		db = sqlite_open(path, 0, &errormsg);
#endif

		sqlite_n_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
		sqlite_n_exec(db, "CREATE TABLE log_msg (session , uid , nick , type , sent INT, ts INT, sentts INT, body )", NULL, NULL, NULL);
		sqlite_n_exec(db, "CREATE TABLE log_status (session , uid , nick , ts INT, status , desc )", NULL, NULL, NULL);

#ifdef HAVE_SQLITE3
		sqlite3_exec(db, "CREATE INDEX ts ON log_msg(ts)", NULL, NULL, NULL); 
		sqlite3_exec(db, "CREATE INDEX uid_ts ON log_msg(uid, ts)", NULL, NULL, NULL); 
#else
		sqlite_exec(db, "CREATE INDEX uid ON log_msg(uid)", NULL, NULL, NULL);
#endif
		sqlite_n_exec(db, "COMMIT", NULL, NULL, NULL);
	} else {
		fclose(testFile);

#ifdef HAVE_SQLITE3		
		sqlite3_open(path, &db);

		/* sqlite3 (unlike sqlite 2) defers opening until it's
		   really needed and sqlite3_open won't complain if
		   database file is wrong or corrupted */
		sqlite3_exec(db, "SELECT * FROM log_msg LIMIT 1", NULL, NULL, NULL);
#else
		db = sqlite_open(path, 0, &errormsg);
#endif
	}

#ifdef HAVE_SQLITE3
	if (sqlite3_errcode(db) != SQLITE_OK) {
		errormsg = sqlite3_errmsg(db);
#else
	if (!db) {
#endif 
		debug("[logsqlite] error opening database - %s\n", errormsg);
		print("logsqlite_open_error", errormsg);
#ifdef HAVE_SQLITE3
		sqlite3_close(db);		// czy to konieczne?
#else
		xfree(errormsg);
#endif
		return 0;
	}
	return db;
}

/*
 * close db
 *
 * 'force' determines whether database should really be closed
 * if we have 'persistent open' set, so that it should be used
 * for example on filename change
 */
void logsqlite_close_db(sqlite_t * db)
{
	if (!db) {
		return;
	}
	debug("[logsqlite] close db\n");
	if (db == logsqlite_current_db) {
		logsqlite_current_db = NULL;
		xfree(logsqlite_current_db_path);
		logsqlite_current_db_path = NULL;

		if (logsqlite_in_transaction)
			sqlite_n_exec(db, "COMMIT", NULL, NULL, NULL);
	}
	sqlite_n_close(db);
}

/**
 * handler wiadomo¶ci
 */

QUERY(logsqlite_msg_handler)
{
	char    **__session = va_arg(ap, char**),    *session = *__session;
	char        **__uid = va_arg(ap, char**),        *uid = *__uid;
	char     ***__rcpts = va_arg(ap, char***),    **rcpts = *__rcpts;
	char       **__text = va_arg(ap, char**),       *text = *__text;
	uint32_t **__format = va_arg(ap, uint32_t**), *format = *__format;
	time_t      *__sent = va_arg(ap, time_t*),       sent = *__sent;
	int        *__class = va_arg(ap, int*),         class = *__class;
	session_t *s = session_find((const char*)session);
	char * gotten_uid = get_uid(s, uid);
	char * gotten_nickname = get_nickname(s, uid);
	char * type = NULL;
	sqlite_t * db;
#ifdef HAVE_SQLITE3	
	sqlite3_stmt * stmt;
#endif 
	int is_sent;

	if (config_logsqlite_log == 0)
		return 0;

	format = NULL;

	if (!session)
		return 0;

	db = logsqlite_prepare_db(s, sent, 1);
	if (!db) {
		return 0;
	}

	switch ((enum msgclass_t)class) {
		case EKG_MSGCLASS_MESSAGE:
			type = xstrdup("msg");
			is_sent = 0;
			break;
		case EKG_MSGCLASS_CHAT:
			type = xstrdup("chat");
			is_sent = 0;
			break;
		case EKG_MSGCLASS_SENT:
			type = xstrdup("msg");
			is_sent = 1;
			break;
		case EKG_MSGCLASS_SENT_CHAT:
			type = xstrdup("chat");
			is_sent = 1;
			break;
		case EKG_MSGCLASS_SYSTEM:
			type = xstrdup("system");
			is_sent = 0;
			break;
		default:
			type = xstrdup("chat");
			is_sent = 0;
			break;
	};

	if (is_sent) {
		gotten_uid = (rcpts) ? rcpts[0] : NULL;
		if (rcpts) {
			gotten_uid      = get_uid(s, rcpts[0]);
			gotten_nickname = get_nickname(s, rcpts[0]);
		}

		if ( gotten_uid == NULL )
			gotten_uid = rcpts[0];

		if ( gotten_nickname == NULL )
			gotten_nickname = rcpts[0];
	} else {
		if ( gotten_uid == NULL )
			gotten_uid = uid;

		if ( gotten_nickname == NULL )
			gotten_nickname = uid;

	}


	debug("[logsqlite] running msg query\n");

#ifdef HAVE_SQLITE3
	sqlite3_prepare(db, "INSERT INTO log_msg VALUES (?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, session, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, gotten_uid, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, gotten_nickname, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 4, type, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 5, is_sent);
	sqlite3_bind_int(stmt, 6, time(0));
	sqlite3_bind_int(stmt, 7, sent);
	sqlite3_bind_text(stmt, 8, text, -1, SQLITE_STATIC);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
#else
	sqlite_exec_printf(db, "INSERT INTO log_msg VALUES(%Q, %Q, %Q, %Q, %i, %i, %i, %Q)", 0, 0, 0,
		session,
		gotten_uid,
		gotten_nickname,
		type,
		is_sent,
		time(0),
		sent,
		text);
#endif 


	xfree(type);

	return 0;
};

/**
 * handler statusów
 */
QUERY(logsqlite_status_handler) {
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	int nstatus	= *(va_arg(ap, int*));
	char *descr	= *(va_arg(ap, char**));
	const char *status;

	session_t *s	= session_find(session);
	char * gotten_uid = get_uid(s, uid);
	char * gotten_nickname = get_nickname(s, uid);
	sqlite_t * db;
#ifdef HAVE_SQLITE3	
	sqlite3_stmt *stmt;
#endif 

	if (!config_logsqlite_log_status)
		return 0;

	if (!session)
		return 0;

	db = logsqlite_prepare_db(s, time(0), 1);
	if (!db) {
		return 0;
	}

	if ( gotten_uid == NULL )
		gotten_uid = uid;

	if ( gotten_nickname == NULL )
		gotten_nickname = uid;

	status = ekg_status_string(nstatus, 0);

	if ( descr == NULL )
		descr = "";

	debug("[logsqlite] running status query\n");

#ifdef HAVE_SQLITE3
	sqlite3_prepare(db, "INSERT INTO log_status VALUES(?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, session, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, gotten_uid, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, gotten_nickname, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 4, time(0));
	sqlite3_bind_text(stmt, 5, status, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 6, descr, -1, SQLITE_STATIC);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

#else
	sqlite_exec_printf(db, "INSERT INTO log_status VALUES(%Q, %Q, %Q, %i, %Q, %Q)", 0, 0, 0,
		session,
		gotten_uid,
		gotten_nickname,
		time(0),
		status,
		descr);
#endif 

	return 0;
}

/* here we print last few messages, like raw_log feature of logs plugin does,
 * but more Konnekt-like (i.e. message-oriented, not console dump) */

static QUERY(logsqlite_newwin_handler) {
	window_t	*w	= *(va_arg(ap, window_t **));
	const char	*uid;
	int		class;
	time_t		ts;
	const char	*rcpts[2] = { NULL, NULL };

	sqlite_t	*db;
#ifdef HAVE_SQLITE3
	sqlite3_stmt	*stmt;
#else
	char		*sql;
	const char	**results;
	const char	**fields;
	sqlite_vm	*vm;

	char		*errors;
	int		count;
#endif

	if (!config_logsqlite_last_print_on_open || !w || !w->target || !w->session || w->id == 1000
			|| !(uid = get_uid_any(w->session, w->target))
			|| !(db = logsqlite_prepare_db(w->session, time(0), 0)))
		return 0;

		/* as we don't log raw messages, we can normally print the old ones
		 * and we can simply the thing one step more by using message_print(),
		 * which is hopefully exported by core */

			/* these ones stolen from /last cmd */
#ifdef HAVE_SQLITE3
	sqlite3_prepare(db, "SELECT * FROM (SELECT ts, body, sent FROM log_msg WHERE uid = ?1 ORDER BY ts DESC LIMIT ?2) ORDER BY ts ASC", -1, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, uid, -1, SQLITE_STATIC);
	sqlite3_bind_int(stmt, 2, config_logsqlite_last_limit);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ts = (time_t) sqlite3_column_int(stmt, 0);

		if (sqlite3_column_int(stmt, 2) == 0) {
#else
	sql = sqlite_mprintf("SELECT * FROM (SELECT ts, body, sent FROM log_msg WHERE uid = '%q' ORDER BY ts DESC LIMIT %i) ORDER BY ts ASC", uid, config_logsqlite_last_limit);
	sqlite_compile(db, sql, NULL, &vm, &errors);
	while (sqlite_step(vm, &count, &results, &fields) == SQLITE_ROW) {
		ts = (time_t) atoi(results[0]);

		if (!xstrcmp(results[2], "0")) {
#endif
			class = EKG_MSGCLASS_CHAT;
			rcpts[0] = NULL;
		} else {
			class = EKG_MSGCLASS_SENT_CHAT;
			rcpts[0] = uid;
		}

			/* TODO: some more distinction between normal and old messages... maybe some weird, new msgclasses?
			 * something like EKG_MSGCLASS_LOG_RECV & EKG_MSGCLASS_LOG_SENT, I mean */
			/* TODO: maybe somehow move these things after 'query started' msg? but haven't got an idea how to */
		message_print(session_uid_get(w->session), (class == EKG_MSGCLASS_CHAT ? uid : session_uid_get(w->session)), rcpts, 
#ifdef HAVE_SQLITE3
			sqlite3_column_text(stmt, 1),
#else
			results[1],
#endif
			NULL, ts, class, NULL, 0, 0);
	};

#ifdef HAVE_SQLITE3
	sqlite3_finalize(stmt);
#else
	xfree(sql); /* XXX: if we use sqlite_mprintf(), shouldn't we use also sqlite_free()? */
	sqlite_finalize(vm, &errors);
#endif
	return 0;
}

int logsqlite_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("logsqlite_open_error", "%! Can't open database: %1\n", 1);
#endif
	return 0;
}

static int logsqlite_plugin_destroy()
{

	if (logsqlite_current_db)
		logsqlite_close_db(logsqlite_current_db);

	plugin_unregister(&logsqlite_plugin);

	debug("[logsqlite] plugin unregistered\n");

	return 0;
}

int logsqlite_plugin_init(int prio)
{
	plugin_register(&logsqlite_plugin, prio);

	logsqlite_setvar_default();

	command_add(&logsqlite_plugin, "logsqlite:last", "puU puU puU puU puU", logsqlite_cmd_last, 0, "-n --number -s --search");

	query_connect_id(&logsqlite_plugin, PROTOCOL_MESSAGE_POST, logsqlite_msg_handler, NULL);
	query_connect_id(&logsqlite_plugin, PROTOCOL_STATUS, logsqlite_status_handler, NULL);
	query_connect_id(&logsqlite_plugin, UI_WINDOW_NEW,	logsqlite_newwin_handler, NULL);

	variable_add(&logsqlite_plugin, ("last_open_window"), VAR_BOOL, 1, &config_logsqlite_last_open_window, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("last_in_window"), VAR_BOOL, 1, &config_logsqlite_last_in_window, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("last_limit"), VAR_INT, 1, &config_logsqlite_last_limit, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("last_print_on_open"), VAR_BOOL, 1, &config_logsqlite_last_print_on_open, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("log_ignored"), VAR_BOOL, 1, &config_logsqlite_log_ignored, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("log_status"), VAR_BOOL, 1, &config_logsqlite_log_status, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("log"), VAR_BOOL, 1, &config_logsqlite_log, NULL, NULL, NULL);
	variable_add(&logsqlite_plugin, ("path"), VAR_DIR, 1, &config_logsqlite_path, NULL, NULL, NULL);

	debug("[logsqlite] plugin registered\n");

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8 noexpandtab
 */
