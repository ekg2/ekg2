DOCS = vars-pl.txt commands-pl.txt
EXTRA_DIST = README $(DOCS) events.txt mouse.txt sim.txt themes.txt ekg2book ekg2book-en

install-data-am:
	$(INSTALL) -m 644 $(DOCS) $(DESTDIR)$(pkgdatadir)/
	which iconv >/dev/null 2>&1 && for FILE in $(DOCS); do iconv -f iso-8859-2 -t utf-8 $$FILE > $(DESTDIR)$(pkgdatadir)/`basename $$FILE .txt`-utf.txt; done || true


