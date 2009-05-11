EXTRA_DIST = commands-pl.txt 

lib_LTLIBRARIES = ioctld.la
ioctld_la_SOURCES = main.c ioctld.h
ioctld_la_LDFLAGS = -module -avoid-version
ioctld_la_CPPFLAGS = $(C_FVISIBILITY)
override libdir = $(plugindir)

libexec_PROGRAMS = ioctld
ioctld_SOURCES = ioctld.c ioctld.h

INCLUDES = -DIOCTLD_PATH=\"$(libexecdir)/ioctld\" 

install-data-am:
	mkdir -p  $(DESTDIR)$(pkgdatadir)/plugins/ioctld/
	$(INSTALL) -m 644 $(EXTRA_DIST) $(DESTDIR)$(pkgdatadir)/plugins/ioctld/

