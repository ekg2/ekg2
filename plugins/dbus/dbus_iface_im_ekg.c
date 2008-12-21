#include <ekg/debug.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/xmalloc.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus.h"
#include <stdio.h>

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_getSessions);

static ekg2_dbus_iface_function_t const ekg2_dbus_iface_im_functions[] = {
	{ "getSessions", DBUS_MESSAGE_TYPE_METHOD_CALL, ekg2_dbus_iface_im_ekg2_getSessions }
};

static EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_getSessions)
{
#define __FUNCTION__ "ekg2_dbus_iface_im_ekg2_getSessions"
	EKG2_DBUS_CALL_HANDLER_VARIABLES;
	session_t *sl;
	char x[1] = "", *tmp_descr, *tmp;

	EKG2_DBUS_INIT_REPLY;

	for (sl = sessions; sl; sl = sl->next)
	{
		session_t *s = sl;

		EKG2_DBUS_ADD_STRING(&(s->uid));
#warning "XXX: Old API here, need updating."
		/* mg: updated? */
#if 0
		EKG2_DBUS_ADD(DBUS_TYPE_BOOLEAN, &(s->connected));
#endif
		tmp = (char *)session_get(s, "status");
		EKG2_DBUS_ADD_STRING(&tmp);
		/* XXX convert to utf before sending, d-bus sux? XXX */
		tmp = (char *)session_descr_get(s);
		tmp = xstrdup(tmp?tmp:"");
		tmp_descr = ekg_convert_string (tmp, NULL, "utf-8");
		EKG2_DBUS_ADD_STRING(tmp_descr ? &tmp_descr : &tmp);
		xfree(tmp_descr);
		xfree(tmp);
	}
	EKG2_DBUS_SEND_REPLY;
	
	return DBUS_HANDLER_RESULT_HANDLED;
#undef __FUNCTION__
}

EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2)
{
	int i, type;
	char const * const function_name = dbus_message_get_member(msg);
	type = dbus_message_get_type(msg);

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
