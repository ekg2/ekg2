lib_LTLIBRARIES = rot13.la
rot13_la_SOURCES = rot13.c
rot13_la_LDFLAGS = -module -avoid-version
rot13_la_CFLAGS = $(C_FVISIBILITY) $(GLIB_CFLAGS) $(AM_CFLAGS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)
