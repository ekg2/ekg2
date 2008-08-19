/* $Id$ */

/*
 *  (C) Copyright 2003-2005 Tomasz Torcz <zdzichu@irc.pl>
 *			    Leszek Krupiñski <leafnode@wafel.com>
 *			    Adam Kuczyñski <dredzik@ekg2.org>
 *			    Adam Mikuta <adamm@ekg2.org>
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
#include <ekg/win32.h>

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#if defined(__MINGW32__) || defined(__FreeBSD__) || defined(__sun)
#include <limits.h>
#endif

#include <stdint.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/log.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h> //print()
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include <ekg/queries.h>

#include <sys/stat.h>
#include <sys/types.h>
#ifndef NO_POSIX_SYSTEM
#include <sys/mman.h>
#include <arpa/inet.h>
#endif

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifndef HAVE_STRLCAT
#  include "compat/strlcat.h"
#endif
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include "main.h"

#undef HAVE_ZLIB		/* disable zlib fjuczer */

PLUGIN_DEFINE(logs, PLUGIN_LOG, NULL);

#ifdef EKG2_WIN32_SHARED_LIB
	EKG2_WIN32_SHARED_LIB_HELPER
#endif

static struct buffer_info buffer_lograw = { NULL, 0, 0 };

static logs_log_t *log_curlog = NULL;

/*
 * log_escape()
 *
 * je¶li trzeba, eskejpuje tekst do logów.
 * 
 *  - str - tekst.
 *
 * zaalokowany bufor.
 */
static char *log_escape(const char *str)
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
		return xstrdup(str);

	for (p = str, size = 0; *p; p++) {
		if (*p == '"' || *p == '\'' || *p == '\r' || *p == '\n' || *p == '\\')
			size += 2;
		else
			size++;
	}

	q = res = xmalloc(size + 3);
	
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

static char *fstring_reverse(fstring_t *fstr) {
	const char *str;
	const short *attr;
	string_t asc;
	int i;

	if (!fstr)
		return NULL;

	attr = fstr->attr;
	str = fstr->str.b;

	if (!attr || !str)
		return NULL;

	asc = string_init(NULL);

	for (i = 0; str[i]; i++) {
#define prev	attr[i-1]
#define cur	attr[i] 
		int reset = 0;

		if (i) {
			if (!(cur & FSTR_BOLD) && (prev & FSTR_BOLD))		reset = 1;
			if (!(cur & FSTR_BLINK) && (prev & FSTR_BLINK))		reset = 1;
			if (!(cur & FSTR_UNDERLINE) && (prev & FSTR_UNDERLINE))	reset = 1;
			if (!(cur & FSTR_REVERSE) && (prev & FSTR_REVERSE))	reset = 1;
			if ((cur & FSTR_NORMAL) && !(prev & FSTR_NORMAL))	reset = 1;	/* colors disappear */

			if (reset) 
				string_append(asc, "%n");
		} else
			reset = 1;

	/* attr */
		if ((cur & FSTR_BLINK) &&	(reset || !(prev & FSTR_BLINK)))	string_append(asc, "%i");
//		if ((cur & FSTR_UNDERLINE) &&	(reset || !(prev & FSTR_UNDERLINE)))	string_append(asc, "%");
//		if ((cur & FSTR_REVERSE) &&	(reset || !(prev & FSTR_REVERSE)))	string_append(asc, "%");

		if (!(cur & FSTR_NORMAL)) {
		/* background color XXX */
#define BGCOLOR(x)	-1
			if (0 && ((reset || BGCOLOR(cur) != BGCOLOR(prev)))) {
				string_append_c(asc, '%');
				switch (BGCOLOR(cur)) {
					case (0): string_append_c(asc, 'l'); break;
					case (1): string_append_c(asc, 's'); break;
					case (2): string_append_c(asc, 'h'); break;
					case (3): string_append_c(asc, 'z'); break;
					case (4): string_append_c(asc, 'e'); break;
					case (5): string_append_c(asc, 'q'); break;
					case (6): string_append_c(asc, 'd'); break;
					case (7): string_append_c(asc, 'x'); break;
				}
			}
#undef BGCOLOR

		/* foreground color */
#define FGCOLOR(x)	((!(x & FSTR_NORMAL)) ? (x & FSTR_FOREMASK) : -1)
			if (((reset || FGCOLOR(cur) != FGCOLOR(prev)) || (i && (prev & FSTR_BOLD) != (cur & FSTR_BOLD)))) {
				string_append_c(asc, '%');
				switch ((cur & FSTR_FOREMASK)) {
					case (0): string_append_c(asc, (cur & FSTR_BOLD) ? 'K' : 'k'); break;
					case (1): string_append_c(asc, (cur & FSTR_BOLD) ? 'R' : 'r'); break;
					case (2): string_append_c(asc, (cur & FSTR_BOLD) ? 'G' : 'g'); break;
					case (3): string_append_c(asc, (cur & FSTR_BOLD) ? 'Y' : 'y'); break;
					case (4): string_append_c(asc, (cur & FSTR_BOLD) ? 'B' : 'b'); break;
					case (5): string_append_c(asc, (cur & FSTR_BOLD) ? 'M' : 'm'); break; /* | fioletowy	 | %m/%p  | %M/%P | %q	| */
					case (6): string_append_c(asc, (cur & FSTR_BOLD) ? 'C' : 'c'); break;
					case (7): string_append_c(asc, (cur & FSTR_BOLD) ? 'W' : 'w'); break;
				}
			}
#undef FGCOLOR
		} else {	/* no color */
			if ((cur & FSTR_BOLD) && (reset || !(prev & FSTR_BOLD)))
				string_append(asc, "%T");
		}

	/* str */
		if (str[i] == '%' || str[i] == '\\') 
			string_append_c(asc, '\\');
		string_append_c(asc, str[i]);
	}

/* reset, and return. */
	string_append(asc, "%n");
	return string_free(asc, 0);

#undef prev
#undef cur
}

