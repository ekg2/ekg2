EXTRA_DIST = vars.txt commands.txt session.txt commands-en.txt vars-en.txt

lib_LTLIBRARIES = gg.la
gg_la_SOURCES = commands.c dcc.c gg.c images.c misc.c pubdir.c pubdir50.c \
	dcc.h gg.h misc.h images.h pubdir50.h pubdir.h token.h
gg_la_LDFLAGS = -module -avoid-version
gg_la_LIBADD = $(LIBGADU_LIBS)
gg_la_CPPFLAGS = $(LIBGADU_CPPFLAGS) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/gg/
	$(INSTALL) -m 644 vars.txt vars-en.txt commands.txt commands-en.txt session.txt $(DESTDIR)$(pkgdatadir)/plugins/gg/

