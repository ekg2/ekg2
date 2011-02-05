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

dist_pkgdata_DATA = \
	docs/commands-pl.txt \
	docs/session-en.txt \
	docs/session-pl.txt \
	docs/vars-en.txt \
	docs/vars-pl.txt

dist_noinst_DATA = \
	docs/events.txt \
	docs/mouse.txt \
	docs/Perl-API-en.txt \
	docs/Perl-API-pl.txt \
	docs/README \
	docs/sim.txt \
	docs/themes-en.txt \
	docs/themes.txt \
	docs/ui-ncurses-en.txt \
	docs/ui-ncurses.txt \
	docs/voip.txt

themedir = $(pkgdatadir)/themes

dist_theme_DATA = \
	contrib/themes/blue_sea_dragon.theme \
	contrib/themes/dmilith.theme \
	contrib/themes/irc-irssi.theme \
	contrib/themes/peres.theme \
	contrib/themes/wiechu.theme

plugin_LTLIBRARIES =

if ENABLE_AUTORESPONDER
plugin_LTLIBRARIES += plugins/autoresponder/autoresponder.la

plugins_autoresponder_autoresponder_la_SOURCES = \
	ekg2-config.h \
	plugins/autoresponder/autoresponder.c

dist_autoresponder_DATA = \
	plugins/autoresponder/vars-en.txt \
	plugins/autoresponder/vars-pl.txt
endif

if ENABLE_FEED
plugin_LTLIBRARIES += plugins/feed/feed.la

plugins_feed_feed_la_SOURCES = \
	ekg2-config.h \
	plugins/feed/feed.c \
	plugins/feed/feed.h \
	plugins/feed/nntp.c \
	plugins/feed/rss.c
endif

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

dist_gg_DATA = \
	plugins/gg/commands-en.txt \
	plugins/gg/commands-pl.txt \
	plugins/gg/session-en.txt \
	plugins/gg/session-pl.txt \
	plugins/gg/vars-en.txt \
	plugins/gg/vars-pl.txt
endif

if ENABLE_GPG
plugin_LTLIBRARIES += plugins/gpg/gpg.la

plugins_gpg_gpg_la_SOURCES = \
	ekg2-config.h \
	plugins/gpg/gpg.c

dist_gpg_DATA = \
	plugins/gpg/commands-en.txt \
	plugins/gpg/commands-pl.txt
endif

if ENABLE_ICQ
plugin_LTLIBRARIES += plugins/icq/icq.la

plugins_icq_icq_la_SOURCES = \
	ekg2-config.h \
	plugins/icq/digest.c \
	plugins/icq/icq.c \
	plugins/icq/icq_caps.c \
	plugins/icq/icq_caps.h \
	plugins/icq/icq_const.h \
	plugins/icq/icq_debug.inc \
	plugins/icq/icq_fieldnames.inc \
	plugins/icq/icq_flap_handlers.c \
	plugins/icq/icq_flap_handlers.h \
	plugins/icq/icq.h \
	plugins/icq/icq_snac_handlers_01service.c \
	plugins/icq/icq_snac_handlers_02location.c \
	plugins/icq/icq_snac_handlers_03buddy.c \
	plugins/icq/icq_snac_handlers_04message.c \
	plugins/icq/icq_snac_handlers_09bos.c \
	plugins/icq/icq_snac_handlers_0Alookup.c \
	plugins/icq/icq_snac_handlers_0Bstatus.c \
	plugins/icq/icq_snac_handlers_13userlist.c \
	plugins/icq/icq_snac_handlers_15extension.c \
	plugins/icq/icq_snac_handlers_17sigon.c \
	plugins/icq/icq_snac_handlers.c \
	plugins/icq/icq_snac_handlers.h \
	plugins/icq/misc.c \
	plugins/icq/misc.h \
	plugins/icq/miscicq.h
endif

if ENABLE_IRC
plugin_LTLIBRARIES += plugins/irc/irc.la

plugins_irc_irc_la_SOURCES = \
	ekg2-config.h \
	plugins/irc/autoacts.c \
	plugins/irc/autoacts.h \
	plugins/irc/input.c \
	plugins/irc/input.h \
	plugins/irc/irc.c \
	plugins/irc/irc.h \
	plugins/irc/irc-ssl.h \
	plugins/irc/IRCVERSION.h \
	plugins/irc/misc.c \
	plugins/irc/misc.h \
	plugins/irc/people.c \
	plugins/irc/people.h

dist_irc_DATA = \
	plugins/irc/commands-pl.txt \
	plugins/irc/session-pl.txt
endif

if ENABLE_JABBER
plugin_LTLIBRARIES += plugins/jabber/jabber.la

plugins_jabber_jabber_la_SOURCES = \
	ekg2-config.h \
	plugins/jabber/commands.c \
	plugins/jabber/digest.c \
	plugins/jabber/jabber.c \
	plugins/jabber/jabber_dcc.c \
	plugins/jabber/jabber_dcc.h \
	plugins/jabber/jabber.h \
	plugins/jabber/jabber_handlers.c \
	plugins/jabber/jabber-ssl.h \
	plugins/jabber/misc.c \
	plugins/jabber/xmlnode.c \
	plugins/jabber/jabber_handlers_iq_error.inc \
	plugins/jabber/jabber_handlers_iq_get.inc \
	plugins/jabber/jabber_handlers_iq_result.inc \
	plugins/jabber/jabber_handlers_tlen.inc

dist_jabber_DATA = \
	plugins/jabber/commands-en.txt \
	plugins/jabber/commands-pl.txt \
	plugins/jabber/session-en.txt \
	plugins/jabber/session-pl.txt
endif

if ENABLE_JOGGER
plugin_LTLIBRARIES += plugins/jogger/jogger.la

plugins_jogger_jogger_la_SOURCES = \
	ekg2-config.h \
	plugins/jogger/drafts.c \
	plugins/jogger/jogger.c \
	plugins/jogger/messages.c
endif

if ENABLE_LOGS
plugin_LTLIBRARIES += plugins/logs/logs.la

plugins_logs_logs_la_SOURCES = \
	ekg2-config.h \
	plugins/logs/main.c \
	plugins/logs/main.h

dist_logs_DATA = \
	plugins/logs/vars-pl.txt
endif

if ENABLE_LOGSQLITE
plugin_LTLIBRARIES += plugins/logsqlite/logsqlite.la

plugins_logsqlite_logsqlite_la_SOURCES = \
	ekg2-config.h \
	plugins/logsqlite/logsqlite.c \
	plugins/logsqlite/logsqlite.h

dist_logsqlite_DATA = \
	plugins/logsqlite/commands-en.txt \
	plugins/logsqlite/commands-pl.txt \
	plugins/logsqlite/vars-en.txt \
	plugins/logsqlite/vars-pl.txt
endif

if ENABLE_MAIL
plugin_LTLIBRARIES += plugins/mail/mail.la

plugins_mail_mail_la_SOURCES = \
	ekg2-config.h \
	plugins/mail/main.c

dist_mail_DATA = \
	plugins/mail/vars-pl.txt
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

dist_ncurses_DATA = \
	plugins/ncurses/commands-pl.txt \
	plugins/ncurses/vars-en.txt \
	plugins/ncurses/vars-pl.txt
endif
