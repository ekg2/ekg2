#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/queries.h>

#include <ekg/commands.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

typedef enum {
	RC_INPUT_PIPE = 1,		/* pipe:/home/user/.ekg/pipe */
	RC_INPUT_UDP,			/* udp:12345 */
	RC_INPUT_TCP,			/* tcp:12345 */
	RC_INPUT_UNIX,			/* unix:/home/user/.ekg/socket */
	RC_INPUT_TCP_CLIENT,
	RC_INPUT_UNIX_CLIENT
} rc_input_type_t;

typedef struct {
	rc_input_type_t type;		/* rodzaj wej¶cia */
	char *path;			/* ¶cie¿ka */
	int fd;				/* deskryptor */
	int mark;			/* do zaznaczania, wnêtrzno¶ci */

	int login_ok;
} rc_input_t;

typedef struct {
	char *str;
	time_t ts;
} remote_backlog_t;

typedef struct {
	remote_backlog_t **backlog;	/* bufor z liniami */		/* XXX, przerobic na liste? */
	int backlog_size;	/* rozmiar backloga */
} remote_window_t;

PLUGIN_DEFINE(remote, PLUGIN_UI, NULL);

static void rc_input_close(rc_input_t *r);

static list_t rc_inputs = NULL;
static char *rc_paths = NULL;
static char *rc_password = NULL;
static int rc_first = 1;

static const char *rc_var_get_value(variable_t *v) {
	if (!v)
		return NULL;

	switch (v->type) {
		case VAR_INT:
		case VAR_BOOL:
			return itoa(*((int *) v->ptr));

		case VAR_THEME:
		case VAR_FILE:
		case VAR_DIR:
		case VAR_STR:
			return *((const char **) v->ptr);

		case VAR_MAP:
		default:
			return "notimplemented";
	}
}

