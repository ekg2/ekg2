EXTRA_DIST = ChangeLog
SUBDIRS = libltdl plugins compat ekg docs m4 contrib
ACLOCAL_AMFLAGS = -I m4
INCLUDES = $(top_builddir) $(LTDLINCL)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

