EXTRA_DIST = vars.txt commands.txt

lib_LTLIBRARIES = gg.la
gg_la_SOURCES = commands.c dcc.c gg.c misc.c pubdir.c pubdir50.c \
	dcc.h gg.h misc.h pubdir50.h pubdir.h token.h
gg_la_LDFLAGS = -module -avoid-version
gg_la_LIBADD = $(LIBGADU_LIBS)
gg_la_CPPFLAGS = $(LIBGADU_CPPFLAGS) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	$(INSTALL) -D -m 644 vars.txt $(DESTDIR)$(datadir)/ekg/plugins/gg/vars.txt
	$(INSTALL) -D -m 644 commands.txt $(DESTDIR)$(datadir)/ekg/plugins/gg/commands.txt

