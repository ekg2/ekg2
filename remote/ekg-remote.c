/* $Id: ekg.c 4601 2008-09-04 16:02:33Z darkjames $ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *			    Robert J. Wo¼ny <speedy@ziew.org>
 *			    Pawe³ Maziarz <drg@infomex.pl>
 *			    Adam Osuchowski <adwol@polsl.gliwice.pl>
 *			    Dawid Jarosz <dawjar@poczta.onet.pl>
 *			    Wojciech Bojdo³ <wojboj@htcon.pl>
 *			    Piotr Domagalski <szalik@szalik.net>
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

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/un.h>

#include <sys/ioctl.h>

#include <sys/stat.h>

#define __USE_BSD
#include <sys/time.h>

#include <sys/resource.h>	/* rlimit */

#include <sys/select.h>

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "compat/getopt.h"
#endif
#include <limits.h>
#include <locale.h>

#include <stdio.h>

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commands.h"
#include "debug.h"
#include "plugins.h"
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "sessions.h"
#include "recode.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

#include "queries.h"

/* tez sprawdzic includy */

EXPORTNOT int ekg_watches_removed;
static char argv0[100];

static int stderr_backup = -1;

int no_mouse = 0;

char *events_all[] = { "protocol-message", "event_avail", "event_away", "event_na", "event_online", "event_descr", NULL };

static void config_postread() {
	query_emit_id(NULL, CONFIG_POSTINIT); 

	/* legacyconfig.c */
#if ! USE_UNICODE
	if (!xstrcasecmp(config_console_charset, "UTF-8")) {
		print("config_error", _("Warning, nl_langinfo(CODESET) reports that you are using utf-8 encoding, but you didn't compile ekg2 with (experimental/untested) --enable-unicode\n"
			    "\tPlease compile ekg2 with --enable-unicode or change your enviroment setting to use not utf-8 but iso-8859-1 maybe? (LC_ALL/LC_CTYPE)\n"));
	}
#endif

	print("remote_console_charset_using", config_console_charset);
}

/* configfile.c */
static int config_read(const char *filename) { return -1; }	/* XXX, czyli przeczytac co jeszcze trzeba przeslac... */

/* sessions.c */
static int session_read(const char *filename) { return -1; }

static int metacontact_read() { return -1; }
static void metacontact_init() { } 
static void metacontacts_destroy() { }

void *metacontacts;
void *metacontact_find_prio(void *m) { return NULL; }

void *newconferences;
void *newconference_find(session_t *s, const char *name) { return NULL; }
static void newconferences_destroy() { }

void *conferences;
static void conferences_destroy() { }

/* themes.c */
void changed_theme(const char *var) { }

const char *prepare_path(const char *filename, int do_mkdir) { return NULL; }		/* completion z tego korzysta, zdalnie i tak nie dziala */
int session_check(session_t *s, int need_private, const char *protocol) { return 0; }	/* dla ncursesa */

