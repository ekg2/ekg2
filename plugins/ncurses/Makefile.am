EXTRA_DIST = vars-pl.txt vars-en.txt commands-pl.txt

lib_LTLIBRARIES = ncurses.la
ncurses_la_SOURCES = main.c old.c completion.c bindings.c contacts.c mouse.c\
	bindings.h completion.h contacts.h ecurses.h old.h mouse.h
ncurses_la_LDFLAGS = -module -avoid-version
ncurses_la_LIBADD = $(NCURSES_LIBS) $(ASPELL_LIBS) $(LIBGPM_LIBS)
ncurses_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)
ncurses_la_CPPFLAGS = $(NCURSES_CPPFLAGS) $(AM_CPPFLAGS)

INCLUDES = -DDATADIR=\"$(datadir)/ekg\" -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/ncurses/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/ncurses/
