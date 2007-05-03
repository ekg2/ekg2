lib_LTLIBRARIES = oss.la
oss_la_SOURCES = oss.c
oss_la_LDFLAGS = -module -avoid-version
oss_la_CPPFLAGS = $(C_FVISIBILITY)

libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/oss/

