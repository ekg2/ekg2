EXTRA_DIST = vars.txt commands.txt

lib_LTLIBRARIES = sms.la
sms_la_SOURCES = sms.c
sms_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sms/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/vars.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/commands.txt

