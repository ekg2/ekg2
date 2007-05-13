/* $Id */

#include <ekg/win32.h>

#ifndef NO_POSIX_SYSTEM
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
// #include <sys/socket.h>	/* <sys/socket.h> included twice, wtf? */	/* introduced in http://lists.ziew.org/mailman/pipermail/ekg2-commit/2007-May/003692.html */
#endif

#include <sys/types.h>
#include <sys/socket.h>		/* this <sys/socket.h> introduced in: 
					http://lists.ziew.org/mailman/pipermail/ekg2-commit/2007-April/003464.html and afair it fix freebsd compilation */
#include <sys/stat.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <ekg/debug.h>
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
#ifndef NO_POSIX_SYSTEM
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
#else
	return -1;
#endif
}

/*
 * rc_input_new_unix()
 *
 * tworzy gniazdo AF_UNIX.
 */
int rc_input_new_unix(const char *path)
{
#ifndef NO_POSIX_SYSTEM
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
#else
	return -1;
#endif
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
