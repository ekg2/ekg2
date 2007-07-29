
/* logsqlite-fuse
 * (C) 2007 Michał Górny
 * covered under GPL2
 *
 * compile like:
 * 	gcc ${CFLAGS} $(pkg-config --cflags fuse sqlite3) -o logsqlite-fuse logsqlite-fuse.c $(pkg-config --libs fuse sqlite3) 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include <errno.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <sqlite3.h>

#define QUERY_COUNT 6
#define READ_ROW_COUNT "20"
#define BUF_START_SIZE 4096

typedef struct {
	char		*sid,
			*uid,
			*data;
	off_t		data_off;
	size_t		data_size,
			buf_size;
	int		db_off,
			eof;
	void		*next;
} myBuffer_t;

typedef struct {
	sqlite3		*db;
	sqlite3_stmt	*stmt[QUERY_COUNT];
	myBuffer_t	*buffers;
} myDB;

static const char *queries[QUERY_COUNT+1] = {
	"SELECT ts FROM log_msg ORDER BY ts DESC LIMIT 1;",
	"SELECT ts FROM log_msg WHERE session = ?1 ORDER BY ts DESC LIMIT 1;",
	"SELECT ts FROM log_msg WHERE session = ?1 AND uid = ?2 ORDER BY ts DESC LIMIT 1;",
	"SELECT DISTINCT session FROM log_msg ORDER BY session ASC LIMIT -1 OFFSET ?1;",
	"SELECT DISTINCT uid FROM log_msg WHERE session = ?2 ORDER BY uid ASC LIMIT -1 OFFSET ?1;",
	"SELECT type, sent, uid, nick, ts, sentts, body FROM log_msg WHERE session = ?1 AND uid = ?2 ORDER BY ts ASC LIMIT " READ_ROW_COUNT " OFFSET ?3;",
	NULL
};

enum statement_n {
	GET_NEWEST,
	GET_NEWEST_SESSION,
	GET_NEWEST_UID,
	GET_SESSIONS,
	GET_UIDS,
	GET_DATA
};

int mySplitPath(const char *path, const char **sid, const char **uid) {
	static char sidbuf[PATH_MAX], uidbuf[PATH_MAX];
	const char *tmp;

	if (strlen(path) > PATH_MAX) return -ENAMETOOLONG;

	*sid = sidbuf;
	*uid = uidbuf;

	path++; /* skip leading '/' */
	if (tmp = strchr(path, '/')) {
		strncpy(sidbuf, path, tmp-path);
		sidbuf[tmp-path] = 0;
		path = tmp+1;

		if (tmp = strchr(path, '/')) {
			strncpy(uidbuf, path, tmp-path);
			uidbuf[tmp-path] = 0;
			path = tmp+1;
			return 2 + (*path != 0);
		} else {
			strcpy(uidbuf, path);
			return 1 + (*path != 0);
		}
	} else {
		strcpy(sidbuf, path);
		return 0 + (*path != 0);
	}
}

int myGetAttr(const char *path, struct stat *out) {
	const char *sid, *uid;
	const int pathargs = mySplitPath(path, &sid, &uid);
	myDB *db = fuse_get_context()->private_data;
	sqlite3_stmt *stmt;
	int timestamp;

	switch (pathargs) {
		case 0:
			stmt = db->stmt[GET_NEWEST];
			break;
		case 1:
			stmt = db->stmt[GET_NEWEST_SESSION];
			sqlite3_bind_text(stmt, 1, sid, -1, SQLITE_STATIC);
			break;
		default:
			stmt = db->stmt[GET_NEWEST_UID];
			sqlite3_bind_text(stmt, 1, sid, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 2, uid, -1, SQLITE_STATIC);
	}

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		sqlite3_reset(stmt);
		return -ENOENT;
	}

	timestamp = sqlite3_column_int(stmt, 0);
	sqlite3_reset(stmt);
	if (pathargs > 2)
		return -ENOTDIR;

	if (pathargs < 2) {
		out->st_mode	= S_IFDIR | 0555;
		out->st_nlink	= 3; /* XXX: + subdirectory count */
	} else {
		out->st_mode	= S_IFREG | 0444;
		out->st_nlink	= 1;
	}
	out->st_uid	= getuid();
	out->st_gid	= getgid();
	out->st_atime	= out->st_mtime	= out->st_ctime	= timestamp;

	return 0;
}

