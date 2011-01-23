EXTRA_DIST =

lib_LTLIBRARIES = jogger.la
jogger_la_SOURCES = jogger.c messages.c drafts.c
jogger_la_LDFLAGS = -module -avoid-version
jogger_la_CFLAGS = $(GLIB_CFLAGS) $(AM_CFLAGS)
jogger_la_LIBADD = $(GLIB_LIBS)
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

#install-data-am:
#	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/jogger/
#	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/jogger/
