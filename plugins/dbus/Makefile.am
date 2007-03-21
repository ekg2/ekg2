lib_LTLIBRARIES = dbus.la
dbus_la_SOURCES = dbus.c dbus_iface_im.c dbus_iface_im_ekg.c dbus_iface_im_ekg_ui.c dbus_iface_im_ekg_protocol.c dbus_iface_im_ekg_session.c dbus_iface_im_ekg_crypto.c dbus_iface_im_ekg_logging.c
dbus_la_CFLAGS = `pkg-config dbus-1 --cflags` -Werror-implicit-function-declaration
dbus_la_LDFLAGS = `pkg-config dbus-1 --libs` -module -avoid-version

libdir = $(plugindir)


