/* X-Chat
 * Copyright (C) 1998-2007 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 *  port to ekg2:
 *  Copyright (C) 2007 Jakub Zawadzki <darkjames@darkjames.ath.cx>
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

#define GTK_DISABLE_DEPRECATED

#include "ekg2-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenubar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkversion.h>
#include <gdk/gdkkeysyms.h>

#include <ekg/stuff.h>
#include <ekg/windows.h>
#include <ekg/userlist.h>
#include <ekg/xmalloc.h>

#include "main.h"
#include "maingui.h"
#include "palette.h"
#include "xtext.h"

#if 0
#include "fe-gtk.h"


#include "../common/xchat.h"
#include "../common/xchatc.h"
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/ignore.h"
#include "../common/fe.h"
#include "../common/server.h"
#include "../common/util.h"
#include "about.h"
#include "ascii.h"
#include "banlist.h"
#include "chanlist.h"
#include "editlist.h"
#include "fkeys.h"
#include "gtkutil.h"
#include "maingui.h"
#include "notifygui.h"
#include "pixmaps.h"
#include "plugingui.h"
#include "search.h"
#include "textgui.h"
#include "urlgrab.h"
#include "userlistgui.h"
#endif

#include "menu.h"

static GSList *submenu_list;

enum {
	M_MENUITEM,
	M_NEWMENU,
	M_END,
	M_SEP,
	M_MENUTOG,
	M_MENURADIO,
	M_MENUSTOCK,
	M_MENUPIX,
	M_MENUSUB
};

struct mymenu {
	char *text;
	void *callback;
	char *image;
	unsigned char type;	/* M_XXX */
	unsigned char id;	/* MENU_ID_XXX (menu.h) */
	unsigned char state;	/* ticked or not? */
	unsigned char sensitive;	/* shaded out? */
	guint key;		/* GDK_x */
};

#define XCMENU_DOLIST 1
#define XCMENU_SHADED 1
#define XCMENU_MARKUP 2
#define XCMENU_MNEMONIC 4

static void menu_about(GtkWidget *wid, gpointer sess) {
	GtkWidget *vbox, *label, *hbox;
	static GtkWidget *about = NULL;
	char buf[512];

	if (about) {
		gtk_window_present(GTK_WINDOW(about));
		return;
	}

	about = gtk_dialog_new();
	gtk_window_set_position(GTK_WINDOW (about), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW (about), FALSE);
	gtk_window_set_title(GTK_WINDOW(about), _("About ekg2"));

	vbox = GTK_DIALOG(about)->vbox;

	wid = gtk_image_new_from_pixbuf(pix_ekg2);
	gtk_container_add(GTK_CONTAINER(vbox), wid);

	label = gtk_label_new(NULL);
	gtk_label_set_selectable(GTK_LABEL (label), TRUE);
	gtk_container_add(GTK_CONTAINER(vbox), label);
	snprintf(buf, sizeof(buf), 
		"<span size=\"x-large\"><b>ekg2-%s</b></span>\n\n"
			"<b>Compiled on</b>: %s\n\n"
			"<small>gtk frontend based on xchat: \302\251 1998-2007 Peter \305\275elezn\303\275 &lt;zed@xchat.org></small>\n"
			"<small>iconsets in userlist copied from psi-0.10 (crystal-gadu.jisp and crystal-roster.jisp)</small>",
			VERSION, compile_time());

	gtk_label_set_markup(GTK_LABEL(label), buf);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);

	hbox = gtk_hbox_new(0, 2);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	wid = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(wid), GTK_CAN_DEFAULT);
	gtk_dialog_add_action_widget(GTK_DIALOG(about), wid, GTK_RESPONSE_OK);
	gtk_widget_grab_default(wid);

	gtk_widget_show_all(about);

	gtk_dialog_run(GTK_DIALOG(about));

	gtk_widget_destroy(about);
	about = NULL;
}

#if 0

/* execute a userlistbutton/popupmenu command */

static void nick_command(session *sess, char *cmd) {
	if (*cmd == '!')
		xchat_exec(cmd + 1);
	else
		handle_command(sess, cmd, TRUE);
}

/* fill in the %a %s %n etc and execute the command */

void nick_command_parse(session *sess, char *cmd, char *nick, char *allnick) {
	char *buf;
	char *host = _("Host unknown");
	struct User *user;
	int len;

/*	if (sess->type == SESS_DIALOG)
	{
		buf = (char *)(GTK_ENTRY (sess->gui->topic_entry)->text);
		buf = strrchr (buf, '@');
		if (buf)
			host = buf + 1;
	} else*/
	{
		user = userlist_find(sess, nick);
		if (user && user->hostname)
			host = strchr(user->hostname, '@') + 1;
	}

	/* this can't overflow, since popup->cmd is only 256 */
	len = strlen(cmd) + strlen(nick) + strlen(allnick) + 512;
	buf = malloc(len);

	auto_insert(buf, len, cmd, 0, 0, allnick, sess->channel, "",
		    server_get_network(sess->server, TRUE), host, sess->server->nick, nick);

	nick_command(sess, buf);

	free(buf);
}

/* userlist button has been clicked */

void userlist_button_cb(GtkWidget *button, char *cmd) {
	int i, num_sel, using_allnicks = FALSE;
	char **nicks, *allnicks;
	char *nick = NULL;
	session *sess;

	sess = current_sess;

	if (strstr(cmd, "%a"))
		using_allnicks = TRUE;

	if (sess->type == SESS_DIALOG) {
		/* fake a selection */
		nicks = malloc(sizeof(char *) * 2);
		nicks[0] = g_strdup(sess->channel);
		nicks[1] = NULL;
		num_sel = 1;
	} else {
		/* find number of selected rows */
		nicks = userlist_selection_list(sess->gui->user_tree, &num_sel);
		if (num_sel < 1) {
			nick_command_parse(sess, cmd, "", "");
			return;
		}
	}

	/* create "allnicks" string */
	allnicks = malloc(((NICKLEN + 1) * num_sel) + 1);
	*allnicks = 0;

	i = 0;
	while (nicks[i]) {
		if (i > 0)
			strcat(allnicks, " ");
		strcat(allnicks, nicks[i]);

		if (!nick)
			nick = nicks[0];

		/* if not using "%a", execute the command once for each nickname */
		if (!using_allnicks)
			nick_command_parse(sess, cmd, nicks[i], "");

		i++;
	}

	if (using_allnicks) {
		if (!nick)
			nick = "";
		nick_command_parse(sess, cmd, nick, allnicks);
	}

	while (num_sel) {
		num_sel--;
		g_free(nicks[num_sel]);
	}

	free(nicks);
	free(allnicks);
}
#endif

/* a popup-menu-item has been selected */

