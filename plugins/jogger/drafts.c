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

#define _POSIX_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include <limits.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#define JOGGER_KEYS_MAX 25
#define JOGGER_VALUES_MAX 14

	/* 10 char-long don't use ':', because they're already on limit (longer ones are discarded) */
const char *utf_jogger_header_keys[JOGGER_KEYS_MAX] = {
	"tytul:",	"temat:",	"subject:",	"tytuł:",		NULL,
	"poziom:",	"level:",						NULL, /* 2 */
	"tag:",									NULL, /* 3 */
	"kategoria:",	"category:",	"kategorie:",	"categories",		NULL, /* 4 */
	"trackback:",								NULL, /* 5 */
	"tidy",									NULL, /* 6 */
	"komentarze",	"comments:",						NULL, /* 7 */
	"miniblog:",								NULL, /* 8 - deprecated */
	NULL
};

const char *utf_jogger_header_values[JOGGER_VALUES_MAX] = {
	"off",		"no",		"nie",		"wylacz",	"wyłącz",
	"on",		"yes",		"tak",		"wlacz",	"włącz",	NULL,

	"jogger",									NULL,
	NULL
};

char *jogger_header_keys[JOGGER_KEYS_MAX];
char *jogger_header_values[JOGGER_VALUES_MAX];

void jogger_free_headers(int real_free) {
	int i;

	for (i = 0; i < JOGGER_KEYS_MAX; i++) {
		if (real_free)
			xfree(jogger_header_keys[i]);
		jogger_header_keys[i] = NULL;
	}
	for (i = 0; i < JOGGER_VALUES_MAX; i++) {
		if (real_free)
			xfree(jogger_header_values[i]);
		jogger_header_values[i] = NULL;
	}
}

void jogger_localize_headers(void *p) {
	int i;

	jogger_free_headers(1);
	for (i = 0; i < JOGGER_KEYS_MAX; i++) {
		char *s = ekg_convert_string_p(utf_jogger_header_keys[i], p);

		if (!s)
			s = xstrdup(utf_jogger_header_keys[i]);
		jogger_header_keys[i] = s;
	}
	for (i = 0; i < JOGGER_VALUES_MAX; i++) {
		char *s = ekg_convert_string_p(utf_jogger_header_values[i], p);

		if (!s)
			s = xstrdup(utf_jogger_header_values[i]);
		jogger_header_values[i] = s;
	}
}

/**
 * ekg_checkoutfile()
 *
 * Tries to open given file (check), and reads it, if expected (checkout).
 * It is designed to be proof to special file problems (especially named pipe ones).
 *
 * @param	fn	- filename to open.
 * @param	data	- pointer to store file contents or NULL, if don't want to read it.
 * @param	len	- pointer to store filelength or NULL, if not needed.
 * @param	hash	- pointer to store filehash or NULL, if not needed.
 *
 * @return	0 on success, errno on failure.
 */
