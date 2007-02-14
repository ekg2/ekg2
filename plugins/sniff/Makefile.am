lib_LTLIBRARIES = sniff.la
sniff_la_SOURCES = sniff.c
sniff_la_LDFLAGS = -module -avoid-version

# tutaj dodajemy wymagane biblioteki
sniff_la_LIBADD = -lpcap

CFLAGS = -std=gnu89 -g -Wall

libdir = $(plugindir)

