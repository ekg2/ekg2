EXTRA_DIST = vars.txt

lib_LTLIBRARIES = ncurses.la
ncurses_la_SOURCES = main.c old.c completion.c bindings.c contacts.c mouse.c\
	bindings.h completion.h contacts.h ecurses.h old.h mouse.h
ncurses_la_LDFLAGS = -module -avoid-version
ncurses_la_LIBADD = $(NCURSES_LIBS)
ncurses_la_CPPFLAGS = $(NCURSES_CPPFLAGS) $(LTDLINCL)

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg\" 

install-data-am:
	mkdir -p $(DESTDIR)$(datadir)/ekg/plugins/ncurses/
	$(INSTALL) -m 644 vars.txt $(DESTDIR)$(datadir)/ekg/plugins/ncurses/vars.txt