static int ekg_checkoutfile(const char *fn, char **data, int *len, char **hash, const int maxlen, const int quiet) {
	static char jogger_hash[sizeof(int)*2+3];
	int mylen, fs, fd;

	if (!fn)
		return EINVAL;

	if ((fd = open(fn, O_RDONLY|O_NONBLOCK)) == -1) { /* we use O_NONBLOCK to get rid of FIFO problems */
		const int err = errno;
		if (err == ENXIO)
			printq("io_nonfile");
		else
			printq("io_cantopen");
		return err;
	}

	{
		struct stat st;

		if ((fstat(fd, &st) == -1) || !S_ISREG(st.st_mode)) {
			close(fd);
			printq("io_nonfile");
			return EISDIR; /* nearest, I think */
		}
		if ((fs = st.st_size) == 0) {
			close(fd);
			printq("io_emptyfile");
			return EINVAL; /* like mmap */
		} else if (maxlen && fs > maxlen) {
			close(fd);
			printq("io_toobig");
			return EFBIG;
		}
	}

	if (data || hash) {
		char *out = xmalloc(fs+1);
		void *p = out;
		int rem = fs, res = 1;

		{
			int cf = fcntl(fd, F_GETFL);

			if (cf == -1) /* evil thing */
				cf = 0;
			else
				cf &= ~O_NONBLOCK;
			fcntl(fd, F_SETFL, cf);
		}

		while ((res = read(fd, p, (rem <= SSIZE_MAX ? rem : SSIZE_MAX)))) {
			if (res == -1) {
				const int err = errno;
				if (err != EINTR && err != EAGAIN) {
					close(fd);
					printq("io_cantread");
					return err;
				}
			} else {
				p += res;
				rem -= res;
			}
		}

		mylen = xstrlen(out);
		{
			struct stat st;

			if (fstat(fd, &st) == -1)
				debug_error("ekg_openfile(): unable to fstat() again!\n");
			else if (st.st_size > fs)
				printq("io_expanded");
			else if (st.st_size < fs)
				printq("io_truncated");
			else if (rem > 0)
				printq("io_truncated_read");
			else if (fs > mylen)
				printq("io_binaryfile", itoa(mylen));
		}

		if (len)
			*len = fs-rem;

			/* I don't want to write my own hashing function, so using EKG2 one
			 * it will fail to hash data after any \0 in file, if there're any
			 * but we also aren't prepared to handle them */
		if (hash) {
			char sizecont[8];

			snprintf(sizecont, 8, "0x%%0%dx", sizeof(int)*2);
			snprintf(jogger_hash, sizeof(int)*2+3, sizecont, ekg_hash(out));
			*hash = jogger_hash;
		}
		if (data)
			*data = out;
		else
			xfree(out);
	} else if (len)
		*len = fs;
	close(fd);

	return 0;
}

#define WARN_PRINT(x) do { if (!outstarted) { outstarted++; print("jogger_warning"); } print(x, tmp); } while (0)

