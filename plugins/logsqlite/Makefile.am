EXTRA_DIST = vars-pl.txt vars-en.txt commands-pl.txt commands-en.txt

lib_LTLIBRARIES = logsqlite.la
logsqlite_la_SOURCES = logsqlite.c logsqlite.h
logsqlite_la_LDFLAGS = -module -avoid-version
logsqlite_la_LIBADD = $(SQLITE_LIBS)
logsqlite_la_CFLAGS = $(SQLITE_CFLAGS)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/logsqlite/`basename $$FILE .txt`-utf.txt; done || true

