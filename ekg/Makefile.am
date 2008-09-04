ekg2_headers = audio.h audio_wav.h configfile.h dynstuff.h dynstuff_inline.h events.h metacontacts.h queries.h objects.h protocol.h recode.h sessions.h strings.h themes.h vars.h xmalloc.h \
		commands.h debug.h emoticons.h log.h msgqueue.h net.h srv.h plugins.h scripts.h stuff.h userlist.h windows.h win32.h ../gettext.h ../ekg2-config.h

bin_PROGRAMS = ekg2
ekg2_SOURCES = audio.c commands.c configfile.c dynstuff.c ekg.c emoticons.c events.c legacyconfig.c log.c metacontacts.c \
       msgqueue.c plugins.c protocol.c recode.c sessions.c stuff.c themes.c userlist.c vars.c queries.h windows.c xmalloc.c net.c srv.c scripts.c strings.c win32.c $(ekg2_headers)

ekg2includedir = $(includedir)/ekg2

ekg2_LDADD = $(top_builddir)/compat/libcompat.a @LIBS@ @STATIC_PLUGINS_LIB@ @DLLIB@ @LTLIBICONV@ @IDN_LIBS@
ekg2_LDFLAGS = -export-dynamic
ekg2_DEPENDENCIES = $(top_builddir)/compat/libcompat.a

localedir = $(datadir)/locale

INCLUDES = -DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" \
       -DPLUGINDIR=\"$(plugindir)\" -DLOCALEDIR=\"$(localedir)\"

$(top_builddir)/compat/libcompat.a:
	make -C $(top_builddir)/compat
