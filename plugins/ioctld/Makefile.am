lib_LTLIBRARIES = ioctld.la
ioctld_la_SOURCES = main.c
ioctld_la_LDFLAGS = -module -avoid-version

libdir = $(plugindir)

libexec_PROGRAMS = ioctld
ioctld_SOURCES = ioctld.c

INCLUDES = -DIOCTLD_PATH=\"$(libexecdir)/ioctld\"
