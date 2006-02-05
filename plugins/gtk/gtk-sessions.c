#include <gtk/gtk.h>

#include <ekg/plugins.h>

#include <stdio.h>
/* 
 * przyciski: Dalej	(GTK_STOCK_GO_FORWARD	gtk-go-forward)
 * 	      Wstecz 	(GTK_STOCK_GO_BACK	gtk-go-back)
 * 	      Anuluj	(GTK_STOCK_GO_CANCEL	gtk-cancel)
 * 	      Zakoncz   (GTK_STOCK_OK		gtk-ok) 
 */

GtkWidget *win;  /* okno */
GtkWidget *pbar; /* progressbar */

GtkWidget *gtk_session_new(void *ptr) {
	GtkWidget *button = NULL;
	GtkWidget *vbox, *vbox2;
	GtkWidget *hbox;
	GtkWidget *slabel; /* step label */
	list_t l;
	
	printf("gtk_session_new(%x)\n", (int) ptr);
	
	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (win), "Tworzenie nowej sesji.. [krok 1 z 4]");

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (win), vbox);

/* step depend, informacje o aktualnym kroku */
	slabel = gtk_label_new (NULL);
	gtk_label_set_text (GTK_LABEL (slabel), "Wybierz plugin pod ktorym sesja bedzie dzialac..");
	gtk_box_pack_start( GTK_BOX(vbox), slabel, TRUE, TRUE, 0);

	vbox2 = gtk_vbox_new (FALSE, 10);
	gtk_box_pack_start( GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
/* step depend. step 1st wybor plugina */
	for (l = plugins; l; l = l->next) {
		GSList *group;
		plugin_t *p = l->data;
		if (p->pclass != PLUGIN_PROTOCOL) continue;

		if (!button) {
			button = gtk_radio_button_new_with_label (NULL, p->name);
			group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		} else button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button), p->name);
		gtk_box_pack_start (GTK_BOX (vbox2), button, TRUE, TRUE, 0);
	}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start( GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
/* PRZYCISKI */
	button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	button = gtk_button_new_from_stock( GTK_STOCK_GO_FORWARD);
	gtk_container_add (GTK_CONTAINER (hbox), button);
	button = gtk_button_new_from_stock( GTK_STOCK_CANCEL);
	gtk_container_add (GTK_CONTAINER (hbox), button);
/* progressbar */
	pbar = gtk_progress_bar_new ();
	gtk_box_pack_end (GTK_BOX (vbox), pbar, FALSE, FALSE, 5);
	
	gtk_widget_set_size_request(win, 300+117, 265);
	gtk_widget_show_all (win);

	return win;
}

