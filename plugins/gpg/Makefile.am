EXTRA_DIST = commands-pl.txt commands-en.txt

lib_LTLIBRARIES = gpg.la
gpg_la_SOURCES = gpg.c
gpg_la_LDFLAGS = -module -avoid-version

gpg_la_LIBADD = $(GPGME_LIBS)
gpg_la_CFLAGS = $(GPGME_CFLAGS) $(GLIB_CFLAGS) $(AM_CFLAGS)
# TODO: add $(C_FVISIBILITY) or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/gpg/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/gpg/