static void popup_menu_cb(GtkWidget *item, char *cmd) {
	char *nick;

	/* the userdata is set in menu_quick_item() */
	nick = g_object_get_data(G_OBJECT(item), "u");
#if 0
	if (!nick) {		/* userlist popup menu */
		/* treat it just like a userlist button */
		userlist_button_cb(NULL, cmd);
		return;
	}

	if (!current_sess)	/* for url grabber window */
		nick_command_parse(sess_list->data, cmd, nick, nick);
	else
		nick_command_parse(current_sess, cmd, nick, nick);
#endif
}

#if 0

GtkWidget *menu_toggle_item(char *label, GtkWidget *menu, void *callback, void *userdata, int state) {
	GtkWidget *item;

	item = gtk_check_menu_item_new_with_label(label);
	gtk_check_menu_item_set_active((GtkCheckMenuItem *) item, state);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show(item);

	return item;
}

#endif

static GtkWidget *menu_quick_item(char *cmd, char *label, GtkWidget *menu, int flags, gpointer userdata, char *icon) {
	GtkWidget *img, *item;

	if (!label)
		item = gtk_menu_item_new();
	else {
		if (icon) {
			/*if (flags & XCMENU_MARKUP)
			   item = gtk_image_menu_item_new_with_markup (label);
			   else */
			item = gtk_image_menu_item_new_with_mnemonic(label);
			img = gtk_image_new_from_file(icon);
			if (img)
				gtk_image_menu_item_set_image((GtkImageMenuItem *) item, img);
			else {
				img = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_MENU);
				if (img)
					gtk_image_menu_item_set_image((GtkImageMenuItem *) item,
								      img);
			}
		} else {
			if (flags & XCMENU_MARKUP) {
				item = gtk_menu_item_new_with_label("");
				if (flags & XCMENU_MNEMONIC)
					gtk_label_set_markup_with_mnemonic(GTK_LABEL
									   (GTK_BIN(item)->child),
									   label);
				else
					gtk_label_set_markup(GTK_LABEL(GTK_BIN(item)->child),
							     label);
			} else {
				if (flags & XCMENU_MNEMONIC)
					item = gtk_menu_item_new_with_mnemonic(label);
				else
					item = gtk_menu_item_new_with_label(label);
			}
		}
	}
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(item), "u", userdata);
	if (cmd)
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(popup_menu_cb), cmd);
	if (flags & XCMENU_SHADED)
		gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
	gtk_widget_show_all(item);

	return item;
}

#if 0

static void menu_quick_item_with_callback(void *callback, char *label, GtkWidget *menu, void *arg) {
	GtkWidget *item;

	item = gtk_menu_item_new_with_label(label);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), arg);
	gtk_widget_show(item);
}

#endif

static GtkWidget *menu_quick_sub(char *name, GtkWidget *menu, GtkWidget **sub_item_ret, int flags, int pos) {
	GtkWidget *sub_menu;
	GtkWidget *sub_item;

	if (!name)
		return menu;

	/* Code to add a submenu */
	sub_menu = gtk_menu_new();
	if (flags & XCMENU_MARKUP) {
		sub_item = gtk_menu_item_new_with_label("");
		gtk_label_set_markup(GTK_LABEL(GTK_BIN(sub_item)->child), name);
	} else {
		if (flags & XCMENU_MNEMONIC)
			sub_item = gtk_menu_item_new_with_mnemonic(name);
		else
			sub_item = gtk_menu_item_new_with_label(name);
	}
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), sub_item, pos);
	gtk_widget_show(sub_item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(sub_item), sub_menu);

	if (sub_item_ret)
		*sub_item_ret = sub_item;

	if (flags & XCMENU_DOLIST)
		/* We create a new element in the list */
		submenu_list = g_slist_prepend(submenu_list, sub_menu);
	return sub_menu;
}

static GtkWidget *menu_quick_endsub() {
	/* Just delete the first element in the linked list pointed to by first */
	if (submenu_list)
		submenu_list = g_slist_remove(submenu_list, submenu_list->data);

	if (submenu_list)
		return (submenu_list->data);
	else
		return NULL;
}

#if 0

static void toggle_cb(GtkWidget *item, char *pref_name) {
	char buf[256];

	if (GTK_CHECK_MENU_ITEM(item)->active)
		snprintf(buf, sizeof(buf), "set %s 1", pref_name);
	else
		snprintf(buf, sizeof(buf), "set %s 0", pref_name);

	handle_command(current_sess, buf, FALSE);
}

static int is_in_path(char *cmd) {
	char *prog = strdup(cmd + 1);	/* 1st char is "!" */
	char *space, *path, *orig;

	orig = prog;		/* save for free()ing */
	/* special-case these default entries. */
	/*                  123456789012345678 */
	if (strncmp(prog, "gnome-terminal -x ", 18) == 0)
		/* don't check for gnome-terminal, but the thing it's executing! */
		prog += 18;

	space = strchr(prog, ' ');	/* this isn't 100% but good enuf */
	if (space)
		*space = 0;

	path = g_find_program_in_path(prog);
	if (path) {
		g_free(path);
		g_free(orig);
		return 1;
	}

	g_free(orig);
	return 0;
}

/* append items to "menu" using the (struct popup*) list provided */

void menu_create(GtkWidget *menu, GSList * list, char *target, int check_path) {
	struct popup *pop;
	GtkWidget *tempmenu = menu, *subitem = NULL;
	int childcount = 0;

	submenu_list = g_slist_prepend(0, menu);
	while (list) {
		pop = (struct popup *)list->data;

		if (!strncasecmp(pop->name, "SUB", 3)) {
			childcount = 0;
			tempmenu = menu_quick_sub(pop->cmd, tempmenu, &subitem, XCMENU_DOLIST, -1);

		} else if (!strncasecmp(pop->name, "TOGGLE", 6)) {
			childcount++;
			menu_toggle_item(pop->name + 7, tempmenu, toggle_cb, pop->cmd,
					 cfg_get_bool(pop->cmd));

		} else if (!strncasecmp(pop->name, "ENDSUB", 6)) {
			/* empty sub menu due to no programs in PATH? */
			if (check_path && childcount < 1)
				gtk_widget_destroy(subitem);
			subitem = NULL;

			if (tempmenu != menu)
				tempmenu = menu_quick_endsub();
			/* If we get here and tempmenu equals menu that means we havent got any submenus to exit from */

		} else if (!strncasecmp(pop->name, "SEP", 3)) {
			menu_quick_item(0, 0, tempmenu, XCMENU_SHADED, 0, 0);

		} else {
			if (!check_path || pop->cmd[0] != '!') {
				menu_quick_item(pop->cmd, pop->name, tempmenu, 0, target, 0);
				/* check if the program is in path, if not, leave it out! */
			} else if (is_in_path(pop->cmd)) {
				childcount++;
				menu_quick_item(pop->cmd, pop->name, tempmenu, 0, target, 0);
			}
		}

		list = list->next;
	}

	/* Let's clean up the linked list from mem */
	while (submenu_list)
		submenu_list = g_slist_remove(submenu_list, submenu_list->data);
}
#endif

