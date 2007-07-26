int key_handle_key_press (GtkWidget * wid, GdkEventKey * evt, window_t *sess);

void gtk_binding_init();

#define HISTORY_MAX 1000
extern char *gtk_history[HISTORY_MAX];
extern int gtk_history_index;