/* 
 * zwraca irssi lub simple lub xml lub NULL
 * w zaleznosci od ustawien log_format w sesji i log:logs 
 */

static int logs_log_format(session_t *s) {
	const char *log_formats;

	if (config_logs_log == LOG_FORMAT_NONE)
		return LOG_FORMAT_NONE;

	if (!s || !(log_formats = session_get(s, "log_formats")))
		return LOG_FORMAT_NONE;

	if (xstrstr(log_formats, "irssi")) 
		return LOG_FORMAT_IRSSI;
	if (config_logs_log == LOG_FORMAT_SIMPLE && xstrstr(log_formats, "simple"))
		return LOG_FORMAT_SIMPLE;
	if (config_logs_log == LOG_FORMAT_XML && xstrstr(log_formats, "xml"))
		return LOG_FORMAT_XML;

	return LOG_FORMAT_NONE;
}

/* zwraca 1 lub  2 lub 3 jesli cos sie zmienilo. (log_format, sciezka, t) i zmienia path w log_window_t i jak cos zamyka plik / otwiera na nowo.
 *	  0 jesli nie.
 *	  -1 jesli cos sie zjebalo.
 */

static int logs_window_check(logs_log_t *ll, time_t t) {
	session_t *s;
	log_window_t *l = ll->lw;
	int chan = 0;
	int tmp;

	if (!l || !(s = session_find(ll->session)))
		return -1;	

	if (l->logformat != (tmp = logs_log_format(s))) {
		l->logformat = tmp;
		chan = 1;
	}
	if (!(l->path)) {
		chan = 2;
	} else {
		int datechanged = 0; /* bitmaska 0x01 (dzien) 0x02 (miesiac) 0x04 (rok) */
		struct tm *tm	= xmemdup(localtime(&(ll->t)), sizeof(struct tm));
		struct tm *tm2	= localtime(&t);

		/* sprawdzic czy dane z (tm == tm2) */
		if (tm->tm_mday != tm2->tm_mday)	datechanged |= 0x01;
		if (tm->tm_mon != tm2->tm_mon)		datechanged |= 0x02;
		if (tm->tm_year != tm2->tm_year)	datechanged |= 0x04;
		if (
				((datechanged & 0x04) && xstrstr(config_logs_path, "%Y")) ||
				((datechanged & 0x02) && xstrstr(config_logs_path, "%M")) ||
				((datechanged & 0x01) && xstrstr(config_logs_path, "%D"))
		   )
			chan = 3;
		/* zalogowac jak sie zmienila data */
		if (datechanged && l->logformat == LOG_FORMAT_IRSSI) { /* yes i know it's wrong place for doing this but .... */
			if (!(l->file)) 
				l->file = logs_open_file(l->path, LOG_FORMAT_IRSSI);
			logs_irssi(l->file, ll->session, NULL,
					prepare_timestamp_format(IRSSI_LOG_DAY_CHANGED, time(NULL)),
					0, EKG_MSGCLASS_SYSTEM);
		}
		xfree(tm);
	}
	ll->t = t;

	if (chan > 1) {
		char *tmp = l->path;

		l->path = logs_prepare_path(s, config_logs_path, ll->uid, t);
		debug("[logs] logs_window_check chan = %d oldpath = %s newpath = %s\n", chan, __(tmp), __(l->path));
#if 0 /* TODO: nie moze byc - bo mogl logformat sie zmienic... */
		if (chan != 3 && !xstrcmp(tmp, l->path))  /* jesli sciezka sie nie zmienila to nie otwieraj na nowo pliku */
			chan = -chan; 
#endif
		xfree(tmp);
	}
	if (chan > 0) {
		if (l->file) { /* jesli plik byl otwarty otwieramy go z nowa sciezka */
			fclose(l->file); l->file = NULL;
			l->file = logs_open_file(l->path, l->logformat);
		} 
	}	
	return chan;
}

static FILE *logs_window_close(logs_log_t *l, int close);	/* forward */

static logs_log_t *logs_log_find(const char *session, const char *uid, int create) {
	list_t l;
	logs_log_t *temp = NULL;

	if (log_curlog && !xstrcmp(log_curlog->session, session) && !xstrcmp(log_curlog->uid, uid)) {
		if (log_curlog->lw) 
			logs_window_check(log_curlog, time(NULL)); /* tutaj ? */
		return log_curlog->lw ? log_curlog : logs_log_new(log_curlog, session, uid);
	}

	for (l=log_logs; l; l = l->next) {
		logs_log_t *ll = l->data;
		if ( (!ll->session || !xstrcmp(ll->session, session)) && !xstrcmp(ll->uid, uid)) {
			log_window_t *lw = ll->lw;
			if (lw || !create) {
				if (lw)
					logs_window_check(ll, time(NULL)); /* tutaj ? */
				return ll;
			} else {
				temp = ll;
				break;
			}
		}
	}
	logs_window_close(log_curlog, 1);

	if (!create)
		return NULL;

	return (log_curlog = logs_log_new(temp, session, uid));
}