static void menu_destroy(GtkWidget *menu, gpointer objtounref) {
	gtk_widget_destroy(menu);
	g_object_unref(menu);
	if (objtounref)
		g_object_unref(G_OBJECT(objtounref));
}

static void menu_popup(GtkWidget *menu, GdkEventButton * event, gpointer objtounref) {
#if (GTK_MAJOR_VERSION != 2) || (GTK_MINOR_VERSION != 0)
	if (event && event->window)
		gtk_menu_set_screen(GTK_MENU(menu), gdk_drawable_get_screen(event->window));
#endif

	g_object_ref(menu);
	g_object_ref_sink(menu);
	g_object_unref(menu);
	g_signal_connect(G_OBJECT(menu), "selection-done", G_CALLBACK(menu_destroy), objtounref);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, event ? event->time : 0);
}

static char *str_copy = NULL;	/* for all pop-up menus */

void menu_nickmenu(window_t *sess, GdkEventButton * event, char *nick, int num_sel) {
	char buf[512];
	GtkWidget *menu = gtk_menu_new();
	userlist_t *user;

	if (str_copy)
		free(str_copy);
	str_copy = xstrdup(nick);

	submenu_list = NULL;	/* first time through, might not be 0 */		/* [XXX, how does this work? memleak? */
	/* more than 1 nick selected? */
	if (num_sel > 1) {
		snprintf(buf, sizeof(buf), "%d nicks selected.", num_sel);
		menu_quick_item(0, buf, menu, 0, 0, 0);
		menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);
	} else {
		user = userlist_find(sess->session, nick);

		/* XXX,
		 * 	jesli nadal nie ma uzytkownika, to szukaj go w konferencjach */

		if (user) {
			GtkWidget *submenu = menu_quick_sub(nick, menu, NULL, XCMENU_DOLIST, -1);

			char *fmt = "<tt><b>%-11s</b></tt> %s";			/* XXX, gettext? (let the translators tweak this if need be) */
			char *real;

		/* UID */
			real = g_markup_escape_text(user->uid, -1);
			snprintf(buf, sizeof(buf), fmt, "UID:", real);
			g_free(real);
			menu_quick_item(0, buf, submenu, XCMENU_MARKUP, 0, 0);

		/* <separator> ? */

		/* XXX, get more data using USERLIST_PRIVHANDLE (?) */
		/* the same like above */

			menu_quick_endsub();
			menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);
		}
	}
#if 0
	if (num_sel > 1)
		menu_create(menu, popup_list, NULL, FALSE);
	else
		menu_create(menu, popup_list, str_copy, FALSE);

	if (num_sel == 0)	/* xtext click */
		menu_add_plugin_items(menu, "\x5$NICK", str_copy);
	else			/* userlist treeview click */
		menu_add_plugin_items(menu, "\x5$NICK", NULL);
#endif
	menu_popup(menu, event, NULL);
}

#if 0

/* stuff for the View menu */

static void menu_showhide_cb(session *sess) {
	if (prefs.hidemenu)
		gtk_widget_hide(sess->gui->menu);
	else
		gtk_widget_show(sess->gui->menu);
}

static void menu_topic_showhide_cb(session *sess) {
	if (prefs.topicbar)
		gtk_widget_show(sess->gui->topic_bar);
	else
		gtk_widget_hide(sess->gui->topic_bar);
}

static void menu_userlist_showhide_cb(session *sess) {
	mg_decide_userlist(sess, TRUE);
}

static void menu_ulbuttons_showhide_cb(session *sess) {
	if (prefs.userlistbuttons)
		gtk_widget_show(sess->gui->button_box);
	else
		gtk_widget_hide(sess->gui->button_box);
}

static void menu_cmbuttons_showhide_cb(session *sess) {
	switch (sess->type) {
	case SESS_CHANNEL:
		if (prefs.chanmodebuttons)
			gtk_widget_show(sess->gui->topicbutton_box);
		else
			gtk_widget_hide(sess->gui->topicbutton_box);
		break;
	default:
		gtk_widget_hide(sess->gui->topicbutton_box);
	}
}

#endif

static void menu_setting_foreach(void (*callback) (window_t *), int id, guint state) {
	int maindone = FALSE;	/* do it only once for EVERY tab */
	list_t l;

	for (l = windows; l; l = l->next) {
		window_t *w = l->data;
		gtk_window_ui_t *gui = gtk_private_ui(w);

		if (!gui->is_tab || !maindone) {
			if (gui->is_tab)
				maindone = TRUE;
			if (id != -1)
				GTK_CHECK_MENU_ITEM(gui->menu_item[id])->active = state;
			if (callback)
				callback(w);
		}
	}
}

#if 0

void menu_bar_toggle(void) {
	prefs.hidemenu = !prefs.hidemenu;
	menu_setting_foreach(menu_showhide_cb, MENU_ID_MENUBAR, !prefs.hidemenu);
}

static void menu_bar_toggle_cb(void) {
	menu_bar_toggle();
	if (prefs.hidemenu)
		fe_message(_("The Menubar is now hidden. You can show it again"
			     " by pressing F9 or right-clicking in a blank part of"
			     " the main text area."), FE_MSG_INFO);
}

static void menu_topicbar_toggle(GtkWidget *wid, gpointer ud) {
	prefs.topicbar = !prefs.topicbar;
	menu_setting_foreach(menu_topic_showhide_cb, MENU_ID_TOPICBAR, prefs.topicbar);
}

static void menu_userlist_toggle(GtkWidget *wid, gpointer ud) {
	prefs.hideuserlist = !prefs.hideuserlist;
	menu_setting_foreach(menu_userlist_showhide_cb, MENU_ID_USERLIST, !prefs.hideuserlist);
}

static void menu_ulbuttons_toggle(GtkWidget *wid, gpointer ud) {
	prefs.userlistbuttons = !prefs.userlistbuttons;
	menu_setting_foreach(menu_ulbuttons_showhide_cb, MENU_ID_ULBUTTONS, prefs.userlistbuttons);
}

static void menu_cmbuttons_toggle(GtkWidget *wid, gpointer ud) {
	prefs.chanmodebuttons = !prefs.chanmodebuttons;
	menu_setting_foreach(menu_cmbuttons_showhide_cb, MENU_ID_MODEBUTTONS,
			     prefs.chanmodebuttons);
}

void menu_middlemenu(session *sess, GdkEventButton * event) {
	GtkWidget *menu;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	menu = menu_create_main(accel_group, FALSE, sess->server->is_away, !sess->gui->is_tab,
				NULL);
	menu_popup(menu, event, accel_group);
}

