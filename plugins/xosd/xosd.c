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
#include "osd.h"

#include <ekg/plugins.h>
#include <ekg/themes.h>
#include <ekg/stuff.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/userlist.h>
#include <ekg/protocol.h>

#include <xosd.h>
#include <string.h>

static int xosd_theme_init();

PLUGIN_DEFINE(xosd, PLUGIN_GENERIC, xosd_theme_init);

xosd *osd;

int xosd_show_message(char *line1, char *line2)
{
	xosd_set_font(osd, xosd_font); 
	
	xosd_set_shadow_offset(osd, xosd_shadow_offset);
	
	xosd_set_colour(osd, xosd_colour);

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
	
	xosd_set_vertical_offset(osd, xosd_vertical_offset);

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
	
	xosd_set_horizontal_offset(osd, xosd_horizontal_offset);

	xosd_set_timeout(osd, xosd_display_timeout);

	if (xstrcmp(line1, "")) 
		xosd_display(osd, 0, XOSD_string, line1);

	if (xstrcmp(line2, ""))
		xosd_display(osd, 1, XOSD_string, line2);
	
	return 0; 
}

static int xosd_protocol_status(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**), *session = *__session;
	char **__uid = va_arg(ap, char**), *uid = *__uid;
	char **__status = va_arg(ap, char**), *status = *__status;
	char **__descr = va_arg(ap, char**), *descr = *__descr;
	userlist_t *u;
	session_t *s;
	
	if (!(s = session_find(session)))
                return 0;

        if (!(u = userlist_find(s, uid)))
                return 0;
	
	int level = ignored_check(s, uid);
	
	if ((level == IGNORE_ALL) || (level & IGNORE_STATUS))
		return 0;

	if (!xosd_display_notify || ((xosd_display_notify == 2) && (!session_int_get(s, "display_notify"))))
		return 0;

	const char *sender;
	char *msgLine1;
	char *msgLine2;
	char format[100];
	
	sender = (u->nickname) ? u->nickname : uid;

	snprintf(format, sizeof(format), "xosd_status_change_%s", status );

	msgLine1 = format_string(format_find(format), sender);

	if (xstrcmp(descr, "") && !(level & IGNORE_STATUS_DESCR))
		if (xosd_text_limit && xstrlen(descr) > xosd_text_limit)
			msgLine2 = format_string(format_find("xosd_status_change_description_long"), xstrmid(descr, 0, xosd_text_limit));
		else
			msgLine2 = format_string(format_find("xosd_status_change_description"), descr);
	else
		msgLine2 = format_string(format_find("xosd_status_change_no_description"));

	xosd_show_message(msgLine1, msgLine2);

	xfree(msgLine1);
	xfree(msgLine2);
					
	return 0;
}

static int xosd_protocol_message(void *data, va_list ap)
{
	char **__session = va_arg(ap, char**), *session = *__session;
	char **__uid = va_arg(ap, char**), *uid = *__uid;
        char ***__rcpts = va_arg(ap, char***);
	char **__text = va_arg(ap, char**), *text = *__text;
	uint32_t **__format = va_arg(ap, uint32_t**), *format = *__format;
	time_t *__sent = va_arg(ap, time_t*), sent = *__sent;
	int *__class = va_arg(ap, int*), class = *__class;
        userlist_t *u;
	session_t *s;
	
	if (!(s = session_find(session)))
                return 0;
		
	u = userlist_find(s, uid);
	
	int level = ignored_check(s, uid);
	
	if ((level == IGNORE_ALL) || (level & IGNORE_MSG))
		return 0;

	if (xosd_display_filter == 1 && window_current && window_current->target && !xstrcmp(get_uid(s, window_current->target), get_uid(s, uid)))
		return 0;

	if (xosd_display_filter == 2 && window_find(uid))
		return 0;

	if (class != EKG_MSGCLASS_SENT && class != EKG_MSGCLASS_SENT_CHAT) {
		const char *sender;
		char *msgLine1;
		char *msgLine2;
		
		sender = (u && u->nickname) ? u->nickname : uid;
		msgLine1 = format_string(format_find("xosd_new_message_line_1"), sender);

		if (xosd_text_limit && xstrlen(text) > xosd_text_limit)
			msgLine2 = format_string(format_find("xosd_new_message_line_2_long"), xstrmid(text, 0, xosd_text_limit));
		else
			msgLine2 = format_string(format_find("xosd_new_message_line_2"), text);

		xosd_show_message(msgLine1, msgLine2);
	
		xfree(msgLine1);
		xfree(msgLine2);
	}

	return 0;
}

