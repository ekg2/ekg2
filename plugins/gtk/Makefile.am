GG_PNGS = iconssets/gg-avail.png iconssets/gg-away.png iconssets/gg-invisible.png iconssets/gg-notavail.png
EXTRA_DIST = chanview-tabs.c chanview-tree.c iconssets/COPYING $(GG_PNGS)

lib_LTLIBRARIES = gtk.la
gtk_la_SOURCES = main.c main.h maingui.c xtext.c xtext.h gtkutil.c gtkutil.h palette.c palette.h menu.c menu.h chanview.c chanview.h bindings.c bindings.h completion.c completion.h userlistgui.c userlistgui.h

gtk_la_LDFLAGS = -module -avoid-version
gtk_la_LIBADD = $(GTK_LIBS)
gtk_la_CPPFLAGS = $(C_FVISIBILITY) $(GTK_CFLAGS) 

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg2\" 

noinst_HEADERS = inline_pngs_gg.h
CLEANFILES = $(noinst_HEADERS)

inline_pngs_gg.h: $(GG_PNGS)
	gdk-pixbuf-csource --raw --build-list gg_avail iconssets/gg-avail.png gg_away iconssets/gg-away.png gg_invisble iconssets/gg-invisible.png gg_notavail iconssets/gg-notavail.png > inline_pngs_gg.h
