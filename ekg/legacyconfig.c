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

/**
 * config_upgrade()
 *
 * Check current configuration file version and upgrade it if needed. Print additional info about changes.
 */

void config_upgrade() {
	const int current_config_version = 1;

	if (config_version >= current_config_version)
		return;
	else
		print("config_upgrade_begin");
	
	if (config_version == 0) /* jabber SASL behavior change */
		print("config_upgrade_major", "We've started using XMPP SASL AUTH by default, so if you're unable to connect to your favorite jabber server, please send us debug info and try to set (within appropriate session):\n\t/session disable_sasl 2");
	
	config_version = current_config_version;
	if (config_save_quit != 2)
		print("config_upgrade_end");
}

