/* $Id$ */

#ifndef __EKG_GG_PUBDIR_H
#define __EKG_GG_PUBDIR_H

#include <ekg/commands.h>

list_t gg_registers;
list_t gg_unregisters;
list_t gg_reminds;
list_t gg_userlists;

int gg_register_done;
char *gg_register_password;
char *gg_register_email;

COMMAND(gg_command_register);
COMMAND(gg_command_unregister);
COMMAND(gg_command_passwd);
COMMAND(gg_command_remind);
COMMAND(gg_command_list);

#endif

