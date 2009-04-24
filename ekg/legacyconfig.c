/*
 *  (C) Copyright 2007 EKG2 authors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "stuff.h" /* config_version here */
#include "themes.h" /* print() & _() */
#include "xmalloc.h" /* x*() */

/**
 * config_upgrade()
 *
 * Check current configuration file version and upgrade it if needed. Print additional info about changes.
 *
 * @todo Instead of hardcoded dates, use strftime() because for instance in USA we have got month and date reversed.
 */

void config_upgrade() {
	const int current_config_version = 7;

#if ! USE_UNICODE
	if (!xstrcasecmp(config_console_charset, "UTF-8")) {
		print("config_error", _("Warning, nl_langinfo(CODESET) reports that you are using utf-8 encoding, but you didn't compile ekg2 with (experimental/untested) --enable-unicode\n"
			    "\tPlease compile ekg2 with --enable-unicode or change your enviroment setting to use not utf-8 but iso-8859-1 maybe? (LC_ALL/LC_CTYPE)\n"));
	}
#endif

	if (xstrcasecmp(console_charset, config_console_charset)) 
		print("console_charset_bad", console_charset, config_console_charset);
	else if (config_version == 0 || config_version == -1)
		print("console_charset_using", config_console_charset);

	if (config_version == -1)
		config_version = current_config_version;

	if (config_version >= current_config_version)
		return;

	print("config_upgrade_begin");

	switch (config_version) { /* versions MUST be sorted, break MUST NOT be used */
		case 0: /* jabber SASL behavior change */
			print("config_upgrade_major", 
				_("We've started using XMPP SASL AUTH by default, so if you're unable to connect to your favorite jabber server,"	\
				"please send us debug info and try to set (within appropriate session):\n"
				"/session disable_sasl 2"), "2007-03-24");

		case 1: /* display_ack values change */
			print("config_upgrade_minor", 
				_("Variable display_ack's values have been changed. "	\
				"An update is done to your settings, but please check the new values."), "2007-03-24");

			switch (config_display_ack) {
				case 1: config_display_ack = 31; break;
				case 2: config_display_ack = 1; break;
				case 3: config_display_ack = 2; break;
			}

			config_changed = 1;

		case 2: /* allow_autoresponder session var */
			print("config_upgrade_minor", 
				_("'allow_autoresponder' session variables have been replaced by 'allowed_sessions' plugin variable. "	\
				"The same way future plugins will be enabled."), "2007-04-06");

		case 3:
			print("config_upgrade_minor",
				_("'logs:away_log' plugin variable have been replaced by 'away_log' irc session variable. " \
				"Also away_log_* formats have been changed to irc_awaylog_* formats. Enjoy"), "2007-07-06");
		case 4:
#if 0		
			print("config_upgrade_major",
				_("Jabber UIDs prefix has been changed from 'jid:' to 'xmpp:'. Your session UIDs were changed " \
				"automagically, and in other areas old prefix will be still supported for some time."), "2007-10-16");
#endif
		case 5:
			print("config_upgrade_minor",
				_("'ping-server' jabber session variable has been renamed to 'ping_server', please set it by hand " \
				"where needed. Sorry for that inconvenience."), "2007-08-24");
		case 6:
			print("config_upgrade_major",
				_("sqlite only logs messages and status-changes when in log_formats session variable you have 'sqlite' " \
				"Your config couldn't be updated automagically, so you must set it by hand."), "2008-08-06");
		case 7:
			print("config_upgrade_minor",
				_("display_pl_chars option is no longer maintained, use /set console_charset US-ASCII"), "2009-04-24");
	}

	config_version = current_config_version;
	if (!config_auto_save)
		print("config_upgrade_end");
}

