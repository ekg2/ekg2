EXTRA_DIST = vars-pl.txt

lib_LTLIBRARIES = rc.la
rc_la_SOURCES = main.c inputs.c rc.h
rc_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)
rc_la_LDFLAGS = -module -avoid-version

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/rc/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/rc/
