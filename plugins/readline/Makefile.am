EXTRA_DIST = vars-pl.txt

lib_LTLIBRARIES = readline.la
readline_la_SOURCES = main.c ui-readline.c ui-readline.h readline-completion.c
readline_la_LDFLAGS = -module -avoid-version
readline_la_LIBADD = $(READLINE_LIBS)
readline_la_CPPFLAGS = $(C_FVISIBILITY) $(READLINE_CPPFLAGS)
readline_la_CFLAGS = $(C_FVISIBILITY) $(READLINE_CFLAGS)

override libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg\" 

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/readline/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/readline/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/readline/`basename $$FILE .txt`-utf.txt; done || true

