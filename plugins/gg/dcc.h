/* $Id$ */

#ifndef __EKG_GG_DCC_H
#define __EKG_GG_DCC_H

#include <ekg/protocol.h>
#include <ekg/commands.h>

COMMAND(gg_command_dcc);

void gg_changed_dcc(const CHAR_T *var);
int gg_dcc_socket_open(int port);
void gg_dcc_socket_close();
void gg_dcc_audio_init();
void gg_dcc_audio_close();
WATCHER(gg_dcc_handler);

#endif


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
