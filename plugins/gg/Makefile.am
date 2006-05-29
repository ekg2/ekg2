EXTRA_DIST = vars-pl.txt commands-pl.txt session-pl.txt commands-en.txt vars-en.txt session-en.txt vars-pl-utf.txt commands-pl-utf.txt session-pl-utf.txt

lib_LTLIBRARIES = gg.la
gg_la_SOURCES = commands.c dcc.c gg.c images.c misc.c pubdir.c pubdir50.c \
	dcc.h gg.h misc.h images.h pubdir50.h pubdir.h token.h
gg_la_LDFLAGS = -module -avoid-version
gg_la_LIBADD = $(LIBGADU_LIBS) $(LIBJPEG_LIBS) $(LIBUNGIF_LIBS)
gg_la_CPPFLAGS = $(LIBGADU_CPPFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/gg/
	$(INSTALL) -m 644 vars-pl.txt vars-en.txt commands-pl.txt commands-en.txt session-pl.txt session-en.txt  vars-pl-utf.txt commands-pl-utf.txt session-pl-utf.txt $(DESTDIR)$(pkgdatadir)/plugins/gg/

