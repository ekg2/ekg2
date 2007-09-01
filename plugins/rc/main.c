/* $Id */

#include <ekg/win32.h>

#include <sys/stat.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "rc.h"

PLUGIN_DEFINE(rc, PLUGIN_UI, NULL);
#ifdef EKG2_WIN32_SHARED_LIB
	EKG2_WIN32_SHARED_LIB_HELPER
#endif

static void rc_input_close(rc_input_t *r);

static list_t rc_inputs = NULL;
static char *rc_paths = NULL;

/*
 * rc_input_handler_line()
 *
 * obs³uga przychodz±cych poleceñ.
 */
static WATCHER_LINE(rc_input_handler_line) {
	rc_input_t *r = data;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}
	if (!data) return -1;
	command_exec(NULL, NULL, watch, 0);
	return 0;
}

/*
 * rc_input_handler_dgram()
 *
 * obs³uga przychodz±cych datagramów.
 */
static WATCHER(rc_input_handler_dgram) {
	rc_input_t *r = data;
	char buf[2048];		/* powinno wystarczyæ dla sieci z MTU 1500 */
	int len;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}
	if (!data) return -1;

	len = read(fd, buf, sizeof(buf) - 1);
	buf[len] = 0;

	command_exec(NULL, NULL, buf, 0);
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
	list_add(&rc_inputs, rn, 0);
	watch_add_line(&rc_plugin, cfd, WATCH_READ_LINE, rc_input_handler_line, rn);
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

		if (w->plugin == &rc_plugin && w->fd == fd)
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
			rc_input_handler = rc_input_handler_dgram;
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
			
			list_add(&rc_inputs, r, 0);

			watch_add(&rc_plugin, rfd, 
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

EXPORT int rc_plugin_init(int prio) {
	plugin_register(&rc_plugin, prio);

	variable_add(&rc_plugin, ("remote_control"), VAR_STR, 1, &rc_paths, rc_paths_changed, NULL, NULL);

	return 0;
}

static int rc_plugin_destroy() {
	list_t l;

	for (l = rc_inputs; l;) {
		rc_input_t *r = l->data;

		l = l->next;

		rc_input_close(r);
	}

	plugin_unregister(&rc_plugin);

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
