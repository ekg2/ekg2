EXTRA_DIST = vars.txt commands.txt vars-pl.txt commands-pl.txt

lib_LTLIBRARIES = sim.la
sim_la_SOURCES = main.c simlite.c simlite.h
sim_la_LDFLAGS = -module -avoid-version
sim_la_LIBADD = $(OPENSSL_LIBS)
sim_la_CPPFLAGS = $(OPENSSL_INCLUDES) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/sim/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/sim/vars.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/sim/commands.txt
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(pkgdatadir)/plugins/sim/vars-pl.txt
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(pkgdatadir)/plugins/sim/commands-pl.txt

