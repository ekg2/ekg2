EXTRA_DIST = 

lib_LTLIBRARIES = python.la
python_la_SOURCES = python-config.c python-config.h python-session.c python-session.h python-ekg.c python-ekg.h python.c python.h
python_la_LDFLAGS = -module -avoid-version
python_la_LIBADD = $(PYTHON_LIBS)
python_la_CPPFLAGS = $(PYTHON_CFLAGS) $(LTDLINCL)

libdir = $(plugindir)
