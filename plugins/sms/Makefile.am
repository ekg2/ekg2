EXTRA_DIST = vars.txt commands.txt vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = sms.la
sms_la_SOURCES = sms.c
sms_la_LDFLAGS = -module -avoid-version
sms_la_CPPFLAGS = $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sms/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/vars.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/commands.txt
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/vars-pl.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/sms/commands-pl.txt

