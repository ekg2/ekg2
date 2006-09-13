EXTRA_DIST = vars-pl.txt vars-en.txt

lib_LTLIBRARIES = ncurses.la
ncurses_la_SOURCES = main.c old.c completion.c bindings.c ecurses.c contacts.c mouse.c\
	bindings.h completion.h contacts.h ecurses.h old.h mouse.h
ncurses_la_LDFLAGS = -module -avoid-version
ncurses_la_LIBADD = $(NCURSES_LIBS) $(ASPELL_LIBS) $(LIBGPM_LIBS)
ncurses_la_CPPFLAGS = $(NCURSES_CPPFLAGS) 

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg\" 

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/ncurses/
	$(INSTALL) -m 644 vars-pl.txt $(DESTDIR)$(pkgdatadir)/plugins/ncurses/
	$(INSTALL) -m 644 vars-en.txt $(DESTDIR)$(pkgdatadir)/plugins/ncurses/

