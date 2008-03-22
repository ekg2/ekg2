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

#ifndef __EKG_GG_MISC_H
#define __EKG_GG_MISC_H

#include <ekg/sessions.h>
#include <ekg/userlist.h>

int gg_status_to_text(const int status);
int gg_text_to_status(const int status, const char *descr);
char *gg_locale_to_cp(char *buf);
char *gg_cp_to_locale(char *buf);
char gg_userlist_type(userlist_t *u);
int gg_blocked_add(session_t *s, const char *uid);
int gg_blocked_remove(session_t *s, const char *uid);
const char *gg_http_error_string(int h);
int gg_userlist_send(struct gg_session *s, userlist_t *userlist);

void gg_convert_string_destroy();
QUERY(gg_convert_string_init);

#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
