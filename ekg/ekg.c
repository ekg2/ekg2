/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Adam Osuchowski <adwol@polsl.gliwice.pl>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
 *                          Wojciech Bojdo³ <wojboj@htcon.pl>
 *                          Piotr Domagalski <szalik@szalik.net>
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
#include "win32.h"

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif
#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/ioctl.h>
#endif

#include <sys/stat.h>
#define __USE_BSD
#include <sys/time.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>	/* rlimit */
#endif

#ifdef __FreeBSD__
#  include <sys/select.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "compat/getopt.h"
#endif
#include <limits.h>
#include <locale.h>

#ifndef NO_POSIX_SYSTEM
#include <pwd.h>
#else
#include <lm.h>
#endif

#ifdef NO_POSIX_SYSTEM
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "audio.h"
#include "commands.h"
#include "debug.h"
#include "events.h"
#include "configfile.h"
#include "emoticons.h"
#include "log.h"
#include "metacontacts.h"
#include "msgqueue.h"
#include "plugins.h"
#include "protocol.h"
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "sessions.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "scripts.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"

#include "queries.h"

char *config_dir;
int mesg_startup;
int ekg_watches_removed;
struct timeval ekg_tv;
static pid_t ekg_pid = 0;
static char argv0[PATH_MAX];

pid_t speech_pid = 0;

static int stderr_backup = -1;

int no_mouse = 0;

	/* less important things which don't need to be checked every main loop iteration
	 * e.g. autoaways */
int ekg_less_important_handler(void *data) {
	struct timeval *tv = data;
	session_t *sl;

	/* sprawd¼ autoawaye ró¿nych sesji */
	for (sl = sessions; sl; sl = sl->next) {
		session_t *s = sl;
		int tmp;

		if (!s->connected || (s->status < EKG_STATUS_AWAY)) /* lowest autostatus is autoxa, so from xa and lower ones
								       we can't go further */
			continue;

		do {
			if ((s->status == EKG_STATUS_AWAY) || (tmp = session_int_get(s, "auto_away")) < 1 || !s->activity)
				break;

			if (tv->tv_sec - s->activity > tmp)
				command_exec(NULL, s, ("/_autoaway"), 0);
		} while (0);

		do {
			if ((tmp = session_int_get(s, "auto_xa")) < 1 || !s->activity)
				break;

			if (tv->tv_sec - s->activity > tmp)
				command_exec(NULL, s, ("/_autoxa"), 0);
		} while (0);
	}

	return 0;
}

/*
 * ekg_loop()
 *
 * g³ówna pêtla ekg. obs³uguje przegl±danie deskryptorów, timery i wszystko,
 * co ma siê dziaæ w tle.
 */

