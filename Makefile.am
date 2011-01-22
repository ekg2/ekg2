EXTRA_DIST = config.rpath mkinstalldirs  m4 gettext.h
SUBDIRS = compat ekg $(REMOTE_SUBDIR) plugins docs contrib po 
DIST_SUBDIRS = compat contrib docs ekg plugins po remote
ACLOCAL_AMFLAGS = -I m4
# XXX: What is the point below? This seems not to affect anything.
INCLUDES = -I$(top_srcdir) $(LTDLINCL)

libtool: $(LIBTOOL_DEPS)
TESTS = check_potfiles
check_SCRIPTS = check_potfiles
