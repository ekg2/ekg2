# TODO: inaczej
EXTRA_DIST = common/Makefile.PL common/Ekg2.pm common/Command.xs  common/Ekg2.xs  common/Plugin.xs  common/Session.xs  common/Timer.xs  common/Userlist.xs  common/Variable.xs  common/Window.xs common/module.h common/typemap\
             irc/Makefile.PL irc/Irc.pm irc/Channel.xs  irc/Irc.xs  irc/Server.xs  irc/User.xs irc/typemap irc/module.h

lib_LTLIBRARIES = perl.la
perl_la_SOURCES = perl_ekg.c perl_ekg.h perl_core.c perl_core.h perl_bless.c perl_bless.h
perl_la_LDFLAGS = -module -avoid-version
perl_la_LIBADD = $(PERL_LIBS)
perl_la_CPPFLAGS = $(PERL_CFLAGS)

libdir = $(plugindir)

perl_dirs = common irc

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
	 cd $$dir && $(MAKE) install $(perl_install_args) && cd ..; \
	done
