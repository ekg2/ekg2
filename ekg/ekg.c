/* $Id$ */

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

#include "ekg2.h"

#include <glib/gprintf.h>
#include <glib/gstdio.h>

#include <sys/types.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/ioctl.h>
#endif

#include <sys/stat.h>
#include <sys/time.h>

#ifndef NO_POSIX_SYSTEM
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>	/* rlimit */
#endif

#include <sys/select.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

#include "emoticons.h"
#include "internal.h"
#include "scripts.h"

char *config_dir;
int mesg_startup;
static char argv0[PATH_MAX];
static gchar last_err_message[128] = {0};

pid_t speech_pid = 0;

static int stderr_backup = -1;

int no_mouse = 0;

/**
 * ekg_autoaway_timer()
 *
 *
 * less important things which don't need to be checked every main loop iteration
 * e.g. autoaways 
 *
 * executed each second.
 */
static TIMER(ekg_autoaway_timer) {
	session_t *sl;
	time_t t;

	if (type)
		return 0;

	t = time(NULL);

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

			if (t - s->activity > tmp)
				command_exec(NULL, s, ("/_autoaway"), 0);
		} while (0);

		do {
			if ((tmp = session_int_get(s, "auto_xa")) < 1 || !s->activity)
				break;

			if (t - s->activity > tmp)
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
	g_main_context_iteration(NULL, FALSE);
	{

#ifdef WATCHES_FIXME
		{		/* przejrzyj deskryptory */
			list_t l;

			for (l = watches; l; l = l->next) {
				watch_t *w = l->data;

				if (!w)
					continue;

				if (!FD_ISSET(w->fd, &rd) && !FD_ISSET(w->fd, &wd)) { /* timeout checking */
					if (w->timeout < 1 || (tv.tv_sec - w->started) < w->timeout)
						continue;
					w->removed = -1;
					if (w->buf) {
						int (*handler)(int, int, char*, void*) = w->handler;
						if (handler(2, w->fd, NULL, w->data) == -1 || w->removed == 1) {
							w->removed = 0;
							watch_free(w);
							continue;
						}
					} else {
						int (*handler)(int, int, int, void*) = w->handler;
						if (handler(2, w->fd, w->type, w->data) == -1 || w->removed == 1) {
							w->removed = 0;
							watch_free(w);
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

						if (session_int_get(s, "auto_back") == 2)
							command_exec(NULL, s, ("/_autoback"), 2);
					}
				}
			}
		}
#endif

	}
#undef tv

	return;
}
#ifndef NO_POSIX_SYSTEM
static void handle_sigusr1()
{
	debug("sigusr1 received\n");
	query_emit(NULL, "ekg-sigusr1");
	signal(SIGUSR1, handle_sigusr1);
}

static void handle_sigusr2()
{
	debug("sigusr2 received\n");
	query_emit(NULL, "ekg-sigusr2");
	signal(SIGUSR2, handle_sigusr2);
}

static void handle_sighup()
{
	ekg_exit();
}

/**
 * Notifies plugins, writes the message to stderr, with last_err_message.
 *
 * We are only allowed to call POSIX-defined "async-signal-safe functions" here
 * (see signal(7)), because anything else can result in a "bad thing" such as a
 * deadlock (e.g. if the signal was a result of a malloc()/free()) or a
 * segfault. There were cases of this happening to ekg2.
 *
 * In order to avoid touching the (possibly corrupt) heap, we invoke statically
 * stored set of abort handlers. This is the mechanism we use to notify UI and
 * logs plugins to reset the terminal and sync their output as necessary.
 */
static void handle_fatal_signal(char *message)
{
	const char *err_msg_prefix = "Last error message (if any): ";
	const char *debug_instructions = "If a file called core is be created, try running the following\r\n"
	"command:\r\n"
	"\r\n"
	"    gdb ekg2 core\r\n"
	"\n"
	"note the last few lines, and then note the output from the ,,bt'' command.\r\n"
	"This will help the program authors find the location of the problem\r\n"
	"and most likely will help avoid such crashes in the future.\r\n";

	if (stderr_backup && stderr_backup != -1)
		dup2(stderr_backup, 2);

	/* Notify plugins of impending doom. */
	ekg2_run_all_abort_handlers();

	/* Now that the terminal is (hopefully) back to plain text mode, write messages. */
	/* There is nothing we can do if this fails, so suppress warnings about ignored results. */
	IGNORE_RESULT(write(2, "\r\n\r\n *** ", 9));
	IGNORE_RESULT(write(2, message, strlen(message)));
	IGNORE_RESULT(write(2, " ***\r\n", 6));
	IGNORE_RESULT(write(2, err_msg_prefix, strlen(err_msg_prefix)));
	IGNORE_RESULT(write(2, last_err_message, strlen(last_err_message)));
	IGNORE_RESULT(write(2, "\r\n", 2));
	IGNORE_RESULT(write(2, debug_instructions, strlen(debug_instructions)));
}

/* See handle_fatal_signal comment. */
static void handle_sigabrt()
{
	signal(SIGABRT, SIG_DFL);
	handle_fatal_signal("Abnormal program termination");
	raise(SIGABRT);
}

/* See handle_fatal_signal comment. */
static void handle_sigsegv()
{
	signal(SIGSEGV, SIG_DFL);
	handle_fatal_signal("Segmentation violation detected");
	raise(SIGSEGV);
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
static char *prepare_batch_line(char *argv[], int n)
{
	size_t len = 0;
	char *buf;
	int i;

	for (i = n; argv[i]; i++)
		len += xstrlen(argv[i]) + 1;

	buf = xmalloc(len);

	for (i = n; argv[i]; i++) {
		xstrcat(buf, argv[i]);
		if (argv[i+1])
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
	static GString *line = NULL;
	char *tmp = NULL;

	char *theme_format;
	int is_UI = 0;

	if (!config_debug)
		return;

	if (line) {
		g_string_append_vprintf(line, format, ap);

		if (line->len == 0 || line->str[line->len - 1] != '\n')
			return;

		line->str[line->len - 1] = '\0';	/* remove '\n' */
		tmp = g_string_free(line, FALSE);
		line = NULL;
	} else {
		int tmplen = g_vasprintf(&tmp, format, ap);

		if (tmplen < 0 || !tmp)	/* OutOfMemory? */
			return;

		if (tmplen == 0 || tmp[tmplen - 1] != '\n') {
			line = g_string_new_len(tmp, tmplen);
			g_free(tmp);
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
		case DEBUG_WARN:		theme_format = "warndebug";	break;
		case DEBUG_OK:			theme_format = "okdebug";	break;
		default:			theme_format = "debug";		break;
	}

	ekg_fix_utf8(tmp); /* debug message can contain random data */
	buffer_add(&buffer_debug, theme_format, tmp);

	query_emit(NULL, "ui-is-initialized", &is_UI);

	if (is_UI && window_debug) {
		print_window_w(window_debug, EKG_WINACT_NONE, theme_format, tmp);
	}
#ifdef STDERR_DEBUG	/* STDERR debug */
	else
		fprintf(stderr, "%s\n", tmp);
#endif
	xfree(tmp);
}

static void glib_debug_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
	static int recurse = 0;

	if (recurse)
		return;

	if (!config_debug)
		return;

	recurse++;
	debug("[%s] %s\n", log_domain, message);
	recurse--;
}

static void glib_print_handler(const gchar *string) {
	debug("%s", string);
}

static void glib_printerr_handler(const gchar *string) {
	g_strlcpy(last_err_message, string, sizeof(last_err_message));

	debug_error("%s", string);
}

static GOptionEntry ekg_options[] = {
	{ "user", 'u', 0, G_OPTION_ARG_STRING, NULL,
		"uses profile NAME", "NAME" },
	{ "theme", 't', 0, G_OPTION_ARG_STRING, NULL,
		"loads theme from FILE", "FILE" },
	{ "no-auto", 'n', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL,
		"does not connect to server automatically", NULL },
	{ "no-mouse", 'm', 0, G_OPTION_ARG_NONE, &no_mouse,
		"does not load mouse support", NULL },
	{ "no-global-config", 'N', 0, G_OPTION_ARG_NONE, NULL,
		"ignores global configuration file", NULL },
	{ "frontend", 'F', 0, G_OPTION_ARG_STRING, NULL,
		"uses NAME frontend (default is ncurses)", "NAME" },

	{ "away", 'a', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, NULL,
		"changes status to ``away''", "[DESCRIPTION]" },
	{ "back", 'b', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, NULL,
		"changes status to ``available''", "[DESCRIPTION]" },
	{ "invisible", 'i', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, NULL,
		"changes status to ``invisible''", "[DESCRIPTION]" },
	{ "dnd", 'd', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, NULL,
		"changes status to ``do not disturb''", "[DESCRIPTION]" },
	{ "free-for-chat", 'f', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, NULL,
		"changes status to ``free for chat''", "[DESCRIPTION]" },
	{ "xa", 'x', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, NULL,
		"changes status to ``very busy''", "[DESCRIPTION]" },

	{ "version", 'v', 0, G_OPTION_ARG_NONE, NULL,
		"print program version and exit", NULL },

	{ NULL }
};

struct option_callback_args {
	gint *new_status;
	gchar **new_descr;
};

gboolean set_status_callback(const gchar *optname, const gchar *optval,
		gpointer data, GError **error)
{
	struct option_callback_args *args = data;

	gchar c = optname[1] == '-' ? optname[2] : optname[1];
	switch (c) {
		case 'a': *args->new_status = EKG_STATUS_AWAY; break;
		case 'b': *args->new_status = EKG_STATUS_AVAIL; break;
		case 'd': *args->new_status = EKG_STATUS_DND; break;
		case 'f': *args->new_status = EKG_STATUS_FFC; break;
		case 'i': *args->new_status = EKG_STATUS_INVISIBLE; break;
		case 'x': *args->new_status = EKG_STATUS_XA; break;
	}
	xfree(*args->new_descr);
	*args->new_descr = xstrdup(optval);

	return TRUE;
}

int main(int argc, char **argv)
{
	gint auto_connect = 1, no_global_config = 0, no_config = 0, new_status = 0, print_version = 0;
	char *tmp = NULL, *new_descr = NULL;
	gchar *load_theme = NULL, *new_profile = NULL, *frontend = NULL;
	GError *err = NULL;
#ifndef NO_POSIX_SYSTEM
	struct rlimit rlim;
#else
	WSADATA wsaData;
#endif

	g_type_init();

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

	setlocale(LC_ALL, "");
	tzset();
#ifdef ENABLE_NLS
	bindtextdomain("ekg2",LOCALEDIR);
	textdomain("ekg2");
#endif
	srand(time(NULL));

	g_strlcpy(argv0, argv[0], sizeof(argv0));

	home_dir = xstrdup(getenv("HOME"));

#ifndef NO_POSIX_SYSTEM
	if (!home_dir) {
		struct passwd *pw;

		if ((pw = getpwuid(getuid())))
			home_dir = xstrdup(pw->pw_dir);
	}
#else
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
	signal(SIGABRT, handle_sigabrt);
	signal(SIGSEGV, handle_sigsegv);
	signal(SIGHUP, handle_sighup);
	signal(SIGTERM, handle_sighup);
	signal(SIGUSR1, handle_sigusr1);
	signal(SIGUSR2, handle_sigusr2);
	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
#endif

	ekg_options[0].arg_data = &new_profile;
	ekg_options[1].arg_data = &load_theme;
	ekg_options[2].arg_data = &auto_connect;
	ekg_options[4].arg_data = &no_global_config;
	ekg_options[5].arg_data = &frontend;
	ekg_options[6].arg_data = &set_status_callback;
	ekg_options[7].arg_data = &set_status_callback;
	ekg_options[8].arg_data = &set_status_callback;
	ekg_options[9].arg_data = &set_status_callback;
	ekg_options[10].arg_data = &set_status_callback;
	ekg_options[11].arg_data = &set_status_callback;
	ekg_options[12].arg_data = &print_version;

	{
		GOptionContext *opt;
		GOptionGroup *g;
		struct option_callback_args optargs = { &new_status, &new_descr };

		opt = g_option_context_new("[COMMAND...]");
		g = g_option_group_new(NULL, NULL, NULL, &optargs, NULL);

		g_option_group_add_entries(g, ekg_options);
		g_option_group_set_translation_domain(g, "ekg2");

		g_option_context_set_description(opt, "Options concerned with status depend on the protocol of particular session -- \n" \
	"some sessions may not support ``do not disturb'' status, etc.\n");
		g_option_context_set_main_group(opt, g);

		if (!g_option_context_parse(opt, &argc, &argv, &err)) {
			g_print("Option parsing failed: %s\n", err->message);
			return 1;
		}

		g_option_context_free(opt);
		/* GOptionGroup is freed implicitly */
	}

	if (print_version) {
		g_print("ekg2-%s (compiled on %s)\n", VERSION, compile_time());
		return 0;
	}

	g_log_set_default_handler(glib_debug_handler, NULL);
	g_set_print_handler(glib_print_handler);
	g_set_printerr_handler(glib_printerr_handler);
	in_autoexec = 1;

	if (argv[1]) {
		batch_line = prepare_batch_line(argv, 1);
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

	/* initialize dynamic module support */
	ekg2_dlinit(argv[0]);

	variable_init();
	variable_set_default();

	queries_init();

	mesg_startup = mesg_set(MESG_CHECK);
#ifdef DEFAULT_THEME 
	if (theme_read(DEFAULT_THEME, 1) == -1) 
#endif
		theme_init();

	window_debug	= window_new(NULL, NULL, -1);			/* debugowanie */
	window_status	= window_new(NULL, NULL, 1);			/* okno stanu */
	window_current	= window_status;

#if 0
	if (!no_global_config)
		config_read(SYSCONFDIR "/ekg2.conf");
#endif

	if (frontend) {
		plugin_load(frontend, -254, 1);
		config_changed = 1;
		g_free(frontend);
	}

	config_read_plugins();
#if 0
	if (!no_global_config)
		config_read(SYSCONFDIR "/ekg2-override.conf");
#endif

/*	  userlist_read(); */
	emoticon_read();
	msg_queue_read();

	if (!frontend) {
#ifdef HAVE_NCURSES
		if (!have_plugin_of_class(PLUGIN_UI)) plugin_load(("ncurses"), -254, 1);
#endif
#ifdef HAVE_GTK
		if (!have_plugin_of_class(PLUGIN_UI)) plugin_load(("gtk"), -254, 1);
#endif
#ifdef HAVE_LIBREADLINE
		if (!have_plugin_of_class(PLUGIN_UI)) plugin_load(("readline"), -254, 1);
#endif
	}

	if (!have_plugin_of_class(PLUGIN_UI)) {
		struct buffer *b;
		for (b = buffer_debug.data; b; b = b->next)
			fprintf(stderr, "%s\n", b->line);
		fprintf(stderr, "\n\nNo UI-PLUGIN!\n");
		return 1;
	} else {
		struct buffer *b;
		for (b = buffer_debug.data; b; b = b->next)
			print_window_w(window_debug, EKG_WINACT_NONE, b->target, b->line);
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
	/* If user does not have a config, don't bug about config upgrades. */
	if (config_read(NULL) == -1)
		config_version = -1;

	/* je¶li ma byæ theme, niech bêdzie theme */
	if (load_theme) {
		theme_read(load_theme, 1);
		g_free(load_theme);
	} else if (config_theme)
		theme_read(config_theme, 1);

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
/*	scripts_init();		*/

	/* it has to be done after plugins are loaded, either we wouldn't know if we are
	 * supporting some protocol in current build */
	if (session_read(NULL) == -1)
		no_config = 1;

	ekg_tls_init();
	config_postread();

	/* status window takes first session if not set before*/
	if (!window_status->session && sessions)
		window_session_set(window_status, sessions);

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

	timer_add(NULL, "autoaway", 1, 1, ekg_autoaway_timer, NULL);

	ekg2_reason_changed = 0;
	/* jesli jest emit: ui-loop (plugin-side) to dajemy mu kontrole, jesli nie 
	 * to wywolujemy normalnie sami ekg_loop() w petelce */
	if (query_emit(NULL, "ui-loop") != -1) {

		/* krêæ imprezê */
		while (1)
			g_main_context_iteration(NULL, TRUE);
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
		list_t l;

		for (l = watches; l; l = l->next) {
			watch_t *w = l->data;

			watch_free(w);
		}
	}

	{
		GSList *pl;

		for (pl = plugins; pl;) {
			const plugin_t *p = pl->data;

			pl = pl->next;

			if (p->pclass != PLUGIN_UI)
				continue;

			p->destroy();

//			if (p->dl) ekg2_dlclose(p->dl);
		}
	}
	list_destroy(watches, 0); watches = NULL;

	if (config_changed && !config_speech_app && config_save_quit == 1) {
		char line[80];

		printf("%s", format_find("config_changed"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[xstrlen(line) - 1] == '\n')
				line[xstrlen(line) - 1] = 0;
			if (!xstrcasecmp(line, "tak") || !xstrcasecmp(line, "yes") || !xstrcasecmp(line, "t") || !xstrcasecmp(line, "y")) {
				config_write();
				session_write();
				metacontact_write();
				script_variables_write();
				if (!config_commit())
					printf(_("Error while saving.\n"));
			}
		} else
			printf("\n");
	} else if (config_save_quit == 2) {
		config_write();
		session_write();
		metacontact_write();
		script_variables_write();

		if (!config_commit())
			printf(_("Error while saving.\n"));

	} else if (config_keep_reason && ekg2_reason_changed && config_save_quit == 1) {
		char line[80];

		printf("%s", format_find("quit_keep_reason"));
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin)) {
			if (line[xstrlen(line) - 1] == '\n')
				line[xstrlen(line) - 1] = 0;
			if (!xstrcasecmp(line, "tak") || !xstrcasecmp(line, "yes") || !xstrcasecmp(line, "t") || !xstrcasecmp(line, "y")) {
				session_write();
				if (!config_commit())
					printf(_("Error while saving.\n"));
			}
		} else
			printf("\n");

	} else if (config_keep_reason && ekg2_reason_changed && config_save_quit == 2) {
		session_write();
		if (!config_commit())
			printf(_("Error while saving.\n"));
	}
	config_exit_exec = NULL; /* avoid freeing it */

/* XXX, think about sequence of unloading. */

	sources_destroy();
	msgs_queue_destroy();
	conferences_destroy();
	newconferences_destroy();
	metacontacts_destroy();
	sessions_free();

	{
		GSList *pl;

		for (pl = plugins; pl; ) {
			const plugin_t *p = pl->data;

			pl = pl->next;
			p->destroy();

//			if (p->dl) ekg2_dlclose(p->dl);
		}
	}

	aliases_destroy();
	theme_free();
	variables_destroy();
	script_variables_free(1);
	emoticons_destroy();
	commands_destroy();
	binding_free();
	lasts_destroy();

	buffer_free(&buffer_debug);	buffer_free(&buffer_speech);
	event_free();
	ekg_tls_deinit();

	/* free internal read_file() buffer */
	read_file(NULL, -1);
	read_file_utf(NULL, -1);

/* windows: */
	windows_destroy();
	window_status = NULL; window_debug = NULL; window_current = NULL;	/* just in case */

/* queries */
	{
		query_t** kk;
		for (kk = queries; kk < &queries[QUERIES_BUCKETS]; ++kk) {
			queries_list_destroy(kk);
		}
	}
	registered_queries_free();

	xfree(home_dir);

	xfree(config_dir);

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
