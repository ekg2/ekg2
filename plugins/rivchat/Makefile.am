lib_LTLIBRARIES = rivchat.la
rivchat_la_SOURCES = rivchat.c rivchat.h misc.c
rivchat_la_LDFLAGS = -module -avoid-version
rivchat_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)
rivchat_la_LIBADD = $(STRL_LIBS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)
