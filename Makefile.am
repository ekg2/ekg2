EXTRA_DIST = ChangeLog
SUBDIRS = plugins compat ekg docs
ACLOCAL_AMFLAGS = -I m4
INCLUDE += $(top_builddir)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