void ekg_loop() {
	static int lock;

	struct timeval stv;
	fd_set rd, wd;
	int ret, maxfd;

	struct timeval tv;

	struct timer *t;		/* timery */
	list_t l;			/* watche */

	if (lock) {
		debug_error("ekg_loop() locked!\n");
		return;
	}
	lock = 1;

	gettimeofday(&tv, NULL);

	/* przejrzyj timery u¿ytkownika, ui, skryptów */
	for (t = timers; t; t = t->next) {
		if (tv.tv_sec > t->ends.tv_sec || (tv.tv_sec == t->ends.tv_sec && tv.tv_usec >= t->ends.tv_usec)) {
			int ispersist = t->persist;
				
			if (ispersist) {
				memcpy(&t->ends, &tv, sizeof(tv));
				t->ends.tv_sec += t->period;
				t->ends.tv_sec += (t->period / 1000);
				t->ends.tv_usec += ((t->period % 1000) * 1000);
				if (t->ends.tv_usec >= 1000000) {
					t->ends.tv_usec -= 1000000;
					t->ends.tv_sec++;
				}
			}

			if ((t->function(0, t->data) == -1) || !ispersist)
				t = timers_removei(t);

		}
	}

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->removed == 1) {
			w->removed = 0;
			watch_free(w);
		}
	}

	/* zerknij na wszystkie niezbêdne deskryptory */

	FD_ZERO(&rd);
	FD_ZERO(&wd);

	for (maxfd = 0, l = watches; l; l = l->next) {
		watch_t *w = l->data;
		if (!w)
			continue;

		if (w->fd > maxfd)
			maxfd = w->fd;
		if ((w->type & WATCH_READ))
			FD_SET(w->fd, &rd);
		if ((w->type & WATCH_WRITE)) {
			if (w->buf && !w->buf->len) continue; /* if we have WATCH_WRITE_LINE and there's nothink to send, ignore this */ 
			FD_SET(w->fd, &wd); 
		}
	}

	stv.tv_sec = 1;
	stv.tv_usec = 0;

	for (t = timers; t; t = t->next) {
		int usec = 0;

		/* zeby uniknac przekrecenia licznika mikrosekund przy
		 * wiekszych czasach, pomijamy dlugie timery */
		if (t->ends.tv_sec - tv.tv_sec > 1)
			continue;

		/* zobacz, ile zostalo do wywolania timera */
		usec = (t->ends.tv_sec - tv.tv_sec) * 1000000 + (t->ends.tv_usec - tv.tv_usec);

		/* jesli wiecej niz sekunda, to nie ma znacznia */
		if (usec >= 1000000)
			continue;
				
		/* jesli mniej niz aktualny timeout, zmniejsz */
		if (stv.tv_sec * 1000000 + stv.tv_usec > usec) {
			stv.tv_sec = 0;
			stv.tv_usec = usec;
		}
	}

	/* na wszelki wypadek sprawd¼ warto¶ci */
	if (stv.tv_sec != 1)
		stv.tv_sec = 0;
	if (stv.tv_usec < 0)
		stv.tv_usec = 1;

	/* sprawd¼, co siê dzieje */
	ret = select(maxfd + 1, &rd, &wd, NULL, &stv);

	/* je¶li wyst±pi³ b³±d, daj znaæ */
	if (ret == -1) {
		/* jaki¶ plugin doda³ do watchów z³y deskryptor. ¿eby
		 * ekg mog³o dzia³aæ dalej, sprawd¼my który to i go
		 * usuñmy z listy. */
		if (errno == EBADF) {
			list_t l;

			for (l = watches; l; l = l->next) {
				watch_t *w = l->data;
				struct stat st;

				if (w && fstat(w->fd, &st)) {
					debug("select(): bad file descriptor: fd=%d, type=%d, plugin=%s\n", w->fd, w->type, (w->plugin) ? w->plugin->name : ("none"));
					watch_free(w);
				}
			}
		} else if (errno != EINTR)
			debug("select() failed: %s\n", strerror(errno));
		lock = 0;
		return;
	}

	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (!w)
			continue;

		if (((w->type == WATCH_WRITE) && FD_ISSET(w->fd, &wd)) || 
			((w->type == WATCH_READ) && FD_ISSET(w->fd, &rd)))
		{
			watch_handle(w);
		}
	}

	if (ekg_watches_removed > 0) {
		debug("ekg_loop() Removed %d watches this loop, let's cleanup calling: list_cleanup() ...\n", ekg_watches_removed);
		list_cleanup(&watches);
		ekg_watches_removed = 0;
	}
	lock = 0;
}

static void handle_sighup()
{
	ekg_exit();
}

static void handle_sigsegv()
{
	signal(SIGSEGV, SIG_DFL);

	if (stderr_backup && stderr_backup != -1)
		dup2(stderr_backup, 2);

	/* wy³±cz plugin ui, ¿eby odda³ terminal */
	if (ui_plugin)
		plugin_unload(ui_plugin);

	fprintf(stderr,
"\r\n"
"\r\n"
"*** Naruszenie ochrony pamiêci ***\r\n"
"\r\n"
"\r\n"
"Je¶li zostanie utworzony plik core.%d, spróbuj uruchomiæ\r\n"
"polecenie:\r\n"
"\r\n"
"    gdb %s core.%d\r\n"
"\n"
"zanotowaæ kilka ostatnich linii, a nastêpnie zanotowaæ wynik polecenia\r\n"
",,bt''. Dziêki temu autorzy dowiedz± siê, w którym miejscu wyst±pi³ b³±d\r\n"
"i najprawdopodobniej pozwoli to unikn±æ tego typu sytuacji w przysz³o¶ci.\r\n"
"Wiêcej szczegó³ów w dokumentacji, w pliku ,,gdb.txt''.\r\n"
"\r\n",
	(int) getpid(), 
	argv0, (int) getpid());

	raise(SIGSEGV);			/* niech zrzuci core */
}

/*
 * handle_stderr()
 *
 * wy¶wietla to, co uzbiera siê na stderr.
 */
static WATCHER_LINE(handle_stderr)	/* sta³y */
{
	if (type) {
		close(fd);
		return 0;
	}

/* XXX */
/*	print("stderr", watch); */
	return 0;
}

