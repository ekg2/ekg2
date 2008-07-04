EXTRA_DIST = vars-pl.txt 

lib_LTLIBRARIES = mail.la
mail_la_SOURCES = main.c
mail_la_LDFLAGS = -module -avoid-version
mail_la_CPPFLAGS = $(C_FVISIBILITY)

override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/mail/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/mail/
	which iconv >/dev/null 2>&1 && for FILE in $(EXTRA_DIST); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/plugins/mail/`basename $$FILE .txt`-utf.txt; done || true
