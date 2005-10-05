EXTRA_DIST = vars.txt

lib_LTLIBRARIES = readline.la
readline_la_SOURCES = main.c ui-readline.c ui-readline.h
readline_la_LDFLAGS = -module -avoid-version
readline_la_LIBADD = $(READLINE_LIBS)
readline_la_CPPFLAGS = $(READLINE_CPPFLAGS) $(LTDLINCL)
readline_la_CFLAGS = $(READLINE_CFLAGS) $(LTDLINCL)

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg\" 

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/readline/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/readline/vars.txt