static char *rc_fstring_reverse(fstring_t *fstr) {
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
		if ((cur & FSTR_UNDERLINE) &&	(reset || !(prev & FSTR_UNDERLINE)))	string_append(asc, "%U");
		if ((cur & FSTR_REVERSE) &&	(reset || !(prev & FSTR_REVERSE)))	string_append(asc, "%V");

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

static string_t remote_what_to_write(char *what, va_list ap) {
	string_t str;
	char *_str;

	str = string_init(what);

	while ((_str = va_arg(ap, char *))) {
		string_append_c(str, '\002');
		/* XXX, kazde cos co wymaga sanityzacji, sanityzowac :) */
		/*      przy okazji mozna tez i rekodowac :) */
		string_append(str, _str);
	}
	string_append_c(str, '\n');

	debug_io("remote_what_to_write: %s\n", str->str);

	return str;
}

static int remote_broadcast(char *what, ...) {
	string_t str;
	va_list ap;
	list_t l;

	va_start(ap, what);
	str = remote_what_to_write(what, ap);
	va_end(ap);

	for (l = rc_inputs; l; l = l->next) {
		rc_input_t *r = l->data;

		if (r->type == RC_INPUT_TCP_CLIENT || r->type == RC_INPUT_UNIX_CLIENT) {
			/* XXX, discarding ret value */
			if (r->login_ok)
				ekg_write(r->fd, str->str, str->len);
		}
	}

	string_free(str, 1);
	return 0;
}

static int remote_writefd(int fd, char *what, ...) {
	string_t str;
	va_list ap;

	va_start(ap, what);
	str = remote_what_to_write(what, ap);
	va_end(ap);

	ekg_write(fd, str->str, str->len);

	string_free(str, 1);
	return 0;
}

static int rc_theme_enumerate_fd = -1;

int rc_theme_enumerate(const char *name, const char *value) {
	char *esc_name;
	char *esc_value;
	
	if (rc_theme_enumerate_fd == -1)
		return 0;

	esc_name = escape(name);
	esc_value = escape(value);

	remote_writefd(rc_theme_enumerate_fd, "FORMAT", esc_name, esc_value, NULL);
	/* XXX, if remote_writefd() fails, we should set rc_theme_enumerate_fd to -1, and return 0 */

	xfree(esc_name);
	xfree(esc_value);
	return 1;
}

/*
 * rc_input_handler_line()
 *
 * obs³uga przychodz±cych poleceñ.
 */
static WATCHER_LINE(rc_input_handler_line) {
	rc_input_t *r = data;
	char **arr;
	int arrcnt;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}

	if (!data) 
		return -1;

	arr = array_make(watch, "\001", 0, 0, 0);
	arrcnt = array_count(arr);

	if (!r->login_ok) {
		if (!xstrcmp(arr[0], "REQLOGIN")) {
			if (arrcnt == 1 && rc_password == NULL)
				r->login_ok = 1;
			else if (arrcnt == 2 && !xstrcmp(rc_password, arr[1]))
				r->login_ok = 1;

			if (!r->login_ok) {
				remote_writefd(fd, "-LOGIN", NULL);
				array_free(arr);
				return -1;
			}

			remote_writefd(fd, "+LOGIN", NULL);

		} else {
			debug_error("unknown command: %s\n", arr[0]);
			array_free(arr);
			return -1;
		}

		array_free(arr);
		return 0;
	}

	if (arr[0]) {
		char *cmd = arr[0];

		if (0) {

/* synchronization ekg2-remote <==> ekg2 */
		} else if (!xstrcmp(cmd, "REQCONFIG")) {
			variable_t *v;

			for (v = variables; v; v = v->next) {
				const char *_val;

				_val = rc_var_get_value(v);
				remote_writefd(fd, "CONFIG", v->name, _val, NULL);	/* _val can be NULL */
			}
			remote_writefd(fd, "+CONFIG", NULL);

		} else if (!xstrcmp(cmd, "REQCOMMANDS")) {
			command_t *c;

			for (c = commands; c; c = c->next) {
				if (c->params) {
					char *tmp = array_join(c->params, " ");
					remote_writefd(fd, "COMMAND", c->name, tmp, NULL);
					xfree(tmp);
				} else
					remote_writefd(fd, "COMMAND", c->name, NULL);
			}
			remote_writefd(fd, "+COMMAND", NULL);

		} else if (!xstrcmp(cmd, "REQPLUGINS")) {
			plugin_t *p;

			for (p = plugins; p; p = p->next)
				remote_writefd(fd, "PLUGIN", p->name, itoa(p->prio), NULL);
			remote_writefd(fd, "+PLUGIN", NULL);

		} else if (!xstrcmp(cmd, "REQFORMATS")) {
		/* XXX, dirty hack */
			rc_theme_enumerate_fd = fd;
			theme_enumerate(rc_theme_enumerate);
			if (rc_theme_enumerate_fd == -1)
				/* XXX, error */
				;

			remote_writefd(fd, "+FORMAT", NULL);

		} else if (!xstrcmp(cmd, "REQBACKLOGS")) {
			window_t *w;

			for (w = windows; w; w = w->next) {
				remote_window_t *n = w->private;
				int i;

				if (!n)
					continue;

				for (i = n->backlog_size; i; i--) {
					remote_writefd(fd, "BACKLOG", itoa(w->id), itoa(n->backlog[i-1]->ts), n->backlog[i-1]->str, NULL);
				}
			}
			remote_writefd(fd, "+BACKLOG", NULL);

		} else if (!xstrcmp(cmd, "REQSESSIONS")) {
			session_t *s;

			for (s = sessions; s; s = s->next) {
				remote_writefd(fd, "SESSION", s->uid, (s->plugin) ? ((plugin_t *) s->plugin)->name : "-", NULL);
				remote_writefd(fd, "SESSIONINFO", s->uid, "STATUS", itoa(s->status), NULL);
				remote_writefd(fd, "SESSIONINFO", s->uid, "CONNECTED", itoa(s->connected), NULL);

				if (s->alias)
					remote_writefd(fd, "SESSIONINFO", s->uid, "ALIAS", s->alias, NULL);
			}
			remote_writefd(fd, "+SESSION", NULL);

		} else if (!xstrcmp(cmd, "REQWINDOWS")) {
			window_t *w;

			for (w = windows; w; w = w->next) {
				remote_writefd(fd, "WINDOW", itoa(w->id), w->target, NULL);	/* NOTE: w->target can be NULL */

				if (w->alias)
					remote_writefd(fd, "WINDOWINFO", itoa(w->id), "ALIAS", w->alias, NULL);

				if (w->session)
					remote_writefd(fd, "WINDOWINFO", itoa(w->id), "SESSION", w->session->uid, NULL);
			}
			remote_writefd(fd, "WINDOW_SWITCH", itoa(window_current->id), NULL);
			remote_writefd(fd, "+WINDOW", NULL);

		} else if (!xstrcmp(cmd, "REQUSERLISTS")) {

#define fix(x) ((x) ? x : "")
			session_t *s;
			window_t *w;
			userlist_t *u;

			/* najpierw sesyjna userlista */
			for (s = sessions; s; s = s->next) {
				for (u = s->userlist; u; u = u->next) {
					char *groups = (u->groups) ? group_to_string(u->groups, 1, 0) : NULL;
					remote_writefd(fd, "SESSIONITEM", s->uid, fix(u->uid), itoa(u->status), fix(u->nickname), fix(groups), NULL);
					xfree(groups);
				}
			}

			/* potem okienkowe userlisty */
			for (w = windows; w; w = w->next) {
				for (u = w->userlist; u; u = u->next) {
					char *groups = (u->groups) ? group_to_string(u->groups, 1, 0) : NULL;
					remote_writefd(fd, "WINDOWITEM", itoa(w->id), fix(u->uid), itoa(u->status), fix(u->nickname), fix(groups), NULL);
					xfree(groups);
				}

			}
			/* XXX, konferencyjne userlisty? */
#undef fix
			remote_writefd(fd, "+USERLIST", NULL);

/* rozniaste */
		} else if (!xstrcmp(cmd, "REQSESSION_CYCLE")) {
			if (arrcnt == 2) {
				int id = atoi(arr[1]);
				int ret;

				ret = window_session_cycle(window_exist(id));
				if (ret == 0)
					remote_writefd(fd, "+SESSION_CYCLE", NULL);
				else
					remote_writefd(fd, "-SESSION_CYCLE", NULL);
			}
		} else if (!xstrcmp(cmd, "REQEXECUTE")) {
			if (arrcnt == 2) {
				int ret;

				ret = command_exec(window_current->target, window_current->session, arr[1], 0);

				/* XXX, send retcode? */

				remote_writefd(fd, "+EXECUTE", NULL);
			} else
				remote_writefd(fd, "-EXECUTE", NULL);
		} else {
			debug_error("unknown command: %s\n", cmd);
		}

	}
	array_free(arr);
	return 0;
}

