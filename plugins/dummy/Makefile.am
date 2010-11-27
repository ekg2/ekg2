lib_LTLIBRARIES = dummy.la
dummy_la_SOURCES = main.c
dummy_la_LDFLAGS = -module -avoid-version
dummy_la_CFLAGS = $(C_FVISIBILITY) $(AM_CFLAGS)

# tutaj dodajemy wymagane biblioteki
dummy_la_LIBADD = 

# tutaj dodajemy opcje kompilatora typu -I
INCLUDES = 

override libdir = $(plugindir)