int myReadDir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	const char *sid, *uid;
	const int pathargs = mySplitPath(path, &sid, &uid);
	myDB *db = fuse_get_context()->private_data;
	sqlite3_stmt *stmt;
	char *nextrow = NULL, *myrow = NULL;
	
	if (pathargs == 0)
		stmt = db->stmt[GET_SESSIONS];
	else {
		stmt = db->stmt[GET_UIDS];
		sqlite3_bind_text(stmt, 2, sid, -1, SQLITE_STATIC);
	}
	sqlite3_bind_int(stmt, 1, offset);

	fprintf(stderr, "myReadDir: path = %s, off = %d\n", path, offset);

	do {
		if (myrow)
			free(myrow);
		myrow = nextrow;
		nextrow = (sqlite3_step(stmt) == SQLITE_ROW ? strdup(sqlite3_column_text(stmt, 0)) : NULL);

		fprintf(stderr, "myReadDir-loop: myrow = %s, nextrow = %s, offset = %d\n", myrow, nextrow, offset);

		if (myrow && filler(buf, myrow, NULL, (nextrow ? ++offset : 0)) == 1)
				break;
	} while (nextrow);
	sqlite3_reset(stmt);

	if (myrow)
		free(myrow);

	return 0;
}

myBuffer_t *myBufferFind(myDB *db, const char *sid, const char *uid, off_t offset) {
	myBuffer_t *out, *prev;

	for (out = db->buffers, prev = NULL; out && (strcmp(out->sid, sid) || strcmp(out->uid, uid)); prev = out, out = out->next);

	if (!out) { /* create */
		out		= malloc(sizeof(myBuffer_t));
		out->sid	= strdup(sid);
		out->uid	= strdup(uid);
		out->data	= NULL;
		out->data_off	= 0;
		out->data_size	= 0;
		out->buf_size 	= 0;
		out->db_off	= 0;
		out->eof	= 0;
		out->next	= 0;
		if (prev)
			prev->next	= out;
		else
			db->buffers	= out;
	} else if (offset < out->data_off) { /* rewind */
		if (out->data) {
			free(out->data);
			out->data	= NULL;
		}
		out->data_off	= 0;
		out->data_size	= 0;
		out->buf_size 	= 0;
		out->db_off	= 0;
		out->eof	= 0;
	}

	return out;
}

/* shamelessly ripped from EKG2, XXX: rewrite to use static buf */
char *log_escape(const char *str)
{
	const char *p;
	char *res, *q;
	int size, needto = 0;

	if (!str)
		return NULL;
	
	for (p = str; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == ',')
			needto = 1;
	}

	if (!needto)
		return strdup(str);

	for (p = str, size = 0; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == '\\')
			size += 2;
		else
			size++;
	}

	q = res = malloc(size + 3);
	
	*q++ = '"';
	
	for (p = str; *p; p++, q++) {
		if (*p == '\\' || *p == '"' || *p == '\'') {
			*q++ = '\\';
			*q = *p;
		} else if (*p == '\n') {
			*q++ = '\\';
			*q = 'n';
		} else if (*p == '\r') {
			*q++ = '\\';
			*q = 'r';
		} else
			*q = *p;
	}
	*q++ = '"';
	*q = 0;

	return res;
}
/* /EKG2 */