void ekg_loop() {
        struct timeval stv;
        fd_set rd, wd;
        int ret, maxfd, status;
	pid_t pid;

	gettimeofday(&ekg_tv, NULL);

#define tv ekg_tv
	{
		{		/* przejrzyj timery u¿ytkownika, ui, skryptów */
			struct timer *t;
			
			for (t = timers; t; t = t->next) {
				if (tv.tv_sec > t->ends.tv_sec || (tv.tv_sec == t->ends.tv_sec && tv.tv_usec >= t->ends.tv_usec)) {
					int ispersist = t->persist;
					
					if (ispersist) {
						memcpy(&t->ends, &tv, sizeof(tv));
						t->ends.tv_sec += t->period;
					}

					if ((t->function(0, t->data) == -1) || !ispersist)
						t = timers_removei(t);
				}
			}
		}

		{		/* removed 'w->removed' watches, timeout checking moved below select() */
			watch_t *w;

			for (w = watches; w;) {
				watch_t *next = w->next;

				if (w->removed == 1) {
					watch_t *tmp;

					w->removed = 0;
					if ((tmp = watch_free(w))) /* if watch was really deleted, we shall jump */
						next = tmp;
				}

				w = next;
			}
		}

                /* auto save */
                if (config_auto_save && config_changed && (tv.tv_sec - last_save) > config_auto_save) {
                        debug("autosaving userlist and config after %d seconds\n", tv.tv_sec - last_save);
                        last_save = tv.tv_sec;

                        if (!config_write(NULL) && !session_write()) {
                                config_changed = 0;
                                reason_changed = 0;
                                print("autosaved");
                        } else
                                print("error_saving");
                }

                /* przegl±danie zdech³ych dzieciaków */
#ifndef NO_POSIX_SYSTEM
                while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			child_t *c;
			debug("child process %d exited with status %d\n", pid, WEXITSTATUS(status));

                        for (c = children; c; c = c->next) {
                                if (pid != c->pid)
                                        continue;

                                if (pid == speech_pid) {
                                        speech_pid = 0;

                                        if (!config_speech_app)
                                                buffer_free(&buffer_speech);

                                        if (buffer_speech.count && !WEXITSTATUS(status)) {
                                                char *str = buffer_tail(&buffer_speech);
                                                say_it(str);
                                                xfree(str);
                                        }
                                }

                                if (c->handler)
                                        c->handler(c, c->pid, c->name, WEXITSTATUS(status), c->private);

				c = children_removei(c);
                        }
                }
#endif
                /* zerknij na wszystkie niezbêdne deskryptory */

                FD_ZERO(&rd);
                FD_ZERO(&wd);

		{
			watch_t *w;

			for (maxfd = 0, w = watches; w; w = w->next) {
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
		}

		stv.tv_sec = 1;
		stv.tv_usec = 0;
		{
			struct timer *t;

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
		}
	
                /* na wszelki wypadek sprawd¼ warto¶ci */

		if (idles) {
			stv.tv_sec = 0;
			if (stv.tv_usec > 20000)
				stv.tv_usec = 20000;
			/* max 50 times per second for idler.. i think it's ok */

			/* execute idles only when stv.tv_usec > 20000? */
		} else {

			if (stv.tv_sec != 1) 
				stv.tv_sec = 0;
			if (stv.tv_usec < 0)
				stv.tv_usec = 1;
		}
                /* sprawd¼, co siê dzieje */
		ret = select(maxfd + 1, &rd, &wd, NULL, &stv);

                /* je¶li wyst±pi³ b³±d, daj znaæ */
		if (ret == -1) {
                        /* jaki¶ plugin doda³ do watchów z³y deskryptor. ¿eby
                         * ekg mog³o dzia³aæ dalej, sprawd¼my który to i go
                         * usuñmy z listy. */
			if (errno == EBADF) {
				watch_t *w;

				for (w = watches; w;) {
					struct stat st;
					watch_t *next = w->next;

					if (w && fstat(w->fd, &st)) {
						watch_t *tmp;
						debug("select(): bad file descriptor: fd=%d, type=%d, plugin=%s\n", w->fd, w->type, (w->plugin) ? w->plugin->name : ("none"));
						if ((tmp = watch_free(w)))
							next = tmp;
					}

					w = next;
				}
			} else if (errno != EINTR)
				debug("select() failed: %s\n", strerror(errno));
			return;
		}

		/* execute idle tasks only when select() return no new data */
		if (idles && ret == 0) {
			/* idles works this way:
			 *  - if handler want to keep idler alive, in idle_handle() we add again to list.. [here we remove old item]
			 *  - else private data is freed in idle_handle() and here we remove only list_t internal struct.
			 *
			 *  yeah, i know it can be do better. note: it's just for gtk.. maybe later we have prios and so (and ekg2 become operation system with scheduler)
			 */
			idle_t *idler = idles;

			LIST_UNLINK2(&idles, idles);
			idle_handle(idler);

			/* Here I think we can do return; coz select() return 0 when nothing happen on given fds */
			/* but to avoid regression on broken systems */

			/* return; */
		}

		{		/* przejrzyj deskryptory */
			watch_t *w, *next;

			for (w = watches; w; w = next) {
				next = w->next;

				if (!FD_ISSET(w->fd, &rd) && !FD_ISSET(w->fd, &wd)) { /* timeout checking */
					watch_t *tmp;

					if (w->timeout < 1 || (tv.tv_sec - w->started) < w->timeout)
						continue;
					w->removed = -1;
					if (w->buf) {
						int (*handler)(int, int, char*, void*) = w->handler;
						if (handler(2, w->fd, NULL, w->data) == -1 || w->removed == 1) {
							w->removed = 0;
							if ((tmp = watch_free(w)))
								next = tmp;
							continue;
						}
					} else {
						int (*handler)(int, int, int, void*) = w->handler;
						if (handler(2, w->fd, w->type, w->data) == -1 || w->removed == 1) {
							w->removed = 0;
							if ((tmp = watch_free(w)))
								next = tmp;
							continue;
						}
					}
					w->removed = 0;

					continue;
				}

				if (w->fd == 0) {
					session_t *s;
					for (s = sessions; s; s = s->next) 
					{
						if (!s->connected || !s->autoaway)
							continue;

						if (session_int_get(s, "auto_back") != 2)
							continue;

						command_exec(NULL, s, ("/_autoback"), 2);
					}
				}
				if (!w->buf) {
					if (((w->type == WATCH_WRITE) && FD_ISSET(w->fd, &wd)) ||
							((w->type == WATCH_READ) && FD_ISSET(w->fd, &rd)))
						watch_handle(w);
				} else {
					if (FD_ISSET(w->fd, &rd) && w->type == WATCH_READ) 		watch_handle_line(w);
					else if (FD_ISSET(w->fd, &wd) && w->type == WATCH_WRITE)	watch_handle_write(w);
				}
			}
		}

		if (ekg_watches_removed > 0) {
			debug("ekg_loop() Removed %d watches this loop, let's cleanup calling: list_cleanup() ...\n", ekg_watches_removed);
			//list_cleanup(&watches); /* not needed anymore, left for historical reasons ( ; */
			ekg_watches_removed = 0;
		}
	}
#undef tv

        return;
}
#ifndef NO_POSIX_SYSTEM
static void handle_sigusr1()
{
        debug("sigusr1 received\n");
        query_emit_id(NULL, EKG_SIGUSR1);
        signal(SIGUSR1, handle_sigusr1);
}