static void open_url_cb(GtkWidget *item, char *url) {
	char buf[512];

	/* pass this to /URL so it can handle irc:// */
	snprintf(buf, sizeof(buf), "URL %s", url);
	handle_command(current_sess, buf, FALSE);
}

static void copy_to_clipboard_cb(GtkWidget *item, char *url) {
	gtkutil_copy_to_clipboard(item, NULL, url);
}

void menu_urlmenu(GdkEventButton * event, char *url) {
	GtkWidget *menu;
	char *tmp, *chop;

	if (str_copy)
		free(str_copy);
	str_copy = strdup(url);

	menu = gtk_menu_new();
	/* more than 51 chars? Chop it */
	if (g_utf8_strlen(str_copy, -1) >= 52) {
		tmp = strdup(str_copy);
		chop = g_utf8_offset_to_pointer(tmp, 48);
		chop[0] = chop[1] = chop[2] = '.';
		chop[3] = 0;
		menu_quick_item(0, tmp, menu, XCMENU_SHADED, 0, 0);
		free(tmp);
	} else {
		menu_quick_item(0, str_copy, menu, XCMENU_SHADED, 0, 0);
	}
	menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);

	/* Two hardcoded entries */
	if (strncmp(str_copy, "irc://", 6) == 0 || strncmp(str_copy, "ircs://", 7) == 0)
		menu_quick_item_with_callback(open_url_cb, _("Connect"), menu, str_copy);
	else
		menu_quick_item_with_callback(open_url_cb, _("Open Link in Browser"), menu,
					      str_copy);
	menu_quick_item_with_callback(copy_to_clipboard_cb, _("Copy Selected Link"), menu,
				      str_copy);
	/* custom ones from urlhandlers.conf */
	menu_create(menu, urlhandler_list, str_copy, TRUE);
	menu_add_plugin_items(menu, "\x4$URL", str_copy);
	menu_popup(menu, event, NULL);
}

static void menu_chan_cycle(GtkWidget *menu, char *chan) {
	char tbuf[256];

	if (current_sess) {
		snprintf(tbuf, sizeof tbuf, "CYCLE %s", chan);
		handle_command(current_sess, tbuf, FALSE);
	}
}

static void menu_chan_part(GtkWidget *menu, char *chan) {
	char tbuf[256];

	if (current_sess) {
		snprintf(tbuf, sizeof tbuf, "part %s", chan);
		handle_command(current_sess, tbuf, FALSE);
	}
}

static void menu_chan_join(GtkWidget *menu, char *chan) {
	char tbuf[256];

	if (current_sess) {
		snprintf(tbuf, sizeof tbuf, "join %s", chan);
		handle_command(current_sess, tbuf, FALSE);
	}
}

void menu_chanmenu(struct session *sess, GdkEventButton * event, char *chan) {
	GtkWidget *menu;
	int is_joined = FALSE;

	if (find_channel(sess->server, chan))
		is_joined = TRUE;

	if (str_copy)
		free(str_copy);
	str_copy = strdup(chan);

	menu = gtk_menu_new();

	menu_quick_item(0, chan, menu, XCMENU_SHADED, str_copy, 0);
	menu_quick_item(0, 0, menu, XCMENU_SHADED, str_copy, 0);

	if (!is_joined)
		menu_quick_item_with_callback(menu_chan_join, _("Join Channel"), menu, str_copy);
	else {
		menu_quick_item_with_callback(menu_chan_part, _("Part Channel"), menu, str_copy);
		menu_quick_item_with_callback(menu_chan_cycle, _("Cycle Channel"), menu, str_copy);
	}

	menu_add_plugin_items(menu, "\x5$CHAN", str_copy);
	menu_popup(menu, event, NULL);
}

static void menu_open_server_list(GtkWidget *wid, gpointer none) {
	fe_serverlist_open(current_sess);
}

static void menu_settings(GtkWidget *wid, gpointer none) {
	extern void setup_open(void);
	setup_open();
}

static void menu_usermenu(void) {
	editlist_gui_open(NULL, NULL, usermenu_list, _("XChat: User menu"),
			  "usermenu", "usermenu.conf", 0);
}

static void usermenu_create(GtkWidget *menu) {
	menu_create(menu, usermenu_list, "", FALSE);
	menu_quick_item(0, 0, menu, XCMENU_SHADED, 0, 0);	/* sep */
	menu_quick_item_with_callback(menu_usermenu, _("Edit This Menu..."), menu, 0);
}

static void usermenu_destroy(GtkWidget *menu) {
	GList *items = ((GtkMenuShell *) menu)->children;
	GList *next;

	while (items) {
		next = items->next;
		gtk_widget_destroy(items->data);
		items = next;
	}
}

void usermenu_update(void) {
	int done_main = FALSE;
	GSList *list = sess_list;
	session *sess;
	GtkWidget *menu;

	while (list) {
		sess = list->data;
		menu = sess->gui->menu_item[MENU_ID_USERMENU];
		if (sess->gui->is_tab) {
			if (!done_main && menu) {
				usermenu_destroy(menu);
				usermenu_create(menu);
				done_main = TRUE;
			}
		} else if (menu) {
			usermenu_destroy(menu);
			usermenu_create(menu);
		}
		list = list->next;
	}
}

#endif

/* XXX, window_current->session */

static void menu_newwindow_window(GtkWidget *wid, gpointer none) {
	int old = new_window_in_tab_config;

	new_window_in_tab_config = 0;
	window_new(NULL, window_current->session, 0);
	new_window_in_tab_config = old;
}

static void menu_newwindow_tab(GtkWidget *wid, gpointer none) {
	int old = new_window_in_tab_config;

	new_window_in_tab_config = 1;
	window_new(NULL, window_current->session, 0);
	new_window_in_tab_config = old;
}

static void menu_detach(GtkWidget *wid, gpointer none) {
	mg_detach(window_current, 0);
}

static void menu_close(GtkWidget *wid, gpointer none) {
	mg_close_sess(window_current);
}

static void menu_quit(GtkWidget *wid, gpointer none) {
	mg_open_quit_dialog(FALSE);
}

#if 0

static void menu_search() {
	search_open(current_sess);
}
#endif

static void menu_resetmarker(GtkWidget *wid, gpointer none) {
	gtk_xtext_reset_marker_pos(GTK_XTEXT(gtk_private_ui(window_current)->xtext));
}

static void menu_flushbuffer(GtkWidget *wid, gpointer none) {
	gtk_xtext_clear(gtk_private(window_current)->buffer);
}

#if 0

static void menu_disconnect(GtkWidget *wid, gpointer none) {
	handle_command(current_sess, "DISCON", FALSE);
}

static void menu_reconnect(GtkWidget *wid, gpointer none) {
	if (current_sess->server->hostname[0])
		handle_command(current_sess, "RECONNECT", FALSE);
	else
		fe_serverlist_open(current_sess);
}

