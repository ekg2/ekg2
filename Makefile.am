EXTRA_DIST = config.rpath mkinstalldirs  m4 gettext.h
SUBDIRS = plugins compat ekg $(REMOTE_SUBDIR) docs contrib po 
ACLOCAL_AMFLAGS = -I m4
# XXX: What is the point below? This seems not to affect anything.
INCLUDES = -I$(top_srcdir) $(LTDLINCL)

libtool: $(LIBTOOL_DEPS)
