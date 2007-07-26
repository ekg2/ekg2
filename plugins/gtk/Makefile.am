EXTRA_DIST = chanview-tabs.c chanview-tree.c

lib_LTLIBRARIES = gtk.la
gtk_la_SOURCES = main.c main.h maingui.c xtext.c xtext.h gtkutil.c gtkutil.h palette.c palette.h menu.c menu.h chanview.c chanview.h bindings.c bindings.h completion.c completion.h userlistgui.c userlistgui.h

gtk_la_LDFLAGS = -module -avoid-version
gtk_la_LIBADD = $(GTK_LIBS)
gtk_la_CPPFLAGS = $(C_FVISIBILITY) $(GTK_CFLAGS) 

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg2\" 