static void menu_join_cb(GtkWidget *dialog, gint response, GtkEntry * entry) {
	switch (response) {
	case GTK_RESPONSE_ACCEPT:
		menu_chan_join(NULL, entry->text);
		break;

	case GTK_RESPONSE_HELP:
		chanlist_opengui(current_sess->server, TRUE);
		break;
	}

	gtk_widget_destroy(dialog);
}

static void menu_join_entry_cb(GtkWidget *entry, GtkDialog * dialog) {
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}

static void menu_join(GtkWidget *wid, gpointer none) {
	GtkWidget *hbox, *dialog, *entry, *label;

	dialog = gtk_dialog_new_with_buttons(_("Join Channel"),
					     GTK_WINDOW(parent_window), 0,
					     _("Retrieve channel list..."), GTK_RESPONSE_HELP,
					     GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					     GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_box_set_homogeneous(GTK_BOX(GTK_DIALOG(dialog)->vbox), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
	hbox = gtk_hbox_new(TRUE, 0);

	entry = gtk_entry_new();
	GTK_ENTRY(entry)->editable = 0;	/* avoid auto-selection */
	gtk_entry_set_text(GTK_ENTRY(entry), "#");
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(menu_join_entry_cb), dialog);
	gtk_box_pack_end(GTK_BOX(hbox), entry, 0, 0, 0);

	label = gtk_label_new(_("Enter Channel to Join:"));
	gtk_box_pack_end(GTK_BOX(hbox), label, 0, 0, 0);

	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(menu_join_cb), entry);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);

	gtk_widget_show_all(dialog);

	gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
	gtk_editable_set_position(GTK_EDITABLE(entry), 1);
}

static void menu_away(GtkCheckMenuItem * item, gpointer none) {
	handle_command(current_sess, item->active ? "away" : "back", FALSE);
}

static void menu_chanlist(GtkWidget *wid, gpointer none) {
	chanlist_opengui(current_sess->server, FALSE);
}

static void menu_banlist(GtkWidget *wid, gpointer none) {
	banlist_opengui(current_sess);
}

static void menu_rpopup(void) {
	editlist_gui_open(_("Text"), _("Replace with"), replace_list, _("XChat: Replace"),
			  "replace", "replace.conf", 0);
}

static void menu_urlhandlers(void) {
	editlist_gui_open(NULL, NULL, urlhandler_list, _("XChat: URL Handlers"), "urlhandlers",
			  "urlhandlers.conf", url_help);
}

static void menu_evtpopup(void) {
	pevent_dialog_show();
}

static void menu_dcc_win(GtkWidget *wid, gpointer none) {
	fe_dcc_open_recv_win(FALSE);
	fe_dcc_open_send_win(FALSE);
}

static void menu_dcc_chat_win(GtkWidget *wid, gpointer none) {
	fe_dcc_open_chat_win(FALSE);
}

#endif

void menu_change_layout(void) {
	if (tab_layout_config == 0) {
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TABS, 1);
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TREE, 0);
		mg_change_layout(0);
	} else {
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TABS, 0);
		menu_setting_foreach(NULL, MENU_ID_LAYOUT_TREE, 1);
		mg_change_layout(2);
	}
}

static void menu_layout_cb(GtkWidget *item, gpointer none) {
	tab_layout_config = 2;
	if (GTK_CHECK_MENU_ITEM(item)->active)
		tab_layout_config = 0;

	menu_change_layout();
}

static GdkPixbuf *pix_book = NULL;	/* XXX */

static struct mymenu mymenu[] = {
	{N_("_ekg2"), 0, 0, M_NEWMENU, 0, 0, 1},
#define menu_open_server_list NULL
#define menu_loadplugin NULL

		{N_("Session Li_st..."), menu_open_server_list, (char *)&pix_book, M_MENUPIX, 0, 0, 1, GDK_s},
		{0, 0, 0, M_SEP, 0, 0, 0},

		{N_("_New"), 0, GTK_STOCK_NEW, M_MENUSUB, 0, 0, 1},
			{N_("Window in Tab..."), menu_newwindow_tab, 0, M_MENUITEM, 0, 0, 1, GDK_t},
			{N_("Window in new window..."), menu_newwindow_window, 0, M_MENUITEM, 0, 0, 1},
			{0, 0, 0, M_END, 0, 0, 0},
		{0, 0, 0, M_SEP, 0, 0, 0},
		{N_("_Load Plugin or Script..."), menu_loadplugin, GTK_STOCK_REVERT_TO_SAVED, M_MENUSTOCK, 0, 0, 1},

		{0, 0, 0, M_SEP, 0, 0, 0},	/* 9 */
#define DETACH_OFFSET (10)
		{0, menu_detach, GTK_STOCK_REDO, M_MENUSTOCK, 0, 0, 1, GDK_I},	/* 10 */
#define CLOSE_OFFSET (11)
		{0, menu_close, GTK_STOCK_CLOSE, M_MENUSTOCK, 0, 0, 1, GDK_w},
		{0, 0, 0, M_SEP, 0, 0, 0},
		{N_("_Quit"), menu_quit, GTK_STOCK_QUIT, M_MENUSTOCK, 0, 0, 1, GDK_q},	/* 13 */
	{N_("_View"), 0, 0, M_NEWMENU, 0, 0, 1},
#if 0
#define MENUBAR_OFFSET (17)
		{N_("_Menu Bar"), menu_bar_toggle_cb, 0, M_MENUTOG, MENU_ID_MENUBAR, 0, 1, GDK_F9},
		{N_("_Topic Bar"), menu_topicbar_toggle, 0, M_MENUTOG, MENU_ID_TOPICBAR, 0, 1},
		{N_("_User List"), menu_userlist_toggle, 0, M_MENUTOG, MENU_ID_USERLIST, 0, 1, GDK_F7},
		{N_("U_serlist Buttons"), menu_ulbuttons_toggle, 0, M_MENUTOG, MENU_ID_ULBUTTONS, 0, 1},
		{N_("M_ode Buttons"), menu_cmbuttons_toggle, 0, M_MENUTOG, MENU_ID_MODEBUTTONS, 0, 1},
		{0, 0, 0, M_SEP, 0, 0, 0},
#endif
		{N_("_Channel Switcher"), 0, 0, M_MENUSUB, 0, 0, 1},	/* 15 */
#define TABS_OFFSET (16)
			{N_("_Tabs"), menu_layout_cb, 0, M_MENURADIO, MENU_ID_LAYOUT_TABS, 0, 1},
			{N_("T_ree"), 0, 0, M_MENURADIO, MENU_ID_LAYOUT_TREE, 0, 1},
			{0, 0, 0, M_END, 0, 0, 0},
#if 0
	{N_("_Server"), 0, 0, M_NEWMENU, 0, 0, 1},
		{N_("_Disconnect"), menu_disconnect, GTK_STOCK_DISCONNECT, M_MENUSTOCK, MENU_ID_DISCONNECT, 0, 1},
		{N_("_Reconnect"), menu_reconnect, GTK_STOCK_CONNECT, M_MENUSTOCK, MENU_ID_RECONNECT, 0, 1},
		{N_("Join Channel..."), menu_join, GTK_STOCK_JUMP_TO, M_MENUSTOCK, MENU_ID_JOIN, 0, 1},
		{0, 0, 0, M_SEP, 0, 0, 0},
#define AWAY_OFFSET (38)
		{N_("Marked Away"), menu_away, 0, M_MENUTOG, MENU_ID_AWAY, 0, 1, GDK_a},

