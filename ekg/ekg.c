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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h> // rlimit

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#else
#  include "compat/getopt.h"
#endif
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commands.h"
#include "events.h"
#include "configfile.h"
#include "emoticons.h"
#include "log.h"
#include "metacontacts.h"
#include "msgqueue.h"
#include "protocol.h"
#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif
#include "sessions.h"
#include "stuff.h"
#include "themes.h"
#include "userlist.h"
#include "vars.h"
#include "windows.h"
#include "xmalloc.h"
#include "ltdl.h"

#ifndef PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
#endif

static pid_t ekg_pid = 0;
static char argv0[PATH_MAX];

time_t last_action = 0;

pid_t speech_pid = 0;

static int stderr_backup = 0;

/*
 * ekg_loop()
 *
 * g³ówna pêtla ekg. obs³uguje przegl±danie deskryptorów, timery i wszystko,
 * co ma siê dziaæ w tle.
 */
void ekg_loop()
{
	struct timeval tv;
	list_t l, m;
	fd_set rd, wd;
	int ret, maxfd, pid, status;

	for (;;) {
		/* przejrzyj timery u¿ytkownika, ui, skryptów */
		for (l = timers; l; ) {
			struct timer *t = l->data;
			struct timeval tv;
			struct timezone tz;

			l = l->next;

			gettimeofday(&tv, &tz);

			if (tv.tv_sec > t->ends.tv_sec || (tv.tv_sec == t->ends.tv_sec && tv.tv_usec >= t->ends.tv_usec)) {
				if (!t->persist)
					list_remove(&timers, t, 0);
				else {
					struct timeval tv;
					struct timezone tz;

					gettimeofday(&tv, &tz);
					tv.tv_sec += t->period;
					memcpy(&t->ends, &tv, sizeof(tv));
				}

				t->function(0, t->data);

				if (!t->persist) {
					t->function(1, t->data);
					xfree(t->name);
					xfree(t);
				}
			}
		}

		/* sprawd¼ timeouty ró¿nych deskryptorów */
		for (l = watches; l; ) {
			watch_t *w = l->data;

			l = l->next;
			
			if (w->timeout < 1 || (time(NULL) - w->started) < w->timeout)
				continue;

			if (w->buf) {
				void (*handler)(int, int, char*, void*) = w->handler;
				handler(2, w->fd, NULL, w->data);
			} else {
				void (*handler)(int, int, int, void*) = w->handler;
				handler(2, w->fd, w->type, w->data);
			}
		}
		
		/* sprawd¼ autoawaye ró¿nych sesji */
		for (l = sessions; l; l = l->next) {
			session_t *s = l->data;
			int tmp;

			if (!s->connected || xstrcmp(s->status, EKG_STATUS_AVAIL))
				continue;

			if ((tmp = session_int_get(s, "auto_away")) < 1 || !s->activity)
				continue;

			if (time(NULL) - s->activity > tmp)
				command_exec(NULL, s, "/_autoaway", 0);
		}

		/* auto save */
		if (config_auto_save && config_changed && (time(NULL) - last_save) > config_auto_save) {
			debug("autosaving userlist and config after %d seconds\n", time(NULL) - last_save);
			last_save = time(NULL);

			if (!config_write(NULL) && !session_write()) {
				config_changed = 0;
				reason_changed = 0;
				print("autosaved");
			} else
				print("error_saving");
		}

		/* przegl±danie zdech³ych dzieciaków */
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			debug("child process %d exited with status %d\n", pid, WEXITSTATUS(status));
			
			for (l = children; l; l = m) {
				child_t *c = l->data;

				m = l->next;

				if (pid != c->pid)
					continue;

				if (pid == speech_pid) {
					speech_pid = 0;

					if (!config_speech_app)
						xfree(buffer_flush(BUFFER_SPEECH, NULL));

					if (buffer_count(BUFFER_SPEECH) && !WEXITSTATUS(status)) {
						char *str = buffer_tail(BUFFER_SPEECH);
						say_it(str);
						xfree(str);
					}
				}

				if (c->handler)
					c->handler(c, c->pid, c->name, WEXITSTATUS(status), c->private);

				xfree(c->name);
				list_remove(&children, c, 1);
			}
		}

		/* zerknij na wszystkie niezbêdne deskryptory */
		
		FD_ZERO(&rd);
		FD_ZERO(&wd);

		for (maxfd = 0, l = watches; l; l = l->next) {
			watch_t *w = l->data;

			if (w->fd > maxfd)
				maxfd = w->fd;
			if ((w->type & WATCH_READ))
				FD_SET(w->fd, &rd);
			if ((w->type & WATCH_WRITE))
				FD_SET(w->fd, &wd);
		}

		/* domy¶lny timeout to 1s */
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* ale je¶li który¶ timer ma wyst±piæ wcze¶niej ni¿ za sekundê
		 * to skróæmy odpowiednio czas oczekiwania */
		
		for (l = timers; l; l = l->next) {
			struct timer *t = l->data;
			struct timeval tv2;
			struct timezone tz;
			int usec = 0;

			gettimeofday(&tv2, &tz);

			/* ¿eby unikn±æ przekrêcenia licznika mikrosekund przy
			 * wiêkszych czasach, pomijamy d³ugie timery */

			if (t->ends.tv_sec - tv2.tv_sec > 5)
				continue;
			
			/* zobacz, ile zosta³o do wywo³ania timera */

			usec = (t->ends.tv_sec - tv2.tv_sec) * 1000000 + (t->ends.tv_usec - tv2.tv_usec);

			/* je¶li wiêcej ni¿ sekunda, to nie ma znacznia */
			
			if (usec >= 1000000)
				continue;

			/* je¶li mniej ni¿ aktualny timeout, zmniejsz */

			if (tv.tv_sec * 1000000 + tv.tv_usec > usec) {
				tv.tv_sec = 0;
				tv.tv_usec = usec;
			}
		}

		/* na wszelki wypadek sprawd¼ warto¶ci */
		
		if (tv.tv_sec < 0)
			tv.tv_sec = 0;

		if (tv.tv_usec < 0)
			tv.tv_usec = 1;
		
		/* sprawd¼, co siê dzieje */

		ret = select(maxfd + 1, &rd, &wd, NULL, &tv);
	
		/* je¶li wyst±pi³ b³±d, daj znaæ */

		if (ret == -1) {
			/* jaki¶ plugin doda³ do watchów z³y deskryptor. ¿eby
			 * ekg mog³o dzia³aæ dalej, sprawd¼my który to i go
			 * usuñmy z listy. */
			if (errno == EBADF) {
				for (l = watches; l; ) {
					watch_t *w = l->data;
					struct stat st;
					
					l = l->next;

					if (!fstat(w->fd, &st))
						continue;

					debug("select(): bad file descriptor: fd=%d, type=%d, plugin=%s\n", w->fd, w->type, (w->plugin) ? w->plugin->name : "none");

					watch_free(w);
				}

				continue;
			}

			if (errno != EINTR)
				debug("select() failed: %s\n", strerror(errno));

			continue;
		}

watches_again:
		/* przejrzyj deskryptory */
		for (l = watches; l; ) {
			watch_t *w = l->data;

			l = l->next;
			
			if ((!FD_ISSET(w->fd, &rd) && !FD_ISSET(w->fd, &wd)))
				continue;

			if (w->fd == 0) {
				for (l = sessions; l; l = l->next) {
					session_t *s = l->data;

					if (!s->connected || !s->autoaway)
						continue;

					if (session_int_get(s, "auto_back") != 2)
						continue;

					command_exec(NULL, s, "/_autoback", 2);
				}
			}

			if (!w->buf) {
				ekg_stdin_want_more = 0;

				if (((w->type == WATCH_WRITE) && FD_ISSET(w->fd, &wd)) || 
				    ((w->type == WATCH_READ) && FD_ISSET(w->fd, &rd)))
					watch_handle(w);

				if (ekg_stdin_want_more && w->fd == 0)
					goto watches_again;
			}
			else
				if (FD_ISSET(w->fd, &rd))
					watch_handle_line(w);

		}
	}
	
	return;
}

