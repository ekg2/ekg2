EXTRA_DIST = vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = sms.la
sms_la_SOURCES = sms.c
sms_la_LDFLAGS = -module -avoid-version
sms_la_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sms/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/sms/
