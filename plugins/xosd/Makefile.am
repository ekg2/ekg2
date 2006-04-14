EXTRA_DIST = vars-pl.txt commands-pl.txt 

lib_LTLIBRARIES = xosd.la
xosd_la_SOURCES = xosd.c
xosd_la_LDFLAGS = -module -avoid-version
xosd_la_LIBADD = $(LIBXOSD_LIBS) -lxosd
xosd_la_CPPFLAGS = $(LIBXOSD_CFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/xosd/
	$(INSTALL) -m 644 commands-pl.txt vars-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/xosd/
