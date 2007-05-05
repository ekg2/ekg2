/*
 *  (C) Copyright 2007	Michał Górny & EKG2 authors
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>		/* mmap */
#include <sys/types.h>
#include <sys/stat.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/themes.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

COMMAND(jogger_prepare) {
		/* XXX, check file and contents */

	session_set(session, "entry_file", params[0]);
	printq("jogger_prepared", params[0]);
	return 0;
}

COMMAND(jogger_publish) {
	const char *fn = (params[0] ? params[0] : session_get(session, "entry_file"));
	int fd, fs;
	char *entry;

	if (!fn) {
		printq("jogger_notprepared");
		return -1;
	}

	if ((fd = open(fn, O_RDONLY|O_NONBLOCK)) == -1) { /* we use O_NONBLOCK to get rid of FIFO problems */
		if (errno == ENXIO)
			printq("jogger_nonfile");
		else
			printq("jogger_cantopen");
		return -1;
	}

	{
		struct stat st;

		if ((fstat(fd, &st) == -1) || !S_ISREG(st.st_mode)) {
			close(fd);
			printq("jogger_nonfile");
			return -1;
		}
		fs = st.st_size;
	}

	if ((entry = mmap(NULL, fs, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		close(fd);
		printq("jogger_cantread");
		return -1;
	}

	command_exec("jogger:", session, entry, 0);

	munmap(entry, fs);
	close(fd);
	session_set(session, "entry_file");	/* XXX: reset always or only if using it? */
	return 0;
}
