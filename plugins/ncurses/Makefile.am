lib_LTLIBRARIES = ncurses.la
ncurses_la_SOURCES = main.c old.c completion.c bindings.c contacts.c \
	bindings.h completion.h contacts.h ecurses.h old.h
ncurses_la_LDFLAGS = -module -avoid-version
ncurses_la_LIBADD = @NCURSES_LIBS@
ncurses_la_INCLUDES = @NCURSES_CFLAGS@

libdir = $(plugindir)