static void handle_sigusr2()
{
        debug("sigusr2 received\n");
        query_emit_id(NULL, EKG_SIGUSR2);
        signal(SIGUSR2, handle_sigusr2);
}

static void handle_sighup()
{
        ekg_exit();
}

static void handle_sigsegv()
{
        plugin_t *p;

        signal(SIGSEGV, SIG_DFL);

        if (stderr_backup && stderr_backup != -1)
                dup2(stderr_backup, 2);

        /* wy³±cz pluginy ui, ¿eby odda³y terminal
	 * destroy also log plugins to make sure that latest changes are written */
        for (p = plugins; p; p = p->next) {
                if (p->pclass != PLUGIN_UI && p->pclass != PLUGIN_LOG)
                        continue;

                p->destroy();
        }

        fprintf(stderr,
"\r\n"
"\r\n"
"*** Naruszenie ochrony pamiêci ***\r\n"
"\r\n"
"Spróbujê zapisaæ ustawienia, ale nie obiecujê, ¿e cokolwiek z tego\r\n"
"wyjdzie. Trafi± one do plików %s/config.%d,\r\n"
"%s/config-<plugin>.%d oraz %s/userlist.%d\r\n"
"\r\n"
"Do pliku %s/debug.%d zapiszê ostatanie komunikaty\r\n"
"z okna debugowania.\r\n"
"\r\n"
"Je¶li zostanie utworzony plik %s/core.%d, spróbuj uruchomiæ\r\n"
"polecenie:\r\n"
"\r\n"
"    gdb %s %s/core.%d\r\n"
"\n"
"zanotowaæ kilka ostatnich linii, a nastêpnie zanotowaæ wynik polecenia\r\n"
",,bt''. Dziêki temu autorzy dowiedz± siê, w którym miejscu wyst±pi³ b³±d\r\n"
"i najprawdopodobniej pozwoli to unikn±æ tego typu sytuacji w przysz³o¶ci.\r\n"
"Wiêcej szczegó³ów w dokumentacji, w pliku ,,gdb.txt''.\r\n"
"\r\n",
config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir,(int) getpid(), argv0, config_dir, (int) getpid());

        config_write_crash();
        userlist_write_crash();
        debug_write_crash();

        raise(SIGSEGV);                 /* niech zrzuci core */
}
#endif

