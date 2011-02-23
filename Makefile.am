SUBDIRS = po

ACLOCAL_AMFLAGS = -I m4
# XXX: we should check for -Wall support?
AM_CFLAGS = -Wall

plugindir = $(libdir)/ekg2/plugins
scriptdir = $(pkgdatadir)/scripts
themedir = $(pkgdatadir)/themes

bin_PROGRAMS = ekg/ekg2

noinst_HEADERS = \
	ekg2.h \
	ekg2-config.h \
	ekg/bindings.h \
	ekg/commands.h \
	ekg/completion.h \
	ekg/configfile.h \
	ekg/debug.h \
	ekg/dynstuff.h \
	ekg/dynstuff_inline.h \
	ekg/emoticons.h \
	ekg/events.h \
	ekg/internal.h \
	ekg/log.h \
	ekg/metacontacts.h \
	ekg/msgqueue.h \
	ekg/net.h \
	ekg/objects.h \
	ekg/plugins.h \
	ekg/protocol.h \
	ekg/queries.h \
	ekg/recode.h \
	ekg/scripts.h \
	ekg/sessions.h \
	ekg/sources.h \
	ekg/srv.h \
	ekg/stuff.h \
	ekg/themes.h \
	ekg/userlist.h \
	ekg/vars.h \
	ekg/win32.h \
	ekg/windows.h \
	ekg/xmalloc.h

ekg_ekg2_SOURCES = \
	gettext.h \
	$(noinst_HEADERS) \
	ekg/bindings.c \
	ekg/commands.c \
	ekg/completion.c \
	ekg/configfile.c \
	ekg/dynstuff.c \
	ekg/ekg.c \
	ekg/emoticons.c \
	ekg/events.c \
	ekg/legacyconfig.c \
	ekg/log.c \
	ekg/metacontacts.c \
	ekg/msgqueue.c \
	ekg/net.c \
	ekg/plugins.c \
	ekg/protocol.c \
	ekg/queries.c \
	ekg/recode.c \
	ekg/scripts.c \
	ekg/sessions.c \
	ekg/sources.c \
	ekg/srv.c \
	ekg/stuff.c \
	ekg/themes.c \
	ekg/userlist.c \
	ekg/vars.c \
	ekg/win32.c \
	ekg/windows.c \
	ekg/xmalloc.c

ekg_ekg2_LDADD = $(EKG_LIBS)
ekg_ekg2_CPPFLAGS = $(AM_CPPFLAGS) $(EKG_CPPFLAGS)

if STATIC_LIBS
ekg_ekg2_LDFLAGS = -static
ekg_ekg2_LDADD += $(plugin_LTLIBRARIES)
endif

AM_CPPFLAGS = -I$(top_builddir)/plugins -I$(top_builddir) -I$(top_srcdir) \
	-DSYSCONFDIR=\"$(sysconfdir)\" -DDATADIR=\"$(pkgdatadir)\" \
	-DPLUGINDIR=\"$(plugindir)\" -DLOCALEDIR=\"$(localedir)\"

BUILT_SOURCES = ekg2.h.gch plugins/ekg2.h.gch
MOSTLYCLEANFILES = ekg2.h.gch plugins/ekg2.h.gch

ekg2.h.gch: $(noinst_HEADERS)
	$(AM_V_CC)$(COMPILE) $(EKG_CPPFLAGS) -o $@ $< || touch $@

plugins/ekg2.h.gch: $(noinst_HEADERS)
	$(MKDIR_P) plugins
	$(AM_V_CC)$(COMPILE) $(EKG_CPPFLAGS) -fPIC -DPIC -o $@ $< || touch $@

TESTS = check_potfiles
check_SCRIPTS = check_potfiles

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

dist_theme_DATA = \
	contrib/themes/blue_sea_dragon.theme \
	contrib/themes/dmilith.theme \
	contrib/themes/irc-irssi.theme \
	contrib/themes/peres.theme \
	contrib/themes/wiechu.theme

dist_script_DATA =
plugin_LTLIBRARIES =
EXTRA_DIST =

if ENABLE_AUTORESPONDER
plugin_LTLIBRARIES += plugins/autoresponder/autoresponder.la

plugins_autoresponder_autoresponder_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/autoresponder/autoresponder.c

dist_autoresponder_DATA = \
	plugins/autoresponder/vars-en.txt \
	plugins/autoresponder/vars-pl.txt
endif

if ENABLE_FEED
plugin_LTLIBRARIES += plugins/feed/feed.la

