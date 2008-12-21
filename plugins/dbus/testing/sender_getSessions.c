#include <stdio.h>
#include <dbus/dbus.h>

int main(int argc, char **argv)
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter args;
	DBusPendingCall* pending;
	DBusError err;
	dbus_uint32_t serial = 0; // unique number to associate replies with requests
	int current_type;
	char *buf, *retek;
	int val, type, boolek;
	dbus_uint32_t level;

   	int ret;
   	dbus_error_init(&err);

	// connect to the bus
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) { 
		fprintf(stderr, " [+] Connection Error (%s)\n", err.message); 
		dbus_error_free(&err); 
	}
	if (NULL == conn) { 
		return 1;
	}

	// create a signal and check for errors 
	msg = dbus_message_new_method_call("org.freedesktop.im", // target for the method call
			"/dupa/dupa", // object to call on
			"org.freedesktop.im.ekg2", // interface to call on
			"getSessions"); // method name
	if (NULL == msg) 
	{ 
		fprintf(stderr, "Message Null\n"); 
		return 1;
	} 

	// append arguments onto signal
//	dbus_message_iter_init_append(msg, &args);

	/*
	buf = strdup("super tajne dane!");
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &buf)) { 
		fprintf(stderr, "Out Of Memory!\n"); 
		return 1;
	}
	free(buf);

	val = 666;
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &val)) { 
		fprintf(stderr, "Out Of Memory!\n"); 
		return 1;
	}*/

   // send message and get a handle for a reply
   if (!dbus_connection_send_with_reply (conn, msg, &pending, -1)) { // -1 is default timeout
      fprintf(stderr, "Out Of Memory!\n"); 
      return (1);
   }
   if (NULL == pending) { 
      fprintf(stderr, "Pending Call Null\n"); 
      return (1); 
   }
   dbus_connection_flush(conn);
   
   printf("Request Sent\n");
   
   // free message
   dbus_message_unref(msg);
   
   // block until we recieve a reply
   dbus_pending_call_block(pending);

   // get the reply message
   msg = dbus_pending_call_steal_reply(pending);
   if (NULL == msg) {
      fprintf(stderr, "Reply Null\n"); 
      return (1); 
   }
   // free the pending message handle
   dbus_pending_call_unref(pending);

   dbus_message_iter_init (msg, &args);
   while ((type = dbus_message_iter_get_arg_type(&args)) != DBUS_TYPE_INVALID)
   {
      if (DBUS_TYPE_STRING == type)
      {
         dbus_message_iter_get_basic(&args, &retek);
	 fprintf(stderr, " str: %s\n", retek);
      } else if (DBUS_TYPE_BOOLEAN) {
         dbus_message_iter_get_basic(&args, &boolek);
	 fprintf(stderr, "bool: %s\n", boolek?"yes":"no");
      }
       dbus_message_iter_next (&args);
   }
	
   // free reply and close connection
   dbus_message_unref(msg);   
   //dbus_connection_close(conn);
   /*
	// send the message and flush the connection
	if (!dbus_connection_send(conn, msg, &serial)) { 
		fprintf(stderr, "Out Of Memory!\n"); 
		return 1;
	}
	dbus_connection_flush(conn);
	
	// free the message 
	dbus_message_unref(msg);
	dbus_connection_close(conn);*/
	return 0;
}

