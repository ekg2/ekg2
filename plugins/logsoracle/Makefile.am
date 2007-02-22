EXTRA_DIST = vars-pl.txt commands-pl.txt commands-en.txt vars-en.txt

lib_LTLIBRARIES = logsoracle.la
logsoracle_la_SOURCES = main.c main.h oralog.c oralog.h commands.c commands.h
logsoracle_la_LDFLAGS = -module -avoid-version
logsoracle_la_LIBADD  = $(LOGS_ORACLE_LIBS) -lclntsh -lpthread 
logsoracle_la_CPPFLAGS =  $(LOGS_ORACLE_CFLAGS)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsoracle/
	$(INSTALL) -m 644 vars-pl.txt commands-pl.txt commands-en.txt vars-en.txt $(DESTDIR)$(pkgdatadir)/plugins/logsoracle/
