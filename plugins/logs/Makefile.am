EXTRA_DIST = vars-pl.txt 

lib_LTLIBRARIES = logs.la
logs_la_SOURCES = main.c main.h
logs_la_LDFLAGS = -module -avoid-version
logs_la_LIBADD = $(ZLIB_LIBS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logs/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/logs/
