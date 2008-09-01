#include <ekg/debug.h>
#include <ekg/sessions.h> /* sessions */
#include <ekg/xmalloc.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/queries.h>
#include <ekg/stuff.h> /* ekg_status_int */
#include <ekg/userlist.h> /* EKG_STATUS_UNKNOWN */
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus.h"

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_getProtocols);
static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_setStatus);

static ekg2_dbus_iface_function_t const ekg2_dbus_iface_im_functions[] = {
	{ "getProtocols", DBUS_MESSAGE_TYPE_METHOD_CALL, ekg2_dbus_iface_im_getProtocols },
	{ "setStatus", DBUS_MESSAGE_TYPE_METHOD_CALL, ekg2_dbus_iface_im_setStatus }
};

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_getProtocols)
{
#define __FUNCTION__ "ekg2_dbus_iface_im_getProtocols"

	EKG2_DBUS_CALL_HANDLER_VARIABLES;
	const plugin_t *p;

	EKG2_DBUS_INIT_REPLY;
	for (p = plugins; p; p = p->next) {
		if (p->pclass == PLUGIN_PROTOCOL) {
			const char **a;

			for (a = p->priv.protocol.protocols; *a; a++)
				EKG2_DBUS_ADD_STRING(a);
		}
	}

	EKG2_DBUS_SEND_REPLY;

	return DBUS_HANDLER_RESULT_HANDLED;
#undef __FUNCTION__
}

/* m setStatus(DBUS_TYPE_STRING:presence, DBUS_TYPE_STRING:description) */
static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_setStatus)
{
#define __FUNCTION__ "ekg2_dbus_iface_im_setStatus"
	EKG2_DBUS_CALL_HANDLER_VARIABLES;
	static char const status_errory[][20] = { "OK", "wrong argument", "session not found" };
	char *error;
	DBusMessageIter iter;
	char const *param, *cmd;
	int current_type, st;
	session_t *s;
	list_t l;

	EKG2_DBUS_INIT_REPLY;

	dbus_message_iter_init (msg, &iter);
	if ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_STRING)
	{
		error = status_errory[1];
		EKG2_DBUS_ADD_STRING(&error);
		goto send_and_return;
	} 
	dbus_message_iter_get_basic (&iter, &param);
	st = ekg_status_int(param);

	dbus_message_iter_next (&iter);
	if ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_STRING)
	{
		error = status_errory[1];
		EKG2_DBUS_ADD_STRING(&error);
		goto send_and_return;
	} 
	dbus_message_iter_get_basic (&iter, &param);

	cmd = ekg_status_string(st, 1);
	for (l = sessions; l; l = l->next)
	{
		s = l->data;
		debug ("changing (%s) to: %s %s\n", s->uid, cmd, param);
		command_exec_format(NULL, s, 1, ("/%s %s"), cmd, param);
	}

	error = status_errory[0];
	EKG2_DBUS_ADD_STRING(&error);

send_and_return:
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
	const plugin_t *p;
	
	reply = dbus_message_new_method_return(msg);
	dbus_message_iter_init_append(reply, &args);
	for (p = plugins; p; p = p->next) {
		if (p->pclass == PLUGIN_PROTOCOL) {
			const char **a;

			for (a = p->priv.protocol.protocols; *a; a++) {
				if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, a)) {
					debug("ekg2_dbus_iface_im_getProtocols cannot allocate memory?\n");
					ekg_oom_handler();
				}
			}
		}
	}

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
	char const * const function_name = dbus_message_get_member(msg);
	type = dbus_message_get_type(msg);

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
