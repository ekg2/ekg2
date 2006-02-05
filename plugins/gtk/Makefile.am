EXTRA_DIST = vars-pl.txt

lib_LTLIBRARIES = gtk.la
gtk_la_SOURCES = main.c gtk-sessions.c
gtk_la_LDFLAGS = -module -avoid-version
gtk_la_LIBADD = $(GTK2_LIBS)
gtk_la_CPPFLAGS = $(GTK2_CFLAGS) 

libdir = $(plugindir)
INCLUDES = -DDATADIR=\"$(datadir)/ekg2\" 

