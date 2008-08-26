GG_PNGS = iconssets/gg-avail.png iconssets/gg-away.png iconssets/gg-invisible.png iconssets/gg-notavail.png
ICQ_PNGS = 
PNGS = iconssets/ffc.png iconssets/avail.png iconssets/away.png iconssets/dnd.png iconssets/xa.png iconssets/invisible.png iconssets/notavail.png iconssets/icon_error.png iconssets/icon_unknown.png
EXTRA_DIST = chanview-tabs.inc chanview-tree.inc iconssets/COPYING $(GG_PNGS) $(ICQ_PNGS) $(PNGS)

lib_LTLIBRARIES = gtk.la
gtk_la_SOURCES = main.c main.h maingui.c maingui.h xtext.c xtext.h gtkutil.c gtkutil.h palette.c palette.h menu.c menu.h chanview.c chanview.h bindings.c bindings.h completion.c completion.h userlistgui.c userlistgui.h

gtk_la_LDFLAGS = -module -avoid-version
gtk_la_LIBADD = $(GTK_LIBS)
gtk_la_CPPFLAGS = $(C_FVISIBILITY) $(GTK_CFLAGS) 

BUILT_SOURCES = inline_pngs_gg.h inline_pngs_icq.h inline_pngs.h

override libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg2\" 

CLEANFILES = inline_pngs_gg.h inline_pngs_icq.h inline_pngs.h

inline_pngs_gg.h: $(GG_PNGS)
	gdk-pixbuf-csource --raw --build-list gg_avail iconssets/gg-avail.png gg_away iconssets/gg-away.png gg_invisible iconssets/gg-invisible.png gg_notavail iconssets/gg-notavail.png > inline_pngs_gg.h

inline_pngs_icq.h: $(ICQ_PNGS)
	gdk-pixbuf-csource --raw --build-list icq_ffc iconssets/icq-ffc.png icq_avail iconssets/icq-avail.png icq_away iconssets/icq-away.png icq_dnd iconssets/icq-dnd.png icq_xa iconssets/icq-xa.png icq_invisible iconssets/icq-invisible.png icq_notavail iconssets/icq-notavail.png icq_unknown iconssets/icq-unknown.png > inline_pngs_icq.h

inline_pngs.h: $(PNGS)
	gdk-pixbuf-csource --raw --build-list ffc iconssets/ffc.png avail iconssets/avail.png away iconssets/away.png dnd iconssets/dnd.png xa iconssets/xa.png invisible iconssets/invisible.png notavail iconssets/notavail.png icon_error iconssets/icon_error.png icon_unknown iconssets/icon_unknown.png > inline_pngs.h

