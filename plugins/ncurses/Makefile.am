lib_LTLIBRARIES = ncurses.la
ncurses_la_SOURCES = main.c old.c completion.c bindings.c contacts.c mouse.c\
	bindings.h completion.h contacts.h ecurses.h old.h mouse.h
ncurses_la_LDFLAGS = -module -avoid-version
ncurses_la_LIBADD = $(NCURSES_LIBS)
ncurses_la_CPPFLAGS = $(NCURSES_CPPFLAGS) $(LTDLINCL)

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg\" 

