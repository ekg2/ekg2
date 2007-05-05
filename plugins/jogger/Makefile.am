EXTRA_DIST =

lib_LTLIBRARIES = jogger.la
jogger_la_SOURCES = jogger.c messages.c drafts.c
jogger_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

#install-data-am:
#	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/jogger/
#	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/jogger/
#	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/jogger/`basename $$FILE .txt`-utf.txt; done || true

