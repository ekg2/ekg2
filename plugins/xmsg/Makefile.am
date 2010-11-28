EXTRA_DIST = session-pl.txt session-en.txt vars-pl.txt vars-en.txt

lib_LTLIBRARIES = xmsg.la
xmsg_la_SOURCES = xmsg.c
xmsg_la_LDFLAGS = -module -avoid-version
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/xmsg/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/xmsg/
