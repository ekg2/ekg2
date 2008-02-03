SUBDIRS = $(CONTRIB_SUBDIRS)
DIST_SUBDIRS = python perl 
EXTRA_DIST = irc-irssi.theme blue_sea_dragon.theme dmilith.theme

themesdir = $(DESTDIR)$(pkgdatadir)/themes
install-data-am:
	mkdir -p $(themesdir)
	$(INSTALL) -m 644 *.theme $(themesdir)

