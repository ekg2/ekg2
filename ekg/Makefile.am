bin_PROGRAMS = ekg2
bin_SCRIPTS = ekg2-config
ekg2_SOURCES = audio.c commands.c configfile.c dynstuff.c ekg.c emoticons.c events.c log.c metacontacts.c \
       msgqueue.c plugins.c protocol.c sessions.c stuff.c themes.c userlist.c vars.c windows.c xmalloc.c \
       audio.h commands.h configfile.h debug.h dynstuff.h emoticons.h events.h\
       log.h metacontacts.h msgqueue.h objects.h plugins.h protocol.h scripts.c scripts.h sessions.h stuff.h \
       themes.h userlist.h vars.h windows.h xmalloc.h

ekg2_LDADD = $(top_builddir)/compat/libcompat.a @LIBS@
ekg2_LDFLAGS = -export-dynamic
ekg2_DEPENDENCIES = 

localedir = $(datadir)/locale

INCLUDES = -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" \
       -DPLUGINDIR=\"$(plugindir)\" $(LTDLINCL)
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