static void handle_sigusr1()
{
	debug("sigusr1 received\n");
	query_emit(NULL, "sigusr1");
	signal(SIGUSR1, handle_sigusr1);
}

static void handle_sigusr2()
{
	debug("sigusr2 received\n");
	query_emit(NULL, "sigusr2");
	signal(SIGUSR2, handle_sigusr2);
}

static void handle_sighup()
{
	ekg_exit();
}

static void handle_sigsegv()
{
	list_t l;

	signal(SIGSEGV, SIG_DFL);

	if (stderr_backup)
		dup2(stderr_backup, 2);

	/* wy³±cz pluginy ui, ¿eby odda³y terminal */
	for (l = plugins; l; l = l->next) {
		plugin_t *p = l->data;

		if (p->pclass != PLUGIN_UI)
			continue;

		p->destroy();
	}

	fprintf(stderr, 
"\r\n"
"\r\n"
"*** Naruszenie ochrony pamiêci ***\r\n"
"\r\n"
"Spróbujê zapisaæ ustawienia, ale nie obiecujê, ¿e cokolwiek z tego\r\n"
"wyjdzie. Trafi± one do plików %s/config.%d\r\n"
"oraz %s/userlist.%d\r\n"
"\r\n"
"Do pliku %s/debug.%d zapiszê ostatanie komunikaty\r\n"
"z okna debugowania.\r\n"
"\r\n"
"Je¶li zostanie utworzony plik %s/core, spróbuj uruchomiæ\r\n"
"polecenie:\r\n"
"\r\n"
"    gdb %s %s/core\r\n"
"\n"
"zanotowaæ kilka ostatnich linii, a nastêpnie zanotowaæ wynik polecenia\r\n"
",,bt''. Dziêki temu autorzy dowiedz± siê, w którym miejscu wyst±pi³ b³±d\r\n"
"i najprawdopodobniej pozwoli to unikn±æ tego typu sytuacji w przysz³o¶ci.\r\n"
"Wiêcej szczegó³ów w dokumentacji, w pliku ,,gdb.txt''.\r\n"
"\r\n",
config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, argv0, config_dir);

	config_write_crash();
	userlist_write_crash();
	debug_write_crash();

	raise(SIGSEGV);			/* niech zrzuci core */
}

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
static void handle_stderr(int type, int fd, char *buf, void *data)
{
	print("stderr", buf);
}

