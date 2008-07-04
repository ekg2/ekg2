EXTRA_DIST = vars-pl.txt 

lib_LTLIBRARIES = logs.la
logs_la_SOURCES = main.c main.h
logs_la_LDFLAGS = -module -avoid-version
logs_la_CPPFLAGS = $(C_FVISIBILITY)
logs_la_LIBADD = $(ZLIB_LIBS)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logs/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/logs/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/logs/`basename $$FILE .txt`-utf.txt; done || true