static logs_log_t *logs_log_new(logs_log_t *l, const char *session, const char *uid) {
	logs_log_t *ll;
	int created = 0;

	debug("[logs] log_new uid = %s session %s", __(uid), __(session));
	ll = l ? l : logs_log_find(session, uid, 0);
	debug(" logs_log_t %x\n", ll);

	if (!ll) {
		ll = xmalloc(sizeof(logs_log_t));
		ll->session = xstrdup(session);
		ll->uid = xstrdup(uid);
		created = 1;
	}

	if (!(ll->lw)) {
		ll->lw = xmalloc(sizeof(log_window_t));
		logs_window_check(ll, time(NULL)); /* l->log_format i l->path, l->t */
		ll->lw->file = logs_open_file(ll->lw->path, ll->lw->logformat);
	}

	if (created) {
		if (ll->lw->logformat == LOG_FORMAT_IRSSI && xstrlen(IRSSI_LOG_EKG2_OPENED)) {
			logs_irssi(ll->lw->file, session, NULL,
					prepare_timestamp_format(IRSSI_LOG_EKG2_OPENED, time(NULL)),
					0, EKG_MSGCLASS_SYSTEM);
		} 
		list_add(&log_logs, ll);
	}
	return ll;
}

static void logs_window_new(window_t *w) {
	const char *uid;

	if (!w->target || !w->session || w->id == 1000)
		return;

	uid = get_uid_any(w->session, w->target);

/* XXX, do we really want/need to create log struct with invalid uid? XXX */
	logs_log_new(NULL, session_uid_get(w->session), uid ? uid : w->target);
}

static FILE *logs_window_close(logs_log_t *l, int close) {
	log_window_t *lw;
	FILE *f;

	if (!l || !(lw = l->lw))
		return NULL;

	f = lw->file;

	xfree(lw->path);
	xfree(lw);
	l->lw = NULL;
	if (close && f) {
		fclose(f);
		return NULL;
	}
	return f;
}

static void logs_changed_path(const char *var) {
	list_t l;
	if (in_autoexec || !log_logs) 
		return;

	for (l = log_logs; l; l = l->next) {
		logs_log_t *ll = l->data;

		if (ll->lw) {
			/* We don't need reopening file../ recreate magic struct.. because it'd be done when we try log smth into it. */
			if (ll->lw->file) {
				fclose(ll->lw->file);
				ll->lw->file = NULL;
			}

			if (ll->lw->path) {
				xfree(ll->lw->path);
				ll->lw->path = NULL;
			}
		}
	}
}

static void logs_changed_raw(const char *var) {
	/* if logs:log_raw == 0, clean LOGRAW buffer */
	if (!config_logs_log_raw)
		buffer_free(&buffer_lograw);
}

static int logs_print_window(session_t *s, window_t *w, const char *line, time_t ts) {
	static plugin_t *ui_plugin = NULL;

	fstring_t *fstr;

	/* it's enough to look for ui_plugin once */
	if (!ui_plugin) ui_plugin = plugin_find(("ncurses"));
	if (!ui_plugin) ui_plugin = plugin_find(("gtk"));
	if (!ui_plugin) {
		debug_error("WARN logs_print_window() called but neither ncurses plugin nor gtk found\n");
		return -1;
	}

	fstr = fstring_new_format(line);
	fstr->ts = ts;

	query_emit_id(ui_plugin, UI_WINDOW_PRINT, &w, &fstr);
	return 0;
}

	/* items == -1 display all */
static int logs_buffer_raw_display(const char *file, int items) {
	struct buffer **bs = NULL;
	struct buffer *b;
	char *beg = NULL, *profile = NULL, *sesja = NULL, *target = NULL;

	int item = 0;
	int i;

	session_t *s;
	window_t *w;

	if (!file) return -1;
	if (!items) return 0;

	/* i'm weird, let's search for logs/__internal__ than check if there are smth after it... & check if there are two '/'	*/
	if ((beg = xstrstr(file, "logs/__internal__")) && xstrlen(beg) > 19 && xstrchr(beg+18, '/') && xstrchr(beg+18, '/') != xstrrchr(beg+18, '/')) {
		profile = beg + 18;
		/* XXX, if profile || sesion hasgot '/' we are in trouble */
		sesja	= xstrchr(profile, '/')+1;
		target	= xstrchr(sesja, '/')+1;
	}
	debug("[logs_buffer_raw_display()] profile: 0x%x sesja: 0x%x target: 0x%x\n", profile, sesja, target);

	if (!profile || !sesja || !target) return -1;

	profile = (xstrcmp(target, "_default_"))	? xstrndup(profile, sesja-profile-1) : NULL;
	sesja	= (xstrcmp(target, "_null_"))		? xstrndup(sesja, target-sesja-1) : NULL;
	target	= xstrdup(target);
	debug("[logs_buffer_raw_display()] profile: %s sesja: %s target: %s\n", __(profile), __(sesja), __(target));

	/* search for session+window */
	s = session_find(sesja);
	w = window_find_s(s, target);


	debug("[logs_buffer_raw_display()] s:0x%x; w:0x%x;\n", s, w);

	if (!w)
		w = window_current;

	if (w) w->lock++;

	for (b = buffer_lograw.data; b; b = b->next) {
		if (!xstrcmp(b->target, file)) {
			/* we asume that (b->ts < (b->next)->ts, it's quite correct unless other plugin do this trick... */
			if (items == -1) { 
				logs_print_window(s, w, b->line, b->ts);
			} else {
				bs		= (struct buffer **) xrealloc(bs, (item+2) * sizeof(struct buffer *));
				bs[item + 1]	= NULL;
				bs[item]	= b;
			}
			item++;
		}
	}
	if (bs) for (i = item < items ? 0 : item-items; i < item; i++) 
		logs_print_window(s, w, bs[i]->line, bs[i]->ts);

	if (w) {
		w->lock--;
		query_emit_id(NULL, UI_WINDOW_REFRESH);
	}

	xfree(bs);

	xfree(profile);
	xfree(sesja);
	xfree(target);
	return item;
}

