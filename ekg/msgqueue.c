/* $Id$ */

/*
 *  (C) Copyright 2001-2002 Piotr Domagalski <szalik@szalik.net>
 *                          Wojtek Kaniewski <wojtekka@irc.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dynstuff.h"
#include "commands.h"
#include "msgqueue.h"
#include "sessions.h"
#include "stuff.h"
#include "xmalloc.h"

list_t msg_queue = NULL;

/*
 * msg_queue_add()
 *
 * dodaje wiadomo¶æ do kolejki wiadomo¶ci.
 * 
 *  - session - sesja, z której wysy³ano
 *  - rcpts - lista odbiorców
 *  - message - tre¶æ wiadomo¶ci
 *  - seq - numer sekwencyjny
 *
 * 0/-1
 */
int msg_queue_add(const char *session, const char *rcpts, const char *message, const char *seq)
{
	msg_queue_t *m = xmalloc(sizeof(msg_queue_t));

	m->session	= xstrdup(session);
	m->rcpts	= xstrdup(rcpts);
	m->message 	= xstrdup(message);
	m->seq 		= xstrdup(seq);
	m->time 	= time(NULL);

	return (list_add(&msg_queue, m, 0) ? 0 : -1);
}

/*
 * msg_queue_remove()
 *
 * usuwa wiadomo¶æ o podanym numerze sekwencyjnym z kolejki wiadomo¶ci.
 *
 *  - msg - struktura opisuj±ca wiadomo¶æ.
 *
 * 0 je¶li usuniêto, -1 je¶li nie ma takiej wiadomo¶ci.
 */
static void msg_queue_remove(msg_queue_t *m)
{
	xfree(m->session);
	xfree(m->rcpts);
	xfree(m->message);
	xfree(m->seq);

	list_remove(&msg_queue, m, 1);
}

/*
 * msg_queue_remove_uid()
 *
 * usuwa wiadomo¶æ z kolejki wiadomo¶ci dla danego
 * u¿ytkownika.
 *
 *  - uin.
 *
 * 0 je¶li usuniêto, -1 je¶li nie ma takiej wiadomo¶ci.
 */
int msg_queue_remove_uid(const char *uid)
{
	list_t l;
	int res = -1;

	for (l = msg_queue; l; ) {
		msg_queue_t *m = l->data;

		l = l->next;

		if (!xstrcasecmp(m->rcpts, uid)) {
			msg_queue_remove(m);
			res = 0;
		}
	}

	return res;
}

/*
 * msg_queue_remove_seq()
 *
 * usuwa wiadomo¶æ z kolejki wiadomo¶æ o podanym numerze sekwencyjnym.
 *
 *  - seq
 *
 * 0/-1
 */
int msg_queue_remove_seq(const char *seq)
{
	int res = -1;
	list_t l;

	if (!seq) 
		return -1;

	for (l = msg_queue; l; ) {
		msg_queue_t *m = l->data;

		l = l->next;

		if (!xstrcasecmp(m->seq, seq)) {
			msg_queue_remove(m);
			res = 0;
		}
	}

	return res;
}

/*
 * msg_queue_free()
 *
 * zwalnia pamiêæ po kolejce wiadomo¶ci.
 */
void msg_queue_free()
{
	list_t l;

	for (l = msg_queue; l; ) {
		msg_queue_t *m = l->data;

		l = l->next;

		msg_queue_remove(m);
	}

	list_destroy(msg_queue, 1);
	msg_queue = NULL;
}

/*
 * msg_queue_flush()
 *
 * wysy³a wiadomo¶ci z kolejki.
 *
 * 0 je¶li wys³ano, -1 je¶li nast±pi³ b³±d przy wysy³aniu, -2 je¶li
 * kolejka pusta.
 */
