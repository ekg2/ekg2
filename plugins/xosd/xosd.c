/* $Id$ */

/*
 *  (C) Copyright 2004-2005 Adam Kuczyñski <dredzik@ekg2.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License Version
 *  2.1 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <ekg/commands.h>
#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/userlist.h>
#include <ekg/protocol.h>
#include <ekg/queries.h>
#include <ekg/themes.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include <xosd.h>
#include <string.h>
// #include <signal.h>

static int xosd_theme_init();

PLUGIN_DEFINE(xosd, PLUGIN_GENERIC, xosd_theme_init);

/* variables from xosd.h */
static char	*xosd_colour;
static char	*xosd_font;
static char	*xosd_outline_colour;
static char	*xosd_shadow_colour;

static int	xosd_display_filter;
static int	xosd_display_notify;
static int	xosd_display_timeout;
static int	xosd_display_welcome;
static int	xosd_horizontal_offset;
static int	xosd_horizontal_position;
static int	xosd_outline_offset;
static int	xosd_shadow_offset;
static int	xosd_short_messages;
static int	xosd_text_limit;
static int	xosd_vertical_offset;
static int	xosd_vertical_position;

static xosd *osd;

static int xosd_show_message(char *line1, char *line2)
{
	if (xosd_font && xosd_set_font(osd, xosd_font)) {
		debug("xosd: font error: %s\n", xosd_error);
		return -1;
	}
	
	xosd_set_colour(osd, xosd_colour);

	xosd_set_shadow_offset(osd, xosd_shadow_offset);
	xosd_set_shadow_colour(osd, xosd_shadow_colour);
	
	xosd_set_outline_offset(osd, xosd_outline_offset);
	xosd_set_outline_colour(osd, xosd_outline_colour);

	switch (xosd_vertical_position) {
		case 0:
			xosd_set_pos(osd, XOSD_top);
			break;

		case 1:
			xosd_set_pos(osd, XOSD_middle);
			break;
			
		case 2:
		default:
			xosd_set_pos(osd, XOSD_bottom);
			break;
	}
	
	switch (xosd_horizontal_position) {
		case 0:
			xosd_set_align(osd, XOSD_left);
			break;

		case 1:
			xosd_set_align(osd, XOSD_center);
			break;

		case 2:
		default:
			xosd_set_align(osd, XOSD_right);
			break;
	}
	
	xosd_set_vertical_offset(osd, xosd_vertical_offset);
	xosd_set_horizontal_offset(osd, xosd_horizontal_offset);

	xosd_set_timeout(osd, xosd_display_timeout);

	xosd_scroll(osd, 2);
	
	if (xstrcmp(line1, ""))	
		xosd_display(osd, 0, XOSD_string, line1);
	
	if (xstrcmp(line2, ""))
		xosd_display(osd, 1, XOSD_string, line2);
	
	return 0; 
}

static COMMAND(xosd_command_msg) 
{
	xosd_show_message((char *) params[0], NULL);
	return 0;
}

static QUERY(xosd_protocol_status)
{
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	int nstatus	= *(va_arg(ap, int*));
	char *descr	= *(va_arg(ap, char**));

	const char *status	= ekg_status_string(nstatus, 0);

	userlist_t *u;
	session_t *s;

	const char *sender;
	char *msgLine1;
	char *msgLine2;
	char *format;
	int level;
	
	if (!(s = session_find(session)))
		return 0;

	if (!(u = userlist_find(s, uid)))
		return 0;
	
	level = ignored_check(s, uid);
	
	if ((level == IGNORE_ALL) || (level & IGNORE_STATUS) || (level & IGNORE_XOSD))
		return 0;

	if (!xosd_display_notify || ((xosd_display_notify == 2) && (!session_int_get(s, "display_notify"))))
		return 0;

	
	sender = (u->nickname) ? u->nickname : uid;

	format = saprintf("xosd_status_change_%s", status);

	msgLine1 = format_string(format_find(format), sender);

	if (xstrcmp(descr, "") && !(level & IGNORE_STATUS_DESCR)) {
		if (xosd_text_limit && xstrlen(descr) > xosd_text_limit)
			msgLine2 = format_string(format_find("xosd_status_change_description_long"), xstrmid(descr, 0, xosd_text_limit));
		else
			msgLine2 = format_string(format_find("xosd_status_change_description"), descr);
	} else
		msgLine2 = format_string(format_find("xosd_status_change_no_description"));

	if (xosd_short_messages)
		msgLine2 = format_string("");

	xosd_show_message(msgLine1, msgLine2);

	xfree(msgLine1);
	xfree(msgLine2);
	xfree(format);
	
	return 0;
}

