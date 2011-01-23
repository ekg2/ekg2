EXTRA_DIST = commands-pl.txt 

lib_LTLIBRARIES = ioctld.la
ioctld_la_SOURCES = main.c ioctld.h
ioctld_la_LDFLAGS = -module -avoid-version
ioctld_la_CFLAGS = $(C_FVISIBILITY) $(GLIB_CFLAGS) $(AM_CFLAGS)
ioctld_la_LIBADD = $(GLIB_LIBS)
override libdir = $(plugindir)

libexec_PROGRAMS = ioctld
ioctld_CPPFLAGS = -DIOCTLD_BUILD=1 $(AM_CPPFLAGS)
ioctld_SOURCES = ioctld.c ioctld.h

INCLUDES = -DIOCTLD_PATH=\"$(libexecdir)/ioctld\" -I$(top_srcdir)

install-data-am:
	mkdir -p  $(DESTDIR)$(pkgdatadir)/plugins/ioctld/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/ioctld/
