bin_PROGRAMS = ekg
ekg_SOURCES = audio.c commands.c configfile.c dynstuff.c ekg.c emoticons.c log.c msgqueue.c plugins.c protocol.c sessions.c stuff.c themes.c userlist.c vars.c windows.c xmalloc.c 
ekg_LDADD = $(top_builddir)/compat/libcompat.la @LIBS@ @PLUGINS_LIBS@

INCLUDES = -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(datadir)/ekg\" -DPLUGINDIR=\"$(plugindir)\"

