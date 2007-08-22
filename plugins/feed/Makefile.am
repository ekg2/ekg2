lib_LTLIBRARIES = feed.la
feed_la_SOURCES = feed.c feed.h rss.c nntp.c
feed_la_LDFLAGS = -module -avoid-version
feed_la_LIBADD = $(EXPAT_LIBS)
feed_la_CPPFLAGS = $(C_FVISIBILITY) $(EXPAT_CPPFLAGS)

libdir = $(plugindir)

