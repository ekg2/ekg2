EXTRA_DIST = vars.txt commands.txt session.txt

lib_LTLIBRARIES = gg.la
gg_la_SOURCES = commands.c dcc.c gg.c misc.c pubdir.c pubdir50.c \
	dcc.h gg.h misc.h pubdir50.h pubdir.h token.h
gg_la_LDFLAGS = -module -avoid-version
gg_la_LIBADD = $(LIBGADU_LIBS)
gg_la_CPPFLAGS = $(LIBGADU_CPPFLAGS) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/gg/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/gg/vars.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/gg/commands.txt
	$(INSTALL) -m 644 session.txt $(DESTDIR)$(pkgdatadir)/plugins/gg/session.txt

