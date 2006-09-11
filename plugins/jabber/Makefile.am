EXTRA_DIST = commands-pl.txt session-pl.txt commands-en.txt session-en.txt

lib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c commands.c misc.c digest.c xmlnode.c jabber_handlers.c jabber.h
jabber_la_LDFLAGS = -module -avoid-version
jabber_la_LIBADD = $(EXPAT_LIBS) $(LIBGNUTLS_LIBS)
jabber_la_CPPFLAGS = $(EXPAT_CPPFLAGS) $(LIBGNUTLS_CFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/jabber/
	$(INSTALL) -m 644 commands-pl.txt session-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/jabber/
	$(INSTALL) -m 644 commands-en.txt session-en.txt $(DESTDIR)$(pkgdatadir)/plugins/jabber/

