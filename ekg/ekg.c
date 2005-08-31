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
#include <locale.h>
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
#include "scripts.h"
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

int no_mouse = 0;

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
                watch_t *watch_last;

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

				/* sprawd¼ scroll timeouty */
				/* XXX: nie tworzyæ variabla globalnego! */
				for (l = sessions; l; l = l->next) {
						session_t *s = l->data;
						int tmp;

						if (!s->connected)
								continue;

						if (!(tmp = session_int_get(s, "scroll_long_desc")) || tmp == -1)
								continue;

						if (time(NULL) - s->scroll_last > tmp)
								command_exec(NULL, s, "/_autoscroll", 0);
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
                /* zapamiêtaj ostatni deskryptor */
                for (watch_last = NULL, l = watches; l; l = l->next) {
                        if (!l->next)
                                watch_last = l->data;
                }

                /* przejrzyj deskryptory */
                for (l = watches; l; ) {
                        watch_t *w = l->data;

                        /* handlery mog± dodawaæ kolejne watche, wiêc je¶li
                         * dotrzemy do ostatniego sprzed wywo³ania pêtli,
                         * koñczymy pracê. */
                        l = (w != watch_last) ? l->next : NULL;

                        if ((!FD_ISSET(w->fd, &rd) && !FD_ISSET(w->fd, &wd)))
                                continue;

                        if (w->fd == 0) {
				list_t session_list;
                                for (
						session_list = sessions;
						session_list;
						session_list = session_list->next)
				{
                                        session_t *s = session_list->data;

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
"wyjdzie. Trafi± one do plików %s/config.%d,\r\n"
"%s/config-<plugin>.%d oraz %s/userlist.%d\r\n"
"\r\n"
"Do pliku %s/debug.%d zapiszê ostatanie komunikaty\r\n"
"z okna debugowania.\r\n"
"\r\n"
"Je¶li zostanie utworzony plik %s/core, spróbuj uruchomiæ\r\n"
"polecenie:\r\n"
"\r\n"
"    gdb %s %s/core.%d\r\n"
"\n"
"zanotowaæ kilka ostatnich linii, a nastêpnie zanotowaæ wynik polecenia\r\n"
",,bt''. Dziêki temu autorzy dowiedz± siê, w którym miejscu wyst±pi³ b³±d\r\n"
"i najprawdopodobniej pozwoli to unikn±æ tego typu sytuacji w przysz³o¶ci.\r\n"
"Wiêcej szczegó³ów w dokumentacji, w pliku ,,gdb.txt''.\r\n"
"\r\n",
config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, (int) getpid(), config_dir, argv0, config_dir, (int) getpid());

        config_write_crash();
        userlist_write_crash();
        debug_write_crash();

        raise(SIGSEGV);                 /* niech zrzuci core */
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
        { "user", required_argument, 0, 'u' },
        { "theme", required_argument, 0, 't' },
        { "no-auto", no_argument, 0, 'n' },
        { "no-mouse", no_argument, 0, 'm' },
        { "no-global-config", no_argument, 0, 'N' },

        { "away", optional_argument, 0, 'a' },
        { "back", optional_argument, 0, 'b' },
        { "invisible", optional_argument, 0, 'i' },
        { "dnd", optional_argument, 0, 'd' },
        { "free-for-chat", optional_argument, 0, 'f' },
        { "xa", optional_argument, 0, 'x' },

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

        setlocale(LC_ALL, "");
        bindtextdomain("ekg2",LOCALEDIR);
        textdomain("ekg2");

        srand(time(NULL));

        strlcpy(argv0, argv[0], sizeof(argv0));

        if (!(home_dir = xstrdup(getenv("HOME")))) {
                if ((pw = getpwuid(getuid())))
                        home_dir = xstrdup(pw->pw_dir);

                if (!home_dir) {
                        fprintf(stderr, _("Can't find user's home directory. Ask administration to fix it.\n"));
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

        while ((c = getopt_long(argc, argv, "b::a::i::d::f::x::u:t:nmNhv", ekg_options, NULL)) != -1) {
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


                        case 'u':
                                new_profile = optarg;
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

                        case 'v':
                                printf("ekg-%s (compiled on %s)\n", VERSION, compile_time());
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

        variable_init();
        variable_set_default();

        mesg_startup = mesg_set(MESG_CHECK);

        theme_init();

        window_new(NULL, NULL, -1);                     /* debugowanie */
        window_current = window_new(NULL, NULL, 1);     /* okno stanu */

        if (!no_global_config)
                config_read(SYSCONFDIR "/ekg2.conf");

	config_read_plugins();

        if (!no_global_config)
                config_read(SYSCONFDIR "/ekg2-override.conf");

/*        userlist_read(); */
        emoticon_read();
        msg_queue_read();

#ifdef HAVE_NCURSES
        if (!have_plugin_of_class(PLUGIN_UI)) plugin_load("ncurses", -254, 1);
#endif

        if (!have_plugin_of_class(PLUGIN_PROTOCOL)) {
#ifdef HAVE_EXPAT
                plugin_load("jabber", -254, 1);
#endif
#ifdef HAVE_LIBGADU
                plugin_load("gg", -254, 1);
#endif
                plugin_load("irc", -254, 1);
        }
	scripts_init();
	config_read(NULL);

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
//	scripts_init();

        /* it has to be done after plugins are loaded, either we wouldn't know if we are
         * supporting some protocol in current build */
        if (session_read() == -1)
                no_config = 1;

        config_postread();

        /* status window takes first session if not setted before*/
	if (!session_current && sessions)
			session_current = (session_t*) sessions->data;

	if (session_current != window_current->session)
		window_current->session = session_current;

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
                command_exec(NULL, s, tmp, 2);
                xfree(tmp);
        }


        if (!sessions)
                goto no_sessions;
        /* po zainicjowaniu protoko³ów, po³±cz siê automagicznie ze
         * wszystkim, co chce siê automagicznie ³±czyæ. */
        for (l = sessions; l; l = l->next) {
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

        /* setting default session */
        if (config_sessions_save && session_current) {
                session_int_set(session_current, "default", 1);
//		config_changed = 1;
	}

        xfree(last_search_first_name);
        xfree(last_search_last_name);
        xfree(last_search_nickname);
        xfree(config_reason_first);

        windows_save();

        if (config_windows_save) {
                array_add(&vars, xstrdup("windows_layout"));
	}

        if (vars) {
                config_write_partly(NULL, vars);
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

                if (p->pclass != PLUGIN_UI)
                        continue;

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

        for (l = plugins; l; ) {
                plugin_t *p = l->data;

                l = l->next;

                p->destroy();
        }

        msg_queue_free();
        alias_free();
        conference_free();
        metacontact_free();
        sessions_free();
        theme_free();
        variable_free();
	script_variables_free(1);
        emoticon_free();
        command_free();
        timer_free();
        binding_free();
        last_free();
        buffer_free();
        event_free();
	

        for (l = windows; l; l = l->next) {
                window_t *w = l->data;

                if (!w)
                        continue;

                xfree(w->target);
        }

        list_destroy(windows, 1);

        xfree(home_dir);

        xfree(config_dir);

        mesg_set(mesg_startup);

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
