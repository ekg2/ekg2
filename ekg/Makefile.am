bin_PROGRAMS = ekg
ekg_SOURCES = audio.c commands.c configfile.c dynstuff.c ekg.c emoticons.c log.c msgqueue.c \
       plugins.c protocol.c sessions.c stuff.c themes.c userlist.c vars.c windows.c xmalloc.c \
       audio.h commands.h configfile.h debug.h dynstuff.h emoticons.h \
       log.h msgqueue.h objects.h plugins.h protocol.h sessions.h stuff.h \
       themes.h userlist.h vars.h windows.h xmalloc.h
ekg_LDADD = $(top_builddir)/compat/libcompat.a @LIBS@ @PLUGINS_LIBS@


INCLUDES = -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(datadir)/ekg\" \
       -DPLUGINDIR=\"$(plugindir)\"
