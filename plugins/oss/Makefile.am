lib_LTLIBRARIES = oss.la
oss_la_SOURCES = oss.c
oss_la_LDFLAGS = -module -avoid-version
oss_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)

install-data-am:
	mkdir -p $(DESTDIR)$(pkgdatadir)/plugins/oss/
