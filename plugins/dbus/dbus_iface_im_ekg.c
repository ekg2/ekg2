#include <ekg/debug.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus.h"
#include <stdio.h>

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_getSessions);

static const ekg2_dbus_iface_function_t ekg2_dbus_iface_im_functions[] = {
	{ "getSessions", DBUS_MESSAGE_TYPE_METHOD_CALL, ekg2_dbus_iface_im_ekg2_getSessions }
};

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_getSessions)
{
#define __FUNCTION__ "ekg2_dbus_iface_im_ekg2_getSessions"
	EKG2_DBUS_CALL_HANDLER;
	list_t l;
	char x[1] = "", *tmp;

	EKG2_DBUS_INIT_REPLY;

	for (l = sessions; l; l = l->next)
	{
		session_t *s = l->data;
		EKG2_DBUS_ADD_STRING(&(s->uid));
		EKG2_DBUS_ADD(DBUS_TYPE_BOOLEAN, &(s->connected));
		EKG2_DBUS_ADD_STRING(&(s->status));
		/* XXX convert to utf before sending, d-bus sux? XXX */
		tmp = mutt_convert_string (s->descr, config_console_charset, "utf-8");
		EKG2_DBUS_ADD_STRING(&tmp);
		xfree(tmp);
	}

	EKG2_DBUS_SEND_REPLY;
	
	return DBUS_HANDLER_RESULT_HANDLED;
#undef __FUNCTION__
}

EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2)
{
	int i, type;
	const char *function_name;
	type = dbus_message_get_type(msg);
	function_name = dbus_message_get_member(msg);
	debug_error("zzzz> %s %d == %d\n", function_name, type, DBUS_MESSAGE_TYPE_METHOD_CALL );
	for (i = 0; i < sizeof(ekg2_dbus_iface_im_functions) / sizeof(ekg2_dbus_iface_function_t); i++)
	{
		if (type == ekg2_dbus_iface_im_functions[i].type && ekg2_dbus_iface_im_functions[i].handler && !xstrcmp(function_name, ekg2_dbus_iface_im_functions[i].name)) {
			debug_function("calling handler\n");
			return ekg2_dbus_iface_im_functions[i].handler(conn, msg, data);
		}
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}
