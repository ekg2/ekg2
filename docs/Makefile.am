DOCS = vars-pl.txt vars-en.txt commands-pl.txt session-pl.txt session-en.txt
EXTRA_DIST = README $(DOCS) events.txt mouse.txt sim.txt voip.txt \
	themes.txt themes-en.txt ui-ncurses.txt ui-ncurses-en.txt \
	ekg2book ekg2book-en Perl-API-pl.txt Perl-API-en.txt

install-data-am:
	$(INSTALL) -m 644 $(DOCS) $(DESTDIR)$(pkgdatadir)/
