EXTRA_DIST = vars-pl.txt 

lib_LTLIBRARIES = logs.la
logs_la_SOURCES = main.c main.h
logs_la_LDFLAGS = -module -avoid-version
logs_la_CFLAGS = $(C_FVISIBILITY) $(GLIB_CFLAGS) $(AM_CFLAGS)
logs_la_LIBADD = $(GLIB_LIBS) $(ZLIB_LIBS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logs/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/logs/