plugins_feed_feed_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/feed/feed.c \
	plugins/feed/feed.h \
	plugins/feed/nntp.c \
	plugins/feed/rss.c
endif

if ENABLE_GG
plugin_LTLIBRARIES += plugins/gg/gg.la

plugins_gg_gg_la_SOURCES = \
	$(noinst_HEADERS) \
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
	$(noinst_HEADERS) \
	plugins/gpg/gpg.c

dist_gpg_DATA = \
	plugins/gpg/commands-en.txt \
	plugins/gpg/commands-pl.txt
endif

if ENABLE_GTK
plugin_LTLIBRARIES += plugins/gtk/gtk.la

GTK_INLINE_PNGS = \
	inline_pngs.h \
	inline_pngs_gg.h \
	inline_pngs_icq.h

plugins_gtk_gtk_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/gtk/bindings.c \
	plugins/gtk/bindings.h \
	plugins/gtk/chanview.c \
	plugins/gtk/chanview.h \
	plugins/gtk/gtkutil.c \
	plugins/gtk/gtkutil.h \
	plugins/gtk/main.c \
	plugins/gtk/maingui.c \
	plugins/gtk/maingui.h \
	plugins/gtk/main.h \
	plugins/gtk/menu.c \
	plugins/gtk/menu.h \
	plugins/gtk/palette.c \
	plugins/gtk/palette.h \
	plugins/gtk/userlistgui.c \
	plugins/gtk/userlistgui.h \
	plugins/gtk/xtext.c \
	plugins/gtk/xtext.h \
	plugins/gtk/chanview-tabs.inc \
	plugins/gtk/chanview-tree.inc \
	$(GTK_INLINE_PNGS)

BUILT_SOURCES += \
	$(GTK_INLINE_PNGS)

MOSTLYCLEANFILES += \
	$(GTK_INLINE_PNGS)

BASE_PNGS = \
	$(top_srcdir)/plugins/gtk/iconssets/avail.png \
	$(top_srcdir)/plugins/gtk/iconssets/away.png \
	$(top_srcdir)/plugins/gtk/iconssets/dnd.png \
	$(top_srcdir)/plugins/gtk/iconssets/ffc.png \
	$(top_srcdir)/plugins/gtk/iconssets/icon_error.png \
	$(top_srcdir)/plugins/gtk/iconssets/icon_unknown.png \
	$(top_srcdir)/plugins/gtk/iconssets/invisible.png \
	$(top_srcdir)/plugins/gtk/iconssets/notavail.png \
	$(top_srcdir)/plugins/gtk/iconssets/xa.png

GG_PNGS = \
	$(top_srcdir)/plugins/gtk/iconssets/gg-avail.png \
	$(top_srcdir)/plugins/gtk/iconssets/gg-away.png \
	$(top_srcdir)/plugins/gtk/iconssets/gg-invisible.png \
	$(top_srcdir)/plugins/gtk/iconssets/gg-notavail.png

ICQ_PNGS = \
	$(top_srcdir)/plugins/gtk/iconssets/icq-avail.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-away.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-dnd.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-ffc.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-invisible.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-notavail.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-unknown.png \
	$(top_srcdir)/plugins/gtk/iconssets/icq-xa.png

inline_pngs.h: $(BASE_PNGS)
	$(AM_V_GEN)$(GDK_PIXBUF_CSOURCE) --raw --build-list \
		avail $(top_srcdir)/plugins/gtk/iconssets/avail.png \
		away $(top_srcdir)/plugins/gtk/iconssets/away.png \
		dnd $(top_srcdir)/plugins/gtk/iconssets/dnd.png \
		ffc $(top_srcdir)/plugins/gtk/iconssets/ffc.png \
		icon_error $(top_srcdir)/plugins/gtk/iconssets/icon_error.png \
		icon_unknown $(top_srcdir)/plugins/gtk/iconssets/icon_unknown.png \
		invisible $(top_srcdir)/plugins/gtk/iconssets/invisible.png \
		notavail $(top_srcdir)/plugins/gtk/iconssets/notavail.png \
		xa $(top_srcdir)/plugins/gtk/iconssets/xa.png \
		> $@

inline_pngs_gg.h: $(BASE_PNGS)
	$(AM_V_GEN)$(GDK_PIXBUF_CSOURCE) --raw --build-list \
		gg_avail $(top_srcdir)/plugins/gtk/iconssets/gg-avail.png \
		gg_away $(top_srcdir)/plugins/gtk/iconssets/gg-away.png \
		gg_invisible $(top_srcdir)/plugins/gtk/iconssets/gg-invisible.png \
		gg_notavail $(top_srcdir)/plugins/gtk/iconssets/gg-notavail.png \
		> $@

