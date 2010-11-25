/* $Id$ */

/*
 *  (C) Copyright 2003 Wojtek Kaniewski <wojtekka@irc.pl
 *		  2004 Piotr Kupisiewicz <deletek@ekg2.org>
 *		  2004 - 2006 Adam Mikuta <adamm@ekg2.org>
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
#include <ekg/userlist.h>

COMMAND(gg_command_image); /* images.c */

extern plugin_t gg_plugin;
extern char *last_tokenid;

/* variables */
extern int gg_config_display_token;
extern int gg_config_audio;
extern int gg_config_dcc;
extern char *gg_config_dcc_ip;
extern char *gg_config_dcc_limit;
extern int gg_config_dcc_port;
extern int gg_config_get_images;
extern char *gg_config_images_dir;
extern int gg_config_image_size;
extern int gg_config_split_messages;

typedef enum {
	GG_QUIET_CHANGE = 0x0001
} gg_quiet_t;

typedef struct {
	struct		gg_session *sess;	/* sesja */
	list_t		searches;		/* operacje szukania */
	list_t		passwds;		/* operacje zmiany has³a */
	gg_quiet_t	quiet;			/* co ma byæ cicho */
	int		curr_prtcl_ver;		/* current protocol version */

		/* (annoying) description scrolling */
	unsigned int	scroll_op	: 1;
	int		scroll_pos;
	time_t		scroll_last;
} gg_private_t;

void gg_register_commands();

WATCHER_SESSION(gg_session_handler);

typedef struct {
	char *uid;
	session_t *session;
} gg_currently_checked_t;

extern list_t gg_currently_checked;

/**
 * gg_userlist_private_t
 *
 * Here we keep all userlist things, which are private to GG protocol, and because of this were removed from core userlist_t.
 */
typedef struct {

	int protocol;		/**< Protocol version */

} gg_userlist_private_t;

#define gg_userlist_priv_get(u) ((gg_userlist_private_t *) userlist_private_get(&gg_plugin, u))

#endif /* __EKG_GG_GG_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