EXPORTNOT void ekg_debug_handler(int level, const char *format, va_list ap) {
	static string_t line = NULL;
	char *tmp;

	int is_UI = 0;
	char *theme_format;

	if (!(tmp = vsaprintf(format, ap)))
		return;

	if (line) {
		string_append(line, tmp);
		xfree(tmp);

		if (line->len == 0 || line->str[line->len - 1] != '\n')
			return;

		line->str[line->len - 1] = '\0';	/* remove '\n' */
		tmp = string_free(line, 0);
		line = NULL;
	} else {
		const size_t tmplen = strlen(tmp);
		if (tmplen == 0 || tmp[tmplen - 1] != '\n') {
			line = string_init(tmp);
			xfree(tmp);
			return;
		}
		tmp[tmplen - 1] = 0;			/* remove '\n' */
	}

	switch(level) {
		case 0:				theme_format = "remote_debug";		break;
		case DEBUG_IO:			theme_format = "remote_iodebug";	break;
		case DEBUG_IORECV:		theme_format = "remote_iorecvdebug";	break;
		case DEBUG_FUNCTION:		theme_format = "remote_fdebug";		break;
		case DEBUG_ERROR:		theme_format = "remote_edebug";		break;
		case DEBUG_WHITE:		theme_format = "remote_wdebug";		break;
		case DEBUG_WARN:		theme_format = "remote_warndebug";	break;
		case DEBUG_OK:			theme_format = "remote_okdebug";	break;
		default:			theme_format = "remote_debug";		break;
	}

	query_emit_id(NULL, UI_IS_INITIALIZED, &is_UI);

	if (is_UI && window_debug) {
		print_window_w(window_debug, EKG_WINACT_NONE, theme_format, tmp);
	}
	else
		fprintf(stderr, "%s\n", tmp);
#ifdef STDERR_DEBUG	/* STDERR debug */
#endif

	xfree(tmp);
}

struct option ekg_options[] = {
	{ "no-mouse", no_argument, 0, 'm' },
	{ "frontend", required_argument, 0, 'F' },
	{ "test", required_argument, 0, 'T' },

	{ "charset", required_argument, 0, 'c' },

	{ "unicode", no_argument, 0, 'U' }, 

	{ "help", no_argument, 0, 'h' },
	{ "version", no_argument, 0, 'v' },
	{ 0, 0, 0, 0 }
};

#define EKG_USAGE N_( \
"Usage: %s [OPTIONS] [COMMANDS]\n" \
"  -m, --no-mouse	       does not load mouse support\n" \
"  -F, --frontend=NAME	       uses NAME frontend (default is ncurses)\n" \
"  -p, --password=haslo        password\n" \
\
"  -h, --help		       displays this help message\n" \
"  -v, --version	       displays program version and exits\n" \
"\n" )


extern int remote_connect(const char *path);
extern int remote_connect2(int fd, const char *password);
extern int remote_connect3();
extern void remote_print_stats();

extern int remote_mail_count;

static QUERY(remote_irc_topic_helper) {
	char **top   = va_arg(ap, char **);
	char **setby = va_arg(ap, char **);
	char **modes = va_arg(ap, char **);

	*top = xstrdup(window_current->irctopic);
	*setby = xstrdup(window_current->irctopicby);
	*modes = xstrdup(window_current->ircmode);

	return 0;
}

static QUERY(remote_mail_count_helper) {
	int *__count = va_arg(ap, int *);

	*__count = remote_mail_count;
	return 0;
}

int main(int argc, char **argv)
{
	int c;
	char *frontend = NULL;
	int testonly = 0;

	char *remote;
	int remotefd;
	char *config_password = NULL;

	struct rlimit rlim;

	/* zostaw po sobie core */
	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);

	setlocale(LC_ALL, "");
	tzset();

	srand(time(NULL));

	strlcpy(argv0, argv[0], sizeof(argv0));

	signal(SIGSEGV, handle_sigsegv);
	signal(SIGHUP, handle_sighup);
	signal(SIGTERM, handle_sighup);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);		/* nie interesuja nas dzieci... nie powinnismy miec zadnego :) */

	while ((c = getopt_long(argc, argv, "c:p:T:F:mhvU", ekg_options, NULL)) != -1) 
	{
		switch (c) {
			case 'T':
				testonly = 1;
			case 'F':
				frontend = optarg;
				break;

			case 'm':
				no_mouse = 1;
				break;

			case 'h':
				printf(_(EKG_USAGE), argv[0]);
				return 0;

			case 'c':
				xfree(config_console_charset);		/* XXX, sensowniej!, /me chce zeby sie wyswietlalo co nl_langinfo() zwrocilo, a co my podalismy. */
				config_console_charset = xstrdup(optarg);
				break;

			case 'p':
				config_password = optarg;
				break;

			case 'U':
#ifdef USE_UNICODE
				config_use_unicode = 1;
#else
				fprintf(stderr, _("EKG2 compiled without unicode support. This just can't work!\n"));
				return 1;
#endif
				break;

			case 'v':
				printf("ekg2-%s (compiled on %s)\n", VERSION, compile_time());
				return 0;

			case '?':
				/* supported by getopt */
				fprintf(stdout, _("To get more information, start program with --help.\n"));
				return 1;

			default:
				break;
		}
	}

	if (!testonly && optind + 1 != argc) {
		fprintf(stdout, _("no destination\n"));	/* netcat-like :) */
		return 1;
	}

	command_init();
	variable_init();
	theme_init();
