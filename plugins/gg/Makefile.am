EXTRA_DIST = vars-pl.txt commands-pl.txt session-pl.txt commands-en.txt vars-en.txt session-en.txt

lib_LTLIBRARIES = gg.la
gg_la_SOURCES = commands.c dcc.c gg.c images.c misc.c pubdir.c pubdir50.c \
	dcc.h gg.h misc.h images.h pubdir50.h pubdir.h token.h
gg_la_LDFLAGS = -module -avoid-version
gg_la_LIBADD = $(LIBGADU_LIBS) $(LIBJPEG_LIBS) $(GG_TOKENGIF_LIBS)
gg_la_CPPFLAGS = $(LIBGADU_CPPFLAGS) $(C_FVISIBILITY)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/gg/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/gg/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/gg/`basename $$FILE .txt`-utf.txt; done || true

