#include <gtk/gtk.h>

#include <ekg/plugins.h>
#include <ekg/stuff.h>

#include <stdio.h>

enum {
	EKG_SESSION_NEW_BACK = 1,	/* Wstecz 	(GTK_STOCK_GO_BACK	gtk-go-back) 	*/
	EKG_SESSION_NEW_FORWARD,	/* Dalej     	(GTK_STOCK_GO_FORWARD   gtk-go-forward) */
	EKG_SESSION_NEW_CANCEL,		/* Anuluj    	(GTK_STOCK_GO_CANCEL    gtk-cancel) 	*/
	EKG_SESSION_NEW_OK,		/* Zakoncz   	(GTK_STOCK_OK           gtk-ok)		*/
} ;

GtkWidget *win;		/* okno */
GtkWidget *slabel;	/* step label */
GtkWidget *pbar;	/* progressbar */
GtkWidget *frame;	/* frame */

GtkWidget *vbox;	/* vbox */

GtkWidget *prev_button, *next_button, *cancel_button, *done_button;

int session_add_step = 0;

extern GtkWidget *gtk_session_step(int step);

#define gtk_widget_disable(x) gtk_widget_set_sensitive (GTK_WIDGET(x), FALSE)
#define gtk_widget_enable(x) gtk_widget_set_sensitive (GTK_WIDGET(x), TRUE)

gint on_session_button_clicked(GtkWidget *widget, gpointer data) {
	int step = session_add_step;
	switch ((int) data) {
		case (EKG_SESSION_NEW_BACK): 
			if (session_add_step < 2)
				return FALSE;
			step--;
			break;
		case (EKG_SESSION_NEW_FORWARD):
			if (session_add_step > 3)
				return FALSE;
			step++;
			break;
		case (EKG_SESSION_NEW_CANCEL):
			step = -1;
			break;
		case (EKG_SESSION_NEW_OK):
			if (session_add_step != 4)
				return FALSE;
			step = 0;
			break;
		default: printf("niewlasiwe cus? (%d) %d\n", session_add_step, (int) data);
			 return FALSE;
	}
	printf("[on_session_button_clicked] prev = %d curr = %d\n", session_add_step, step); 

	gtk_session_step(step);		/* zmieniamy step */
	gtk_widget_show_all (win);	/* uaktualniamy okienko */

	if (step < 2)	gtk_widget_disable(prev_button);
	else		gtk_widget_enable(prev_button);
/* zmiana przycisku 'Dalej' na 'Zakoncz'.. lub odwrotnie.. */
	if (step == 4)	gtk_widget_hide(next_button);
	else		gtk_widget_hide(done_button);
	return TRUE;
}

/* TODO: stworzyc okienko:
 *    1. wybor plugina ktory bedzie zarzadzal sesjami.. po prostu wyswietlamy pluginy z typem PROTOCOL
 *    2. jesli plugin to:
 *       a) irc: 
 *           -> user ma wpisac nazwe serwera i swoj nickname.
 *           -> nazwa sesji domyslna: irc:%NAZWA_USERA@%NAZWA_SERWERA chyba ze istnieje wtedy dodajemy numerki...
 *       b) gg:
 *           -> user ma wpisac numerek gg i swoje haslo.
 *           -> nazwa sesji domyslna: gg:%NUMEREKUSERA jesli taka sesja istnieje to informujemy usera i czekamy az poda inny numerek.
 *       c) jabber:
 *           -> user ma podac swoj jid. i adres do serwera jesli jest rozny niz w jid.
 *           -> user moze podac resource [domyslnie: EKG2]
 *           -> nazwa sesji to jid:%JID jesli istnieje i ma taki sam resource to wtedy czekamy na inny... 
 *   3. pokazac userowi uid sesji i pozwolic mu dodac alias do sesji.
 *   4. tyle, enjoy ;)
 *   --------
 *   dodac sesje z parametrami podanymi przez usera. sprobowac nie przez command_exec_format() tylko przez natywne procedury...
 *   to nie ma byc frontend do ekg2. tylko GUI.
 */

