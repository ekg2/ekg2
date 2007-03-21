#ifndef __EKG2_DBUS_H
#define __EKG2_DBUS_H

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <iconv.h>

#define DBUS_ORG_FREEDESKTOP_IM_INTERFACE "org.freedesktop.im"

#define EKG2_DBUS_IFACE_HANDLER(x) DBusHandlerResult x(DBusConnection *conn, DBusMessage *msg, void *data)

struct ekg2_dbus_iface_proto {
	char *ifaceline;
	char *name;
	DBusHandleMessageFunction handler;
};
typedef struct ekg2_dbus_iface_proto ekg2_dbus_iface_proto_t;

struct ekg2_dbus_iface_function {
	char *name;
	int type; /* DBUS_MESSAGE_TYPE_METHOD_CALL or DBUS_MESSAGE_TYPE_SIGNAL */
	DBusHandleMessageFunction handler;
};
typedef struct ekg2_dbus_iface_function ekg2_dbus_iface_function_t;

char *mutt_convert_string (char *ps, const char *from, const char *to);

#define EKG2_DBUS_CALL_HANDLER DBusMessage *reply; \
		DBusMessageIter args; \
		dbus_uint32_t serial = 0;

#define EKG2_DBUS_INIT_REPLY reply = dbus_message_new_method_return(msg); \
		dbus_message_iter_init_append(reply, &args)

#define EKG2_DBUS_ADD(type, x) do { \
			if (!dbus_message_iter_append_basic(&args, type, (x) )) { \
				debug("%s cannot allocate memory?\n", __FUNCTION__); \
				ekg_oom_handler(); \
			} \
		} while(0)

#define EKG2_DBUS_ADD_STRING(x) EKG2_DBUS_ADD(DBUS_TYPE_STRING, x)

#define EKG2_DBUS_SEND_REPLY do {  \
			if (!dbus_connection_send(conn, reply, &serial)) { \
				debug("Cannot send reply!\n"); \
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED; /* XXX */ \
			} \
			dbus_connection_flush(conn); \
		} while(0)
	
#endif /* __EKG2_DBUS_H */
