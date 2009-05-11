EXTRA_DIST = $(EXTRA_DIST_DOC) jabber_handlers_iq_error.inc jabber_handlers_iq_get.inc jabber_handlers_iq_result.inc jabber_handlers_tlen.inc

EXTRA_DIST_DOC = commands-pl.txt session-pl.txt commands-en.txt session-en.txt

lib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c commands.c misc.c digest.c xmlnode.c jabber_dcc.c jabber_handlers.c jabber.h jabber_dcc.h jabber-ssl.h
jabber_la_LDFLAGS = -module -avoid-version
jabber_la_LIBADD = $(EXPAT_LIBS) $(JABBER_SSL_LIBS) $(ZLIB_LIBS)
jabber_la_CPPFLAGS = $(C_FVISIBILITY) $(EXPAT_CPPFLAGS) $(JABBER_SSL_CPPFLAGS)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/jabber/
	$(INSTALL) -m 644 $(EXTRA_DIST_DOC) $(DESTDIR)$(pkgdatadir)/plugins/jabber/