static QUERY(xosd_protocol_message)
{
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
		char **UNUSED(rcpts)	= *(va_arg(ap, char***));
	char *text	= *(va_arg(ap, char**));
		uint32_t *UNUSED(format) = *(va_arg(ap, uint32_t**));
		time_t UNUSED(sent)	 = *(va_arg(ap, time_t*));
	int class	= *(va_arg(ap, int*));
	
	userlist_t *u;
	session_t *s;
	int level;
	
	if (!(s = session_find(session)))
		return 0;

	if (session_check(s, 0, "irc"))
		return 0;
		
	u = userlist_find(s, uid);
	
	level = ignored_check(s, uid);
	
	if ((level == IGNORE_ALL) || (level & IGNORE_MSG) || (level & IGNORE_XOSD))
		return 0;

	if (xosd_display_filter == 1 && window_current && window_current->target && !xstrcmp(get_uid(s, window_current->target), get_uid(s, uid)))
		return 0;

	if (xosd_display_filter == 2 && window_find_s(s, uid))
		return 0;

	if (class != EKG_MSGCLASS_SENT && class != EKG_MSGCLASS_SENT_CHAT) {
		const char *sender;
		char *msgLine1;
		char *msgLine2;
		
		sender = (u && u->nickname) ? u->nickname : uid;
		msgLine1 = format_string(format_find("xosd_new_message_line_1"), sender);

		if (xosd_text_limit && xstrlen(text) > xosd_text_limit) {
			char *tmp = xstrmid(text, 0, xosd_text_limit);
			msgLine2 = format_string(format_find("xosd_new_message_line_2_long"), tmp);
			xfree(tmp);
		} else
			msgLine2 = format_string(format_find("xosd_new_message_line_2"), text);

		if (xosd_short_messages)
			msgLine2 = format_string("");

		xosd_show_message(msgLine1, msgLine2);
	
		xfree(msgLine1);
		xfree(msgLine2);
	}

	return 0;
}

static QUERY(xosd_irc_protocol_message)
{
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
	char *text	= *(va_arg(ap, char**));
	int isour	= *(va_arg(ap, int*));
	int foryou	= *(va_arg(ap, int*));
	int private	= *(va_arg(ap, int*));
	char *channame	= *(va_arg(ap, char**));
	
	session_t *s;
	char *msgLine1;
	char *msgLine2;

	debug("[xosd_irc_protocol_message] %s %d %d\n", session, foryou, isour);
	
	if (!(s = session_find(session)))
		return 0;
	
	if (!foryou || isour)
		return 0;
	
	if (private)
		msgLine1 = format_string(format_find("xosd_new_message_line_1"), uid);
	else
		msgLine1 = format_string(format_find("xosd_new_message_irc"), uid, channame);

	if (xosd_text_limit && xstrlen(text) > xosd_text_limit)
		msgLine2 = format_string(format_find("xosd_new_message_line_2_long"), xstrmid(text, 0, xosd_text_limit));
	else
		msgLine2 = format_string(format_find("xosd_new_message_line_2"), text);

	if (xosd_short_messages)
		msgLine2 = format_string("");

	xosd_show_message(msgLine1, msgLine2);
	
	xfree(msgLine1);
	xfree(msgLine2);

	return 0;
}

static TIMER(xosd_display_welcome_message) /* temporary timer */
{
	if (type)
		return 0;
	if (xosd_display_welcome) { 
		char *line1 = format_string(format_find("xosd_welcome_message_line_1"));
		char *line2 = format_string(format_find("xosd_welcome_message_line_2"));

		xosd_show_message(line1, line2);

		xfree(line1);
		xfree(line2);		
	}
	return -1;
}

static QUERY(xosd_setvar_default)
{
	xfree(xosd_colour);
	xfree(xosd_font);
	xfree(xosd_outline_colour);
	xfree(xosd_shadow_colour);

	xosd_colour = xstrdup("#00dd00");
	xosd_font = xstrdup("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-2");
	xosd_outline_colour = xstrdup("#000000");
	xosd_shadow_colour = xstrdup("#000000");
	
	xosd_display_filter = 0;
	xosd_display_notify = 1;
	xosd_display_timeout = 6;
	xosd_display_welcome = 1;
	xosd_horizontal_offset = 48;
	xosd_horizontal_position = 0;
	xosd_outline_offset = 0;
	xosd_shadow_offset = 2;
	xosd_short_messages = 0;
	xosd_text_limit = 50;
	xosd_vertical_offset = 48;
	xosd_vertical_position = 2;
	return 0;
}
#if 0
static void xosd_killed() {
	debug("xosd killed\n");
	xosd_destroy(osd);
	osd = NULL;
}
#endif

