EXTRA_DIST = vars.txt commands.txt

lib_LTLIBRARIES = sim.la
sim_la_SOURCES = main.c simlite.c simlite.h
sim_la_LDFLAGS = -module -avoid-version
sim_la_LIBADD = $(OPENSSL_LIBS)
sim_la_CPPFLAGS = $(OPENSSL_INCLUDES) $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	$(INSTALL) -D -m 644 vars.txt $(DESTDIR)$(datadir)/ekg/plugins/sim/vars.txt
	$(INSTALL) -D -m 644 commands.txt $(DESTDIR)$(datadir)/ekg/plugins/sim/commands.txt

