lib_LTLIBRARIES = remote.la
remote_la_SOURCES = remote.c
remote_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)
remote_la_LDFLAGS = -module -avoid-version
remote_la_LIBADD = $(STRL_LIBS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)