int xosd_plugin_init(int prio)
{
	va_list dummy;

	PLUGIN_CHECK_VER("xosd");

	osd = xosd_create(2);
	if (!osd) {
		debug("xosd: error creating osd: %s\n", xosd_error);
		return -1;
	}
	plugin_register(&xosd_plugin, prio);

	xosd_setvar_default(NULL, dummy);

	variable_add(&xosd_plugin, ("colour"), VAR_STR, 1, &xosd_colour, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("font"), VAR_STR, 1, &xosd_font, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("outline_colour"), VAR_STR, 1, &xosd_outline_colour, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("shadow_colour"), VAR_STR, 1, &xosd_shadow_colour, NULL, NULL, NULL);
	
	variable_add(&xosd_plugin, ("display_filter"), VAR_MAP, 1, &xosd_display_filter, NULL,
			variable_map(3, 0, 0, "all", 1, 2, "only inactive", 2, 1, "only new"), NULL);
	variable_add(&xosd_plugin, ("display_notify"), VAR_MAP, 1, &xosd_display_notify, NULL,
			variable_map(3, 0, 0, "none", 1, 2, "all", 2, 1, "session-depend"), NULL);
	variable_add(&xosd_plugin, ("display_timeout"), VAR_INT, 1, &xosd_display_timeout, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("display_welcome"), VAR_BOOL, 1, &xosd_display_welcome, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("horizontal_offset"), VAR_INT, 1, &xosd_horizontal_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("horizontal_position"), VAR_MAP, 1, &xosd_horizontal_position, NULL,
			variable_map(3, 0, 2, "left", 1, 0, "center", 2, 1, "right"), NULL);
	variable_add(&xosd_plugin, ("outline_offset"), VAR_INT, 1, &xosd_outline_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("shadow_offset"), VAR_INT, 1, &xosd_shadow_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("short_messages"), VAR_BOOL, 1, &xosd_short_messages, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("text_limit"), VAR_INT, 1, &xosd_text_limit, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("vertical_offset"), VAR_INT, 1, &xosd_vertical_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, ("vertical_position"), VAR_MAP, 1, &xosd_vertical_position, NULL,
			variable_map(3, 0, 2, "top", 1, 0, "center", 2, 1, "bottom"), NULL);
	
	query_connect_id(&xosd_plugin, PROTOCOL_MESSAGE, xosd_protocol_message, NULL);
	query_connect_id(&xosd_plugin, IRC_PROTOCOL_MESSAGE, xosd_irc_protocol_message, NULL);
	query_connect_id(&xosd_plugin, PROTOCOL_STATUS, xosd_protocol_status, NULL);
	
	timer_add(&xosd_plugin, "xosd:display_welcome_timer", 1, 0, xosd_display_welcome_message, NULL);

	command_add(&xosd_plugin, ("xosd:msg"), ("!"), xosd_command_msg, COMMAND_ENABLEREQPARAMS, NULL);
	return 0;
}

static int xosd_theme_init() {
#ifndef NO_DEFAULT_THEME
	format_add("xosd_new_message_irc", _("new message from %1 at %2"), 1);
	format_add("xosd_new_message_line_1", _("new message from %1"), 1);
	format_add("xosd_new_message_line_2_long", "%1...", 1);
	format_add("xosd_new_message_line_2", "%1", 1);
	format_add("xosd_status_change_avail", _("%1 is online,"), 1);
	format_add("xosd_status_change_away", _("%1 is away,"), 1);
	format_add("xosd_status_change_dnd", _("%1: do not disturb,"), 1);
	format_add("xosd_status_change_xa", _("%1 is extended away,"), 1);
	format_add("xosd_status_change_notavail", _("%1 is offline,"), 1);
	format_add("xosd_status_change_invisible", _("%1 is invisible,"), 1);
	format_add("xosd_status_change_chat", _("%1 is free for chat,"), 1);
	format_add("xosd_status_change_error", _("%1: status error,"), 1);
	format_add("xosd_status_change_blocking", _("%1 is blocking us,"), 1);
	format_add("xosd_status_change_description", "%1", 1);
	format_add("xosd_status_change_description_long", "%1...", 1);
	format_add("xosd_status_change_no_description", _("[no description]"), 1);
	format_add("xosd_welcome_message_line_1", _("ekg2 XOnScreenDisplay plugin"), 1);
	format_add("xosd_welcome_message_line_2", _("Author: Adam 'dredzik' Kuczynski"), 1);
#endif
	return 0;
}

static int xosd_plugin_destroy()
{
	xosd_destroy(osd);
	
	plugin_unregister(&xosd_plugin);
	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
