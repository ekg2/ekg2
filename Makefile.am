SUBDIRS = po

ACLOCAL_AMFLAGS = -I m4

plugindir = $(libdir)/ekg2/plugins

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
ekg_ekg2_LDADD = @EKG_LIBS@

plugin_LTLIBRARIES =

if ENABLE_GG
plugin_LTLIBRARIES += plugins/gg/gg.la

plugins_gg_gg_la_SOURCES = \
	ekg2-config.h \
	plugins/gg/commands.c \
	plugins/gg/dcc.c \
	plugins/gg/dcc.h \
	plugins/gg/gg.c \
	plugins/gg/gg.h \
	plugins/gg/images.c \
	plugins/gg/images.h \
	plugins/gg/misc.c \
	plugins/gg/misc.h \
	plugins/gg/pubdir50.c \
	plugins/gg/pubdir50.h \
	plugins/gg/pubdir.c \
	plugins/gg/pubdir.h \
	plugins/gg/token.h

plugins_gg_gg_la_CPPFLAGS = @GG_CPPFLAGS@
plugins_gg_gg_la_LDFLAGS = -module -avoid-version @GG_LDFLAGS@
plugins_gg_gg_la_LIBADD = @GG_LIBS@
endif

if ENABLE_NCURSES
plugin_LTLIBRARIES += plugins/ncurses/ncurses.la

plugins_ncurses_ncurses_la_SOURCES = \
	ekg2-config.h \
	plugins/ncurses/backlog.c \
	plugins/ncurses/backlog.h \
	plugins/ncurses/bindings.c \
	plugins/ncurses/bindings.h \
	plugins/ncurses/contacts.c \
	plugins/ncurses/contacts.h \
	plugins/ncurses/ecurses.h \
	plugins/ncurses/input.c \
	plugins/ncurses/input.h \
	plugins/ncurses/lastlog.c \
	plugins/ncurses/lastlog.h \
	plugins/ncurses/main.c \
	plugins/ncurses/mouse.c \
	plugins/ncurses/mouse.h \
	plugins/ncurses/nc-stuff.c \
	plugins/ncurses/nc-stuff.h \
	plugins/ncurses/notify.c \
	plugins/ncurses/notify.h \
	plugins/ncurses/spell.c \
	plugins/ncurses/spell.h \
	plugins/ncurses/statusbar.c \
	plugins/ncurses/statusbar.h

plugins_ncurses_ncurses_la_CPPFLAGS = @NCURSES_CPPFLAGS@
plugins_ncurses_ncurses_la_LDFLAGS = -module -avoid-version @NCURSES_LDFLAGS@
plugins_ncurses_ncurses_la_LIBADD = @NCURSES_LIBS@
endif
