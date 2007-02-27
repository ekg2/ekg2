EXTRA_DIST = vars-pl.txt commands-pl.txt 

lib_LTLIBRARIES = xosd.la
xosd_la_SOURCES = xosd.c
xosd_la_LDFLAGS = -module -avoid-version
xosd_la_LIBADD = $(LIBXOSD_LIBS) -lxosd
xosd_la_CPPFLAGS = $(LIBXOSD_CFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/xosd/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/xosd/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/xosd/`basename $$FILE .txt`-utf.txt; done || true

