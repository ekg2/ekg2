/* $Id$ */

/*
 *  (C) Copyright 2003 Jan Kowalski <jan.kowalski@gdzies.pl>
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
	
	if (xosd_shadow_offset) 
		xosd_set_shadow_offset(osd, xosd_shadow_offset);
	
	xosd_set_colour(osd, xosd_colour);

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

	const char *sender;
	char *msgLine1;
	char *msgLine2;
	char format[100];
	
	userlist_t *u = userlist_find(session_find(session), uid);
	
	sender = (u && u->nickname) ? u->nickname : uid;

	snprintf(format, sizeof(format), "xosd_status_change_%s", status );

	msgLine1 = format_string(format_find(format), sender);

	if (xstrcmp(descr, ""))
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

	
	if (class != EKG_MSGCLASS_SENT && class != EKG_MSGCLASS_SENT_CHAT) {
		const char *sender;
		char *msgLine1;
		char *msgLine2;
	
		userlist_t *u = userlist_find(session_find(session), uid);
	
		sender = (u && u->nickname) ? u->nickname : uid;
		msgLine1 = format_string(format_find("xosd_new_message_line_1"), sender);

		if(xosd_text_limit && xstrlen(text) > xosd_text_limit)
			msgLine2 = format_string(format_find("xosd_new_message_line_2_long"), xstrmid(text, 0, xosd_text_limit));
		else
			msgLine2 = format_string(format_find("xosd_new_message_line_2"), text);

		xosd_show_message(msgLine1, msgLine2);
	
		xfree(msgLine1);
		xfree(msgLine2);
	}

	return 0;
}

void xosd_setvar_default()
{
	xfree(xosd_font);
	xfree(xosd_colour);

	xosd_font = xstrdup("-adobe-helvetica-bold-r-normal-*-*-120-*-*-p-*-iso8859-2");
	xosd_colour = xstrdup("#00dd00");
	
	xosd_shadow_offset = 2;
	xosd_vertical_position = 2;
	xosd_vertical_offset = 48;
	xosd_horizontal_position = 0;
	xosd_horizontal_offset = 48;
	xosd_display_timeout = 6;
	xosd_text_limit = 50;
	
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
	variable_add(&xosd_plugin, "vertical_position", VAR_INT, 1, &xosd_vertical_position, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "vertical_offset", VAR_INT, 1, &xosd_vertical_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "horizontal_position", VAR_INT, 1, &xosd_horizontal_position, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "horizontal_offset", VAR_INT, 1, &xosd_horizontal_offset, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "display_timeout", VAR_INT, 1, &xosd_display_timeout, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "text_limit", VAR_INT, 1, &xosd_text_limit, NULL, NULL, NULL);
	variable_add(&xosd_plugin, "display_welcome", VAR_BOOL, 1, &xosd_display_welcome, NULL, NULL, NULL);
	
	query_connect(&xosd_plugin, "protocol-message", xosd_protocol_message, NULL);
	query_connect(&xosd_plugin, "protocol-status", xosd_protocol_status, NULL);

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
	format_add("xosd_status_change_no_description", "", 1);

	format_add("xosd_welcome_message_line_1", _("ekg2 XOnScreenDisplay plugin"), 1);
	format_add("xosd_welcome_message_line_2", _("Author: Adam 'dredzik' Kuczynski"), 1);
	
	if (xosd_display_welcome) 
		xosd_show_message(format_string(format_find("xosd_welcome_message_line_1")), format_string(format_find("xosd_welcome_message_line_2")));

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
