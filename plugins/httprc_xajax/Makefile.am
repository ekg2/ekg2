EXTRA_DIST = xajax_0.2.4.js ekg2.js ekg2.css

lib_LTLIBRARIES = httprc_xajax.la
httprc_xajax_la_SOURCES = httprc_xajax.c
httprc_xajax_la_LDFLAGS = -module -avoid-version
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

override libdir = $(plugindir)

INCLUDES = -DDATADIR=\"$(pkgdatadir)\" -I$(top_srcdir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/httprc_xajax/
	$(INSTALL) -m 644 $(EXTRA_DIST)  $(DESTDIR)$(pkgdatadir)/plugins/httprc_xajax/
