EXTRA_DIST = commands.txt

lib_LTLIBRARIES = jabber.la
jabber_la_SOURCES = jabber.c commands.c misc.c digest.c xmlnode.c jabber.h
jabber_la_LDFLAGS = -module -avoid-version
jabber_la_LIBADD = $(EXPAT_LIBS) $(LIBGNUTLS_LIBS)
jabber_la_CPPFLAGS = $(EXPAT_CPPFLAGS) $(LIBGNUTLS_CFLAGS) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(datadir)/ekg/plugins/jabber/
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(datadir)/ekg/plugins/jabber/commands.txt

