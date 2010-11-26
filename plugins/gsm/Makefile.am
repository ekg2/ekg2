lib_LTLIBRARIES = gsm.la
gsm_la_SOURCES = main.c
gsm_la_LDFLAGS = -module -avoid-version
gsm_la_LIBADD = $(LIBGSM_LIBS)
gsm_la_CFLAGS = $(C_FVISIBILITY) $(LIBGSM_CFLAGS) $(AM_CFLAGS)

override libdir = $(plugindir)

