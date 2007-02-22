EXTRA_DIST = vars-pl.txt commands-pl.txt commands-en.txt vars-en.txt

lib_LTLIBRARIES = logsoracle.la
logsoracle_la_SOURCES = main.c main.h oralog.c oralog.h commands.c commands.h
logsoracle_la_LDFLAGS = -module -avoid-version
logsoracle_la_LIBADD  = -lclntsh -lpthread 
logsoracle_la_CPPFLAGS =  

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logsoracle/
	$(INSTALL) -m 644 vars-pl.txt commands-pl.txt commands-en.txt vars-en.txt $(DESTDIR)$(pkgdatadir)/plugins/logsoracle/
