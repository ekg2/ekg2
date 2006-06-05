/* $Id */

#include <ekg/win32.h>

#include <sys/stat.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <ekg/commands.h>
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

list_t rc_inputs = NULL;

char *rc_paths = NULL;

/*
 * rc_input_handler_line()
 *
 * obs³uga przychodz±cych poleceñ.
 */
WATCHER(rc_input_handler_line)
{
	rc_input_t *r = data;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}

	command_exec(NULL, NULL, watch, 0);
	return 0;
}

/*
 * rc_input_handler_dgram()
 *
 * obs³uga przychodz±cych datagramów.
 */
WATCHER(rc_input_handler_dgram)
{
	rc_input_t *r = data;
	char buf[2048];		/* powinno wystarczyæ dla sieci z MTU 1500 */
	int len;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}

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
WATCHER(rc_input_handler_accept)
{
	rc_input_t *r = data, *rn;
	struct sockaddr sa;
	int salen = sizeof(sa), cfd;

	if (type == 1) {
		rc_input_close(r);
		return 0;
	}

	if ((cfd = accept(fd, &sa, &salen)) == -1) {
		debug("[rc] accept() failed: %s\n", strerror(errno));
		return -1;
	}

	rn = xmalloc(sizeof(rn));
	memset(&rn, 0, sizeof(rn));

	rn->fd = cfd;
	rn->path = xstrdup(r->path);
	rn->type = (r->type == RC_INPUT_TCP) ? RC_INPUT_TCP_CLIENT : RC_INPUT_UNIX_CLIENT;
	rn->watch = WATCH_READ_LINE;
	list_add(&rc_inputs, rn, 0);
	watch_add(&rc_plugin, rn->fd, rn->watch, 1, rc_input_handler_line, rn);
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

/*
 * rc_paths_changed()
 *
 * zmieniono zmienn± remote_control. dodaj nowe kana³y wej¶ciowe, usuñ te,
 * których ju¿ nie ma.
 */
static void rc_paths_changed(const char *name)
{
	char **paths = array_make(rc_paths, ",; ", 0, 1, 1);
	int (*rc_input_new)(const char *);
	watcher_handler_func_t *rc_input_handler;
	list_t l;
	int i;

	/* usuñ znaczniki. zaznaczymy sobie te, które s± nadal wpisane */
	for (l = rc_inputs; l; l = l->next) {
		rc_input_t *r = l->data;

		r->mark = 0;
	}

	/* przejrzyj, czego chce u¿ytkownik */
	for (i = 0; paths[i]; i++) {
		rc_input_t *r;
		rc_input_type_t type;
		const char *path;

		if ((r = rc_input_find(paths[i]))) {
			r->mark = 1;
			continue;
		}

		rc_input_new = NULL;
		rc_input_handler = NULL;
		type = 0;
		path = NULL;
		
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
			rc_input_handler = rc_input_handler_line;
			path = paths[i] + 5;
			type = RC_INPUT_PIPE;
		}

		if (rc_input_new) {
			rc_input_t r, *rr;

			memset(&r, 0, sizeof(r));

			if ((r.fd = (*rc_input_new)(path)) == -1)
				continue;

			r.mark = 1;
			r.path = xstrdup(paths[i]);
			r.type = type;
			r.watch = (rc_input_handler != rc_input_handler_line) ? WATCH_READ : WATCH_READ_LINE;
			
			rr = list_add(&rc_inputs, &r, sizeof(r));

			watch_add(&rc_plugin, r.fd, r.watch, 1, rc_input_handler, rr);

			continue;
		}

		debug("[rc] unknown input type: %s\n", paths[i]);
	}

	/* usuñ te, które nie zosta³y zaznaczone */
	for (l = rc_inputs; l; ) {
		rc_input_t *r = l->data;

		l = l->next;

		if (r->mark)
			continue;

		watch_remove(&rc_plugin, r->fd, r->watch);
	}

	array_free(paths);
}

int rc_plugin_init(int prio)
{
	plugin_register(&rc_plugin, prio);

	variable_add(&rc_plugin, "remote_control", VAR_STR, 1, &rc_paths, rc_paths_changed, NULL, NULL);

	return 0;
}

static int rc_plugin_destroy()
{
	list_t l;

	for (l = rc_inputs; l; l = l->next) {
		rc_input_t *r = l->data;

		rc_input_close(r);
	}

	list_destroy(rc_inputs, 1);

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
