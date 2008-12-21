#include <ekg/debug.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus.h"

EKG2_DBUS_IFACE_HANDLER(ekg2_dbus_iface_im_ekg2_protocol)
{
	debug_error("XXXXXwe're in handler dude: %s\n", dbus_message_get_interface(msg));
	return DBUS_HANDLER_RESULT_HANDLED;
}
