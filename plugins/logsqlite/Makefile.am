EXTRA_DIST = vars-pl.txt vars-en.txt commands-pl.txt commands-en.txt

lib_LTLIBRARIES = logsqlite.la
logsqlite_la_SOURCES = logsqlite.c logsqlite.h
logsqlite_la_LDFLAGS = -module -avoid-version
logsqlite_la_LIBADD = $(SQLITE_LIBS)
logsqlite_la_CFLAGS = $(GLIB_CFLAGS) $(SQLITE_CFLAGS) $(AM_CFLAGS)
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
