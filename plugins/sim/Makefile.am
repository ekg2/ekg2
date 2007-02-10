EXTRA_DIST = vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = sim.la
sim_la_SOURCES = main.c simlite.c simlite.h
sim_la_LDFLAGS = -module -avoid-version
sim_la_LIBADD = $(OPENSSL_LIBS)
sim_la_CPPFLAGS = $(OPENSSL_INCLUDES)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sim/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/sim/

