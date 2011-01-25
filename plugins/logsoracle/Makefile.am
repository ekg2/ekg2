EXTRA_DIST = vars-pl.txt commands-pl.txt commands-en.txt vars-en.txt

lib_LTLIBRARIES = logsoracle.la
logsoracle_la_SOURCES = main.c main.h oralog.c oralog.h commands.c commands.h
logsoracle_la_LDFLAGS = -module -avoid-version
logsoracle_la_LIBADD  = $(LOGS_ORACLE_LIBS) -lclntsh -lpthread 
logsoracle_la_CFLAGS = $(GLIB_CFLAGS) $(LOGS_ORACLE_CFLAGS) $(AM_CFLAGS)
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsoracle/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/logsoracle/