/*
 * ekg_debug_handler()
 *
 * obs³uguje informacje debugowania libgadu i klienta.
 */
void ekg_debug_handler(int level, const char *format, va_list ap)
{
	static string_t line = NULL;
	char *tmp;
	int is_UI = 0;

	if (!config_debug)
		return;

	query_emit(NULL, "ui-is-initialized", &is_UI);

	if (!is_UI) {
		/* printf(format, ap); */ /* uncomment for debuging */
		return;
	}

	tmp = vsaprintf(format, ap);

	if (line) {
		string_append(line, tmp);
		xfree(tmp);
		tmp = NULL;

		if (line->str[xstrlen(line->str) - 1] == '\n') {
			tmp = string_free(line, 0);
			line = NULL;
		}
	} else {
		if (tmp[xstrlen(tmp) - 1] != '\n') {
			line = string_init(tmp);
			xfree(tmp);
			tmp = NULL;
		}
	}
		
	if (!tmp)
		return;

	tmp[xstrlen(tmp) - 1] = 0;

	buffer_add(BUFFER_DEBUG, NULL, tmp, DEBUG_MAX_LINES);
	print_window("__debug", NULL, 0, "debug", tmp);
	xfree(tmp);
}

struct option ekg_options[] = {
	{ "back", optional_argument, 0, 'b' },
	{ "away", optional_argument, 0, 'a' },
	{ "invisible", optional_argument, 0, 'i' },
	{ "dnd", optional_argument, 0, 'd' },
	{ "chat", optional_argument, 0, 'f' },
	{ "xa", optional_argument, 0, 'x' },
	{ "private", no_argument, 0, 'p' },
	{ "no-auto", no_argument, 0, 'n' },
	{ "frontend", required_argument, 0, 'f' },
	{ "help", no_argument, 0, 'h' },
	{ "theme", required_argument, 0, 't' },
	{ "user", required_argument, 0, 'u' },
	{ "version", no_argument, 0, 'v' },
	{ "no-global-config", no_argument, 0, 'N' },
	{ 0, 0, 0, 0 }
};

