/* $Id$ */

#ifndef __EKG_GG_PUBDIR50_H
#define __EKG_GG_PUBDIR50_H

#include <ekg/commands.h>

COMMAND(gg_command_find);
COMMAND(gg_command_change);

void gg_session_handler_search50(session_t *s, struct gg_event *e);
void gg_session_handler_change50(session_t *s, struct gg_event *e);

#endif
