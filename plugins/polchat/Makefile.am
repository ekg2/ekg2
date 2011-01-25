EXTRA_DIST = polchat_handlers.inc

lib_LTLIBRARIES = polchat.la
polchat_la_SOURCES = polchat.c
polchat_la_LDFLAGS = -module -avoid-version
polchat_la_CFLAGS = $(C_FVISIBILITY) $(GLIB_CFLAGS) $(AM_CFLAGS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)
