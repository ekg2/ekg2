/*
 *  (C) Copyright 2007	Michał Górny & EKG2 developers
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

PLUGIN_DEFINE(mpd, PLUGIN_GENERIC, NULL);

typedef enum {
	MPD_RECONNECTING	= -1,
	MPD_DISCONNECTED	= 0,
	MPD_RESOLVING,
	MPD_CONNECTING,
	MPD_CONNECTED,
} mpd_state_t;

static char	*mpd_config_host	= NULL;
static char	*mpd_config_password	= NULL;
static int	 mpd_config_port	= 6600;
static mpd_state_t mpd_state		= 0;
static watch_t	*mpd_watch		= NULL;

static TIMER(mpd_connect);

static void mpd_disconnect(const char *dummy) {
	if (mpd_state == MPD_DISCONNECTED) {
		timer_remove(&mpd_plugin, "conn");
		timer_add(&mpd_plugin, "conn", 5, 1, &mpd_connect, NULL);
	} else {
		mpd_state	= MPD_RECONNECTING;

		if (mpd_watch) {
			watch_write(mpd_watch, "close\n");
			watch_timeout_set(mpd_watch, 5);
		}
	}
}

static WATCHER_LINE(mpd_handler) {
	if (type) {
		mpd_state		= MPD_DISCONNECTED;
		mpd_watch->type		= WATCH_NONE;
		watch_free(mpd_watch);
		mpd_watch		= NULL;

		close(fd);
		timer_add(&mpd_plugin, "conn", (MPD_RECONNECTING ? 5 : 300), 1, &mpd_connect, NULL);

		return 0;
	}

	if (mpd_state == MPD_CONNECTING) {
		mpd_state = MPD_CONNECTED;
		if (!xstrncmp(watch, "OK MPD", 6))
			debug("[mpd] Connected to %s\n", watch+3);
		else {
			debug_error("[mpd] Unknown server response: %s\n", watch);
			mpd_disconnect(NULL);
		}

		return 0;
	}

	return 0;
}

static WATCHER(mpd_resolver) {
	int res;
	struct sockaddr_in sin;
	watch_t *w;

	if (type)
		return 0;
	if (mpd_state == MPD_RECONNECTING) {
		timer_add(&mpd_plugin, "conn", 5, 1, &mpd_connect, NULL);
		close(fd);
		return -1;
	}

	res		= read(fd, &sin.sin_addr, sizeof(struct in_addr));
	close(fd);

	sin.sin_family	= PF_INET;
	sin.sin_port	= htons(mpd_config_port);
		/* stolen from jabber plugin */

	if ((res != sizeof(struct in_addr)) || (res && sin.sin_addr.s_addr == INADDR_NONE)) {
		if (res == -1)
			debug_error("[mpd] unable to read data from resolver: %s\n", strerror(errno));
		else
			debug_error("[mpd] read %d bytes from resolver. not good\n", res);

		mpd_state = MPD_DISCONNECTED;
		timer_add(&mpd_plugin, "conn", 300, 1, &mpd_connect, NULL);
		return -1;
	}

	fd		= socket(PF_INET, SOCK_STREAM, 0);
	res		= 1; /* == one */
	
	if ((fd == -1) || (ioctl(fd, FIONBIO, &res) == -1)
			|| (connect(fd, (struct sockaddr*) &sin, sizeof(sin)) == 1 && errno != EINPROGRESS)) {
		debug_error("[mpd] connecting failed: %s\n", strerror(errno));
		if (fd != -1)
			close(fd);

		mpd_state = MPD_DISCONNECTED;
		timer_add(&mpd_plugin, "conn", 300, 1, &mpd_connect, NULL);
		return -1;
	}

	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &res, sizeof(res));

	mpd_state	= MPD_CONNECTING;
	w		= watch_add_line(&mpd_plugin, fd, WATCH_READ_LINE, &mpd_handler, NULL);
	mpd_watch	= watch_add_line(&mpd_plugin, fd, WATCH_WRITE_LINE, NULL, NULL);
	watch_timeout_set(w, 30);

	return -1;
}

static TIMER(mpd_connect) {
	if (type)
		return 0;
	if (mpd_state != MPD_DISCONNECTED)
		return -1;

	if (ekg_resolver2(&mpd_plugin, mpd_config_host, &mpd_resolver, NULL) == NULL) {
		print("generic_error", strerror(errno));
		return 0; /* retry */
	} else {
		mpd_state	= MPD_RESOLVING;
		timer_remove(&mpd_plugin, "conn");
		return -1;
	}
}

int mpd_plugin_init(int prio) {
	variable_add(&mpd_plugin, "host", VAR_STR, 1, &mpd_config_host, &mpd_disconnect, NULL, NULL);
	variable_add(&mpd_plugin, "password", VAR_STR, 0, &mpd_config_password, &mpd_disconnect, NULL, NULL);
	variable_add(&mpd_plugin, "port", VAR_INT, 1, &mpd_config_port, &mpd_disconnect, NULL, NULL);

	mpd_config_host = xstrdup("127.0.0.1");

	plugin_register(&mpd_plugin, prio);

	timer_add(&mpd_plugin, "conn", 5, 1, &mpd_connect, NULL);

	return 0;
}

static int mpd_plugin_destroy() {
	mpd_watch->type		= WATCH_NONE;
	watch_free(mpd_watch);

	plugin_unregister(&mpd_plugin);

	return 0;
}
