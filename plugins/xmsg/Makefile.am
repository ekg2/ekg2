EXTRA_DIST = session-pl.txt session-en.txt vars-pl.txt vars-en.txt

lib_LTLIBRARIES = xmsg.la
xmsg_la_SOURCES = xmsg.c
xmsg_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/xmsg/
	$(INSTALL) -m 644 ${EXTRA_DIST} $(DESTDIR)$(pkgdatadir)/plugins/xmsg/

