EXTRA_DIST = vars-pl.txt

lib_LTLIBRARIES = readline.la
readline_la_SOURCES = main.c ui-readline.c ui-readline.h
readline_la_LDFLAGS = -module -avoid-version
readline_la_LIBADD = $(READLINE_LIBS)
readline_la_CPPFLAGS = $(READLINE_CPPFLAGS)
readline_la_CFLAGS = $(READLINE_CFLAGS)

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg\" 

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/readline/
	$(INSTALL) -m 644 vars-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/readline/

