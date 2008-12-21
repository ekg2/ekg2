#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtksignal.h>

#include <ekg/plugins.h>

extern int ui_quit;

extern plugin_t gtk_plugin;

typedef struct {
	GtkWidget
	 *xtext, *vscrollbar, *window,	/* toplevel */
	 *topic_entry, *note_book, *main_table, *user_tree,	/* GtkTreeView */
	 *user_box,		/* userlist box */
	 *dialogbutton_box, *topicbutton_box, 
	 *topic_bar, *hpane_left, *hpane_right, *vpane_left, *vpane_right, *menu, *bar,	/* connecting progress bar */
	 *nick_box,		/* contains label to the left of input_box */
	 *nick_label, *op_xpm,	/* icon to the left of nickname */
	 *namelistinfo,		/* label above userlist */
	 *input_box;

#define MENU_ID_NUM 12
	GtkWidget *menu_item[MENU_ID_NUM + 1];	/* some items we may change state of */

	void *chanview;		/* chanview.h */

	int pane_left_size;	/*last position of the pane */
	int pane_right_size;

	guint16 is_tab;		/* is tab or toplevel? */
	guint16 ul_hidden;	/* userlist hidden? */
} gtk_window_ui_t;

typedef struct {
	gtk_window_ui_t *gui;

	void *tab;			/* (chan *) */

	/* information stored when this tab isn't front-most */
	void *user_model;	/* for filling the GtkTreeView */
	void *buffer;		/* xtext_Buffer */
	gfloat old_ul_value;	/* old userlist value (for adj) */
} gtk_window_t;


/* config */
extern int mainwindow_width_config;
extern int mainwindow_height_config;
extern int gui_pane_left_size_config;
extern int gui_tweaks_config;
extern int tab_small_config;
extern int tab_pos_config;
extern int max_auto_indent_config;
extern int thin_separator_config;

extern int show_marker_config;
extern int tint_red_config;
extern int tint_green_config;
extern int tint_blue_config;
extern int transparent_config;
extern int wordwrap_config;
extern int indent_nicks_config;
extern int show_separator_config;
extern char *font_normal_config;
extern int transparent_config;

extern int gui_ulist_pos_config;
extern int tab_pos_config;

extern int tab_layout_config;
extern int contacts_config;
extern int backlog_size_config;

extern int gui_pane_left_size_config;
extern int gui_pane_right_size_config;

extern int new_window_in_tab_config;

#define hidemenu_config 0
#define topicbar_config 1

#define mainwindow_left_config 0
#define mainwindow_top_config 0
#define newtabstofront_config 2

#define gtk_private_ui(w) (((gtk_window_t*) w->private)->gui)
#define gtk_private(w) ((gtk_window_t*) w->private)

#define gui_win_state_config 0

#define truncchans_config 20
#define tab_sort_config 1
#define tab_icons_config 0
#define style_namelistgad_config 0

#define chanmodebuttons_config -1
#define gui_quit_dialog_config -1

#define FOCUS_NEW_ALL	     1
#define FOCUS_NEW_ONLY_ASKED 2

#define paned_userlist_config 0		/* XXX xchat def: 1 */
#define style_inputbox_config 0		/* XXX xchat commented def: 1 */

extern int gtk_ui_window_switch_lock;