/*
 * przygotowanie nazwy pliku bez rozszerzenia
 * %S - sesja nasza
 * %u - u¿ytkownik (uid), z którym piszemy
 * %U - u¿ytkownik (nick)   -||-
 * %Y, %M, %D - rok, miesi±c, dzieñ
 * zwraca ¶cie¿kê, która nale¿y rêcznie zwolniæ przez xfree()
 */

static char *logs_prepare_path(session_t *session, const char *logs_path, const char *uid, time_t sent) {
	char *uidtmp, datetime[5];
	struct tm *tm = NULL;
	string_t buf;

	if (!logs_path)
		return NULL;

	buf = string_init(NULL);

	while (*logs_path) {
		if ((char)*logs_path == '%' && (logs_path+1) != NULL) {
			switch (*(logs_path+1)) {
				case 'S':	string_append_n(buf, session ? session->uid : "_null_", -1);
						break;
				case 'P':	string_append_n(buf, config_profile ? config_profile : "_default_", -1);
						break;
				case 'u':	uidtmp = xstrdup(get_uid(session, uid));
						goto attach; /* avoid code duplication */
				case 'U':	uidtmp = xstrdup(get_nickname(session, uid));
attach:
						if (!uidtmp) uidtmp = xstrdup(uid);

						if (xstrchr(uidtmp, '/'))
							*(xstrchr(uidtmp, '/')) = 0; // strip resource
						string_append_n(buf, uidtmp ? uidtmp : uid, -1);
						xfree(uidtmp);
						break;
				case 'Y':	if (!tm) tm = localtime(&sent);
							snprintf(datetime, 5, "%4d", tm->tm_year+1900);
						string_append_n(buf, datetime, 4);
						break;
				case 'M':	if (!tm) tm = localtime(&sent);
							snprintf(datetime, 3, "%02d", tm->tm_mon+1);
						string_append_n(buf, datetime, 2);
						break;
				case 'D':	if (!tm) tm = localtime(&sent);
							snprintf(datetime, 3, "%02d", tm->tm_mday);
						string_append_n(buf, datetime, 2);
						break;
				default:	string_append_c(buf, *(logs_path+1));
			};

			logs_path++;
		} else if (*logs_path == '~' && (*(logs_path+1) == '/' || *(logs_path+1) == '\0')) {
			string_append_n(buf, home_dir, -1);
			//string_append_c(buf, '/');
		} else
			string_append_c(buf, *logs_path);
		logs_path++;
	};

	// sanityzacja sciezki - wywalic "../", zamienic znaki spec. na inne
	// zamieniamy szkodliwe znaki na minusy, spacje na podkreslenia
	// TODO
	xstrtr(buf->str, ' ', '_');

	return string_free(buf, 0);
}

/*
 * otwarcie pliku do zapisu/odczytu
 * tworzy wszystkie katalogi po drodze, je¶li nie istniej± i mkdir =1
 * ff - xml 2 || irssi 3 || simple 1
 * zwraca numer deskryptora b±d¼ NULL
 */

static FILE* logs_open_file(char *path, int ff) {
	char fullname[PATH_MAX];
#ifdef HAVE_ZLIB
	int zlibmode = 0;
#endif
	if (ff != LOG_FORMAT_IRSSI && ff != LOG_FORMAT_SIMPLE && ff != LOG_FORMAT_XML && ff != LOG_FORMAT_RAW) {
		if (ff == LOG_FORMAT_NONE)
			debug("[logs] opening log file %s with ff == LOG_FORMAT_NONE CANCELLED\n", __(path), ff);
		else	debug("[logs] opening log file %s with ff == %d CANCELED\n", __(path), ff);
		return NULL;
	}

	debug("[logs] opening log file %s ff:%d\n", __(path), ff);

	if (!path) {
		errno = EACCES; /* = 0 ? */
		return NULL;
	}

	{	/* check if such file was already open SLOW :( */
		list_t l;

		for (l=log_logs; l; l = l->next) {
			logs_log_t *ll = l->data;
			log_window_t *lw;

			if (!ll || !(lw = ll->lw))
				continue;

/*			debug_error("here: %x [%s, %s] [%d %d]\n", lw->file, lw->path, path, lw->logformat, ff); */

			if (lw->file && lw->logformat == ff && !xstrcmp(lw->path, path)) {
				FILE *f = lw->file;
				lw->file = NULL;	/* simulate fclose() on this */
				return f;		/* simulate fopen() here */
			}
		}
	}

	if (mkdir_recursive(path, 0)) {
		print("directory_cant_create", path, strerror(errno));
		return NULL;
	}

	strlcpy(fullname, path, PATH_MAX);

	if (ff == LOG_FORMAT_IRSSI)		strlcat(fullname, ".log", PATH_MAX);
	else if (ff == LOG_FORMAT_SIMPLE)	strlcat(fullname, ".txt", PATH_MAX);
	else if (ff == LOG_FORMAT_XML)		strlcat(fullname, ".xml", PATH_MAX);
	else if (ff == LOG_FORMAT_RAW)		strlcat(fullname, ".raw", PATH_MAX);

#ifdef HAVE_ZLIB /* z log.c i starego ekg1. Wypadaloby zaimplementowac... */
	/* nawet je¶li chcemy gzipowane logi, a istnieje nieskompresowany log,
	 * olewamy kompresjê. je¶li loga nieskompresowanego nie ma, dodajemy
	 * rozszerzenie .gz i balujemy. */
	if (config_log & 4) {
		struct stat st;
		if (stat(fullname, &st) == -1) {
			gzFile f;

			if (!(f = gzopen(path, "a")))
				return NULL;

			gzputs(f, buf);
			gzclose(f);

			zlibmode = 1;
		}
	}
	if (zlibmode) {
		/* XXX, ustawic jakas flage... */
		strlcat(fullname, ".gz", PATH_MAX);
	}
#endif

	/* if xml, prepare xml file */
	if (ff == LOG_FORMAT_XML) {
		FILE *fdesc = fopen(fullname, "r+");
		if (!fdesc) {
			if (!(fdesc = fopen(fullname, "w+")))
				return NULL;
			fputs("<?xml version=\"1.0\"?>\n", fdesc);
			fputs("<!DOCTYPE ekg2log PUBLIC \"-//ekg2log//DTD ekg2log 1.0//EN\" ", fdesc);
			fputs("\"http://www.ekg2.org/DTD/ekg2log.dtd\">\n", fdesc);
			fputs("<ekg2log xmlns=\"http://www.ekg2.org/DTD/\">\n", fdesc);
			fputs("</ekg2log>\n", fdesc);
		} 
		return fdesc;
	}

	return fopen(fullname, "a+");
}

