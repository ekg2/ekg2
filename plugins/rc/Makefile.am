EXTRA_DIST = vars.txt

lib_LTLIBRARIES = rc.la
rc_la_SOURCES = main.c inputs.c rc.h
rc_la_LDFLAGS = -module -avoid-version
rc_la_CPPFLAGS = $(LTDLINCL)

libdir = $(plugindir)

install-data-am:
	$(INSTALL) -D -m 644 vars.txt $(DESTDIR)$(datadir)/ekg/plugins/rc/vars.txt

