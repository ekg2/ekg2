#ifndef __GTK_MAINGUI_H
#define __GTK_MAINGUI_H

gboolean mg_populate_userlist(window_t *sess);
void mg_populate(window_t *sess);

void fe_set_channel(window_t *sess);
void mg_changui_new(window_t *sess, gtk_window_t *res, int tab, int focus);
void fe_set_tab_color(window_t *sess, int col);
void fe_close_window(window_t *sess);
void mg_apply_setup(void);
void mg_change_layout(int type);
void mg_switch_page(int relative, int num);
void mg_detach(window_t *sess, int mode);

void mg_close_sess(window_t *sess);
void mg_open_quit_dialog(gboolean minimize_button);

void mg_changui_new(window_t *sess, gtk_window_t *res, int tab, int focus);

#endif