/*
 * zapis w formacie znanym z ekg1
 * typ,uid,nickname,timestamp,{timestamp wyslania dla odleglych}, text
 */

static void logs_simple(FILE *file, const char *session, const char *uid, const char *text, time_t sent, msgclass_t class, const char *status) {
	char *textcopy;
	const char *timestamp = prepare_timestamp_format(config_logs_timestamp, time(0));

	session_t *s = session_find((const char*)session);
	const char *gotten_uid = get_uid(s, uid);
	const char *gotten_nickname = get_nickname(s, uid);

	if (!file)
		return;
	textcopy = log_escape(text);

	if (!gotten_uid)	gotten_uid = uid;
	if (!gotten_nickname)	gotten_nickname = uid;

	switch (class) {
		case EKG_MSGCLASS_MESSAGE	: fputs("msgrecv,", file);
						  break;
		case EKG_MSGCLASS_CHAT		: fputs("chatrecv,", file);
						  break;
		case EKG_MSGCLASS_SENT		: fputs("msgsend,", file);
						  break;
		case EKG_MSGCLASS_SENT_CHAT	: fputs("chatsend,", file);
						  break;
		case EKG_MSGCLASS_SYSTEM	: fputs("msgsystem,", file);
						  break;
		case EKG_MSGCLASS_PRIV_STATUS	: fputs("status,", file);
						  break;
		default				: fputs("chatrecv,", file);
						  break;
	};

	/*
	 * chatsend,<numer>,<nick>,<czas>,<tre¶æ>
	 * chatrecv,<numer>,<nick>,<czas_otrzymania>,<czas_nadania>,<tre¶æ>
	 * status,<numer>,<nick>,[<ip>],<time>,<status>,<descr>
	 */

	fputs(gotten_uid, file);      fputc(',', file);
	fputs(gotten_nickname, file); fputc(',', file);
	if (class == EKG_MSGCLASS_PRIV_STATUS) {
		userlist_t *u = userlist_find(s, gotten_uid);
		const char *ip, *port;
		int __ip = user_private_item_get_int(u, "ip");

		ip = __ip ? inet_ntoa(*((struct in_addr*) &__ip)) : "0.0.0.0";

		if (!(port = user_private_item_get(u, "port")))
			port = "0";

		fputs(ip, file);
		fputc(':', file);
		fputs(port, file); 
		fputc(',', file);
	}

	fputs(timestamp, file); fputc(',', file);

	if (class == EKG_MSGCLASS_MESSAGE || class == EKG_MSGCLASS_CHAT) {
		const char *senttimestamp = prepare_timestamp_format(config_logs_timestamp, sent);
		fputs(senttimestamp, file);
		fputc(',', file);
	} else if (class == EKG_MSGCLASS_PRIV_STATUS) {
		fputs(status, file); 
		fputc(',', file);
	}
	if (textcopy) fputs(textcopy, file);
	fputs("\n", file);

	xfree(textcopy);
	fflush(file);
}

/*
 * zapis w formacie xml
 */

static void logs_xml(FILE *file, const char *session, const char *uid, const char *text, time_t sent, msgclass_t class) {
	session_t *s;
	char *textcopy;
	const char *timestamp = prepare_timestamp_format(config_logs_timestamp, time(NULL));
/*	const char *senttimestamp = prepare_timestamp_format(config_logs_timestamp, sent); */
	char *gotten_uid, *gotten_nickname;
	const char *tmp;

	if (!file)
		return;

	textcopy	= xml_escape( text);

	s = session_find((const char*)session);
	gotten_uid	= xml_escape( (tmp = get_uid(s, uid))		? tmp : uid);
	gotten_nickname = xml_escape( (tmp = get_nickname(s, uid))	? tmp : uid);

	fseek(file, -11, SEEK_END); /* wracamy przed </ekg2log> */

	/*
	 * <message class="chatsend">
	 * <time>
	 *	<sent>...</sent>
	 *	<received>...</received>
	 * </time>
	 * <sender>
	 *	<uid>...</uid>
	 *	<nick>...</nick>
	 * </sender>
	 * <body>
	 *	(#PCDATA)
	 * </body>
	 * </message>
	 */

	fputs("<message class=\"",file);

	switch (class) {
		case EKG_MSGCLASS_MESSAGE	: fputs("msgrecv", file);	  break;
		case EKG_MSGCLASS_CHAT		: fputs("chatrecv", file);	  break;
		case EKG_MSGCLASS_SENT		: fputs("msgsend", file);	  break;
		case EKG_MSGCLASS_SENT_CHAT	: fputs("chatsend", file);	  break;
		case EKG_MSGCLASS_SYSTEM	: fputs("msgsystem", file);	  break;
		default				: fputs("chatrecv", file);	  break;
	};

	fputs("\">\n", file);

	fputs("\t<time>\n", file);
	fputs("\t\t<received>", file); fputs(timestamp, file); fputs("</received>\n", file);
	if (class == EKG_MSGCLASS_MESSAGE || class == EKG_MSGCLASS_CHAT) {
		fputs("\t\t<sent>", file); fputs(timestamp, file); fputs("</sent>\n", file);
	}
	fputs("\t</time>\n", file);

	fputs("\t<sender>\n", file);
	fputs("\t\t<uid>", file);   fputs(gotten_uid, file);	   fputs("</uid>\n", file);
	fputs("\t\t<nick>", file);  fputs(gotten_nickname, file);  fputs("</nick>\n", file);
	fputs("\t</sender>\n", file);

	fputs("\t<body>\n", file);
	if (textcopy) fputs(textcopy, file);
	fputs("\t</body>\n", file);

	fputs("</message>\n", file);
	fputs("</ekg2log>\n", file);

	xfree(textcopy);
	xfree(gotten_uid);
	xfree(gotten_nickname);
	fflush(file);
}