/*
 * prepare_batch_line()
 *
 * funkcja bierze podane w linii poleceñ argumenty i robi z nich pojedyñcz±
 * liniê poleceñ.
 *
 * - argc - wiadomo co ;)
 * - argv - wiadomo co ;)
 * - n - numer argumentu od którego zaczyna siê polecenie.
 *
 * zwraca stworzon± linie w zaalokowanym buforze lub NULL przy b³êdzie.
 */
static char *prepare_batch_line(int argc, char *argv[], int n)
{
        size_t len = 0;
        char *buf;
        int i;

        for (i = n; i < argc; i++)
                len += xstrlen(argv[i]) + 1;

        buf = xmalloc(len);

        for (i = n; i < argc; i++) {
                xstrcat(buf, argv[i]);
                if (i < argc - 1)
                        xstrcat(buf, " ");
        }

        return buf;
}

/*
 * handle_stderr()
 *
 * wy¶wietla to, co uzbiera siê na stderr.
 */
static WATCHER_LINE(handle_stderr)	/* sta³y */
{
/* XXX */
/*	print("stderr", watch); */
	return 0;
}

/**
 * ekg_debug_handler()
 *
 * debug message [if config_debug set] coming direct from libgadu (by libgadu_debug_handler())
 * or by debug() or by debug_ext()<br>
 * XXX, doc more. But function now is ok.
 * 
 * @sa debug_ext()
 *
 * @bug It can happen than internal string_t @a line will be not freed.
 *
 * @param level 
 * @param format
 * @param ap
 *
 */

void ekg_debug_handler(int level, const char *format, va_list ap) {
	static string_t line = NULL;
	char *tmp;

	int is_UI = 0;
	char *theme_format;

	if (!config_debug)
		return;

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
		const size_t tmplen = xstrlen(tmp);
		if (tmplen == 0 || tmp[tmplen - 1] != '\n') {
			line = string_init(tmp);
			xfree(tmp);
			return;
		}
		tmp[tmplen - 1] = 0;			/* remove '\n' */
	}

	switch(level) {
		case 0:				theme_format = "debug";		break;
		case DEBUG_IO:			theme_format = "iodebug";	break;
		case DEBUG_IORECV:		theme_format = "iorecvdebug";	break;
		case DEBUG_FUNCTION:		theme_format = "fdebug";	break;
		case DEBUG_ERROR:		theme_format = "edebug";	break;
		case DEBUG_WHITE:		theme_format = "wdebug";	break;
		default:			theme_format = "debug";		break;
	}

	buffer_add(&buffer_debug, theme_format, tmp);

	query_emit_id(NULL, UI_IS_INITIALIZED, &is_UI);

	if (is_UI && window_debug)
		print_window_w(window_debug, 0, theme_format, tmp);
#if 0			/* STDERR debug */
	else
		fprintf(stderr, "%s\n", tmp);
#endif

	xfree(tmp);
}

struct option ekg_options[] = {
        { "user", required_argument, 0, 'u' },
        { "theme", required_argument, 0, 't' },
        { "no-auto", no_argument, 0, 'n' },
        { "no-mouse", no_argument, 0, 'm' },
        { "no-global-config", no_argument, 0, 'N' },
	{ "frontend", required_argument, 0, 'F' },

        { "away", optional_argument, 0, 'a' },
        { "back", optional_argument, 0, 'b' },
        { "invisible", optional_argument, 0, 'i' },
        { "dnd", optional_argument, 0, 'd' },
        { "free-for-chat", optional_argument, 0, 'f' },
        { "xa", optional_argument, 0, 'x' },

#ifdef USE_UNICODE
	{ "unicode", no_argument, 0, 'U' }, 
#endif

