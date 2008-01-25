lib_LTLIBRARIES = ruby.la
ruby_la_SOURCES = ruby_ekg.c
ruby_la_LDFLAGS = -module -avoid-version
ruby_la_LIBADD = $(RUBY_LIBS)
ruby_la_CPPFLAGS = $(C_FVISIBILITY) $(RUBY_CFLAGS)

libdir = $(plugindir)