/*
 * zapis w formacie gaim'a
 */

#if 0
static void logs_gaim()
{
}
#endif

/*
 * write to file like irssi do.
 */

static void logs_irssi(FILE *file, const char *session, const char *uid, const char *text, time_t sent, msgclass_t class) {
	const char *nuid = NULL;	/* get_nickname(session_find(session), uid) */

	if (!file)
		return;

	switch (class) {
		case EKG_MSGCLASS_PRIV_STATUS:
		{
			userlist_t *u = userlist_find(session_find(session), uid);
			const char *ip, *port;
			int __ip = user_private_item_get_int(u, "ip");

			ip = __ip ? inet_ntoa(*((struct in_addr*) &__ip)) : "0.0.0.0";

			if (!(port = user_private_item_get(u, "port")))
				port = "0";

			fprintf(file, "%s * %s reports status: %s [~notirc@%s:%s] /* {status} */\n", prepare_timestamp_format(config_logs_timestamp, sent), nuid ? nuid : __(uid), __(text), ip, port);
			break;
		}

		case EKG_MSGCLASS_SYSTEM: /* other messages like session started, session closed and so on */
			fprintf(file, "%s\n", __(text));
			break;

		case EKG_MSGCLASS_MESSAGE:	/* just normal message */
			fprintf(file, "%s <%s> %s\n", prepare_timestamp_format(config_logs_timestamp, sent), nuid ? nuid : __(uid), __(text));
			break;

		default: /* everythink else */
			debug("[LOGS_IRSSI] UTYPE = %d\n", class);
			return; /* to avoid flushisk file */
	}
	fflush(file);
}

/* 
 * zwraca na przemian jeden z dwóch statycznych buforów, wiêc w obrêbie
 * jednego wyra¿enia mo¿na wywo³aæ tê funkcjê dwukrotnie.
 */
/* w sumie starczylby 1 statyczny bufor ... */
static const char *prepare_timestamp_format(const char *format, time_t t)  {
	static char buf[2][100];
	struct tm *tm = localtime(&t);
	static int i = 0;

	if (!format)
		return itoa(t);

	if (!format[0])
		return "";

	i = i % 2;

	if (!strftime(buf[i], sizeof(buf[0]), format, tm))
		return "TOOLONG";

	return buf[i++];
}

/**
 * glowny handler
 */

static QUERY(logs_handler) {
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	char **rcpts	= *(va_arg(ap, char***));
	char *text	= *(va_arg(ap, char**));
		uint32_t **UNUSED(format)	= va_arg(ap, uint32_t**);
	time_t	 sent	= *(va_arg(ap, time_t*));
	int  class	= *(va_arg(ap, int*));
		char **UNUSED(seq)		= va_arg(ap, char**);

	session_t *s = session_find(session); // session pointer
	log_window_t *lw;
	char *conf_uid = NULL;		/* conference-uid */
	char *target_uid;

	/* olewamy jesli to irc i ma formatke irssi like, czekajac na irc-protocol-message */
	if (session_check(s, 0, "irc") && logs_log_format(s) == LOG_FORMAT_IRSSI) 
		return 0;

	class &= ~(EKG_NO_THEMEBIT | EKG_MSGCLASS_NOT2US);

	target_uid = (class >= EKG_MSGCLASS_SENT) ? rcpts[0] : uid;

	/* XXX, think more about conferences-logging */
	if (class < EKG_MSGCLASS_SENT) {
		int recipients_count = array_count((char **) rcpts);

		if (recipients_count > 0) {
			struct conference *c;
			
			if ((c = conference_find_by_uids(s, uid, (const char **) rcpts, recipients_count, 0)))
				conf_uid = c->name;
			else
				debug_error("logs_handler() smth strange happen (c == NULL) && recipients_count > 0 [%d]\n", recipients_count);
		}
	}

	if (!(lw = logs_log_find(session, conf_uid ? conf_uid : target_uid, 1)->lw)) {
		debug_error("[LOGS:%d] logs_handler, shit happen\n", __LINE__);
		return 0;
	}

	if ( !(lw->file) && !(lw->file = logs_open_file(lw->path, lw->logformat)) ) {
		debug_error("[LOGS:%d] logs_handler Cannot open/create file: %s\n", __LINE__, __(lw->path));
		return 0;
	}

	/*	debug("[LOGS_MSG_HANDLER] %s : %s %d %x\n", target_uid, lw->path, lw->logformat, lw->file); */

	/* uid = uid | target_uid ? */

	switch (lw->logformat) {
		case LOG_FORMAT_SIMPLE:
			logs_simple(lw->file, session, target_uid, text, sent, class, (char*)NULL);
			break;

		case LOG_FORMAT_XML:
			logs_xml(lw->file, session, uid, text, sent, class);
			break;

		case LOG_FORMAT_IRSSI:
			logs_irssi(lw->file, session, uid, text, sent, EKG_MSGCLASS_MESSAGE);
			break;
	}
	return 0;
}