        { "help", no_argument, 0, 'h' },
        { "version", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
};

#define EKG_USAGE N_( \
"Usage: %s [OPTIONS] [COMMANDS]\n" \
"  -u, --user=NAME             uses profile NAME\n" \
"  -t, --theme=FILE            loads theme from FILE\n"\
"  -n, --no-auto               does not connect to server automatically\n" \
"  -m, --no-mouse              does not load mouse support\n" \
"  -N, --no-global-config      ignores global configuration file\n" \
"  -F, --frontend=NAME         uses NAME frontend (default is ncurses)\n" \
\
"  -a, --away[=DESCRIPTION]    changes status to ``away''\n" \
"  -b, --back[=DESCRIPTION]    changes status to ``available''\n" \
"  -i, --invisible[=DESCR]     changes status to ``invisible''\n" \
"  -d, --dnd[=DESCRIPTION]     changes status to ``do not disturb''\n" \
"  -f, --free-for-chat[=DESCR] changes status to ``free for chat''\n" \
"  -x, --xa[=DESCRIPTION]      changes status to ``very busy''\n" \
\
"  -h, --help                  displays this help message\n" \
"  -v, --version               displays program version and exits\n" \
"\n" \
"Options concerned with status depend on the protocol of particular session --\n" \
"some sessions may not support ``do not disturb'' status, etc.\n" \
"\n" )


int main(int argc, char **argv)
{
        int auto_connect = 1, c = 0, no_global_config = 0, no_config = 0, new_status = 0;
        char *tmp = NULL, *new_descr = NULL;
        char *load_theme = NULL, *new_profile = NULL, *frontend = NULL;
        struct passwd *pw;
#ifndef NO_POSIX_SYSTEM
        struct rlimit rlim;
#else
	WSADATA wsaData;
#endif

#ifndef NO_POSIX_SYSTEM
        /* zostaw po sobie core */
        rlim.rlim_cur = RLIM_INFINITY;
        rlim.rlim_max = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &rlim);
#endif
#ifdef NO_POSIX_SYSTEM
	if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
		fprintf(stderr, "WSAStartup() failed? wtf?!.. Oh, I see windows ;>");
	}
#endif

        ekg_started = time(NULL);
        ekg_pid = getpid();

        ekg2_dlinit();
        setlocale(LC_ALL, "");
	tzset();
#ifdef ENABLE_NLS
        bindtextdomain("ekg2",LOCALEDIR);
	textdomain("ekg2");
#endif
        srand(time(NULL));

        strlcpy(argv0, argv[0], sizeof(argv0));

        if (!(home_dir = xstrdup(getenv("HOME")))) {
                if ((pw = getpwuid(getuid())))
                        home_dir = xstrdup(pw->pw_dir);
	}
#ifdef NO_POSIX_SYSTEM
	if (!home_dir)
		home_dir = xstrdup(getenv("USERPROFILE"));

	if (!home_dir)
		home_dir = xstrdup("c:\\");
#endif
	if (!home_dir) {
		fprintf(stderr, _("Can't find user's home directory. Ask administration to fix it.\n"));
		return 1;
	}

        command_init();
#ifndef NO_POSIX_SYSTEM
        signal(SIGSEGV, handle_sigsegv);
        signal(SIGHUP, handle_sighup);
	signal(SIGTERM, handle_sighup);
        signal(SIGUSR1, handle_sigusr1);
        signal(SIGUSR2, handle_sigusr2);
        signal(SIGALRM, SIG_IGN);
        signal(SIGPIPE, SIG_IGN);
#endif
#ifdef USE_UNICODE
        while ((c = getopt_long(argc, argv, "b::a::i::d::f::x::u:F:t:nmNhvU", ekg_options, NULL)) != -1) 
#else
        while ((c = getopt_long(argc, argv, "b::a::i::d::f::x::u:F:t:nmNhv", ekg_options, NULL)) != -1) 
