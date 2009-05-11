EXTRA_DIST = vars-pl.txt vars-en.txt

lib_LTLIBRARIES = autoresponder.la
autoresponder_la_SOURCES = autoresponder.c
autoresponder_la_LDFLAGS = -module -avoid-version
autoresponder_la_CPPFLAGS = $(C_FVISIBILITY)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/autoresponder/
	$(INSTALL) -m 644 ${EXTRA_DIST} $(DESTDIR)$(pkgdatadir)/plugins/autoresponder/
 
