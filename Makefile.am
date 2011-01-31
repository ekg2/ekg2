SUBDIRS = po

bin_PROGRAMS = ekg/ekg2

ekg_ekg2_SOURCES = \
	ekg2-config.h \
	gettext.h \
	ekg/bindings.c \
	ekg/bindings.h \
	ekg/commands.c \
	ekg/commands.h \
	ekg/completion.c \
	ekg/completion.h \
	ekg/configfile.c \
	ekg/configfile.h \
	ekg/debug.h \
	ekg/dynstuff.c \
	ekg/dynstuff.h \
	ekg/dynstuff_inline.h \
	ekg/ekg.c \
	ekg/emoticons.c \
	ekg/emoticons.h \
	ekg/events.c \
	ekg/events.h \
	ekg/legacyconfig.c \
	ekg/log.c \
	ekg/log.h \
	ekg/metacontacts.c \
	ekg/metacontacts.h \
	ekg/msgqueue.c \
	ekg/msgqueue.h \
	ekg/net.c \
	ekg/net.h \
	ekg/objects.h \
	ekg/plugins.c \
	ekg/plugins.h \
	ekg/protocol.c \
	ekg/protocol.h \
	ekg/queries.c \
	ekg/queries.h \
	ekg/recode.c \
	ekg/recode.h \
	ekg/recode_tables.h \
	ekg/scripts.c \
	ekg/scripts.h \
	ekg/sessions.c \
	ekg/sessions.h \
	ekg/srv.c \
	ekg/srv.h \
	ekg/strings.c \
	ekg/strings.h \
	ekg/stuff.c \
	ekg/stuff.h \
	ekg/themes.c \
	ekg/themes.h \
	ekg/userlist.c \
	ekg/userlist.h \
	ekg/vars.c \
	ekg/vars.h \
	ekg/win32.c \
	ekg/win32.h \
	ekg/windows.c \
	ekg/windows.h \
	ekg/xmalloc.c \
	ekg/xmalloc.h

ekg_ekg2_CPPFLAGS = \
	-DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" \
	-DPLUGINDIR=\"$(plugindir)\" -DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir) $(AM_CPPFLAGS)
