EXTRA_DIST = vars.txt commands.txt

lib_LTLIBRARIES = sms.la
sms_la_SOURCES = sms.c
sms_la_LDFLAGS = -module -avoid-version
sms_la_CPPFLAGS = $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	$(INSTALL) -D -m 644 vars.txt $(DESTDIR)$(datadir)/ekg/plugins/sms/vars.txt
	$(INSTALL) -D -m 644 commands.txt $(DESTDIR)$(datadir)/ekg/plugins/sms/commands.txt

