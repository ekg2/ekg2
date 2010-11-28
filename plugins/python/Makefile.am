EXTRA_DIST = commands-en.txt commands-pl.txt

lib_LTLIBRARIES = python.la
python_la_SOURCES = python-config.c python-config.h \
		    python-user.c python-user.h \
		    python-session.c python-session.h \
		    python-plugin.c python-plugin.h \
		    python-window.c python-window.h \
		    python-ekg.c python-ekg.h \
		    python.c python.h
python_la_LDFLAGS = -module -avoid-version
python_la_LIBADD = $(PYTHON_LIBS)
python_la_CFLAGS = $(PYTHON_CFLAGS) $(AM_CFLAGS)
# TODO: add $(C_FVISIBILITY) to *_CFLAGS or comment why this plugin should not be using it.

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/python/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/python/
