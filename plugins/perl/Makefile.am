EXTRA_DIST = 

lib_LTLIBRARIES = perl.la
perl_la_SOURCES = perl_ekg.c perl_ekg.h perl_core.c perl_core.h perl_bless.c perl_bless.h
perl_la_LDFLAGS = -module -avoid-version
perl_la_LIBADD = $(PERL_LIBS)
perl_la_CPPFLAGS = $(PERL_CFLAGS) $(LTDLINCL)

libdir = $(plugindir)
