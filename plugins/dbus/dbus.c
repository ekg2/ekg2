/*
 *  (C) Copyright 2006	Michal 'GiM' Spadlinski  < gim913 a@t gmail d.o.t com>
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

#include <errno.h>
#include <string.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/xmalloc.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus.h"
#include "dbus_iface_im.h"
#include "dbus_iface_im_ekg.h"
#include "dbus_iface_im_ekg_ui.h"
#include "dbus_iface_im_ekg_protocol.h"
#include "dbus_iface_im_ekg_session.h"
#include "dbus_iface_im_ekg_crypto.h"
#include "dbus_iface_im_ekg_logging.h"


PLUGIN_DEFINE(dbus, PLUGIN_GENERIC, NULL);

static DBusConnection *conn;
static DBusError err;

static ekg2_dbus_iface_proto_t const ekg2_dbus_interfaces[] =
{
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE "'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE,
		ekg2_dbus_iface_im },
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2",
		ekg2_dbus_iface_im_ekg2 },
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.ui'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.ui",
		ekg2_dbus_iface_im_ekg2_ui },
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.protocol'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.protocol",
		ekg2_dbus_iface_im_ekg2_protocol },
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.session'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.session",
		ekg2_dbus_iface_im_ekg2_session },
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.crypto'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.crypto",
		ekg2_dbus_iface_im_ekg2_crypto },
	{ "interface='" DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.logging'",
		DBUS_ORG_FREEDESKTOP_IM_INTERFACE ".ekg2.logging",
		ekg2_dbus_iface_im_ekg2_logging }
};

#define EKG2_DBUS_MATCH(str)  do { \
   			dbus_bus_add_match(conn, str, &err); \
			if (dbus_error_is_set(&err)) { \
				debug("match error at %s (probably resources exhausted): %s\n", str, err.message); \
				dbus_connection_close(conn); conn = NULL; \
				return -1; \
			}  \
		} while(0)


struct ekg2_dbus_watch_data {
	DBusConnection *con;
	DBusWatch *dbw;
};
typedef struct ekg2_dbus_watch_data * ekg_dbus_watch_data_t;

/* This is main message (from dbus) handler.
 *
 * This function iterates over ekg2_dbus_interfaces (you have read
 * README, don't you?), finds proper interface handler, and passes
 * control to it. Proper interface handlers are responsible for further
 * parsing.
 */
DBusHandlerResult ekg2_dbus_message_handler(DBusConnection *conn, DBusMessage *msg, void *empty)
{
	int i;

	if (NULL == msg) {
		return DBUS_HANDLER_RESULT_HANDLED;
	} else {
		char const * const iface = dbus_message_get_interface(msg);

		debug("path: %s signal:%d\n", dbus_message_get_path(msg), dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_SIGNAL);
		debug("interface: %s\n", dbus_message_get_interface(msg));

		for (i = 0; i < sizeof(ekg2_dbus_interfaces)/sizeof(ekg2_dbus_iface_proto_t); i++)
			if (!xstrcmp(iface, ekg2_dbus_interfaces[i].name) && ekg2_dbus_interfaces[i].handler)
				return ekg2_dbus_interfaces[i].handler(conn, msg, empty);

	}
	return DBUS_HANDLER_RESULT_HANDLED;
}


WATCHER(ekg2_dbus_read_watch)
{
	ekg_dbus_watch_data_t p = (ekg_dbus_watch_data_t)data;

	debug("in read watch!\n");
	if (type)
	{
		debug("fd lost in time o_0?\n");
		return 0;
	}

	dbus_connection_ref(p->con);

	dbus_watch_handle(p->dbw, DBUS_WATCH_READABLE);
	/* run the dispatcher */
	while (dbus_connection_dispatch(p->con) == DBUS_DISPATCH_DATA_REMAINS);

	dbus_connection_unref(p->con);
	return 0;
}

WATCHER(ekg2_dbus_write_watch)
{
	/*ekg_dbus_watch_data_t p = (ekg_dbus_watch_data_t)data;*/
	debug("in write watch!\n");
	if (type)
	{
		debug("fd lost in time o_0?\n");
		return 0;
	}
	return 0;
}