GtkWidget *gtk_session_step(int step) {
	GtkWidget *vbox2;
	GSList *group = NULL;

	list_t l;
	char *title; /* step depend, tytul.. w formacie "Tworzenie nowej sesji.. [krok %nrkroku z 4]" */
	char *instr; /* step depend, informacje o aktualnym kroku */

	printf("[gtk_session_step] step = %d\n", step);
/* step depend... frame */
	if (frame) {
		gtk_widget_destroy(frame);
	}
	
	frame = gtk_frame_new(NULL);
	gtk_box_pack_start( GTK_BOX(vbox), frame, TRUE, TRUE, 0);

	switch(step) {
		case (0):	// ok
			/* stworzyc sesje.  */
			printf("TWORZENIE SESJI:....\n");
			/* no break */
		case (-1):	// canel.
			 /* zniszczyc wszystko */
			gtk_widget_destroy(win);
			win 	= NULL;
			frame	= NULL;
			return NULL;
		case (1):	// plugin
			instr = "Wybierz plugin pod ktorym sesja bedzie dzialac..";
			vbox2 = gtk_vbox_new (FALSE, 10);
			gtk_container_add (GTK_CONTAINER (frame), vbox2);
			for (l = plugins; l; l = l->next) {
				GtkWidget *button;
				plugin_t *p = l->data;

				if (p->pclass != PLUGIN_PROTOCOL) continue;

				button = gtk_radio_button_new_with_label (group, p->name);
				group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
				gtk_box_pack_start (GTK_BOX (vbox2), button, TRUE, TRUE, 0);
			}
			break;
		case (2): // username / password.. / server etc.
			instr = "Podaj dane potrzebne do stworzenia sesji";
			break;
		case (3):	// uid / alias
			instr = "Zmien uid / alias dla swojej sesji..";
			vbox2 = gtk_vbox_new (FALSE, 10);
			gtk_container_add (GTK_CONTAINER (frame), vbox2);
			
			{ /* TODO, zrobic jakos ladniej.... 
			   * - > UID 	[EDIT_z_uidem]
			   *  -> ALIAS	[EDIT_Z_aliasem]
			   */
				GtkWidget *label= gtk_label_new ("Uid");
				GtkWidget *edit = gtk_entry_new ();
				gtk_box_pack_start (GTK_BOX(vbox2), label, FALSE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX(vbox2), edit, FALSE, TRUE, 0);

				label = gtk_label_new ("Alias");
				edit = gtk_entry_new ();
				gtk_box_pack_start (GTK_BOX(vbox2), label, FALSE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX(vbox2), edit, FALSE, TRUE, 0);
			}
			break;
		case (4):	// autoconnect, autoreconnect, autoreconnect_time ? :>
			instr = "Po kliknieciu na zakoncz Twoja sesja zostanie utworzona.. ;)";
			break;
		default:
			return NULL;
	}
	title = saprintf("Tworzenie nowej sesji.. [krok %d z 4]", step);
	gtk_window_set_title (GTK_WINDOW (win), title);
	xfree(title);

	gtk_label_set_text(GTK_LABEL (slabel), instr);
/*	gtk_frame_set_label(GTK_FRAME (frame), instr); */
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (pbar), (1.0 / 4) * (step));
	
	session_add_step = step;
	return NULL;
}

GtkWidget *gtk_session_new_window(void *ptr) {
	GtkWidget *hbox;

	if (win) {
		/* pozwolic na tworzenie kilku sesji at one time? 
		 * moze, zrobic jakas strukturke z najwazniejszym widgetami.. TODO */
		/* poka okno */
		return win;
	}

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (win), vbox);

	slabel = gtk_label_new (NULL);
	gtk_box_pack_start( GTK_BOX(vbox), slabel, TRUE, TRUE, 0);

/* tutaj ramka.. przerzucono tworzenie do gtk_session_step() */
	frame = NULL;

/* progressbar */
	pbar = gtk_progress_bar_new ();
	gtk_box_pack_end (GTK_BOX (vbox), pbar, FALSE, FALSE, 5);

/* PRZYCISKI */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	cancel_button = gtk_button_new_from_stock( GTK_STOCK_CANCEL);
	gtk_container_add (GTK_CONTAINER (hbox), cancel_button);
	g_signal_connect (G_OBJECT (cancel_button),"clicked",G_CALLBACK (on_session_button_clicked), (void *) EKG_SESSION_NEW_CANCEL);

	prev_button = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	gtk_container_add (GTK_CONTAINER (hbox), prev_button);
	g_signal_connect (G_OBJECT (prev_button),"clicked",G_CALLBACK (on_session_button_clicked), (void *) EKG_SESSION_NEW_BACK);

	next_button = gtk_button_new_from_stock( GTK_STOCK_GO_FORWARD);
	gtk_container_add (GTK_CONTAINER (hbox), next_button);
	g_signal_connect (G_OBJECT (next_button),"clicked",G_CALLBACK (on_session_button_clicked), (void *) EKG_SESSION_NEW_FORWARD);

	done_button = gtk_button_new_from_stock( GTK_STOCK_OK);
	gtk_container_add (GTK_CONTAINER (hbox), done_button);
	g_signal_connect (G_OBJECT (done_button),"clicked",G_CALLBACK (on_session_button_clicked), (void *) EKG_SESSION_NEW_OK);

/* ... */
	gtk_widget_set_size_request(win, 300+117, 265);
	gtk_session_step(1);
	gtk_widget_show_all (win);

	gtk_widget_hide(done_button);
	gtk_widget_disable(prev_button);

	return win;
}

