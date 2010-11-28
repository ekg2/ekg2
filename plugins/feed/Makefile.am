lib_LTLIBRARIES = feed.la
feed_la_SOURCES = feed.c feed.h rss.c nntp.c
feed_la_LDFLAGS = -module -avoid-version
feed_la_LIBADD = $(EXPAT_LIBS)
feed_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)
feed_la_CPPFLAGS = $(EXPAT_CPPFLAGS) $(AM_CPPFLAGS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)
