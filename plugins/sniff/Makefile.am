EXTRA_DIST = sniff_dns.inc sniff_gg.inc sniff_rivchat.inc

lib_LTLIBRARIES = sniff.la
sniff_la_SOURCES = sniff.c
sniff_la_LDFLAGS = -module -avoid-version
sniff_la_CPPFLAGS = $(C_FVISIBILITY)

# tutaj dodajemy wymagane biblioteki
sniff_la_LIBADD = -lpcap

CFLAGS = -std=gnu89 -g -Wall

override libdir = $(plugindir)

