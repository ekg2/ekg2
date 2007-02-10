EXTRA_DIST = commands-pl.txt session-pl.txt commands-en.txt session-en.txt

lib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c commands.c misc.c digest.c xmlnode.c jabber_dcc.c jabber_handlers.c jabber.h jabber_dcc.h jabber-ssl.h
jabber_la_LDFLAGS = -module -avoid-version
jabber_la_LIBADD = $(EXPAT_LIBS) $(JABBER_SSL_LIBS) $(ZLIB_LIBS)
jabber_la_CPPFLAGS = $(EXPAT_CPPFLAGS) $(JABBER_SSL_CPPFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/jabber/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/jabber/