int msg_queue_flush(const char *session)
{
	list_t l;
	int sent = 0;

	if (!msg_queue)
		return -2;

	for (l = msg_queue; l; l = l->next) {
		msg_queue_t *m = l->data;

		m->mark = 1;
	}

	for (l = msg_queue; l;) {
		msg_queue_t *m = l->data;
		session_t *s;

		l = l->next;

		/* czy wiadomo¶æ dodano w trakcie opró¿niania kolejki? */
		if (!m->mark)
			continue;

		/* wiadomo¶æ wysy³ana z nieistniej±cej ju¿ sesji? usuwamy. */
		if (!(s = session_find(m->session))) {
			msg_queue_remove(m);
			continue;
		}

		if (session && xstrcmp(m->session, session)) 
			continue;

		command_exec_format(NULL, s, 1, ("/msg \"%s\" %s"), m->rcpts, m->message);

		msg_queue_remove(m);

		sent = 1;
	}

	return (sent) ? 0 : -1;
}

/*
 * msg_queue_count_session()
 *
 * zwraca liczbê wiadomo¶ci w kolejce dla danej sesji.
 *
 * - uin.
 */
int msg_queue_count_session(const char *uid)
{
	list_t l;
	int count = 0;

	for (l = msg_queue; l; l = l->next) {
		msg_queue_t *m = l->data;

		if (!xstrcasecmp(m->session, uid))
			count++;
	}

	return count;
}

/*
 * msg_queue_write()
 *
 * zapisuje niedostarczone wiadomo¶ci na dysku.
 *
 * 0/-1
 */
int msg_queue_write()
{
	const char *path;
	list_t l;
	int num = 0;

	if (!msg_queue)
		return -1;

	path = prepare_path("queue", 1);
#ifndef NO_POSIX_SYSTEM
	if (mkdir(path, 0700) && errno != EEXIST)
#else
	if (mkdir(path) && errno != EEXIST) 
#endif
		return -1;
	for (l = msg_queue; l; l = l->next) {
		msg_queue_t *m = l->data;
		char *fn;
		FILE *f;

		fn = saprintf("%s/%ld.%d", path, (long) m->time, num++);

		if (!(f = fopen(fn, "w"))) {
			xfree(fn);
			continue;
		}

		chmod(fn, 0600);
		xfree(fn);

		fprintf(f, "v1\n%s\n%s\n%ld\n%s\n%s", m->session, m->rcpts, m->time, m->seq, m->message);

		fclose(f);
	}

	return 0;
}

/*
 * msg_queue_read()
 *
 * wczytuje kolejkê niewys³anych wiadomo¶ci z dysku.
 *
 * 0/-1
 */
int msg_queue_read()
{
	const char *path;
	struct dirent *d;
	DIR *dir;

	path = prepare_path("queue", 0);

	if (!(dir = opendir(path)))
		return -1;

	while ((d = readdir(dir))) {
		msg_queue_t m;
		struct stat st;
		string_t msg;
		char *fn, *buf;
		FILE *f;

		fn = saprintf("%s/%s", path, d->d_name);
		
		if (stat(fn, &st) || !S_ISREG(st.st_mode)) {
			xfree(fn);
			continue;
		}

		if (!(f = fopen(fn, "r"))) {
			xfree(fn);
			continue;
		}

		memset(&m, 0, sizeof(m));

		buf = read_file(f);

		if (!buf || xstrcmp(buf, "v1")) {
			xfree(buf);
			fclose(f);
			continue;
		}
		xfree(buf);

		if (!(m.session = read_file(f))) {
			fclose(f);
			continue;
		}
	
		if (!(m.rcpts = read_file(f))) {
			xfree(m.session);
			fclose(f);
			continue;
		}

		if (!(buf = read_file(f))) {
			xfree(m.session);
			xfree(m.rcpts);
			fclose(f);
			continue;
		}

		m.time = atoi(buf);
		xfree(buf);

		if (!(m.seq = read_file(f))) {
			xfree(m.session);
			xfree(m.rcpts);
			fclose(f);
			continue;
		}
		
		msg = string_init(NULL);

		buf = read_file(f);

		while (buf) {
			string_append(msg, buf);
			xfree(buf);
			buf = read_file(f);
			if (buf)
				string_append(msg, "\r\n");
		}

		m.message = string_free(msg, 0);

		list_add(&msg_queue, &m, sizeof(m));

		fclose(f);
		unlink(fn);
		xfree(fn);
	}

	closedir(dir);

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
