lib_LTLIBRARIES = gpg.la
gpg_la_SOURCES = gpg.c
gpg_la_LDFLAGS = -module -avoid-version

gpg_la_LIBADD	= $(GPGME_LIBS)
gpg_la_CPPFLAGS = $(GPGME_CFLAGS)

override libdir = $(plugindir)

