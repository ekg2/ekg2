EXTRA_DIST = vars.txt commands.txt

lib_LTLIBRARIES = logsqlite.la
logsqlite_la_SOURCES = logsqlite.c logsqlite.h
logsqlite_la_LDFLAGS = -module -avoid-version
logsqlite_la_LIBADD = $(SQLITE_LIBS)
logsqlite_la_CFLAGS = $(SQLITE_CFLAGS) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/vars.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/commands.txt
