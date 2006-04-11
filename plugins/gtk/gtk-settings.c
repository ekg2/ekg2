#include <gtk/gtk.h>

GtkWidget *settings_win;         /* okno */
enum { 
	COLUMN_ALL = 0, /* PIXBUF + COLUMN_NAME */
	COLUMN_NAME,	/* NAZWA - */
	N_COLUMNS
};

void name_set_func_text (GtkTreeViewColumn *tree_column, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	gchar *name;
	gtk_tree_model_get (model, iter, COLUMN_NAME, &name, -1);
	g_object_set (GTK_CELL_RENDERER (cell), "text", name, NULL);

}

void gtk_settings_list_add(GtkTreeStore *list, char *what) {
	GtkTreeIter iter;
	gtk_tree_store_append (list, &iter, NULL);
	gtk_tree_store_set (list, &iter, COLUMN_ALL, NULL, COLUMN_NAME, what);
}

GtkWidget *gtk_settings_window(void *ptr) {
	GtkWidget *hbox, *slabel;
	GtkWidget *tree;
	GtkCellRenderer *renderer;
	GtkTreeStore *list_store;

	GtkTreeViewColumn *column;

	settings_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

        hbox = gtk_hbox_new (FALSE, 5);
        gtk_container_add (GTK_CONTAINER (settings_win), hbox);
/* LISTA... */
	list_store = gtk_tree_store_new (N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
	/* KOLUMNY... */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "Ustawienia:", renderer, "pixbuf", COLUMN_ALL, NULL);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_ALL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, name_set_func_text, NULL, NULL);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, "", renderer, "text", COLUMN_NAME, NULL);

	gtk_tree_view_column_set_visible( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_ALL), TRUE);
	gtk_tree_view_column_set_visible( gtk_tree_view_get_column (GTK_TREE_VIEW(tree), COLUMN_NAME), FALSE);
	/* ELEMENTY... */
	gtk_settings_list_add(list_store, "Pluginy");
	gtk_settings_list_add(list_store, "Skrypty");
	gtk_settings_list_add(list_store, "Formatki");
	
	gtk_box_pack_start (GTK_BOX (hbox), tree, FALSE, FALSE, 0);
	gtk_widget_set_size_request(tree, 165, 365);
/* TODO */


	
	gtk_window_set_resizable(GTK_WINDOW(settings_win), FALSE);
	gtk_widget_show_all (settings_win);
	return settings_win;
}

