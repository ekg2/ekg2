/* $Id: $ */

/*
 *  (C) Copyright XXX
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

#ifndef __EKG_NET_H
#define __EKG_NET_H
#ifndef EKG2_WIN32_NOFUNCTION

#include "plugins.h"
#include "sessions.h"
#include "srv.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INADDR_NONE
#  define INADDR_NONE (unsigned long) 0xffffffff
#endif

watch_t *ekg_resolver2(plugin_t *plugin, const char *server, watcher_handler_func_t async, void *data);
watch_t *ekg_resolver3(plugin_t *plugin, const char *server, watcher_handler_func_t async, void *data, const int port, const int proto);
watch_t *ekg_resolver4(plugin_t *plugin, const char *server, watcher_handler_func_t async, void *data, const int proto_port, const int port, const int proto);

watch_t *ekg_connect(session_t *session, const char *server, const int proto_port, const int port, watcher_handler_func_t async);

#ifdef __cplusplus
}
#endif

#endif /* EKG2_WIN32_NOFUNCTION */
#endif /* __EKG_NET_H */

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
