/* $Id$ */

/*
 *  (C) Copyright 2001-2003 Wojtek Kaniewski <wojtekka@irc.pl>
 *                          Robert J. Wo¼ny <speedy@ziew.org>
 *                          Pawe³ Maziarz <drg@infomex.pl>
 *                          Wojciech Bojdo³ <wojboj@htc.net.pl>
 *                          Piotr Wysocki <wysek@linux.bydg.org>
 *                          Dawid Jarosz <dawjar@poczta.onet.pl>
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

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#ifndef HAVE_STRLCPY
#  include "compat/strlcpy.h"
#endif

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>

#include "ioctld.h"

static int ioctld_sock = -1;
static int ioctld_pid = -1;
static const char *ioctld_sock_path = NULL;

PLUGIN_DEFINE(ioctld, PLUGIN_GENERIC, NULL);

/*
 * ioctld_kill()
 *
 * zajmuje siê usuniêciem ioctld z pamiêci.
 */
static void ioctld_kill()
{
        if (ioctld_pid > 0)
                kill(ioctld_pid, SIGINT);
}

/*
 * ioctld_parse_seq()
 *
 * zamieñ string na odpowiedni± strukturê dla ioctld.
 *
 *  - seq,
 *  - data.
 *
 * 0/-1.
 */
static int ioctld_parse_seq(const char *seq, struct action_data *data)
{
	char **entries;
	int i;

        if (!data || !seq)
                return -1;

	memset(data, 0, sizeof(struct action_data));

	entries = array_make(seq, ",", 0, 0, 1);

	for (i = 0; entries[i] && i < IOCTLD_MAX_ITEMS; i++) {
		int delay;
		char *tmp;
		
		if ((tmp = xstrchr(entries[i], '/'))) {
			*tmp = 0;
			delay = atoi(tmp + 1);
		} else
			delay = IOCTLD_DEFAULT_DELAY;
			
		data->value[i] = atoi(entries[i]);
		data->delay[i] = delay;
	}

	array_free(entries);

	return 0;
}

/*
 * ioctld_socket()
 *
 * inicjuje gniazdo dla ioctld.
 *
 *  - path - ¶cie¿ka do gniazda.
 *
 * 0/-1.
 */
static int ioctld_socket(const char *path)
{
	struct sockaddr_un sockun;
	int i, usecs = 50000;

	if (ioctld_sock != -1)
		close(ioctld_sock);

	if ((ioctld_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
		return -1;

	sockun.sun_family = AF_UNIX;
	strlcpy(sockun.sun_path, path, sizeof(sockun.sun_path));

	for (i = 5; i; i--) {
		if (connect(ioctld_sock, (struct sockaddr*) &sockun, sizeof(sockun)) != -1)
			return 0;

		usleep(usecs);
	}

        return -1;
}

/*
 * ioctld_send()
 *
 * wysy³a do ioctld polecenie uruchomienia danej akcji.
 *
 *  - seq - sekwencja danych,
 *  - act - rodzaj akcji.
 *
 * 0/-1.
 */
static int ioctld_send(const char *seq, int act, int quiet)
{
	struct action_data data;

	if (*seq == '$')	/* dla kompatybilno¶ci ze starym zachowaniem */
		seq++;

	if (!xisdigit(*seq)) {
		const char *tmp = format_find(seq);

		if (!xstrcmp(tmp, "")) {
			printq("events_seq_not_found", seq);
			return -1;
		}

		seq = tmp;
	}

	if (ioctld_parse_seq(seq, &data)) {
		printq("events_seq_incorrect", seq);
		return -1;
	}

	data.act = act;

	return send(ioctld_sock, &data, sizeof(data), 0);
}

static COMMAND(command_beeps_spk)
{
	PARASC
	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	return ((ioctld_send(params[0], ACT_BEEPS_SPK, quiet) == -1) ? -1 : 0);
}

static COMMAND(command_blink_leds)
{
	PARASC
	if (!params[0]) {
		printq("not_enough_params", name);
		return -1;
	}

	return ((ioctld_send(params[0], ACT_BLINK_LEDS, quiet) == -1) ? -1 : 0);
}

int ioctld_plugin_init(int prio)
{
	plugin_register(&ioctld_plugin, prio);

	ioctld_sock_path = prepare_path(".socket", 1);
	
	if (!(ioctld_pid = fork())) {
		execl(IOCTLD_PATH, "ioctld", ioctld_sock_path, (void *) NULL);
		exit(0);
	}
	
	ioctld_socket(ioctld_sock_path);
	
	atexit(ioctld_kill);

	command_add(&ioctld_plugin, TEXT("ioctld:beeps_spk"), TEXT("?"), command_beeps_spk, 0, NULL);
	command_add(&ioctld_plugin, TEXT("ioctld:blink_leds"), TEXT("?"), command_blink_leds, 0, NULL);
	
	return 0;
}

static int ioctld_plugin_destroy()
{
	plugin_unregister(&ioctld_plugin);

	if (ioctld_sock != -1)
		close(ioctld_sock);

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