#define EKG_USAGE \
"u¿ycie: %s [OPCJE] [KOMENDY]\n" \
"  -N, --no-global-config     ignoruje globalny plik konfiguracyjny\n" \
"  -u, --user=NAZWA           korzysta z profilu o podanej nazwie\n" \
"  -t, --theme=PLIK           ³aduje opis wygl±du z podanego pliku\n" \
"  -n, --no-auto              nie ³±czy siê automatycznie z serwerem\n" \
"  -a, --away[=OPIS]          zmienia stan na ,,zajêty''\n" \
"  -b, --back[=OPIS]          zmienia stan na ,,dostêpny''\n" \
"  -i, --invisible[=OPIS]     zmienia stan na ,,niewidoczny''\n" \
"  -d, --dnd[=OPIS]           zmienia stan na ,,nie przeszkadzaæ''\n" \
"  -f, --free-for-chat[=OPIS] zmienia stan na ,,chêtny do rozmowy''\n" \
"  -x, --xa[=OPIS]            zmienia stan na ,,bardzo zajêty''\n" \
"  -v, --version              wy¶wietla wersje programu i wychodzi\n" \
"\n" \
"Opcje dotycz±ce stanu zale¿± od w³a¶ciwo¶ci protoko³u danej sesji --\n" \
"niektóre sesje mog± nie obs³ugiwaæ stanu ,,nie przeszkadzaæ'' itp.\n" \
"\n"


