lib_LTLIBRARIES = pcm.la
pcm_la_SOURCES = main.c
pcm_la_LDFLAGS = -module -avoid-version
pcm_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)

INCLUDES = -I$(top_srcdir)
override libdir = $(plugindir)
