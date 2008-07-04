EXTRA_DIST = vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = sms.la
sms_la_SOURCES = sms.c
sms_la_LDFLAGS = -module -avoid-version

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sms/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/sms/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/sms/`basename $$FILE .txt`-utf.txt; done || true

