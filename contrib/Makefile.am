SUBDIRS = $(CONTRIB_SUBDIRS)
DIST_SUBDIRS = python perl 
EXTRA_DIST = themes/irc-irssi.theme themes/blue_sea_dragon.theme themes/dmilith.theme themes/peres.theme

themesdir = $(DESTDIR)$(pkgdatadir)/themes
install-data-am:
	mkdir -p $(themesdir)
	$(INSTALL) -m 644 themes/* $(themesdir)

