EXTRA_DIST = sniff_dns.inc sniff_gg.inc sniff_rivchat.inc

lib_LTLIBRARIES = sniff.la
sniff_la_SOURCES = sniff.c
sniff_la_LDFLAGS = -module -avoid-version
sniff_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)
sniff_la_LIBADD = -lpcap

override libdir = $(plugindir)

