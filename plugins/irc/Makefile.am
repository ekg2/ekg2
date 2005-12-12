EXTRA_DIST = commands-pl.txt session-pl.txt

lib_LTLIBRARIES = irc.la
irc_la_SOURCES = irc.c misc.c people.c input.c misc.h irc.h people.h \
		input.h IRCVERSION.h autoacts.c autoacts.h
irc_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/irc/
	$(INSTALL) -m 644 commands-pl.txt session-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/irc/

