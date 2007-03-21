#include <ekg/debug.h>
#include <ekg/xmalloc.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/queries.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus.h"

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_getProtocols);

static const ekg2_dbus_iface_function_t ekg2_dbus_iface_im_functions[] = {
	{ "getProtocols", DBUS_MESSAGE_TYPE_METHOD_CALL, ekg2_dbus_iface_im_getProtocols }
};

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_getProtocols)
{
#define __FUNCTION__ "ekg2_dbus_iface_im_getProtocols"

	EKG2_DBUS_CALL_HANDLER;
	char **protos = NULL;
	int i;
	
	query_emit_id (NULL, GET_PLUGIN_PROTOCOLS, &protos);
	
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &args);
	i = 0;
	while (protos[i])
	{
		EKG2_DBUS_ADD_STRING(&(protos[i++]));
	}
	xfree(protos);

	EKG2_DBUS_SEND_REPLY;

	return DBUS_HANDLER_RESULT_HANDLED;
#undef __FUNCTION__
}

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_getPresence)
{
	DBusMessage *reply;
	DBusMessageIter args;
	char **protos = NULL;
	dbus_uint32_t serial = 0;
	int i;
	
	query_emit_id (NULL, GET_PLUGIN_PROTOCOLS, &protos);
	
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &args);
	i = 0;
	while (protos[i])
	{
		if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &(protos[i++]) )) {
			debug("ekg2_dbus_iface_im_getProtocols cannot allocate memory?\n");
			ekg_oom_handler();
		}
	}
	xfree(protos);

	if (!dbus_connection_send(conn, reply, &serial)) {
		debug("Cannot send reply!\n");
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; /* XXX */
	}
	dbus_connection_flush(conn);
	
	return DBUS_HANDLER_RESULT_HANDLED;
}

EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im)
{
	int i, type;
	const char *function_name;
	type = dbus_message_get_type(msg);
	function_name = dbus_message_get_member(msg);
	debug_error("zzzz> %s %d == %d\n", function_name, type,DBUS_MESSAGE_TYPE_METHOD_CALL );
	for (i = 0; i < sizeof(ekg2_dbus_iface_im_functions) / sizeof(ekg2_dbus_iface_function_t); i++)
	{
		if (type == ekg2_dbus_iface_im_functions[i].type && ekg2_dbus_iface_im_functions[i].handler && !xstrcmp(function_name, ekg2_dbus_iface_im_functions[i].name)) {
			debug_function("calling handler\n");
			return ekg2_dbus_iface_im_functions[i].handler(conn, msg, data);
		}
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}