void myBufferStep(sqlite3_stmt *stmt, myBuffer_t *buf) {
	if (buf->eof)
		return;

	buf->data_off	+= buf->data_size;
	buf->data_size	= 0;
	if (!buf->data) {
		buf->data	= malloc(BUF_START_SIZE);
		buf->buf_size	= BUF_START_SIZE;
	}

	fprintf(stderr, "myBufferStep: data_off = %d, buf_size = %d\n", buf->data_off, buf->buf_size);

	sqlite3_bind_int(stmt, 3, buf->db_off);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const int is_sent	= sqlite3_column_int(stmt, 1);
		const char *type	= sqlite3_column_text(stmt, 0);
		char *body		= log_escape(sqlite3_column_text(stmt, 6));
		char tsbuf[12];
		int r;

		if (!strcmp(type, "msg")) {
			if (is_sent)	type = "msgsend";
			else		type = "msgrecv";
		} else if (!strcmp(type, "system"))
			type = "msgsystem";
		else { /* "chat" */
			if (is_sent)	type = "chatsend";
			else		type = "chatrecv";
		}

		if (is_sent)
			tsbuf[0]	= 0;
		else {
			const int sts	= sqlite3_column_int(stmt, 5);
			r	= snprintf(tsbuf, sizeof(tsbuf), "%d,", (sts ? sts : sqlite3_column_int(stmt, 4)));
			if (r == -1 || r >= sizeof(tsbuf))
				tsbuf[0]	= 0; /* ignore second timestamp, if shit happens */
		}

		do {
			r	= snprintf(buf->data + buf->data_size, buf->buf_size - buf->data_size, "%s,%s,%s,%d,%s%s\n",
					type, sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
					sqlite3_column_int(stmt, 4), tsbuf, body);

			if (r >= (buf->buf_size - buf->data_size)) {
				buf->buf_size	= buf->data_size + r + 1;
				buf->data	= realloc(buf->data, buf->buf_size);
				r		= -1;
			} else if (r == -1) {
				buf->buf_size	= buf->data_size + BUF_START_SIZE;
				buf->data	= realloc(buf->data, buf->buf_size);
			} else
				buf->data_size += r;
		} while (r == -1);

		buf->db_off++;
		free(body);
	}
	sqlite3_reset(stmt);

	if (buf->data_size == 0) /* no data read */
		buf->eof	= 1;
}

int myReadFile(const char *path, char *out, size_t count, off_t offset, struct fuse_file_info *fi) {
	const char *sid, *uid;
	const int pathargs = mySplitPath(path, &sid, &uid);
	myDB *db = fuse_get_context()->private_data;
	sqlite3_stmt *stmt = db->stmt[GET_DATA];
	myBuffer_t *buf = myBufferFind(db, sid, uid, offset);
	int written = 0;

	sqlite3_bind_text(stmt, 1, sid, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, uid, -1, SQLITE_STATIC);

	fprintf(stderr, "myReadFile: path = %s, count = %d, offset = %d\n", path, count, offset);
	while (count > 0) {
		while (offset >= buf->data_off + buf->data_size && !buf->eof)
			myBufferStep(stmt, buf);
		if (buf->eof)
			break;

		const int toend		= buf->data_size - buf->data_off;
		const int towrite	= (count > toend ? toend : count);

		fprintf(stderr, "myReadFile-loop: count = %d, offset = %d, toend = %d, towrite = %d, written = %d\n", count, offset, toend, towrite, written);
		memcpy(out, buf->data + (offset - buf->data_off), towrite);
		
		out	+= towrite;
		count	-= towrite;
		offset	+= towrite;
		written	+= towrite;
	}

	return written;
};

static struct fuse_operations ops = {
	.getattr	= &myGetAttr,
	.readdir	= &myReadDir,
	.read		= &myReadFile
};

int main(int argc, char *argv[]) {
	myDB	db;

	if (argc < 3) {
		fprintf(stderr, "SYNOPSIS: %s database-path mount-point\n", argv[0]);

		return 1;
	}

	sqlite3_open(argv[1], &(db.db));
	{		/* make the db really open */
		char *err;
		if (sqlite3_exec(db.db, "SELECT * FROM log_msg LIMIT 1;", NULL, NULL, &err) != SQLITE_OK) {
			fprintf(stderr, "Unable to open the database: %s.\n", err);
			sqlite3_free(err);
			return 2;
		}
	}

	{
		const char **query;
		sqlite3_stmt **stmt;

		for (query = queries, stmt = db.stmt; *query; query++, stmt++) {
			if (sqlite3_prepare(db.db, *query, -1, stmt, NULL) != SQLITE_OK) {
				fprintf(stderr, "Unable to compile queries: %s [on %s].\n", sqlite3_errmsg(db.db), *query);
				return 3;
			}
		}
	}
	db.buffers = NULL;

	{
		int fuse_argc = 4;
		char *fuse_argv[] = {"fusermount", "-o", "direct_io,rw,debug", argv[2], NULL};

		fuse_main(fuse_argc, fuse_argv, &ops, &db);
	}
}
