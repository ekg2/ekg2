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

#include "debug.h"
#include "dynstuff.h"
#include "commands.h"
#include "msgqueue.h"
#include "protocol.h"
#include "sessions.h"
#include "stuff.h"
#include "xmalloc.h"

msg_queue_t *msg_queue = NULL;

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
int msg_queue_add(const char *session, const char *rcpts, const char *message, const char *seq, msgclass_t class)
{
	msg_queue_t *m = xmalloc(sizeof(msg_queue_t));

	m->session	= xstrdup(session);
	m->rcpts	= xstrdup(rcpts);
	m->message 	= xstrdup(message);
	m->seq 		= xstrdup(seq);
	m->time 	= time(NULL);
	m->class	= class;

	return (LIST_ADD2(&msg_queue, m) ? 0 : -1);
}

static LIST_FREE_ITEM(list_msg_queue_free, msg_queue_t *) {
	xfree(data->session);
	xfree(data->rcpts);
	xfree(data->message);
	xfree(data->seq);
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
	msg_queue_t *m;
	int res = -1;

	for (m = msg_queue; m; ) {
		msg_queue_t *next = m->next;

		if (!xstrcasecmp(m->rcpts, uid)) {
			LIST_REMOVE2(&msg_queue, m, list_msg_queue_free);
			res = 0;
		}

		m = next;
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
	msg_queue_t *m;

	if (!seq) 
		return -1;

	for (m = msg_queue; m; ) {
		msg_queue_t *next = m->next;

		if (!xstrcasecmp(m->seq, seq)) {
			LIST_REMOVE2(&msg_queue, m, list_msg_queue_free);
			res = 0;
		}

		m = next;
	}

	return res;
}

/*
 * msg_queue_free()
 *
 * zwalnia pamiêæ po kolejce wiadomo¶ci.
 */
void msg_queue_free() {
	LIST_DESTROY2(msg_queue, list_msg_queue_free);
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
	msg_queue_t *m;
	int ret = -1;

	if (!msg_queue)
		return -2;

	for (m = msg_queue; m; m = m->next)
		m->mark = 1;

	for (m = msg_queue; m;) {
		session_t *s;
		msg_queue_t *next = m->next;
		char *cmd = "/msg \"%s\" %s";

		/* czy wiadomo¶æ dodano w trakcie opró¿niania kolejki? */
		if (!m->mark)
			continue;

		if (session && xstrcmp(m->session, session)) 
			continue;
				/* wiadomo¶æ wysy³ana z nieistniej±cej ju¿ sesji? usuwamy. */
		else if (!(s = session_find(m->session))) {
			LIST_REMOVE2(&msg_queue, m, list_msg_queue_free);
			continue;
		}

		switch (m->class) {
			case EKG_MSGCLASS_SENT_CHAT:
				cmd = "/chat \"%s\" %s";
				break;
			case EKG_MSGCLASS_SENT:
				break;
			default:
				debug_error("msg_queue_flush(), unsupported message class in query: %d\n", m->class);
		}
		command_exec_format(NULL, s, 1, cmd, m->rcpts, m->message);

		LIST_REMOVE2(&msg_queue, m, list_msg_queue_free);

		ret = 0;
		m = next;
	}

	return ret;
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
	msg_queue_t *m;
	int count = 0;

	for (m = msg_queue; m; m = m->next) {
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
	msg_queue_t *m;
	int num = 0;

	if (!msg_queue)
		return -1;

	if (mkdir_recursive(prepare_pathf("queue"), 1))		/* create ~/.ekg2/[PROFILE/]queue/ */
		return -1;

	for (m = msg_queue; m; m = m->next) {
		const char *fn;
		FILE *f;

		if (!(fn = prepare_pathf("queue/%ld.%d", (long) m->time, num++)))	/* prepare_pathf() ~/.ekg2/[PROFILE/]queue/TIME.UNIQID */
			continue;

		if (!(f = fopen(fn, "w")))
			continue;

		chmod(fn, 0600);
		fprintf(f, "v2\n%s\n%s\n%ld\n%s\n%d\n%s", m->session, m->rcpts, m->time, m->seq, m->class, m->message);
		fclose(f);
	}

	return 0;
}

/**
 * msg_queue_read()
 *
 * Read msgqueue of not sended messages.<br>
 * msgqueue is subdir ("queue") in ekg2 config directory.
 *
 * @todo	return count of readed messages?
 *
 * @todo	code which handle errors is awful and it need rewriting.
 *
 * @return	-1 if fail to open msgqueue directory<br>
 * 		 0 on success.
 */

int msg_queue_read() {
	struct dirent *d;
	DIR *dir;

	if (!(dir = opendir(prepare_pathf("queue"))))		/* opendir() ~/.ekg2/[PROFILE/]/queue */
		return -1;

	while ((d = readdir(dir))) {
		const char *fn;

		msg_queue_t m;
		struct stat st;
		string_t msg;
		char *buf;
		FILE *f;
		int filever = 0;

		if (!(fn = prepare_pathf("queue/%s", d->d_name)))
			continue;

		if (stat(fn, &st) || !S_ISREG(st.st_mode))
			continue;

		if (!(f = fopen(fn, "r")))
			continue;

		memset(&m, 0, sizeof(m));

		buf = read_file(f, 0);
		
		if (buf && *buf == 'v')
			filever = atoi(buf+1);
		if (!filever || filever > 2) {
			fclose(f);
			continue;
		}

		if (!(m.session = read_file(f, 1))) {
			fclose(f);
			continue;
		}
	
		if (!(m.rcpts = read_file(f, 1))) {
			xfree(m.session);
			fclose(f);
			continue;
		}

		if (!(buf = read_file(f, 0))) {
			xfree(m.session);
			xfree(m.rcpts);
			fclose(f);
			continue;
		}

		m.time = atoi(buf);

		if (!(m.seq = read_file(f, 1))) {
			xfree(m.session);
			xfree(m.rcpts);
			fclose(f);
			continue;
		}
	
		if (filever == 2) {
			if (!(buf = read_file(f, 0))) {
				xfree(m.session);
				xfree(m.rcpts);
				fclose(f);
				continue;
			}

			m.class = atoi(buf);
		} else
			m.class = EKG_MSGCLASS_SENT;

		msg = string_init(NULL);

		buf = read_file(f, 0);

		while (buf) {
			string_append(msg, buf);
			buf = read_file(f, 0);
			if (buf)
				string_append(msg, "\r\n");
		}

		m.message = string_free(msg, 0);

		LIST_ADD2(&msg_queue, xmemdup(&m, sizeof(m)));

		fclose(f);
		unlink(fn);
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