		{N_("_Usermenu"), 0, 0, M_NEWMENU, MENU_ID_USERMENU, 0, 1},	/* 39 */

	{N_("S_ettings"), 0, 0, M_NEWMENU, 0, 0, 1},
		{N_("_Preferences"), menu_settings, GTK_STOCK_PREFERENCES, M_MENUSTOCK, 0, 0, 1},

		{N_("Advanced"), 0, GTK_STOCK_JUSTIFY_LEFT, M_MENUSUB, 0, 0, 1},
			{N_("Auto Replace..."), menu_rpopup, 0, M_MENUITEM, 0, 0, 1},
			{N_("CTCP Replies..."), menu_ctcpguiopen, 0, M_MENUITEM, 0, 0, 1},
			{N_("Text Events..."), menu_evtpopup, 0, M_MENUITEM, 0, 0, 1},
			{N_("URL Handlers..."), menu_urlhandlers, 0, M_MENUITEM, 0, 0, 1},
			{N_("Userlist Buttons..."), menu_ulbuttons, 0, M_MENUITEM, 0, 0, 1},
			{0, 0, 0, M_END, 0, 0, 0},	/* 52 */
#endif
	{N_("_Window"), 0, 0, M_NEWMENU, 0, 0, 1},
#if 0
		{N_("Ban List..."), menu_banlist, 0, M_MENUITEM, 0, 0, 1},
		{N_("Channel List..."), menu_chanlist, 0, M_MENUITEM, 0, 0, 1},
		{N_("Character Chart..."), ascii_open, 0, M_MENUITEM, 0, 0, 1},
		{N_("Direct Chat..."), menu_dcc_chat_win, 0, M_MENUITEM, 0, 0, 1},
		{N_("File Transfers..."), menu_dcc_win, 0, M_MENUITEM, 0, 0, 1},
		{N_("Ignore List..."), ignore_gui_open, 0, M_MENUITEM, 0, 0, 1},
		{N_("Notify List..."), notify_opengui, 0, M_MENUITEM, 0, 0, 1},
		{N_("Plugins and Scripts..."), menu_pluginlist, 0, M_MENUITEM, 0, 0, 1},
#endif
		{0, 0, 0, M_SEP, 0, 0, 0},
		{N_("Reset Marker Line"), menu_resetmarker, 0, M_MENUITEM, 0, 0, 1, GDK_m},
		{N_("C_lear Text"), menu_flushbuffer, GTK_STOCK_CLEAR, M_MENUSTOCK, 0, 0, 1, GDK_l},

#define menu_search NULL

#define SEARCH_OFFSET 20	/* ? */
		{N_("Search Text..."), menu_search, GTK_STOCK_FIND, M_MENUSTOCK, 0, 0, 1, GDK_f},
#if 0
#endif
	{N_("_Help"), 0, 0, M_NEWMENU, 0, 0, 1},	/* 69 */
		{N_("_About"), menu_about, GTK_STOCK_ABOUT, M_MENUSTOCK, 0, 0, 1},
	{0, 0, 0, M_END, 0, 0, 0},
};

GtkWidget *create_icon_menu(char *labeltext, void *stock_name, int is_stock) {
	GtkWidget *item, *img;

	if (is_stock)
		img = gtk_image_new_from_stock(stock_name, GTK_ICON_SIZE_MENU);
	else
		img = gtk_image_new_from_pixbuf(*((GdkPixbuf **)stock_name));
	item = gtk_image_menu_item_new_with_mnemonic(labeltext);
	gtk_image_menu_item_set_image((GtkImageMenuItem *) item, img);
	gtk_widget_show(img);

	return item;
}

#if GTK_CHECK_VERSION(2,4,0)

/* Override the default GTK2.4 handler, which would make menu
   bindings not work when the menu-bar is hidden. */
static gboolean menu_canacaccel(GtkWidget *widget, guint signal_id, gpointer user_data) {
	/* GTK2.2 behaviour */
	return GTK_WIDGET_IS_SENSITIVE(widget);
}

#endif

#if 0

/* === STUFF FOR /MENU === */

static GtkMenuItem *
menu_find_item(GtkWidget *menu, char *name)
{
	GList *items = ((GtkMenuShell *) menu)->children;
	GtkMenuItem *item;
	GtkWidget *child;
	const char *labeltext;

	while (items) {
		item = items->data;
		child = GTK_BIN(item)->child;
		if (child) {	/* separators arn't labels, skip them */
			labeltext = g_object_get_data(G_OBJECT(item), "name");
			if (!labeltext)
				labeltext = gtk_label_get_text(GTK_LABEL(child));
			if (!menu_streq(labeltext, name, 1))
				return item;
		} else if (name == NULL) {
			return item;
		}
		items = items->next;
	}

	return NULL;
}

static GtkWidget *
menu_find_path(GtkWidget *menu, char *path)
{
	GtkMenuItem *item;
	char *s;
	char name[128];
	int len;

	/* grab the next part of the path */
	s = strchr(path, '/');
	len = s - path;
	if (!s)
		len = strlen(path);
	len = MIN(len, sizeof(name) - 1);
	memcpy(name, path, len);
	name[len] = 0;

	item = menu_find_item(menu, name);
	if (!item)
		return NULL;

	menu = gtk_menu_item_get_submenu(item);
	if (!menu)
		return NULL;

	path += len;
	if (*path == 0)
		return menu;

	return menu_find_path(menu, path + 1);
}

static GtkWidget *
menu_find(GtkWidget *menu, char *path, char *label)
{
	GtkWidget *item = NULL;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu)
		item = (GtkWidget *)menu_find_item(menu, label);
	return item;
}

static void
menu_foreach_gui(menu_entry * me, void (*callback) (GtkWidget *, menu_entry *, char *))
{
	GSList *list = sess_list;
	int tabdone = FALSE;
	session *sess;

	if (!me->is_main)
		return;		/* not main menu */

	while (list) {
		sess = list->data;
		/* do it only once for tab sessions, since they share a GUI */
		if (!sess->gui->is_tab || !tabdone) {
			callback(sess->gui->menu, me, NULL);
			if (sess->gui->is_tab)
				tabdone = TRUE;
		}
		list = list->next;
	}
}