/*
 * rc_input_handler_accept()
 *
 * obs³uga przychodz±cych po³±czeñ.
 */
static WATCHER(rc_input_handler_accept) {
	rc_input_t *r = data, *rn;
	struct sockaddr sa;
	socklen_t salen = sizeof(sa), cfd;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}
	if (!data) return -1;

	if ((cfd = accept(fd, &sa, &salen)) == -1) {
		debug_error("[rc] accept() failed: %s\n", strerror(errno));
		return -1;
	}

	debug("rc_input_handler_accept() new connection... [%s] %d\n", r->path, cfd);

	rn	= xmalloc(sizeof(rc_input_t));

	rn->fd		= cfd;
	rn->path	= saprintf("%sc", r->path);	/* maybe ip:port of client or smth? */
	rn->type	= (r->type == RC_INPUT_TCP) ? RC_INPUT_TCP_CLIENT : RC_INPUT_UNIX_CLIENT;
	list_add(&rc_inputs, rn);
	watch_add_line(&remote_plugin, cfd, WATCH_READ_LINE, rc_input_handler_line, rn);
	return 0;
}

/*
 * rc_input_find()
 *
 * zwraca strukturê rc_input_t kana³u wej¶ciowego o podanej ¶cie¿ce.
 */
static rc_input_t *rc_input_find(const char *path)
{
	list_t l;

	for (l = rc_inputs; l; l = l->next) {
		rc_input_t *r = l->data;

		if (!xstrcmp(r->path, path))
			return r;
	}
	
	return NULL;
}

static watch_t *rc_watch_find(int fd) {
	list_t l;
	
	for (l = watches; l; l = l->next) {
		watch_t *w = l->data;

		if (w && w->plugin == &remote_plugin && w->fd == fd)
			return w;
	}

	return NULL;
}

/*
 * rc_input_close()
 *
 * zamyka kana³ wej¶ciowy.
 */
