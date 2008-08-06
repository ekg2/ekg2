ICQ_DEFS = -UDBG_CAPHTML -DDBG_CAPMTN -DDBG_CAPXTRAZ -UDBG_CAPXTRAZ_MUC -DDBG_NEWCAPS -DDBG_OSCARFT -DDBG_AIMCONTACTSEND

lib_LTLIBRARIES = icq.la
icq_la_SOURCES = icq.c icq_flap_handlers.c icq_snac_handlers.c digest.c misc.c
icq_la_LDFLAGS = -module -avoid-version
icq_la_CPPFLAGS = $(C_FVISIBILITY) $(ICQ_DEFS)

override libdir = $(plugindir)
