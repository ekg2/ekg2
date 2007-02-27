EXTRA_DIST = vars-pl.txt

lib_LTLIBRARIES = rc.la
rc_la_SOURCES = main.c inputs.c rc.h
rc_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/rc/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/rc/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/rc/`basename $$FILE .txt`-utf.txt; done || true

