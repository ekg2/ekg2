ekg2_headers = audio.h audio_wav.h configfile.h dynstuff.h events.h metacontacts.h queries.h objects.h protocol.h sessions.h themes.h vars.h xmalloc.h \
		commands.h debug.h emoticons.h log.h msgqueue.h plugins.h scripts.h stuff.h userlist.h windows.h win32.h ../gettext.h ../ekg2-config.h

bin_PROGRAMS = ekg2
bin_SCRIPTS = ekg2-config
ekg2_SOURCES = audio.c commands.c configfile.c dynstuff.c ekg.c emoticons.c events.c legacyconfig.c log.c metacontacts.c \
       msgqueue.c plugins.c protocol.c sessions.c stuff.c themes.c userlist.c vars.c queries.h windows.c xmalloc.c scripts.c win32.c $(ekg2_headers)

ekg2includedir = $(includedir)/ekg2
ekg2include_HEADERS = $(ekg2_headers) 

ekg2_LDADD = $(top_builddir)/compat/libcompat.a @LIBS@ @STATIC_PLUGINS_LIB@ @DLLIB@ 
ekg2_LDFLAGS = -export-dynamic
ekg2_DEPENDENCIES = 

localedir = $(datadir)/locale

INCLUDES = -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" \
       -DPLUGINDIR=\"$(plugindir)\" -DLOCALEDIR=\"$(localedir)\"
