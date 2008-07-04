EXTRA_DIST = xajax_0.2.4.js ekg2.js ekg2.css

lib_LTLIBRARIES = httprc_xajax.la
httprc_xajax_la_SOURCES = httprc_xajax.c
httprc_xajax_la_LDFLAGS = -module -avoid-version

override libdir = $(plugindir)

INCLUDES = -DDATADIR=\"$(pkgdatadir)\"

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/httprc_xajax/
	$(INSTALL) -m 644 $(EXTRA_DIST)  $(DESTDIR)$(pkgdatadir)/plugins/httprc_xajax/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/httprc_xajax/`basename $$FILE .txt`-utf.txt; done || true

