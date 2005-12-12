EXTRA_DIST = vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = logsqlite.la
logsqlite_la_SOURCES = logsqlite.c logsqlite.h
logsqlite_la_LDFLAGS = -module -avoid-version
logsqlite_la_LIBADD = $(SQLITE_LIBS)
logsqlite_la_CFLAGS = $(SQLITE_CFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
	$(INSTALL) -m 644 vars-pl.txt commands-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