COMMAND(jogger_prepare) {
	const char *fn		= (params[0] ? params[0] : session_get(session, "entry_file"));
	int len;
	char *entry, *s, *hash;
	int seen		= 0;
	int outstarted		= 0;

	if (!fn) {
		printq("invalid_params", name);
		return -1;
	}

	if (ekg_checkoutfile(prepare_path_user(fn), &entry, NULL, &hash, 0, quiet))
		return -1;
	len = xstrlen(entry);
	s = entry;

	s += xstrspn(s, " \n\r");	/* get on to first real char */
	while (*s == '(') {	/* parse headers */
		const char *sep		= xstrchr(s, ':');
		const char *end		= xstrchr(s, ')');
		const char *next	= xstrchr(s, '\n');

		char tmp[24];		/* longest correct key has 10 chars + '(' + \0 */
		strlcpy(tmp, s, 20);
		xstrcpy(tmp+20, "..."); /* add ellipsis and \0 */
		xstrtr(tmp, '\n', 0);

		if (!sep || !end || !next || (sep > end) || (end+1+xstrspn(end+1, " ") != next)) {
			WARN_PRINT("jogger_warning_brokenheader");
			if (!next)
				s = entry+len;
			continue;
		} else if ((*(s+1) == ' ') || (*(sep-1) == ' '))
			WARN_PRINT("jogger_warning_wrong_key_spaces");
		else if (end-sep-1 <= xstrspn(sep+1, " "))
			WARN_PRINT("jogger_warning_wrong_value_empty");
		else {
			int i = 1;
			const char **p = (sep-s < 12 ? jogger_header_keys : NULL);

			for (; *p; i++, p++) { /* awaiting second NULL here */
				for (; *p; p++) { /* awaiting single NULL here */
					if (!xstrncasecmp(tmp+1, *p, xstrlen(*p))) {
						if (seen & (1<<i))
							WARN_PRINT("jogger_warning_duplicated_header");
						else
							seen |= (1<<i);
						break;
					}
				}
				if (*p)
					break;
			}

			if (!p || !*p)
				WARN_PRINT("jogger_warning_wrong_key");
			else if (i == 2) {
				char *lastn;
				
				if (strtol(sep+1, &lastn, 10) == 0 && lastn == sep+1)
					WARN_PRINT("jogger_warning_wrong_value_level");
			} else if (i == 3 || i == 4) {
				const char *firstcomma	= xstrchr(sep+1, ',');
				const char *firstspace	= xstrchr(sep+1, ' ');

				if ((!firstcomma || (firstcomma > end)) && firstspace && (firstspace < end))
					WARN_PRINT("jogger_warning_spacesep");
				else if (i == 4) {
					char *values		= xstrndup(sep+1, end-sep-1);
					if (cssfind(values, "techblog", ',', 1) && cssfind(values, "miniblog", ',', 1))
						WARN_PRINT("jogger_warning_miniblog_techblog");
					else if (cssfind(values, "techblog", 0, 1))
						WARN_PRINT("jogger_warning_techblog_only");
					xfree(values);
				}
			} else if (i == 5) {
				const char *first = sep+1+xstrcspn(sep+1, " ");
				if (xstrncmp(first, "http://", 7) && xstrncmp(first, "https://", 8)) /* XXX: https trackbacks? */
					WARN_PRINT("jogger_warning_malformed_url");
			} else if (i == 6 || i == 7) {
				const int jmax = i-5;
				int j = 1;
				char *myval = xstrndup(sep+1, end-sep-1);
				const char **q = jogger_header_values;

				for (; *q && j <= jmax; j++, q++) { /* second NULL or jmax */
					for (; *q; q++) { /* first NULL */
						if (!xstrcasecmp(myval, *q))
							break;
					}
					if (*q)
						break;
				}

				if (!*q || (j > jmax)) {
					char *endval;
					int n = strtol(myval, &endval, 10);

					if (n == 0 && endval == myval) {
						if (*myval == ' ' || *(myval+xstrlen(myval)-1) == ' ')
							WARN_PRINT("jogger_warning_wrong_value_spaces");
						else
							WARN_PRINT("jogger_warning_wrong_value");
					}
					if (n > jmax)
						WARN_PRINT("jogger_warning_wrong_value");
				}
				xfree(myval);
			} else if (i == 8)
				WARN_PRINT("jogger_warning_deprecated_miniblog");
		}

		s = next+1;
	}

	s += xstrspn(s, " \n\r");	/* get on to first real char (again) */
	if (*s == '(') {
		char tmp[14];
		strlcpy(tmp, s, 10);
		xstrcpy(tmp+10, "...");
		xstrtr(tmp, '\n', 0);
		WARN_PRINT("jogger_warning_mislocated_header");
	}
	if (!xstrstr(s, "<EXCERPT>") && (len - (s-entry) > 4096)) {
		char tmp[21];
		strlcpy(tmp, s+4086, 20);
		tmp[20] = 0;
		xstrtr(tmp, '\n', ' '); /* sanitize */
		WARN_PRINT("jogger_warning_noexcerpt");
	}

	xfree(entry);
	if (params[0])
		session_set(session, "entry_file", params[0]);
	session_set(session, "entry_hash", hash);
	printq("jogger_prepared", fn);
	return 0;
}

COMMAND(jogger_publish) {
	const char *fn = (params[0] ? params[0] : session_get(session, "entry_file"));
	const char *oldhash = (!xstrcmp(session_get(session, "entry_file"), fn) ? session_get(session, "entry_hash") : NULL);
	char *entry, *hash;

	if (!fn) {
		printq("jogger_notprepared");
		return -1;
	}

	if (ekg_checkoutfile(prepare_path_user(fn), &entry, NULL, &hash, 0, quiet))
		return -1;
	if (oldhash && xstrcmp(oldhash, hash)) {
		print("jogger_hashdiffers");
		xfree(entry);
		session_set(session, "entry_hash", hash);
		return -1;
	}

	command_exec("jogger:", session, entry, 0);

	xfree(entry);
	if (!oldhash) {
		session_set(session, "entry_hash", hash);
		session_set(session, "entry_file", fn);
	}
	return 0;
}
