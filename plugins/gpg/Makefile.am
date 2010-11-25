EXTRA_DIST = commands-pl.txt commands-en.txt

lib_LTLIBRARIES = gpg.la
gpg_la_SOURCES = gpg.c
gpg_la_LDFLAGS = -module -avoid-version

gpg_la_LIBADD	= $(GPGME_LIBS)
gpg_la_CPPFLAGS = $(GPGME_CFLAGS)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/gpg/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/gpg/
