/* $Id */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include "rc.h"

/*
 * rc_input_new_inet()
 *
 * tworzy nowe gniazdo AF_INET.
 */
int rc_input_new_inet(const char *path, int type)
{
	struct sockaddr_in sin;
	int port, fd;
	uint32_t addr = INADDR_ANY;

	if (strchr(path, ':')) {
		char *tmp = xstrdup(path), *c = strchr(tmp, ':');

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
		debug("[rc] socket() failed: %s\n", strerror(errno));
		return -1;
	}

#ifdef SO_REUSEADDR
	{
		int one = 1;

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == -1)
			debug("[rc] setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
	}
#endif
	
	if (bind(fd, (struct sockaddr*) &sin, sizeof(sin))) {
		debug("[rc] bind() failed: %s\n", strerror(errno));
		return -1;
	}

	if (type == SOCK_STREAM && listen(fd, 10)) {
		debug("[rc] listen() failed: %s\n", strerror(errno));
		return -1;
	}

	return fd;
}

int rc_input_new_tcp(const char *path)
{
	return rc_input_new_inet(path, SOCK_STREAM);
}

int rc_input_new_udp(const char *path)
{
	return rc_input_new_inet(path, SOCK_DGRAM);
}

/*
 * rc_input_new_pipe()
 *
 * tworzy nazwany potok (named pipe).
 */
int rc_input_new_pipe(const char *path)
{
	struct stat st;
	int fd;

	if (!stat(path, &st) && !S_ISFIFO(st.st_mode)) {
		debug("[rc] file exists, but isn't a pipe\n");
		return -1;
	}

	if (mkfifo(path, 0600) == -1 && errno != EEXIST) {
		debug("[rc] mkfifo() failed: %s\n", strerror(errno));
		return -1;
	}

	if ((fd = open(path, O_RDWR | O_NONBLOCK)) == -1) {
		debug("[rc] open() failed: %s\n", strerror(errno));
		return -1;
	}

	return fd;
}

/*
 * rc_input_new_unix()
 *
 * tworzy gniazdo AF_UNIX.
 */
int rc_input_new_unix(const char *path)
{
	struct sockaddr_un sun;
	int fd;

	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		debug("[rc] socket() failed: %s\n", strerror(errno));
		return -1;
	}
	
	if (bind(fd, (struct sockaddr*) &sun, sizeof(sun))) {
		debug("[rc] bind() failed: %s\n", strerror(errno));
		return -1;
	}

	if (listen(fd, 10)) {
		debug("[rc] listen() failed: %s\n", strerror(errno));
		return -1;
	}

	return fd;
}

/*
 * rc_input_close()
 *
 * zamyka kana³ wej¶ciowy.
 */
void rc_input_close(rc_input_t *r)
{
	if (!r)
		return;

	debug("[rc] closing %s\n", r->path);

	if (r->type == RC_INPUT_PIPE)
		unlink(r->path);

	xfree(r->path);
	close(r->fd);

	list_remove(&rc_inputs, r, 1);
}

