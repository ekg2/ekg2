EXTRA_DIST = vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = sms.la
sms_la_SOURCES = sms.c
sms_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sms/
	$(INSTALL) -m 644 commands-pl.txt vars-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/

