/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *                2004 Piotr Kupisiewicz <deletek@ekg2.org>
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

#ifndef __EKG_GG_GG_H
#define __EKG_GG_GG_H

#include <libgadu.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>

COMMAND(gg_command_image); /* images.c */

plugin_t gg_plugin;
int gg_userlist_put_config;
char *last_tokenid;

/* variables */
int gg_config_display_token;
int gg_config_dcc;
char *gg_config_dcc_dir;
char *gg_config_dcc_ip;
char *gg_config_dcc_limit;
int gg_config_dcc_port;
int gg_config_split_messages;

typedef enum {
	GG_QUIET_CHANGE = 0x0001
} gg_quiet_t;

typedef struct {
	struct gg_session *sess;	/* sesja */
	list_t searches;		/* operacje szukania */
	list_t passwds;			/* operacje zmiany has³a */
	gg_quiet_t quiet;		/* co ma byæ cicho */
} gg_private_t;

void gg_register_commands();

void gg_session_handler_msg(session_t *s, struct gg_event *e);
void gg_session_handler(int type, int fd, int watch, void *data);

COMMAND(gg_command_modify);

#endif /* __EKG_GG_GG_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
