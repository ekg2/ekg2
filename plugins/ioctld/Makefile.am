lib_LTLIBRARIES = ioctld.la
ioctld_la_SOURCES = main.c ioctld.h
ioctld_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

libexec_PROGRAMS = ioctld
ioctld_SOURCES = ioctld.c ioctld.h

INCLUDES = -DIOCTLD_PATH=\"$(libexecdir)/ioctld\"
