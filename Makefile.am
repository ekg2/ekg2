EXTRA_DIST = m4
SUBDIRS = libltdl plugins compat ekg docs contrib po
ACLOCAL_AMFLAGS = -I m4
INCLUDES = $(top_builddir) $(LTDLINCL)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

