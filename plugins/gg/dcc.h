/* $Id$ */

#ifndef __EKG_GG_DCC_H
#define __EKG_GG_DCC_H

#include <ekg/protocol.h>
#include <ekg/commands.h>

struct gg_dcc *gg_dcc_socket;

COMMAND(gg_command_dcc);

int gg_dcc_socket_open();
void gg_dcc_socket_close();
void gg_dcc_handler(int type, int fd, int watch, void *data);

#endif


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