inline_pngs_icq.h: $(ICQ_PNGS)
	$(AM_V_GEN)$(GDK_PIXBUF_CSOURCE) --raw --build-list \
		icq_ffc $(top_srcdir)/plugins/gtk/iconssets/icq-ffc.png \
		icq_avail $(top_srcdir)/plugins/gtk/iconssets/icq-avail.png \
		icq_away $(top_srcdir)/plugins/gtk/iconssets/icq-away.png \
		icq_dnd $(top_srcdir)/plugins/gtk/iconssets/icq-dnd.png \
		icq_xa $(top_srcdir)/plugins/gtk/iconssets/icq-xa.png \
		icq_invisible $(top_srcdir)/plugins/gtk/iconssets/icq-invisible.png \
		icq_notavail $(top_srcdir)/plugins/gtk/iconssets/icq-notavail.png \
		icq_unknown $(top_srcdir)/plugins/gtk/iconssets/icq-unknown.png \
		> $@

EXTRA_DIST += $(BASE_PNGS) $(GG_PNGS) $(ICQ_PNGS)
endif

if ENABLE_ICQ
plugin_LTLIBRARIES += plugins/icq/icq.la

plugins_icq_icq_la_SOURCES = \
	$(noinst_HEADERS) \
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
	$(noinst_HEADERS) \
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
	$(noinst_HEADERS) \
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
	$(noinst_HEADERS) \
	plugins/jogger/drafts.c \
	plugins/jogger/jogger.c \
	plugins/jogger/messages.c
endif

if ENABLE_LOGS
plugin_LTLIBRARIES += plugins/logs/logs.la

plugins_logs_logs_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/logs/main.c \
	plugins/logs/main.h

dist_logs_DATA = \
	plugins/logs/vars-pl.txt
endif

if ENABLE_LOGSQLITE
plugin_LTLIBRARIES += plugins/logsqlite/logsqlite.la

plugins_logsqlite_logsqlite_la_SOURCES = \
	$(noinst_HEADERS) \
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
	$(noinst_HEADERS) \
	plugins/mail/main.c

dist_mail_DATA = \
	plugins/mail/vars-pl.txt
endif

if ENABLE_NCURSES
plugin_LTLIBRARIES += plugins/ncurses/ncurses.la

plugins_ncurses_ncurses_la_SOURCES = \
	$(noinst_HEADERS) \
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
	plugins/ncurses/nc-strings.c \
	plugins/ncurses/nc-strings.h \
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

if ENABLE_PERL
plugin_LTLIBRARIES += plugins/perl/perl.la

plugins_perl_perl_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/perl/perl_bless.c \
	plugins/perl/perl_bless.h \
	plugins/perl/perl_core.c \
	plugins/perl/perl_core.h \
	plugins/perl/perl_ekg.c \
	plugins/perl/perl_ekg.h

dist_script_DATA += \
	contrib/perl/audioscrobbler_bot.pl \
	contrib/perl/autoop.pl \
	contrib/perl/cycki.pl \
	contrib/perl/dns.pl \
	contrib/perl/dupa.pl \
	contrib/perl/ggbe.pl \
	contrib/perl/irc.pl \
	contrib/perl/sample.pl \
	contrib/perl/slownik.pl \
	contrib/perl/xmms.pl

if ENABLE_PERL_MODULES
$(top_srcdir)/plugins/perl/common/Makefile: $(top_srcdir)/plugins/perl/common/Makefile.PL
	$(AM_V_GEN)cd $(top_srcdir)/plugins/perl/common && $(PERL) Makefile.PL INC="-I$(abs_top_builddir) -I../../.."
$(top_srcdir)/plugins/perl/irc/Makefile: $(top_srcdir)/plugins/perl/irc/Makefile.PL
	$(AM_V_GEN)cd $(top_srcdir)/plugins/perl/irc && $(PERL) Makefile.PL INC="-I$(abs_top_builddir) -I../../.."

perl-build-common: $(top_srcdir)/plugins/perl/common/Makefile
	cd $(top_srcdir)/plugins/perl/common && $(MAKE) CC="$(CC)" CCFLAGS="$(plugins_perl_perl_la_CPPFLAGS) $(CFLAGS)"
perl-build-irc: $(top_srcdir)/plugins/perl/irc/Makefile
	cd $(top_srcdir)/plugins/perl/irc && $(MAKE) CC="$(CC)" CCFLAGS="$(plugins_perl_perl_la_CPPFLAGS) $(CFLAGS)"

