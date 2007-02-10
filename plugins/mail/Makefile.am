EXTRA_DIST = vars-pl.txt 

lib_LTLIBRARIES = mail.la
mail_la_SOURCES = main.c
mail_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/mail/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/mail/