static void
menu_update_cb(GtkWidget *menu, menu_entry * me, char *target)
{
	GtkWidget *item;

	item = menu_find(menu, me->path, me->label);
	if (item) {
		gtk_widget_set_sensitive(item, me->enable);
		/* must do it without triggering the callback */
		if (GTK_IS_CHECK_MENU_ITEM(item))
			GTK_CHECK_MENU_ITEM(item)->active = me->state;
	}
}

/* radio state changed via mouse click */
static void
menu_radio_cb(GtkCheckMenuItem * item, menu_entry * me)
{
	me->state = 0;
	if (item->active)
		me->state = 1;

	/* update the state, incase this was changed via right-click. */
	/* This will update all other windows and menu bars */
	menu_foreach_gui(me, menu_update_cb);

	if (me->state && me->cmd)
		handle_command(current_sess, me->cmd, FALSE);
}

/* toggle state changed via mouse click */
static void
menu_toggle_cb(GtkCheckMenuItem * item, menu_entry * me)
{
	me->state = 0;
	if (item->active)
		me->state = 1;

	/* update the state, incase this was changed via right-click. */
	/* This will update all other windows and menu bars */
	menu_foreach_gui(me, menu_update_cb);

	if (me->state)
		handle_command(current_sess, me->cmd, FALSE);
	else
		handle_command(current_sess, me->ucmd, FALSE);
}

static GtkWidget *
menu_radio_item(char *label, GtkWidget *menu, void *callback, void *userdata,
		int state, char *groupname)
{
	GtkWidget *item;
	GtkMenuItem *parent;
	GSList *grouplist = NULL;

	parent = menu_find_item(menu, groupname);
	if (parent)
		grouplist = gtk_radio_menu_item_get_group((GtkRadioMenuItem *) parent);

	item = gtk_radio_menu_item_new_with_label(grouplist, label);
	gtk_check_menu_item_set_active((GtkCheckMenuItem *) item, state);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show(item);

	return item;
}

static void
menu_reorder(GtkMenu * menu, GtkWidget *item, int pos)
{
	if (pos == 0xffff)	/* outbound.c uses this default */
		return;

	if (pos < 0)		/* position offset from end/bottom */
		gtk_menu_reorder_child(menu, item,
				       (g_list_length(GTK_MENU_SHELL(menu)->children) + pos) - 1);
	else
		gtk_menu_reorder_child(menu, item, pos);
}

static GtkWidget *
menu_add_radio(GtkWidget *menu, menu_entry * me)
{
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		item = menu_radio_item(me->label, menu, menu_radio_cb, me, me->state, me->group);
		menu_reorder(GTK_MENU(menu), item, me->pos);
	}
	return item;
}

static GtkWidget *
menu_add_toggle(GtkWidget *menu, menu_entry * me)
{
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		item = menu_toggle_item(me->label, menu, menu_toggle_cb, me, me->state);
		menu_reorder(GTK_MENU(menu), item, me->pos);
	}
	return item;
}

static GtkWidget *
menu_add_item(GtkWidget *menu, menu_entry * me, char *target)
{
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		item = menu_quick_item(me->cmd, me->label, menu,
				       me->
				       markup ? XCMENU_MARKUP | XCMENU_MNEMONIC : XCMENU_MNEMONIC,
				       target, me->icon);
		menu_reorder(GTK_MENU(menu), item, me->pos);
	}
	return item;
}

static GtkWidget *menu_add_sub(GtkWidget *menu, menu_entry *me) {
	GtkWidget *item = NULL;
	char *path = me->path + me->root_offset;
	int pos;

	if (path[0] != 0)
		menu = menu_find_path(menu, path);
	if (menu) {
		pos = me->pos;
		if (pos < 0)	/* position offset from end/bottom */
			pos = g_list_length(GTK_MENU_SHELL(menu)->children) + pos;
		menu_quick_sub(me->label, menu, &item,
			       me->markup ? XCMENU_MARKUP | XCMENU_MNEMONIC : XCMENU_MNEMONIC,
			       pos);
	}
	return item;
}

static void menu_del_cb(GtkWidget *menu, menu_entry * me, char *target) {
	GtkWidget *item = menu_find(menu, me->path + me->root_offset, me->label);
	if (item)
		gtk_widget_destroy(item);
}

static void menu_add_cb(GtkWidget *menu, menu_entry * me, char *target) {
	GtkWidget *item;
	GtkAccelGroup *accel_group;

	if (me->group)		/* have a group name? Must be a radio item */
		item = menu_add_radio(menu, me);
	else if (me->ucmd)	/* have unselect-cmd? Must be a toggle item */
		item = menu_add_toggle(menu, me);
	else if (me->cmd || !me->label)	/* label=NULL for separators */
		item = menu_add_item(menu, me, target);
	else
		item = menu_add_sub(menu, me);

	if (item) {
		gtk_widget_set_sensitive(item, me->enable);
		if (me->key) {
			accel_group = g_object_get_data(G_OBJECT(menu), "accel");
			if (accel_group)	/* popup menus don't have them */
				gtk_widget_add_accelerator(item, "activate", accel_group, me->key,
							   me->modifier, GTK_ACCEL_VISIBLE);
		}
	}
}

char *fe_menu_add(menu_entry *me) {
	char *text;

	menu_foreach_gui(me, menu_add_cb);

	if (!me->markup)
		return NULL;

	if (!pango_parse_markup(me->label, -1, 0, NULL, &text, NULL, NULL))
		return NULL;

	/* return the label with markup stripped */
	return text;
}

void fe_menu_del(menu_entry *me) {
	menu_foreach_gui(me, menu_del_cb);
}

void fe_menu_update(menu_entry * me) {
	menu_foreach_gui(me, menu_update_cb);
}

#endif

/* used to add custom menus to the right-click menu */

static void menu_add_plugin_mainmenu_items(GtkWidget *menu) {
#if 0
	GSList *list;
	menu_entry *me;

	list = menu_list;	/* outbound.c */
	while (list) {
		me = list->data;
		if (me->is_main)
			menu_add_cb(menu, me, NULL);
		list = list->next;
	}
#endif
}

#if 0

void
menu_add_plugin_items(GtkWidget *menu, char *root, char *target)
{
	GSList *list;
	menu_entry *me;

	list = menu_list;	/* outbound.c */
	while (list) {
		me = list->data;
		if (!me->is_main && !strncmp(me->path, root + 1, root[0]))
			menu_add_cb(menu, me, target);
		list = list->next;
	}
}

/* === END STUFF FOR /MENU === */

#endif

