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

#ifndef __EKG_GG_PUBDIR_H
#define __EKG_GG_PUBDIR_H

#include <ekg/commands.h>

extern list_t gg_registers;
extern list_t gg_unregisters;
extern list_t gg_reminds;
extern list_t gg_userlists;

extern int gg_register_done;
extern char *gg_register_password;
extern char *gg_register_email;

COMMAND(gg_command_register);
COMMAND(gg_command_unregister);
COMMAND(gg_command_passwd);
COMMAND(gg_command_remind);
COMMAND(gg_command_list);

#endif


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
