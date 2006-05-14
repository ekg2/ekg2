EXTRA_DIST = vars-pl.txt vars-pl-utf.txt

lib_LTLIBRARIES = logs.la
logs_la_SOURCES = main.c main.h
logs_la_LDFLAGS = -module -avoid-version
logs_la_LIBADD = 

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/logs/
	$(INSTALL) -m 644 vars-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/logs/vars-pl.txt
	$(INSTALL) -m 644 vars-pl-utf.txt $(DESTDIR)$(pkgdatadir)/plugins/logs/vars-pl-utf.txt
