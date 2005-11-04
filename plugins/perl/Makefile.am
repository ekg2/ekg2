EXTRA_DIST = common irc

lib_LTLIBRARIES = perl.la
perl_la_SOURCES = perl_ekg.c perl_ekg.h perl_core.c perl_core.h perl_bless.c perl_bless.h
perl_la_LDFLAGS = -module -avoid-version
perl_la_LIBADD = $(PERL_LIBS)
perl_la_CPPFLAGS = $(PERL_CFLAGS) 

libdir = $(plugindir)

perl_dirs = common irc
SUBDIRS   = SCRIPTS 
# SUBDIRS   = SCRIPTS $(perl_dirs)

all-local:
	for dir in $(perl_dirs); do \
	 cd $$dir && \
	 if [ ! -f Makefile ]; then \
	   $(PERL) Makefile.PL; \
	 fi && \
	 $(MAKE) CC="$(CC)" CCFLAGS="$(PERL_CFLAGS) $(CFLAGS)" && \
	 cd ..; \
	done

clean-generic:
	for dir in $(perl_dirs); do \
	 cd $$dir; \
	 $(MAKE) clean; \
	 cd ..; \
	done


install-exec-local:
	for dir in $(perl_dirs); do \
	 cd $$dir && $(MAKE) install && cd ..; \
	done