/*
 * status handler
 */

static QUERY(logs_status_handler) {
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	int status	= *(va_arg(ap, int*));
	char *descr	= *(va_arg(ap, char**));

	log_window_t *lw;

	/* joiny, party	ircowe jakies inne query. lub zrobic to w pluginie irc... ? */
	/* 
	   if (session_check(s, 0, "irc") && !xstrcmp(logs_log_format(s), "irssi"))
	   return 0;
	   */
	if (config_logs_log_status <= 0)
		return 0;

	if (!(lw = logs_log_find(session, uid, 1)->lw)) {
		debug_error("[LOGS:%d] logs_status_handler, shit happen\n", __LINE__);
		return 0;
	}

	if ( !(lw->file) && !(lw->file = logs_open_file(lw->path, lw->logformat)) ) {
		debug_error("[LOGS:%d] logs_status_handler Cannot open/create file: %s\n", __LINE__, __(lw->path));
		return 0;
	}

	if (!descr)
		descr = "";

	switch (lw->logformat) {
		case LOG_FORMAT_SIMPLE:
		{
			logs_simple(lw->file, session, uid, descr, time(NULL), EKG_MSGCLASS_PRIV_STATUS, ekg_status_string(status, 0));
			break;
		}

		case LOG_FORMAT_XML:
		{
			// logs_xml(lw->file, session, uid, descr, time(NULL), EKG_MSGCLASS_PRIV_STATUS, status);
			break;
		}

		case LOG_FORMAT_IRSSI:
		{
			char *_what = saprintf("%s (%s)", descr, __(ekg_status_string(status, 0)));
			logs_irssi(lw->file, session, uid, _what, time(NULL), EKG_MSGCLASS_PRIV_STATUS);
			xfree(_what);
			break;
		}
	}
	return 0;
}

static QUERY(logs_handler_irc) {
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	char *text	= *(va_arg(ap, char**));
		int  *UNUSED(isour)	= va_arg(ap, int*);
		int  *UNUSED(foryou)	= va_arg(ap, int*);
		int  *UNUSED(private)	= va_arg(ap, int*);
	char *channame	= *(va_arg(ap, char**));
	log_window_t *lw;

	if (!(lw = logs_log_find(session, channame, 1)->lw)) {
		debug_error("[LOGS:%d] logs_handler_irc, shit happen\n", __LINE__);
		return 0;
	}

	if ( !(lw->file) && !(lw->file = logs_open_file(lw->path, lw->logformat)) ) { 
		debug_error("[LOGS:%d] logs_handler_irc Cannot open/create file: %s\n", __LINE__, __(lw->path));
		return 0;
	}

	switch (lw->logformat) {
		case LOG_FORMAT_IRSSI:
			logs_irssi(lw->file, session, uid, text, time(NULL), EKG_MSGCLASS_MESSAGE);
			break;
	}
	return 0;
}

static QUERY(logs_handler_raw) {
	window_t *w	= *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));
	char *path;
	char *str;

	if (!config_logs_log_raw) return 0;
	if (!w || !line || w->id == 0) return 0;	/* don't log debug window */

	/* line->str + line->attr == ascii str with formats */
	path = logs_prepare_path(w->id != 1 ? w->session : NULL, "~/.ekg2/logs/__internal__/%P/%S/%u", window_target(w), 0);
	str  = fstring_reverse(line);

	buffer_add(&buffer_lograw, path, str);

	xfree(str);
	xfree(path);

	return 0;
}

static QUERY(logs_handler_newwin) {
	window_t *w = *(va_arg(ap, window_t **));

/* w->floating */

	logs_window_new(w);
	if (config_logs_log_raw) {
		FILE *f;
		char *line;
		char *path;

		path = logs_prepare_path(w->id != 1 ? w->session : NULL, "~/.ekg2/logs/__internal__/%P/%S/%u", window_target(w), 0 /* time(NULL) */ ); 
		debug("logs_handler_newwin() loading buffer from: %s\n", __(path));

		f = logs_open_file(path, LOG_FORMAT_RAW);

		if (!f) {
			debug("[LOGS:%d] Cannot open/create file: %s\n", __LINE__, __(path));
			xfree(path);
			return 0;
		}

		/* XXX, in fjuczer it can be gzipped file, WARN HERE */
		while ((line = read_file(f, 0)))
			buffer_add_str(&buffer_lograw, path, line);

		ftruncate(fileno(f), 0);	/* works? */
		fclose(f);

/*	XXX, unlink file instead of truncating? 
		if (unlink(path+'.raw') == -1) debug("[LOGS:%d] Cannot unlink file: %s (%d %s)\n", __LINE__, __(path), errno, strerror(errno));
*/		

		logs_buffer_raw_display(path, config_logs_remind_number);
		xfree(path);
	}
	return 0;
}

static QUERY(logs_postinit) {
	window_t *w;
	for (w = windows; w; w = w->next)
		logs_window_new(w);
	return 0;
}