/* windows: */
	window_debug	= window_new(NULL, NULL, -1);			/* debugowanie */
	window_status	= window_new(NULL, NULL, 1);			/* okno stanu */
	window_current = window_status;			/* XXX!!! */

	if (testonly) {
		remote = NULL;	/* silence gcc warning */
		goto _test;
	}
	remote = argv[optind];
	if ((remotefd = remote_connect(remote)) == -1) {
		perror("remote_connect()");	/* XXX, ladniejsze */
		return 1;
	}

	printf("Connected to: %s, fd: %d\n", remote, remotefd);

	if (!remote_connect2(remotefd, config_password)) {
		perror("remote_connect2()");	/* XXX, ladniejsze */
		return 1;
	}

_test:
	in_autoexec = 1;

	query_connect_id(NULL, IRC_TOPIC, remote_irc_topic_helper, NULL);
	query_connect_id(NULL, MAIL_COUNT, remote_mail_count_helper, NULL);

	if (frontend)
		plugin_load(frontend);

#ifdef HAVE_NCURSES
	if (!ui_plugin)
		plugin_load("ncurses");
#endif
#ifdef HAVE_GTK
	if (!ui_plugin)
		plugin_load("gtk");
#endif
#ifdef HAVE_READLINE
	if (!ui_plugin)
		plugin_load("readline");
#endif
	if (!ui_plugin || ui_plugin->pclass != PLUGIN_UI) {
		fprintf(stderr, "No UI-PLUGIN!\n");
		ekg_exit();
		return 0;		/* never here */
	}

	if (testonly) {
		ekg_exit();
		return 0;		/* never here */
	}

	query_emit_id(NULL, SESSION_EVENT);			/* XXX, dla ncures, zeby sie statusbar odswiezyl */

	config_read(NULL);

	if (!remote_connect3()) {
		perror("remote_connect3()");	/* XXX ladniejsze */
		ekg_exit();
		return 1;
	}

	in_autoexec = 0;

	/* wypada³oby obserwowaæ stderr */
	{
		int fd[2];

		if (!pipe(fd)) {
			fcntl(fd[0], F_SETFL, O_NONBLOCK);
			fcntl(fd[1], F_SETFL, O_NONBLOCK);
			watch_add_line(NULL, fd[0], WATCH_READ_LINE, handle_stderr, NULL);
			stderr_backup = fcntl(2, F_DUPFD, 0);
			dup2(fd[1], 2);
		}
	}

	if (config_display_welcome)
		print("remote_welcome", remote);

	/* protocol_init(); */		/* ekg2-remote: ignored, however we should debug_wtf() if we recv some strange query.. */
	metacontact_init();
	session_read(NULL);

	config_postread();

	/* status window takes first session if not set before*/
	if (!session_current && sessions)
		session_current = sessions;

	if (session_current != window_current->session)
		window_current->session = session_current;
	window_debug->session = window_current->session;

	metacontact_read(); /* read the metacontacts info */

	/* jesli jest emit: ui-loop (plugin-side) to dajemy mu kontrole, jesli nie 
	 * to wywolujemy normalnie sami ekg_loop() w petelce */
	if (query_emit_id(NULL, UI_LOOP) != -1) {
		/* krêæ imprezê */
		while (1) {
			ekg_loop();
		}
	}

	ekg_exit();

	return 0;
}

void ekg_exit() {
	int i;

	for (i = 0; i < SEND_NICKS_MAX; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}
	send_nicks_count = 0;

	if (ui_plugin)
		plugin_unload(ui_plugin);

	remote_plugins_destroy();
	watches_destroy();

/* XXX, think about sequence of unloading. */

	conferences_destroy();
	newconferences_destroy();
	metacontacts_destroy();
	sessions_free();
	theme_free();
	variables_destroy();
	commands_destroy();
	timers_destroy();
	binding_free();
	remote_recode_destroy();

	windows_destroy();
	queries_destroy();
	close(stderr_backup);

	remote_print_stats();

	exit(0);
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
