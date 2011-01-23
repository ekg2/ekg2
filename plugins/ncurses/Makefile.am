EXTRA_DIST = vars-pl.txt vars-en.txt commands-pl.txt

lib_LTLIBRARIES = ncurses.la
ncurses_la_SOURCES = main.c nc-stuff.c backlog.c bindings.c contacts.c input.c lastlog.c mouse.c notify.c spell.c statusbar.c\
	backlog.h bindings.h contacts.h ecurses.h nc-stuff.h input.h lastlog.h mouse.h notify.h spell.h statusbar.h
ncurses_la_LDFLAGS = -module -avoid-version
ncurses_la_LIBADD = $(GLIB_LIBS) $(NCURSES_LIBS) $(ASPELL_LIBS) $(LIBGPM_LIBS)
ncurses_la_CFLAGS = $(C_FVISIBILITY) $(GLIB_CFLAGS) $(AM_CFLAGS)
ncurses_la_CPPFLAGS = $(NCURSES_CPPFLAGS) $(AM_CPPFLAGS)

INCLUDES = -DDATADIR=\"$(datadir)/ekg\" -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/ncurses/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/ncurses/