static void xosd_display_welcome_message()
{
	xosd_show_message(format_string(format_find("xosd_welcome_message_line_1")), format_string(format_find("xosd_welcome_message_line_2")));
	timer_remove(&xosd_plugin, "xosd:display_welcome_timer");
}

void xosd_setvar_default()
{
	xfree(xosd_font);
	xfree(xosd_colour);
	xfree(xosd_outline_colour);

	xosd_font = xstrdup("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-2");
	xosd_colour = xstrdup("#00dd00");
	xosd_shadow_offset = 2;
	xosd_vertical_position = 2;
	xosd_vertical_offset = 48;
	xosd_horizontal_position = 0;
	xosd_horizontal_offset = 48;
	xosd_display_timeout = 6;
	xosd_text_limit = 50;
	xosd_outline_offset = 0;
	xosd_outline_colour = xstrdup("#000000");
	xosd_display_filter = 0;
	xosd_display_notify = 1;
	xosd_display_welcome = 1;
}


int xosd_plugin_init()
{
	plugin_register(&xosd_plugin);

	osd = NULL;	
	osd = xosd_create(2);
	if (osd == NULL)	
		return 1;

	xosd_setvar_default();

	variable_add(&xosd_plugin, "font", VAR_STR, 1, &xosd_font, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "colour", VAR_STR, 1, &xosd_colour, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "shadow_offset", VAR_INT, 1, &xosd_shadow_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "vertical_position", VAR_MAP, 1, &xosd_vertical_position, NULL, variable_map(3, 0, 2, "top", 1, 0, "center", 2, 1, "bottom"), NULL);
	variable_add(&xosd_plugin, "vertical_offset", VAR_INT, 1, &xosd_vertical_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "horizontal_position", VAR_MAP, 1, &xosd_horizontal_position, NULL, variable_map(3, 0, 2, "left", 1, 0, "center", 2, 1, "right"), NULL);
	variable_add(&xosd_plugin, "horizontal_offset", VAR_INT, 1, &xosd_horizontal_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "display_timeout", VAR_INT, 1, &xosd_display_timeout, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "text_limit", VAR_INT, 1, &xosd_text_limit, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "outline_offset", VAR_INT, 1, &xosd_outline_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "outline_colour", VAR_STR, 1, &xosd_outline_colour, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "display_filter", VAR_MAP, 1, &xosd_display_filter, NULL, variable_map(3, 0, 0, "all", 1, 2, "only inactive", 2, 1, "only new"), NULL);
	variable_add(&xosd_plugin, "display_notify", VAR_MAP, 1, &xosd_display_notify, NULL, variable_map(3, 0, 0, "none", 1, 2, "all", 2, 1, "session-depend"), NULL);
	variable_add(&xosd_plugin, "display_welcome", VAR_BOOL, 1, &xosd_display_welcome, NULL, NULL, NULL);
	
	query_connect(&xosd_plugin, "protocol-message", xosd_protocol_message, NULL);
	query_connect(&xosd_plugin, "protocol-status", xosd_protocol_status, NULL);
	
	if (xosd_display_welcome) 
		timer_add(&xosd_plugin, "xosd:display_welcome_timer", 1, 0, xosd_display_welcome_message, NULL);

	return 0;
}

static int xosd_theme_init()
{
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
