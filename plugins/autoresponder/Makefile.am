EXTRA_DIST = vars-pl.txt vars-en.txt

lib_LTLIBRARIES = autoresponder.la
autoresponder_la_SOURCES = autoresponder.c
autoresponder_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/autoresponder/
	$(INSTALL) -m 644 ${EXTRA_DIST} $(DESTDIR)$(pkgdatadir)/plugins/autoresponder/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/gg/`basename $$FILE .txt`-utf.txt; done || true
 
