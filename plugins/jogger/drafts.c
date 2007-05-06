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
#include <stdlib.h>
#include <unistd.h>

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
	"tag:",									NULL,
	"poziom:",	"level:",						NULL,
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
 * jogger_openfile()
 *
 * Opens given file and mmaps it.
 *
 * @param	fn	- filename to open.
 * @param	fd	- pointer to store fd.
 * @param	fs	- pointer to store filesize.
 * @param	hash	- pointer to store filehash or NULL, if not needed.
 *
 * @return	Pointer to file contents or NULL on failure.
 *
 * @sa	jogger_closefile	- close such opened file.
 */
static char *jogger_openfile(const char *fn, int *fd, int *fs, char **hash) {
	static char jogger_hash[sizeof(int)*2+3];
	char *out;

	if (!fn || !fd || !fs)
		return NULL;

	if ((*fd = open(fn, O_RDONLY|O_NONBLOCK)) == -1) { /* we use O_NONBLOCK to get rid of FIFO problems */
		if (errno == ENXIO)
			print("jogger_nonfile");
		else
			print("jogger_cantopen");
		return NULL;
	}

	{
		struct stat st;

		if ((fstat(*fd, &st) == -1) || !S_ISREG(st.st_mode)) {
			close(*fd);
			print("jogger_nonfile");
			return NULL;
		}
		if ((*fs = st.st_size) == 0) {
			close(*fd);
			print("jogger_emptyfile");
			return NULL;
		}
	}

	if ((out = mmap(NULL, *fs, PROT_READ, MAP_PRIVATE, *fd, 0)) == MAP_FAILED) {
		close(*fd);
		print("jogger_cantread");
		return NULL;
	}

		/* I don't want to write my own hashing function, so using EKG2 one
		 * it will fail to hash data after any \0 in file, if there're any
		 * but we also aren't prepared to handle them */
	if (hash) {
		char sizecont[8];

		snprintf(sizecont, 8, "0x%%0%dx", sizeof(int)*2);
		snprintf(jogger_hash, sizeof(int)*2+3, sizecont, ekg_hash(out));
		*hash = jogger_hash;
	}

	return out;
}

/**
 * jogger_closefile()
 *
 * Closes file opened by jogger_openfile().
 *
 * @param	fd	- returned fd.
 * @param	data	- returned data.
 * @param	fs	- returned filesize.
 *
 * @sa	jogger_openfile	- open and mmap file.
 */
static void jogger_closefile(int fd, char *data, int fs) {
	munmap(data, fs);
	close(fd);
}

COMMAND(jogger_prepare) {
	int fd, fs;
	char *entry, *s, *hash;
	int seen = 0;

	if (!(entry = jogger_openfile(prepare_path_user(params[0]), &fd, &fs, &hash)))
		return -1;
	s = entry;

	s += xstrspn(s, " \n\r");	/* get on to first real char */
	while (*s == '(') {	/* parse headers */
		const char *sep		= xstrchr(s, ':');
		const char *end		= xstrchr(s, ')');
		const char *next	= xstrchr(s, '\n');

		char tmp[24];		/* longest correct key has 10 chars + '(' + \0 */
		xstrncpy(tmp, s, 20);
		xstrcpy(tmp+20, "..."); /* add ellipsis and \0 */
		xstrtr(tmp, '\n', 0);

		if (!sep || !end || !next || (sep > end) || (end+1+xstrspn(end+1, " ") != next)) {
			print("jogger_warning_brokenheader", tmp);
			if (!next)
				s = entry+fs;
		} else if ((*(s+1) == ' ') || (*(sep-1) == ' '))
			print("jogger_warning_wrong_key_spaces", tmp);
		else {
			int i = 1;
			const char **p = (sep-s < 12 ? jogger_header_keys : NULL);

			for (; *p; i++, p++) { /* awaiting second NULL here */
				for (; *p; p++) { /* awaiting single NULL here */
					if (!xstrncasecmp(tmp+1, *p, xstrlen(*p))) {
						if (seen & (1<<i))
							print("jogger_warning_duplicated_header", tmp);
						else
							seen |= (1<<i);
						break;
					}
				}
				if (*p)
					break;
			}

			if (!p || !*p)
				print("jogger_warning_wrong_key", tmp);
			else if (i == 4) {
				char *values = xstrndup(sep+1, end-sep-1);
				if (cssfind(values, "techblog", ',', 1) && cssfind(values, "miniblog", ',', 1))
					print("jogger_warning_miniblog_techblog", tmp);
				xfree(values);
			} else if (i == 5) {
				const char *first = sep+1+xstrcspn(sep+1, " ");
				if (xstrncmp(first, "http://", 7) && xstrncmp(first, "https://", 8)) /* XXX: https trackbacks? */
					print("jogger_warning_malformed_url", tmp);
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
							print("jogger_warning_wrong_value_spaces", tmp);
						else
							print("jogger_warning_wrong_value", tmp);
					}
					if (n > jmax)
						print("jogger_warning_wrong_value", tmp);
				}
				xfree(myval);
			} else if (i == 8)
				print("jogger_warning_deprecated_miniblog", tmp);
		}

		s = next+1;
	}

	s += xstrspn(s, " \n\r");	/* get on to first real char (again) */
	if (*s == '(') {
		char tmp[14];
		xstrncpy(tmp, s, 10);
		xstrcpy(tmp+10, "...");
		xstrtr(tmp, '\n', 0);
		print("jogger_warning_mislocated_header", tmp);
	}
	if (!xstrstr(s, "<EXCERPT>") && (fs - (s-entry) > 4096)) {
		char tmp[21];
		xstrncpy(tmp, s+4086, 20);
		tmp[20] = 0;
		xstrtr(tmp, '\n', ' '); /* sanitize */
		print("jogger_warning_noexcerpt", tmp);
	}

	jogger_closefile(fd, entry, fs);
	session_set(session, "entry_file", params[0]);
	session_set(session, "entry_hash", hash);
	printq("jogger_prepared", params[0]);
	return 0;
}

COMMAND(jogger_publish) {
	const char *fn = (params[0] ? params[0] : session_get(session, "entry_file"));
	const char *oldhash = (!xstrcmp(session_get(session, "entry_file"), fn) ? session_get(session, "entry_hash") : NULL);
	int fd, fs;
	char *entry, *hash;

	if (!fn) {
		printq("jogger_notprepared");
		return -1;
	}

	if (!(entry = jogger_openfile(prepare_path_user(fn), &fd, &fs, (oldhash ? &hash : NULL))))
		return -1;
	if (oldhash && xstrcmp(oldhash, hash)) {
		print("jogger_hashdiffers");
		jogger_closefile(fd, entry, fs);
		session_set(session, "entry_hash", hash);
		return -1;
	}

	command_exec("jogger:", session, entry, 0);

	jogger_closefile(fd, entry, fs);
	session_set(session, "entry_file", NULL);	/* XXX: reset always or only if using it? */
	return 0;
}