int main(int argc, char **argv)
{
	int auto_connect = 1, c = 0, no_global_config = 0, no_config = 0;
	char *tmp = NULL, *new_status = NULL, *new_descr = NULL;
	char *load_theme = NULL, *new_profile = NULL;
	struct passwd *pw;
	struct rlimit rlim;
	list_t l;

	/* zostaw po sobie core */
	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);

	ekg_started = time(NULL);
        ekg_pid = getpid();

	lt_dlinit();

	srand(time(NULL));

	strlcpy(argv0, argv[0], sizeof(argv0));

	if (!(home_dir = xstrdup(getenv("HOME")))) {
		if ((pw = getpwuid(getuid())))
			home_dir = xstrdup(pw->pw_dir);

		if (!home_dir) {
			fprintf(stderr, "Nie mogê znale¼æ katalogu domowego. Popro¶ administratora, ¿eby to naprawi³.\n");
			return 1;
		}
	}

	command_init();

	signal(SIGSEGV, handle_sigsegv);
	signal(SIGHUP, handle_sighup);
	signal(SIGUSR1, handle_sigusr1);
	signal(SIGUSR2, handle_sigusr2);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	while ((c = getopt_long(argc, argv, "b::a::i::d::x::pnc:hot:u:vN", ekg_options, NULL)) != -1) {
		switch (c) {
			case 'b':
				if (!optarg && argv[optind] && argv[optind][0] != '-')
					optarg = argv[optind++];

				new_status = EKG_STATUS_AWAY;
				xfree(new_descr);
				new_descr = xstrdup(optarg);
			        break;
				
			case 'a':
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
				
                        case 'r':
                                if (!optarg && argv[optind] && argv[optind][0] != '-')
                                        optarg = argv[optind++];

                                new_status = EKG_STATUS_FREE_FOR_CHAT;
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

			case 'n':
				auto_connect = 0;
				break;

			case 'N':
				no_global_config = 1;
				break;

			case 'h':
				printf(EKG_USAGE, argv[0]);
				return 0;

			case 'u':
				new_profile = optarg;
				break;

			case 't':
				load_theme = optarg;
				break;

			case 'v':
			    	printf("ekg-%s (compiled on %s)\n", VERSION, compile_time());
				return 0;

			case '?':
				/* obs³ugiwane przez getopt */
				fprintf(stdout, "Aby uzyskaæ wiêcej informacji, uruchom program z opcj± --help.\n");
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
		config_dir = saprintf("%s/ekg%s", getenv("HOME_ETC"), tmp);
	else
		config_dir = saprintf("%s/.ekg%s", home_dir, tmp);

	xfree(tmp);
	tmp = NULL;

	variable_init();
	variable_set_default();

	mesg_startup = mesg_set(MESG_CHECK);

	theme_init();

	window_new(NULL, NULL, -1);			/* debugowanie */
	window_current = window_new(NULL, NULL, 1);	/* okno stanu */

	if (!no_global_config)
		config_read(SYSCONFDIR "/ekg.conf");

	config_read(NULL);

	if (!no_global_config)
		config_read(SYSCONFDIR "/ekg-override.conf");

/*        userlist_read(); */
	emoticon_read();
	msg_queue_read();

#ifdef HAVE_NCURSES
	if (!have_plugin_of_class(PLUGIN_UI)) plugin_load("ncurses", 1);
#endif

        if (!have_plugin_of_class(PLUGIN_PROTOCOL)) {
#ifdef HAVE_EXPAT
                plugin_load("jabber", 1);
#endif
#ifdef HAVE_LIBGADU
                plugin_load("gg", 1);
#endif
	}

	config_read_later(NULL);

	/* je¶li ma byæ theme, niech bêdzie theme */
	if (load_theme)
		theme_read(load_theme, 1);
	else {
		if (config_theme)
			theme_read(config_theme, 1);
	}

	theme_cache_reset();

	in_autoexec = 0;

	time(&last_action);

	/* wypada³oby obserwowaæ stderr */
	if (!batch_mode) {
		int fd[2];

		if (!pipe(fd)) {
			fcntl(fd[0], F_SETFL, O_NONBLOCK);
			fcntl(fd[1], F_SETFL, O_NONBLOCK);
			watch_add(NULL, fd[0], WATCH_READ_LINE, 1, handle_stderr, NULL);
			stderr_backup = fcntl(2, F_DUPFD, 0);
			dup2(fd[1], 2);
		}
	}

	if (!batch_mode && config_display_welcome)
		print("welcome", VERSION);

	if (!config_log_path)
		config_log_path = xstrdup(prepare_path("history", 0));

	protocol_init();
	events_init();
	metacontact_init();

	/* it has to be done after plugins are loaded, either we wouldn't know if we are
	 * supporting some protocol in current build */
	if (session_read() == -1)
		no_config = 1;

	/* status window takes first session */
	if (sessions) {
		session_current = (session_t*) sessions->data;
		window_current->session = (session_t*) sessions->data;
	}
	
	metacontact_read(); /* read the metacontacts info */

	/* wylosuj opisy i zmieñ stany klientów */
	for (l = sessions; l; l = l->next) {
		session_t *s = l->data;
		const char *cmd = NULL;
		char *tmp;

		if (new_status)
			session_status_set(s, new_status);

		if (new_descr)
			session_descr_set(s, new_descr);
		
		if (!xstrcmp(s->status, EKG_STATUS_AVAIL) || !xstrcmp(s->status, EKG_STATUS_NA))
			cmd = "back";

		if (!cmd)
			cmd = s->status;

		tmp = saprintf("/%s %s", cmd, (new_descr) ? new_descr : "");
		command_exec(NULL, s, tmp, 1);
		xfree(tmp);
	}


	if (!sessions)
		goto no_sessions;
	/* po zainicjowaniu protoko³ów, po³±cz siê automagicznie ze
	 * wszystkim, co chce siê automagicznie ³±czyæ. */
	for (l = sessions, (l->prev) ? l = l->prev : l; l; l = l->next) {
		session_t *s = l->data;

		if (auto_connect && session_int_get(s, "auto_connect") == 1)
			command_exec(NULL, s, "/connect", 0);
	}

no_sessions:
	if (config_auto_save)
		last_save = time(NULL);

	if (no_config)
                print("no_config");

	reason_changed = 0;

	/* krêæ imprezê */
	while (1) {
		ekg_loop();
		debug("ekg_loop() exited. not good. big bada boom!\n");
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
	char **vars = NULL;
	list_t l;
	int i;

	msg_queue_write();

	xfree(last_search_first_name);
	xfree(last_search_last_name);
	xfree(last_search_nickname);
	xfree(config_reason_first);

	if (config_windows_save)
		array_add(&vars, xstrdup("windows_layout"));

	if (vars) {
		config_write_partly(vars);
		array_free(vars);
	}

	for (i = 0; i < SEND_NICKS_MAX; i++) {
		xfree(send_nicks[i]);
		send_nicks[i] = NULL;
	}
	send_nicks_count = 0;

	for (l = children; l; l = l->next) {
		child_t *c = l->data;

		kill(c->pid, SIGTERM);
		xfree(c->name);
	}

	for (l = watches; l; ) {
		watch_t *w = l->data;

		l = l->next;

		watch_free(w);
	}

	for (l = plugins; l; ) {
		plugin_t *p = l->data;

		l = l->next;

		p->destroy();
	} 

	list_destroy(watches, 0);

	if (config_changed && !config_speech_app && config_save_quit == 1) {
		char line[80];

		printf("%s", format_find("config_changed"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[xstrlen(line) - 1] == '\n')
				line[xstrlen(line) - 1] = 0;
			if (!xstrcasecmp(line, "tak") || !xstrcasecmp(line, "yes") || !xstrcasecmp(line, "t") || !xstrcasecmp(line, "y")) {
        			if (config_write(NULL) || session_write() || metacontact_write()) 
					printf("Wyst±pi³ b³±d podczas zapisu.\n");
			}
		} else
			printf("\n");
	} else if (config_save_quit == 2) {
                if (config_write(NULL) || session_write() || metacontact_write())
	                printf("Wyst±pi³ b³±d podczas zapisu.\n");

	} else if (config_keep_reason && reason_changed && config_save_quit == 1) {
                char line[80];

                printf("%s", format_find("quit_keep_reason"));
                fflush(stdout);
                if (fgets(line, sizeof(line), stdin)) {
                        if (line[xstrlen(line) - 1] == '\n')
                                line[xstrlen(line) - 1] = 0;
                        if (!xstrcasecmp(line, "tak") || !xstrcasecmp(line, "yes") || !xstrcasecmp(line, "t") || !xstrcasecmp(line, "y")) {
                                if (session_write())
                                        printf("Wyst±pi³ b³±d podczas zapisu.\n");
                        }
                } else
                        printf("\n");

	} else if (config_keep_reason && reason_changed && config_save_quit == 2) {
	        if (session_write())
        	        printf("Wyst±pi³ b³±d podczas zapisu.\n");
	}

	msg_queue_free();
	alias_free();
	conference_free();
	sessions_free();
	theme_free();
	variable_free();
	emoticon_free();
	command_free();
	timer_free();
	binding_free();
	last_free();
	buffer_free();
	event_free();
	metacontact_free();

	list_destroy(windows, 1);

	xfree(home_dir);

	xfree(config_dir);

	mesg_set(mesg_startup);

	exit(0);
}