static QUERY(logs_handler_killwin)  {
	window_t *w = *(va_arg(ap, window_t **));
	logs_window_close(logs_log_find(w->session ? w->session->uid : NULL, w->target, 0), 1);
	return 0;
}

static QUERY(logs_setvar_default) {
	xfree(config_logs_path);
	xfree(config_logs_timestamp);
	config_logs_path = xstrdup("~/.ekg2/logs/%S/%u");
	config_logs_timestamp = NULL;
	return 0;
}

EXPORT int logs_plugin_init(int prio) {

	PLUGIN_CHECK_VER("logs");

	plugin_register(&logs_plugin, prio);
	
	query_connect_id(&logs_plugin, SET_VARS_DEFAULT,logs_setvar_default, NULL);
	query_connect_id(&logs_plugin, PROTOCOL_MESSAGE_POST, logs_handler, NULL);
	query_connect_id(&logs_plugin, IRC_PROTOCOL_MESSAGE, logs_handler_irc, NULL);
	query_connect_id(&logs_plugin, UI_WINDOW_NEW,	logs_handler_newwin, NULL);
	query_connect_id(&logs_plugin, UI_WINDOW_PRINT,	logs_handler_raw, NULL);
	query_connect_id(&logs_plugin, UI_WINDOW_KILL,	logs_handler_killwin, NULL);
	query_connect_id(&logs_plugin, PROTOCOL_STATUS, logs_status_handler, NULL);
	query_connect_id(&logs_plugin, CONFIG_POSTINIT, logs_postinit, NULL);
	/* XXX, implement UI_WINDOW_TARGET_CHANGED, IMPORTANT!!!!!! */

	/* TODO: maksymalna ilosc plikow otwartych przez plugin logs */
	variable_add(&logs_plugin, ("log_max_open_files"), VAR_INT, 1, &config_logs_max_files, NULL /* XXX: logs_changed_maxfd */, NULL, NULL); 
	variable_add(&logs_plugin, ("log"), VAR_MAP, 1, &config_logs_log, &logs_changed_path, 
			variable_map(3, 
				LOG_FORMAT_NONE, 0, "none", 
				LOG_FORMAT_SIMPLE, LOG_FORMAT_XML, "simple", 
				LOG_FORMAT_XML, LOG_FORMAT_SIMPLE, "xml"), 
			NULL);
	variable_add(&logs_plugin, ("log_raw"), VAR_BOOL, 1, &config_logs_log_raw, &logs_changed_raw, NULL, NULL);
	variable_add(&logs_plugin, ("log_ignored"), VAR_INT, 1, &config_logs_log_ignored, NULL, NULL, NULL);
	variable_add(&logs_plugin, ("log_status"), VAR_BOOL, 1, &config_logs_log_status, NULL, NULL, NULL);
	variable_add(&logs_plugin, ("path"), VAR_DIR, 1, &config_logs_path, &logs_changed_path, NULL, NULL);
	variable_add(&logs_plugin, ("remind_number"), VAR_INT, 1, &config_logs_remind_number, NULL, NULL, NULL);
	variable_add(&logs_plugin, ("timestamp"), VAR_STR, 1, &config_logs_timestamp, NULL, NULL, NULL);

	return 0;
}

static int logs_plugin_destroy() {
	list_t old_logs = log_logs;
	struct buffer *b;

	for (; log_logs; log_logs = log_logs->next) {
		logs_log_t *ll = log_logs->data;
		FILE *f = NULL;
		time_t t = time(NULL);
		int ff = (ll->lw) ? ll->lw->logformat : logs_log_format(session_find(ll->session));

		/* TODO: rewrite */
		if (ff == LOG_FORMAT_IRSSI && xstrlen(IRSSI_LOG_EKG2_CLOSED)) {
			char *path	= (ll->lw) ? xstrdup(ll->lw->path) : logs_prepare_path(session_find(ll->session), config_logs_path, ll->uid, t);
			f		= (ll->lw) ? logs_window_close(log_logs->data, 0) : NULL; 

			if (!f) 
				f = logs_open_file(path, ff);
			xfree(path);
		} else 
			logs_window_close(log_logs->data, 1);

		if (f) {
			if (ff == LOG_FORMAT_IRSSI && xstrlen(IRSSI_LOG_EKG2_CLOSED)) {
				logs_irssi(f, ll->session, NULL,
						prepare_timestamp_format(IRSSI_LOG_EKG2_CLOSED, t), 0,
						EKG_MSGCLASS_SYSTEM);
			}
			fclose(f);
		}

		xfree(ll->session);
		xfree(ll->uid);
	}
	list_destroy(old_logs, 1);	log_logs = NULL;

	if (config_logs_log_raw) for (b = buffer_lograw.data; b;) {
		static FILE *f = NULL;
		static char *oldtarget = NULL;
		/*
		 * path: b->target
		 *   ts: b->ts
		 *  str: b->line
		 */
		if (f && !xstrcmp(b->target, oldtarget));		/* if file is already opened and current target match old one, use it */
		else {
			if (f) fclose(f);				/* close file */
			f = logs_open_file(b->target, LOG_FORMAT_RAW);	/* otherwise try to open new file/reopen */
		}

		if (f) {
			fprintf(f, "%i %s\n", (unsigned int) b->ts, b->line);
		} else debug_error("[LOGS:%d] Cannot open/create file: %s\n", __LINE__, __(b->target));

		xfree(b->line);
		xfree(oldtarget);
		oldtarget = b->target;

		b = LIST_REMOVE2(&(buffer_lograw.data), b, NULL);

		if (!b) {
			if (f) fclose(f);
			xfree(oldtarget);
		}
	}
	/* just in case */
	buffer_free(&buffer_lograw);

	plugin_unregister(&logs_plugin);
	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
