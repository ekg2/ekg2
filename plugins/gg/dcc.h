/* $Id$ */

#ifndef __EKG_GG_DCC_H
#define __EKG_GG_DCC_H

#include <ekg/protocol.h>
#include <ekg/commands.h>

COMMAND(gg_command_dcc);

void gg_changed_dcc(const char *var);
int gg_dcc_socket_open(int port);
void gg_dcc_socket_close();
void gg_dcc_audio_init();
void gg_dcc_audio_close();
dcc_t *gg_dcc_find(void *D);


WATCHER(gg_dcc_handler);
#ifdef HAVE_GG_DCC7
WATCHER(gg_dcc7_handler);
#endif
#endif


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