#endif
	{
                switch (c) {
                        case 'a':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_AWAY;
                                xfree(new_descr);
                                new_descr = xstrdup(optarg);
                                break;

                        case 'b':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_AVAIL;
                                xfree(new_descr);
                                new_descr = xstrdup(optarg);
                                break;

                        case 'i':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_INVISIBLE;
                                xfree(new_descr);
                                new_descr = xstrdup(optarg);
                                break;

                        case 'd':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_DND;
                                xfree(new_descr);
                                new_descr = xstrdup(optarg);
                                break;

                        case 'f':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_FFC;
                                xfree(new_descr);
                                new_descr = xstrdup(optarg);
                                break;

                        case 'x':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_XA;
                                xfree(new_descr);
                                new_descr = xstrdup(optarg);
                                break;


                        case 'u':
                                new_profile = optarg;
                                break;
			case 'F':
				frontend = optarg;
				break;
                        case 't':
                                load_theme = optarg;
                                break;

                        case 'n':
                                auto_connect = 0;
                                break;

                        case 'm':
                                no_mouse = 1;
                                break;

                        case 'N':
                                no_global_config = 1;
                                break;


                        case 'h':
                                printf(_(EKG_USAGE), argv[0]);
                                return 0;

#ifdef USE_UNICODE
			case 'U':
				config_use_unicode = 1;
				break;
#endif

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

        in_autoexec = 1;

        if (optind < argc) {
                batch_line = prepare_batch_line(argc, argv, optind);
                batch_mode = 1;
        }

        if ((config_profile = new_profile))
                tmp = saprintf("/%s", config_profile);
        else
                tmp = xstrdup("");

        if (getenv("HOME_ETC"))
                config_dir = saprintf("%s/ekg2%s", getenv("HOME_ETC"), tmp);
        else
                config_dir = saprintf("%s/.ekg2%s", home_dir, tmp);

        xfree(tmp);
        tmp = NULL;

	{
		query_t **ll;

		for (ll = queries; ll <= &queries[QUERY_EXTERNAL]; ll++)
			*ll = NULL;
	}

        variable_init();
        variable_set_default();

        mesg_startup = mesg_set(MESG_CHECK);
#ifdef DEFAULT_THEME 
	if (theme_read(DEFAULT_THEME, 1) == -1) 
#endif
		theme_init();

	window_debug	= window_new(NULL, NULL, -1);                   /* debugowanie */
	window_status	= window_new(NULL, NULL, 1);     		/* okno stanu */
	window_current	= window_status;

        if (!no_global_config)
                config_read(SYSCONFDIR "/ekg2.conf");

        if (frontend) {
                plugin_load(frontend, -254, 1);
		config_changed = 1;
	}

	config_read_plugins();
        if (!no_global_config)
                config_read(SYSCONFDIR "/ekg2-override.conf");

/*        userlist_read(); */
        emoticon_read();
        msg_queue_read();

#ifdef HAVE_NCURSES
        if (!have_plugin_of_class(PLUGIN_UI)) plugin_load(("ncurses"), -254, 1);
#endif
#ifdef HAVE_GTK
	if (!have_plugin_of_class(PLUGIN_UI)) plugin_load(("gtk"), -254, 1);
#endif
#ifdef HAVE_READLINE
	if (!have_plugin_of_class(PLUGIN_UI)) plugin_load(("readline"), -254, 1);
#endif
	if (!have_plugin_of_class(PLUGIN_UI)) fprintf(stderr, "No UI-PLUGIN!\n");
	else {
		struct buffer *b;
		for (b = buffer_debug.data; b; b = b->next)
			print_window_w(window_debug, 0, b->target, b->line);
	}

        if (!have_plugin_of_class(PLUGIN_PROTOCOL)) {
#ifdef HAVE_EXPAT
                plugin_load(("jabber"), -254, 1);
#endif
#ifdef HAVE_LIBGADU
                plugin_load(("gg"), -254, 1);
#endif
                plugin_load(("irc"), -254, 1);
        }
	theme_plugins_init();

	scripts_init();
	config_read(NULL);

        /* je¶li ma byæ theme, niech bêdzie theme */
	if (load_theme)		theme_read(load_theme, 1);
	else if (config_theme)	theme_read(config_theme, 1);

        in_autoexec = 0;

	/* XXX, unidle() was here */

        /* wypada³oby obserwowaæ stderr */
        if (!batch_mode) {
#ifndef NO_POSIX_SYSTEM
                int fd[2];

                if (!pipe(fd)) {
                        fcntl(fd[0], F_SETFL, O_NONBLOCK);
                        fcntl(fd[1], F_SETFL, O_NONBLOCK);
                        watch_add_line(NULL, fd[0], WATCH_READ_LINE, handle_stderr, NULL);
                        stderr_backup = fcntl(2, F_DUPFD, 0);
                        dup2(fd[1], 2);
                }
#endif
        }

        if (!batch_mode && config_display_welcome)
                print("welcome", VERSION);

        protocol_init();
        events_init();
        metacontact_init();
	audio_initialize();
/*	scripts_init();		*/

        /* it has to be done after plugins are loaded, either we wouldn't know if we are
         * supporting some protocol in current build */
        if (session_read(NULL) == -1)
                no_config = 1;

        config_postread();

        /* status window takes first session if not set before*/
	if (!session_current && sessions)
			session_current = sessions;

	if (session_current != window_current->session)
		window_current->session = session_current;

        metacontact_read(); /* read the metacontacts info */

	{
		session_t *s;

		/* wylosuj opisy i zmieñ stany klientów */
		for (s = sessions; s; s = s->next) {
			const char *cmd = NULL;

			if (new_status)
				session_status_set(s, new_status);

			if (new_descr)
				session_descr_set(s, new_descr);

			cmd = ekg_status_string(s->status, 1);

			command_exec_format(NULL, s, 2, ("/%s %s"), cmd, (new_descr) ? new_descr : "");
		}

		/* po zainicjowaniu protoko³ów, po³±cz siê automagicznie ze
		 * wszystkim, co chce siê automagicznie ³±czyæ. */
		for (s = sessions; s; s = s->next) {
			if (auto_connect && session_int_get(s, "auto_connect") == 1)
				command_exec(NULL, s, ("/connect"), 0);
		}
	}

        if (config_auto_save)
                last_save = time(NULL);

	if (no_config) {
#ifdef HAVE_LIBGADU
		if (plugin_find("gg"))
			print("no_config");
		else
			print("no_config_gg_not_loaded");
#else
		print("no_config_no_libgadu");
#endif
	}

	idle_add(NULL, ekg_less_important_handler, &ekg_tv);
        reason_changed = 0;
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

/*
 * ekg_exit()
 *
 * wychodzi z klienta sprz±taj±c przy okazji wszystkie sesje, zwalniaj±c
 * pamiêæ i czyszcz±c pokój.
 */
void ekg_exit()
{
	char *exit_exec = config_exit_exec;
	extern int ekg2_dlclose(void *plugin);
	int i;

	msg_queue_write();

	xfree(last_search_first_name);
	xfree(last_search_last_name);
	xfree(last_search_nickname);
	xfree(last_search_uid);

	windows_save();

	/* setting windows layout */
	if (config_windows_save) {
		const char *vars[] = { "windows_layout", NULL };
		config_write_partly(NULL, vars);
	}

	/* setting default session */
	if (config_sessions_save && session_current) {
		const char *vars[] = { "session_default", NULL };
		xfree(config_session_default); config_session_default = xstrdup(session_current->uid);

		config_write_partly(NULL, vars);
	}

	for (i = 0; i < SEND_NICKS_MAX; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}
	send_nicks_count = 0;

	{
		child_t *c;

		for (c = children; c; c = c->next) {
#ifndef NO_POSIX_SYSTEM
			kill(c->pid, SIGTERM);
#else
			/* TerminateProcess / TerminateThread */
#endif
		}
		children_destroy();
	}

	{
		watch_t *w;

		for (w = watches; w;) {
			watch_t *tmp;
			watch_t *next = w->next;

			if ((tmp = watch_free(w)))
				next = tmp;

			w = next;
		}
	}

	{
		plugin_t *p, *next;

		for (p = plugins; p; p = next) {
			next = p->next;

			if (p->pclass != PLUGIN_UI)
				continue;

			p->destroy();

//			if (p->dl) ekg2_dlclose(p->dl);
		}
	}
	LIST_DESTROY2(watches, NULL);	 watches = NULL;

	if (config_changed && !config_speech_app && config_save_quit == 1) {
		char line[80];

		printf("%s", format_find("config_changed"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[xstrlen(line) - 1] == '\n')
				line[xstrlen(line) - 1] = 0;
			if (!xstrcasecmp(line, "tak") || !xstrcasecmp(line, "yes") || !xstrcasecmp(line, "t") || !xstrcasecmp(line, "y")) {
				if (config_write(NULL) || session_write() || metacontact_write() || script_variables_write())
					printf(_("Error while saving.\n"));
			}
		} else
			printf("\n");
	} else if (config_save_quit == 2) {
		if (config_write(NULL) || session_write() || metacontact_write() || script_variables_write())
			printf(_("Error while saving.\n"));

	} else if (config_keep_reason && reason_changed && config_save_quit == 1) {
		char line[80];

		printf("%s", format_find("quit_keep_reason"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[xstrlen(line) - 1] == '\n')
				line[xstrlen(line) - 1] = 0;
			if (!xstrcasecmp(line, "tak") || !xstrcasecmp(line, "yes") || !xstrcasecmp(line, "t") || !xstrcasecmp(line, "y")) {
				if (session_write())
					printf(_("Error while saving.\n"));
			}
		} else
			printf("\n");

	} else if (config_keep_reason && reason_changed && config_save_quit == 2) {
		if (session_write())
			printf(_("Error while saving.\n"));
	}
	config_exit_exec = NULL; /* avoid freeing it */

/* XXX, think about sequence of unloading. */

	msgs_queue_destroy();
	conference_free();
	newconference_free();
	metacontact_free();
	sessions_free();

	{
		plugin_t *p;

		for (p = plugins; p; ) {
			plugin_t *next = p->next;

			p->destroy();

//			if (p->dl) ekg2_dlclose(p->dl);

			p = next;
		}
	}

	audio_deinitialize();
	alias_free();
	theme_free();
	variable_free();
	script_variables_free(1);
	emoticons_destroy();
	commands_destroy();
	timers_destroy();
	binding_free();
	lasts_destroy();

	buffer_free(&buffer_debug);	buffer_free(&buffer_speech);
	event_free();

	xfree(read_file(NULL, -1));	/* free internal read_file() buffer */

	{
		window_t *w;

		for (w = windows; w; w = w->next) /* XXX: some LIST_ITEM_FREE ? */
			xfree(w->target);

		LIST_DESTROY2(windows, NULL);	window_status = NULL; window_debug = NULL; window_current = NULL;	/* just in case */
	}

	{
		query_t **ll;

		for (ll = queries; ll <= &queries[QUERY_EXTERNAL]; ll++) {
			query_t *q;

			for (q = *ll; q; ) {	/* free other queries... connected by protocol_init() for example */
				query_t *next = q->next;

				query_free(q);

				q = next;
			}

			LIST_DESTROY2(*ll, NULL); /* XXX: really needed? */
		}
	}
	query_external_free();

	xfree(home_dir);

	xfree(config_dir);
	xfree(console_charset);

	mesg_set(mesg_startup);
#ifdef NO_POSIX_SYSTEM
	WSACleanup();
#endif
	close(stderr_backup);

	if (exit_exec) {
#ifndef NO_POSIX_SYSTEM
		execl("/bin/sh", "sh", "-c", exit_exec, NULL);
#else
		/* XXX, like in cmd_exec() */
#endif
		/* should we return some error code if exec() failed?
		 * AKA this line shouldn't be ever reached */
	}

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
