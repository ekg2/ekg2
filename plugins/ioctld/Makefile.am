EXTRA_DIST = commands.txt

lib_LTLIBRARIES = ioctld.la
ioctld_la_SOURCES = main.c ioctld.h
ioctld_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

libexec_PROGRAMS = ioctld
ioctld_SOURCES = ioctld.c ioctld.h

INCLUDES = -DIOCTLD_PATH=\"$(libexecdir)/ioctld\" $(LTDLINCL)

install-data-am:
	mkdir -p  $(DESTDIR)$(datadir)/ekg/plugins/ioctld/
	$(INSTALL) -m 644 commands.txt $(DESTDIR)$(datadir)/ekg/plugins/ioctld/commands.txt

