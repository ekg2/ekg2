EXTRA_DIST = vars.txt vars-pl.txt

lib_LTLIBRARIES = rc.la
rc_la_SOURCES = main.c inputs.c rc.h
rc_la_LDFLAGS = -module -avoid-version
rc_la_CPPFLAGS = $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/rc/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/rc/vars.txt
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/rc/vars-pl.txt