dbus_bool_t ekg2_dbus_add_watch(DBusWatch *w, void *data)
{
	DBusConnection *con = (DBusConnection *)data;
	ekg_dbus_watch_data_t ekg_watcher_data;
	unsigned int flags;
	int fd;

	debug(" ekg2_dbus_add_watch step 1\n");
	if (!dbus_watch_get_enabled(w))
		return 1;
	debug(" ekg2_dbus_add_watch step 2\n");

	ekg_watcher_data = xcalloc(1, sizeof(struct ekg2_dbus_watch_data));
	ekg_watcher_data->con = con;
	ekg_watcher_data->dbw = w;

	fd = dbus_watch_get_fd(w);
	flags = dbus_watch_get_flags(w);

	if (flags & DBUS_WATCH_READABLE)
	{
		debug (" ekg2_dbus_add_watch readable bit set\n");
		watch_add(&dbus_plugin, fd, WATCH_READ, ekg2_dbus_read_watch, ekg_watcher_data);
	}
	if (flags & DBUS_WATCH_WRITABLE)
	{
		debug (" ekg2_dbus_add_watch writeable bit set\n");
		watch_add(&dbus_plugin, fd, WATCH_WRITE, ekg2_dbus_write_watch, ekg_watcher_data);
	}
	dbus_watch_set_data(w, ekg_watcher_data, NULL);
	return 1;
}

void ekg2_dbus_remove_watch(DBusWatch *w, void *data)
{
	int fd = dbus_watch_get_fd(w), flags = dbus_watch_get_flags(w);
	
	debug(" ekg2_dbus_remove_watch step 1\n");
	if (!dbus_watch_get_enabled(w))
		return;
	debug(" ekg2_dbus_remove_watch step 2\n");

	if (flags & DBUS_WATCH_WRITABLE)
		watch_remove(&dbus_plugin, fd, WATCH_WRITE);
	if (flags & DBUS_WATCH_READABLE)
		watch_remove(&dbus_plugin, fd, WATCH_READ);

	dbus_watch_set_data(w, NULL, NULL);
	xfree(data);

	return;
}

void ekg2_dbus_toggle_watch(DBusWatch *watch, void *data)
{
	debug(" ekg2_dbus_toggle_watch\n");
	if (dbus_watch_get_enabled(watch))
		ekg2_dbus_add_watch(watch, data);
	else
		ekg2_dbus_remove_watch(watch, data);
}

/* This function connects plugin to dbus session in a system
 * it set up some dummy watch functions, finally, it informs
 * dbus about interfaces, we want to watch (defined in
 * ekg2_dbus_interfaces array) and associate main filter:
 * ekg2_dbus_message_handler()
 */
EXPORT int dbus_plugin_init(int prio) {
	int ret, i;

   	dbus_error_init(&err);

	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) { 
		debug(" [-] dbus connection failed: %s\n", err.message); 
		dbus_error_free(&err); 
		return -1;
	}
	if (NULL == conn) { 
		debug(" [-] dbus initialization failed\n");
		return -1;
	}

	ret = dbus_bus_request_name(conn, DBUS_ORG_FREEDESKTOP_IM_INTERFACE, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) { 
		debug(" [-] dbus name setting error: %s\n", err.message);
		dbus_error_free(&err); 
		dbus_connection_close(conn); conn = NULL;
		return -1;
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
		debug(" [-] dbus, name setting error\n");
		return -1;
	}

	if(!dbus_connection_set_watch_functions (conn,
				ekg2_dbus_add_watch,
				ekg2_dbus_remove_watch,
				ekg2_dbus_toggle_watch,
				conn,
				NULL))
	{
		debug(" [-] dbus, couldn't set watches!\n");
		dbus_connection_close(conn); conn = NULL;
		return -1;
	}
	
	for (i = 0; i < sizeof(ekg2_dbus_interfaces)/sizeof(ekg2_dbus_iface_proto_t); i++)
		EKG2_DBUS_MATCH(ekg2_dbus_interfaces[i].ifaceline);

	dbus_connection_add_filter(conn, ekg2_dbus_message_handler, NULL, NULL);
	dbus_connection_flush(conn);

	plugin_register(&dbus_plugin, prio);

	debug("plugin initialized!\n");
	return 0;
}

static int dbus_plugin_destroy() {
	debug("destroying plugin\n");
	if (conn)
	{
		debug("+calling set watch NULL\n");
		dbus_connection_set_watch_functions(conn, NULL, NULL, NULL, /*data*/NULL, NULL);
		debug("+after call!\n");
		dbus_connection_close(conn);
	}

	plugin_unregister(&dbus_plugin);
	return 0;
}