perl-clean-common: $(top_srcdir)/plugins/perl/common
	cd $(top_srcdir)/plugins/perl/common && test ! -f Makefile || $(MAKE) clean
perl-clean-irc: $(top_srcdir)/plugins/perl/irc
	cd $(top_srcdir)/plugins/perl/irc && test ! -f Makefile || $(MAKE) clean

perl-distclean:
	-rm -f $(top_srcdir)/plugins/perl/common/Makefile $(top_srcdir)/plugins/perl/irc/Makefile

perl-install-common: $(top_srcdir)/plugins/perl/common/Makefile
	cd $(top_srcdir)/plugins/perl/common && $(MAKE) install DESTDIR="$(DESTDIR)"
perl-install-irc: $(top_srcdir)/plugins/perl/irc/Makefile
	cd $(top_srcdir)/plugins/perl/irc && $(MAKE) install DESTDIR="$(DESTDIR)"

all-local: perl-build-common perl-build-irc
clean-local: perl-clean-common perl-clean-irc
distclean-local: perl-distclean
install-exec-local: perl-install-common perl-install-irc
endif

EXTRA_DIST += \
	plugins/perl/common/Command.xs \
	plugins/perl/common/Ekg2.pm \
	plugins/perl/common/Ekg2.xs \
	plugins/perl/common/Makefile.PL \
	plugins/perl/common/module.h \
	plugins/perl/common/Plugin.xs \
	plugins/perl/common/Session.xs \
	plugins/perl/common/Timer.xs \
	plugins/perl/common/typemap \
	plugins/perl/common/Userlist.xs \
	plugins/perl/common/Variable.xs \
	plugins/perl/common/Watch.xs \
	plugins/perl/common/Window.xs \
	plugins/perl/irc/Channel.xs \
	plugins/perl/irc/Irc.pm \
	plugins/perl/irc/Irc.xs \
	plugins/perl/irc/Makefile.PL \
	plugins/perl/irc/module.h \
	plugins/perl/irc/Server.xs \
	plugins/perl/irc/typemap \
	plugins/perl/irc/User.xs
endif

if ENABLE_POLCHAT
plugin_LTLIBRARIES += plugins/polchat/polchat.la

plugins_polchat_polchat_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/polchat/polchat.c \
	plugins/polchat/polchat_handlers.inc
endif

if ENABLE_PYTHON
plugin_LTLIBRARIES += plugins/python/python.la

plugins_python_python_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/python/python.c \
	plugins/python/python-config.c \
	plugins/python/python-config.h \
	plugins/python/python-ekg.c \
	plugins/python/python-ekg.h \
	plugins/python/python.h \
	plugins/python/python-plugin.c \
	plugins/python/python-plugin.h \
	plugins/python/python-session.c \
	plugins/python/python-session.h \
	plugins/python/python-user.c \
	plugins/python/python-user.h \
	plugins/python/python-window.c \
	plugins/python/python-window.h

dist_python_DATA = \
	plugins/python/commands-en.txt \
	plugins/python/commands-pl.txt

dist_script_DATA += \
	contrib/python/notify-bubble.py \
	contrib/python/sample.py
endif

if ENABLE_READLINE
plugin_LTLIBRARIES += plugins/readline/readline.la

plugins_readline_readline_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/readline/completion.c \
	plugins/readline/main.c \
	plugins/readline/ui-readline.c \
	plugins/readline/ui-readline.h

dist_readline_DATA = \
	plugins/readline/vars-en.txt \
	plugins/readline/vars-pl.txt
endif

if ENABLE_RIVCHAT
plugin_LTLIBRARIES += plugins/rivchat/rivchat.la

plugins_rivchat_rivchat_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/rivchat/misc.c \
	plugins/rivchat/rivchat.c \
	plugins/rivchat/rivchat.h
endif

if ENABLE_SIM
plugin_LTLIBRARIES += plugins/sim/sim.la

plugins_sim_sim_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/sim/main.c \
	plugins/sim/simlite.c \
	plugins/sim/simlite.h

dist_sim_DATA = \
	plugins/sim/commands-pl.txt \
	plugins/sim/vars-pl.txt
endif

if ENABLE_SMS
plugin_LTLIBRARIES += plugins/sms/sms.la

plugins_sms_sms_la_SOURCES = \
	$(noinst_HEADERS) \
	plugins/sms/sms.c

dist_sms_DATA = \
	plugins/sms/commands-pl.txt \
	plugins/sms/vars-pl.txt
endif
