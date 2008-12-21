#include <ekg/debug.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>
#include <dbus/dbus.h>
#include "dbus.h"

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_session_setStatus);

static ekg2_dbus_iface_function_t const ekg2_dbus_iface_im_ekg2_session_functions[] = {
	{ "setStatus", DBUS_MESSAGE_TYPE_METHOD_CALL, ekg2_dbus_iface_im_ekg2_session_setStatus }
};

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_session_setStatus)
{
	static const char status_errory[][20] = { "OK", "wrong argument", "session not found" };
	char *error;
	EKG2_DBUS_CALL_HANDLER_VARIABLES;
	DBusMessageIter iter;
	char const *param, *cmd;
	int current_type, st;
	session_t *s;

	EKG2_DBUS_INIT_REPLY;

	dbus_message_iter_init (msg, &iter);
	if ((current_type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_STRING)
	{
		error = status_errory[1];
		EKG2_DBUS_ADD_STRING(&error);
		goto send_and_return;
	} else {
		dbus_message_iter_get_basic (&iter, &param);
		if ((s = session_find(param)) == NULL)
		{
			error = status_errory[2];
			EKG2_DBUS_ADD_STRING(&error);
			goto send_and_return;
		}
	}

	dbus_message_iter_next (&iter);
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

	/* session_unidle(s); */
	/* should we use status provided in dbus parameter
	 * or keep current status
	 */
	cmd = ekg_status_string(st, 1);
	debug ("changing to: %s %s\n", cmd, param);
	command_exec_format(NULL, s, 1, ("/%s %s"), cmd, param);

	error = status_errory[0];
	EKG2_DBUS_ADD_STRING(&error);

send_and_return:
	EKG2_DBUS_SEND_REPLY;

	return DBUS_HANDLER_RESULT_HANDLED;
}


EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_session)
{
	int i, type;
	char const * const function_name = dbus_message_get_member(msg);

	debug_error("XXXXXwe're in handler dude: %s\n", dbus_message_get_interface(msg));

	type = dbus_message_get_type(msg);

	debug_error("zzzz> %s %d == %d\n", function_name, type, DBUS_MESSAGE_TYPE_METHOD_CALL );

	for (i = 0; i < sizeof(ekg2_dbus_iface_im_ekg2_session_functions) / sizeof(ekg2_dbus_iface_function_t); i++)
	{
		if (type == ekg2_dbus_iface_im_ekg2_session_functions[i].type && 
				ekg2_dbus_iface_im_ekg2_session_functions[i].handler &&
				!xstrcmp(function_name, ekg2_dbus_iface_im_ekg2_session_functions[i].name)) {
			debug_function("calling handler\n");
			return ekg2_dbus_iface_im_ekg2_session_functions[i].handler(conn, msg, data);
		}
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}