static void rc_input_close(rc_input_t *r) {
	if (!r)
		return;

	debug_function("[rc] closing (0x%x) fd: %d path:%s\n", r, r->fd, r->path);

	if (r->type == RC_INPUT_PIPE)
		unlink(r->path);

	if (r->fd != -1) {
		watch_t *w = rc_watch_find(r->fd);

		if (w) {
			if (w->data == r)
				debug_function("[rc] rc_input_close() watch 0x%x OK\n", r);
			else	debug_error("[rc] rc_input_close() watch: 0x%x r: 0x%x\n", w->data, r);

			w->data = NULL;	/* to avoid double free */
			watch_free(w);	/* free watch */
		}
		/* i don't really know if we really must do it.. coz WATCH_READ_LINE watches close always fd...., 
		 * maybe let's move it to handlers which really need it? */
		close(r->fd);
		r->fd = -1;
	}

	xfree(r->path);
	list_remove(&rc_inputs, r, 1);
}

/*
 * rc_input_new_inet()
 *
 * tworzy nowe gniazdo AF_INET.
 */
static int rc_input_new_inet(const char *path, int type)
{
	struct sockaddr_in sin;
	int port, fd;
	uint32_t addr = INADDR_ANY;

	if (xstrchr(path, ':')) {
		char *tmp = xstrdup(path), *c = xstrchr(tmp, ':');

		port = atoi(c + 1);
		*c = 0;
		addr = inet_addr(tmp);
		xfree(tmp);
	} else
		port = atoi(path);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = addr;

	if ((fd = socket(AF_INET, type, 0)) == -1) {
		debug_error("[rc] socket() failed: %s\n", strerror(errno));
		return -1;
	}

#ifdef SO_REUSEADDR
	{
		int one = 1;

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
			debug_error("[rc] setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
	}
#endif
	
	if (bind(fd, (struct sockaddr*) &sin, sizeof(sin))) {
		debug_error("[rc] bind() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	if (type == SOCK_STREAM && listen(fd, 10)) {
		debug_error("[rc] listen() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

static int rc_input_new_tcp(const char *path)
{
	return rc_input_new_inet(path, SOCK_STREAM);
}

static int rc_input_new_udp(const char *path)
{
	return rc_input_new_inet(path, SOCK_DGRAM);
}

/*
 * rc_input_new_pipe()
 *
 * tworzy nazwany potok (named pipe).
 */
static int rc_input_new_pipe(const char *path)
{
	struct stat st;
	int fd;

	if (!stat(path, &st) && !S_ISFIFO(st.st_mode)) {
		debug_error("[rc] file exists, but isn't a pipe\n");
		return -1;
	}

	if (mkfifo(path, 0600) == -1 && errno != EEXIST) {
		debug_error("[rc] mkfifo() failed: %s\n", strerror(errno));
		return -1;
	}

	if ((fd = open(path, O_RDWR | O_NONBLOCK)) == -1) {
		debug_error("[rc] open() failed: %s\n", strerror(errno));
		return -1;
	}

	return fd;
}

/*
 * rc_input_new_unix()
 *
 * tworzy gniazdo AF_UNIX.
 */
static int rc_input_new_unix(const char *path)
{
	struct sockaddr_un beeth;
	int fd;

	beeth.sun_family = AF_UNIX;
	strlcpy(beeth.sun_path, path, sizeof(beeth.sun_path));

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		debug("[rc] socket() failed: %s\n", strerror(errno));
		return -1;
	}
	
	if (bind(fd, (struct sockaddr*) &beeth, sizeof(beeth))) {
		debug("[rc] bind() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, 10)) {
		debug("[rc] listen() failed: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

/*
 * rc_paths_changed()
 *
 * zmieniono zmienn± remote_control. dodaj nowe kana³y wej¶ciowe, usuñ te,
 * których ju¿ nie ma.
 */
static void rc_paths_changed(const char *name)
{
	char **paths = array_make(rc_paths, ",; ", 0, 1, 1);
	list_t l;
	int i;

	/* usuñ znaczniki. zaznaczymy sobie te, które s± nadal wpisane */
	for (l = rc_inputs; l; l = l->next) {
		rc_input_t *r = l->data;

		r->mark = 0;
	}

	/* przejrzyj, czego chce u¿ytkownik */
	for (i = 0; paths[i]; i++) {
		watcher_handler_func_t *rc_input_handler = NULL;
		int (*rc_input_new)(const char *)	 = NULL;

		rc_input_type_t type			 = 0;
		const char *path			 = NULL;

		rc_input_t *r;

		if ((r = rc_input_find(paths[i]))) {
			r->mark = 1;
			continue;
		}

		if (!strncmp(paths[i], "tcp:", 4)) {
			rc_input_new = rc_input_new_tcp;
			rc_input_handler = rc_input_handler_accept;
			path = paths[i] + 4;
			type = RC_INPUT_TCP;
		}

		if (!strncmp(paths[i], "udp:", 4)) {
			rc_input_new = rc_input_new_udp;
			rc_input_handler = (watcher_handler_func_t*) rc_input_handler_line;
			path = paths[i] + 4;
			type = RC_INPUT_UDP;
		}
		
		if (!strncmp(paths[i], "unix:", 5)) {
			rc_input_new = rc_input_new_unix;
			rc_input_handler = rc_input_handler_accept;
			path = paths[i] + 5;
			type = RC_INPUT_UNIX;
		}

		if (!strncmp(paths[i], "pipe:", 5)) {
			rc_input_new = rc_input_new_pipe;
			rc_input_handler = (watcher_handler_func_t*) rc_input_handler_line;
			path = paths[i] + 5;
			type = RC_INPUT_PIPE;
		}

		if (rc_input_new) {
			rc_input_t *r;
			int rfd;

			if ((rfd = (*rc_input_new)(path)) == -1) continue;

			r = xmalloc(sizeof(rc_input_t));

			r->fd	= rfd;
			r->mark	= 1;
			r->path	= xstrdup(paths[i]);
			r->type	= type;
			
			list_add(&rc_inputs, r);

			watch_add(&remote_plugin, rfd, 
				((void *) rc_input_handler != (void *) rc_input_handler_line) ? WATCH_READ : WATCH_READ_LINE, 
				rc_input_handler, r);
			continue;
		}

		debug_error("[rc] unknown input type: %s\n", paths[i]);
	}

	/* usuñ te, które nie zosta³y zaznaczone */
	for (l = rc_inputs; l; ) {
		/* to usunie rowniez aktualnie nawiazane polaczenia... */
		rc_input_t *r = l->data;

		l = l->next;

		if (r->mark)
			continue;

		rc_input_close(r);		/* it'll remove l->data */
	}

	array_free(paths);
}

static int remote_window_new(window_t *w) {
	remote_window_t *n;

	if (w->private)
		return 0;

	n = xmalloc(sizeof(remote_window_t));

	w->private = n;
	return 0;
}

static void remote_backlog_add(window_t *w, remote_backlog_t *str) {
	remote_window_t *n = w->private;
	
	if (!w)
		return;
#define config_backlog_size -1

	if (n->backlog_size == config_backlog_size) {
		remote_backlog_t *line = n->backlog[n->backlog_size - 1];

		xfree(line->str);
		xfree(line);

		n->backlog_size--;
	} else 
		n->backlog = xrealloc(n->backlog, (n->backlog_size + 1) * sizeof(char *));

	memmove(&n->backlog[1], &n->backlog[0], n->backlog_size * sizeof(char *));

	n->backlog[0] = str;

	n->backlog_size++;
}

static int remote_window_kill(window_t *w) {
	remote_window_t *n = w->private;

	if (!n) 
		return -1;

	w->private = 0;

	if (n->backlog) {
		int i;

		for (i = 0; i < n->backlog_size; i++)
			xfree(n->backlog[i]);

		xfree(n->backlog);

		n->backlog = NULL;
		n->backlog_size = 0;
	}

	xfree(n);
	return 0;
}

/* XXX, nicer? */
/* XXX, pipe: && udp: sucks */
/* XXX, ssl: zlib: ? */
static QUERY(remote_postinit) {
	if (rc_inputs)
		return 1;

	if (!rc_first)
		printf("!!! rc_inputs == NULL, need reconfiguration of remote plugin!\n");

	printf("Hi,\nI'm remote_postinit() function\n");

	if (rc_first)
		printf("According to remote:first_run value, this is your first run (or you manually changed it!)\n");

	printf("I'm here to help you configure remote plugin\n");
	printf("\n");
	printf("remote:remote_control (Current value: %s)\n", rc_paths ? rc_paths : "null");
	printf("\te.g.: tcp:127.0.0.1:1234;tcp:1234;udp:127.0.0.1:1234;unix:mysocket;pipe:/tmp/mypipe\n");
	printf("\t      (tcp:* or unix:* is prefered!\n");

	do {
		char *tmp;

		printf("(ekg2-remote) ");
		fflush(stdout);
		
		tmp = read_file(stdin, 0);

		variable_set("remote:remote_control", tmp);
		if (!rc_inputs)
			printf("Sorry, rc_inputs still NULL, try again\n");

	} while (rc_inputs == NULL);
	printf("\n");

	/* XXX, haslo pozniej */
	variable_set("remote:password", itoa(getpid()));
	printf("Your password is: %s\n", rc_password);

	variable_set("remote:first_run", "0");

	printf("\n");
	printf("ekg2-remote-plugin: configured!\n");
	printf("remember to change password (/set remote:password yournewpassword) and to save configuration after connect!\n");

	return 0;
}

static QUERY(remote_ui_is_initialized) {
	int *tmp = va_arg(ap, int *);

	*tmp = 1;
	return 0;
}

static QUERY(remote_ui_window_clear) {
	window_t *w	= *(va_arg(ap, window_t **));

	remote_broadcast("WINDOW_CLEAR", itoa(w->id), NULL);
	return 0;
}

static QUERY(remote_ui_window_new) {
	window_t *w	= *(va_arg(ap, window_t **));

	remote_broadcast("WINDOW_NEW", itoa(w->id), w->target, NULL);	/* w->target can be NULL */

	if (w->alias)
		remote_broadcast("WINDOWINFO", itoa(w->id), "ALIAS", w->alias, NULL);

	if (w->session)
		remote_broadcast("WINDOWINFO", itoa(w->id), "SESSION", w->session->uid, NULL);

	remote_window_new(w);
	return 0;
}

static QUERY(remote_ui_window_kill) {
	window_t *w	= *(va_arg(ap, window_t **));

	remote_broadcast("WINDOW_KILL", itoa(w->id), NULL);

	remote_window_kill(w);
	return 0;
}

static QUERY(remote_ui_window_switch) {
	window_t *w	= *(va_arg(ap, window_t **));

	remote_broadcast("WINDOW_SWITCH", itoa(w->id), NULL);
	return 0;
}

static QUERY(remote_ui_window_print) {
	window_t *w	= *(va_arg(ap, window_t **));
	fstring_t *line = *(va_arg(ap, fstring_t **));
	char *fstr;

	remote_window_t *n;

	if (w == window_debug)		/* XXX! */
		goto cleanup;

	/* XXX, sanityzowac (? na pewno?)*/

	if (!(n = w->private)) { 
		/* BUGFIX, cause @ ui-window-print handler (not ncurses plugin one, ncurses plugin one is called last cause of 0 prio)
		 *	plugin may call print_window() 
		 */
		remote_window_new(w);	
		if (!(n = w->private)) {
			debug("remote_ui_window_print() IInd CC still not w->private, quitting...\n");
			return -1;
		}
	}

	fstr = rc_fstring_reverse(line);
	{
		remote_backlog_t *ln = xmalloc(sizeof(remote_backlog_t));

		ln->ts = line->ts;
		ln->str = fstr;

		remote_backlog_add(w, ln);
	}

	remote_broadcast("WINDOW_PRINT", itoa(w->id), itoa(line->ts), fstr, NULL);		/* XXX, using id is ok? */

cleanup:
	fstring_free(line);
	return -1;		/* XXX, sry, jak ktos potrzebuje tego stringa oprocz nas, to go nie dostanie. (memleaki sa gorsze) */
}

static QUERY(remote_ui_beep) {
	remote_broadcast("BEEP", NULL);
	return 0;
}

static QUERY(remote_variable_changed) {
	char *name = *(va_arg(ap, char**));
	const char *_val;
	variable_t *v;
	
	if (!(v = variable_find(name))) {
		debug_error("remote_variable_changed(%s) damn!\n", name);
		return 0;		/* XXX, we don't care ? */
	}

	_val = rc_var_get_value(v);
	remote_broadcast("VARIABLE_CHANGED", name, _val, NULL);

	return 0;
}

static QUERY(remote_protocol_connected) {
	char *uid	= *(va_arg(ap, char **));

	remote_broadcast("SESSIONINFO", uid, "CONNECTED", "1", NULL);
	return 0;
}

static QUERY(remote_protocol_disconnected) {
	char *uid	= *(va_arg(ap, char **));
	char *reason	= *(va_arg(ap, char **));
	int type	= *(va_arg(ap, int*));

	remote_broadcast("SESSIONINFO", uid, "CONNECTED", "0", NULL);
	return 0;
}

static QUERY(remote_ui_window_target_changed) {
	window_t *w = *(va_arg(ap, window_t **));

/* wysylamy wszystko, bo nie wiemy co sie zmienilo, a co nie */
	remote_broadcast("WINDOWINFO", itoa(w->id), "ALIAS", w->alias, NULL);
	remote_broadcast("WINDOWINFO", itoa(w->id), "TARGET", w->target, NULL);
	remote_broadcast("WINDOWINFO", itoa(w->id), "SESSION", w->session ? w->session->uid : NULL, NULL);
	return 0;
}

static QUERY(remote_session_changed) {
#if 0	/* fajne, ale dla nas fajniejsze jest UI_WINDOW_TARGET_CHANGED */

	/* emitowane gdy sie zmienia session_current */
	/* XXX, zweryfikowac jeszcze raz (tylko core) */

	if (!session_current) {
		debug_error("remote_session_changed() but session_current == NULL?\n");
		return 0;
	}

	remote_broadcast("SESSIONCHANGED", session_current->uid, NULL);
#endif
	return 0;
}

static QUERY(remote_session_renamed) {
	char *alias = *(va_arg(ap, char **));
	session_t *s;

	if (!(s = session_find(alias))) {
		debug_error("remote_session_renamed(%s) damn!\n", alias);
		return 0;
	}

	remote_broadcast("SESSIONINFO", s->uid, "ALIAS", s->alias, NULL);

	return 0;
}

static QUERY(remote_userlist_refresh) {
	/* ze wstepnej analizy wynika ze ulubionym query po userlist_add() jest emitowanie USERLIST_REFRESH...
	 * bez parametrow, najwygodniejsze, 
	 *
	 * w zwiazku z tym mozemy wyslac (ponownie) userliste ze wszystkich (sesji,okien) poprzedzajac jakims CLEARSMTH (b. niefajne)
	 * albo przynajmniej dodac do USERLIST_REFRESH parametr na ktorej sesji/oknie robimy te operacje...
	 *
	 * Tutaj przelecimy wszystkie znane listy, i sprawdzimy czy jakis pointer sie zgadza, jak tak to wyslemy tylko ja.
	 */

	return 0;
}


static QUERY(remote_all_contacts_changed) {
//	remote_broadcast((data) ? "REFRESH_USERLIST_FULL\n" : "REFRESH_USERLIST\n");	/* XXX, nie przetwarzane */
	/* XXX, inaczej, to tak nie bedzie dzialac. trzeba zrobic wsparcie dla wszystkich */
	return 0;
}

EXPORT int remote_plugin_init(int prio) {
	int is_UI = 0;

	PLUGIN_CHECK_VER("remote");

	query_emit_id(NULL, UI_IS_INITIALIZED, &is_UI);

	if (is_UI)
		return -1;

	plugin_register(&remote_plugin, prio);

	variable_add(&remote_plugin, ("remote_control"), VAR_STR, 1, &rc_paths, rc_paths_changed, NULL, NULL);
	variable_add(&remote_plugin, ("first_run"), VAR_INT, 2, &rc_first, NULL, NULL, NULL);
	variable_add(&remote_plugin, ("password"), VAR_STR, 0, &rc_password, NULL, NULL, NULL);

	query_connect_id(&remote_plugin, UI_IS_INITIALIZED, remote_ui_is_initialized, NULL);
	query_connect_id(&remote_plugin, CONFIG_POSTINIT, remote_postinit, NULL);

	query_connect_id(&remote_plugin, UI_WINDOW_SWITCH, remote_ui_window_switch, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_KILL, remote_ui_window_kill, NULL);
	query_connect_id(&remote_plugin, UI_BEEP, remote_ui_beep, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_PRINT, remote_ui_window_print, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_CLEAR, remote_ui_window_clear, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_NEW, remote_ui_window_new, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_TARGET_CHANGED, remote_ui_window_target_changed, NULL);
	query_connect_id(&remote_plugin, VARIABLE_CHANGED, remote_variable_changed, NULL);

	/* SESSION_EVENT */
	query_connect_id(&remote_plugin, PROTOCOL_CONNECTED, remote_protocol_connected, NULL);
	query_connect_id(&remote_plugin, PROTOCOL_DISCONNECTED, remote_protocol_disconnected, NULL);

	query_connect_id(&remote_plugin, SESSION_CHANGED, remote_session_changed, NULL);
	query_connect_id(&remote_plugin, SESSION_RENAMED, remote_session_renamed, NULL);

	query_connect_id(&remote_plugin, USERLIST_REFRESH, remote_userlist_refresh, NULL);

#if 0

	query_connect_id(&remote_plugin, UI_WINDOW_TARGET_CHANGED, ncurses_ui_window_target_changed, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_ACT_CHANGED, ncurses_ui_window_act_changed, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_REFRESH, ncurses_ui_window_refresh, NULL);
	query_connect_id(&remote_plugin, UI_WINDOW_UPDATE_LASTLOG, ncurses_ui_window_lastlog, NULL);
	query_connect_id(&remote_plugin, UI_REFRESH, ncurses_ui_refresh, NULL);
	query_connect_id(&remote_plugin, SESSION_ADDED, ncurses_statusbar_query, NULL);
	query_connect_id(&remote_plugin, SESSION_REMOVED, ncurses_statusbar_query, NULL);
	query_connect_id(&remote_plugin, BINDING_SET, ncurses_binding_set_query, NULL);
	query_connect_id(&remote_plugin, BINDING_COMMAND, ncurses_binding_adddelete_query, NULL);
	query_connect_id(&remote_plugin, BINDING_DEFAULT, ncurses_binding_default, NULL);
	query_connect_id(&remote_plugin, CONFERENCE_RENAMED, ncurses_conference_renamed, NULL);

	query_connect_id(&remote_plugin, PROTOCOL_DISCONNECTING, ncurses_session_disconnect_handler, NULL);
#endif

	/* podanie czegokolwiek jako data do remote_all_contacts_changed() powoduje wyzerowanie n->start */
	query_connect_id(&remote_plugin, UI_REFRESH, remote_all_contacts_changed, (void *) 1);
	query_connect_id(&remote_plugin, USERLIST_REFRESH, remote_all_contacts_changed, NULL /* ? */);

	query_connect_id(&remote_plugin, SESSION_CHANGED, remote_all_contacts_changed, (void *) 1);
	query_connect_id(&remote_plugin, SESSION_EVENT, remote_all_contacts_changed, NULL);

	query_connect_id(&remote_plugin, METACONTACT_ADDED, remote_all_contacts_changed, NULL);
	query_connect_id(&remote_plugin, METACONTACT_REMOVED, remote_all_contacts_changed, NULL);
	query_connect_id(&remote_plugin, METACONTACT_ITEM_ADDED, remote_all_contacts_changed, NULL);
	query_connect_id(&remote_plugin, METACONTACT_ITEM_REMOVED, remote_all_contacts_changed, NULL);

	query_connect_id(&remote_plugin, USERLIST_CHANGED, remote_all_contacts_changed, NULL);
	query_connect_id(&remote_plugin, USERLIST_ADDED, remote_all_contacts_changed, NULL);
	query_connect_id(&remote_plugin, USERLIST_REMOVED, remote_all_contacts_changed, NULL);
	query_connect_id(&remote_plugin, USERLIST_RENAMED, remote_all_contacts_changed, NULL);



/* XXX, signal()? on ^C, do ekg_exit() etc..? */

	return 0;
}

static int remote_plugin_destroy() {
	list_t l;

	for (l = rc_inputs; l;) {
		rc_input_t *r = l->data;

		l = l->next;

		rc_input_close(r);
	}

	plugin_unregister(&remote_plugin);

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