GtkWidget *menu_create_main(void *accel_group, int bar, int away, int toplevel, GtkWidget **menu_widgets) {
	int i = 0;
	GtkWidget *item;
	GtkWidget *menu = 0;
	GtkWidget *menu_item = 0;
	GtkWidget *menu_bar;
	GtkWidget *usermenu = 0;
	GtkWidget *submenu = 0;
	int close_mask = GDK_CONTROL_MASK;
	int away_mask = GDK_MOD1_MASK;
	char *key_theme = NULL;
	GtkSettings *settings;
	GSList *group = NULL;

	if (bar)
		menu_bar = gtk_menu_bar_new();
	else
		menu_bar = gtk_menu_new();

	/* /MENU needs to know this later */
	g_object_set_data(G_OBJECT(menu_bar), "accel", accel_group);

#if GTK_CHECK_VERSION(2,4,0)
	g_signal_connect(G_OBJECT(menu_bar), "can-activate-accel", G_CALLBACK(menu_canacaccel), 0);
#endif

#if 0
	/* set the initial state of toggles */
	mymenu[MENUBAR_OFFSET].state = !prefs.hidemenu;
	mymenu[MENUBAR_OFFSET + 1].state = prefs.topicbar;
	mymenu[MENUBAR_OFFSET + 2].state = !prefs.hideuserlist;
	mymenu[MENUBAR_OFFSET + 3].state = prefs.userlistbuttons;
	mymenu[MENUBAR_OFFSET + 4].state = prefs.chanmodebuttons;

	mymenu[AWAY_OFFSET].state = away;
#endif

	switch (tab_layout_config) {
		case 0:
			mymenu[TABS_OFFSET].state = 1;
			mymenu[TABS_OFFSET + 1].state = 0;
			break;
		default:
			mymenu[TABS_OFFSET].state = 0;
			mymenu[TABS_OFFSET + 1].state = 1;
	}

	/* change Close binding to ctrl-shift-w when using emacs keys */
	settings = gtk_widget_get_settings(menu_bar);
	if (settings) {
		g_object_get(settings, "gtk-key-theme-name", &key_theme, NULL);
		if (key_theme) {
			if (!xstrcasecmp(key_theme, "Emacs")) {
				close_mask = GDK_SHIFT_MASK | GDK_CONTROL_MASK;
				mymenu[SEARCH_OFFSET].key = 0;
			}
			g_free(key_theme);
		}
	}

	/* Away binding to ctrl-alt-a if the _Help menu conflicts (FR/PT/IT) */
	{
		char *help = _("_Help");
		char *under = strchr(help, '_');
		if (under && (under[1] == 'a' || under[1] == 'A'))
			away_mask = GDK_MOD1_MASK | GDK_CONTROL_MASK;
	}

	if (!toplevel) {
		mymenu[DETACH_OFFSET].text = N_("_Detach Tab");
		mymenu[CLOSE_OFFSET].text = N_("_Close Tab");
	} else {
		mymenu[DETACH_OFFSET].text = N_("_Attach Window");
		mymenu[CLOSE_OFFSET].text = N_("_Close Window");
	}

	while (1) {
		item = NULL;
#if 0
		if (mymenu[i].id == MENU_ID_USERMENU && !prefs.gui_usermenu) {
			i++;
			continue;
		}
#endif
		switch (mymenu[i].type) {
		case M_NEWMENU:
			if (menu)
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
			item = menu = gtk_menu_new();
#if 0
			if (mymenu[i].id == MENU_ID_USERMENU)
				usermenu = menu;
#endif
			menu_item = gtk_menu_item_new_with_mnemonic(_(mymenu[i].text));
			/* record the English name for /menu */
			g_object_set_data(G_OBJECT(menu_item), "name", mymenu[i].text);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item);
			gtk_widget_show(menu_item);
			break;

		case M_MENUPIX:
			item = create_icon_menu(_(mymenu[i].text), mymenu[i].image, FALSE);
			goto normalitem;

		case M_MENUSTOCK:
			item = create_icon_menu(_(mymenu[i].text), mymenu[i].image, TRUE);
			goto normalitem;

		case M_MENUITEM:
			item = gtk_menu_item_new_with_mnemonic(_(mymenu[i].text));
		      normalitem:
			if (mymenu[i].key != 0)
				gtk_widget_add_accelerator(item, "activate", accel_group,
							   mymenu[i].key,
							   mymenu[i].key == GDK_F1 ? 0 :
							   mymenu[i].key == GDK_w ? close_mask :
							   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
			if (mymenu[i].callback)
				g_signal_connect(G_OBJECT(item), "activate",
						 G_CALLBACK(mymenu[i].callback), 0);
			if (submenu)
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
			else
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			break;

		case M_MENUTOG:
			item = gtk_check_menu_item_new_with_mnemonic(_(mymenu[i].text));
		      togitem:
			/* must avoid callback for Radio buttons */
			GTK_CHECK_MENU_ITEM(item)->active = mymenu[i].state;
			/*gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
			   mymenu[i].state); */
#if 0
			if (mymenu[i].key != 0)
				gtk_widget_add_accelerator(item, "activate", accel_group,
							   mymenu[i].key,
							   mymenu[i].id ==
							   MENU_ID_AWAY ? away_mask :
							   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
#endif
			if (mymenu[i].callback)
				g_signal_connect(G_OBJECT(item), "toggled",
						 G_CALLBACK(mymenu[i].callback), 0);
			if (submenu)
				gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
			else
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			gtk_widget_set_sensitive(item, mymenu[i].sensitive);
			break;
		case M_MENURADIO:
			item = gtk_radio_menu_item_new_with_mnemonic(group, _(mymenu[i].text));
			group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
			goto togitem;

		case M_SEP:
			item = gtk_menu_item_new();
			gtk_widget_set_sensitive(item, FALSE);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			break;

		case M_MENUSUB:
			group = NULL;
			submenu = gtk_menu_new();
			item = create_icon_menu(_(mymenu[i].text), mymenu[i].image, TRUE);
			/* record the English name for /menu */
			g_object_set_data(G_OBJECT(item), "name", mymenu[i].text);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_widget_show(item);
			break;

		/*case M_END: */ default:
			if (!submenu) {
				if (menu) {
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
					menu_add_plugin_mainmenu_items(menu_bar);
				}
#if 0
				if (usermenu)
					usermenu_create(usermenu);
#endif
				return (menu_bar);
			}
			submenu = NULL;
		}

		/* record this GtkWidget * so it's state might be changed later */
		if (mymenu[i].id != 0 && menu_widgets)
			/* this ends up in sess->gui->menu_item[MENU_ID_XXX] */
			menu_widgets[mymenu[i].id] = item;
		i++;
	}
}

/* usuniete itemy z menu:
 * 	rawlog [fajne, ale trudne do realizacji, my mamy osobne okienko debug
 *
	{N_("Save Text..."), menu_savebuffer, GTK_STOCK_SAVE, M_MENUSTOCK, 0, 0, 1},
 */

