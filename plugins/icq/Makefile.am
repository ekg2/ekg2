EXTRA_DIST = icq_debug.inc icq_fieldnames.inc 

ICQ_DEFS = -UDBG_CAPHTML -DDBG_CAPMTN -DDBG_CAPXTRAZ -UDBG_CAPXTRAZ_MUC -DDBG_NEWCAPS -DDBG_OSCARFT -DDBG_AIMCONTACTSEND

lib_LTLIBRARIES = icq.la
icq_la_SOURCES = icq.c icq.h icq_caps.c icq_caps.h icq_const.h icq_flap_handlers.c icq_flap_handlers.h icq_snac_handlers.c icq_snac_handlers.h icq_snac_handlers_01service.c icq_snac_handlers_02location.c icq_snac_handlers_03buddy.c icq_snac_handlers_04message.c icq_snac_handlers_09bos.c icq_snac_handlers_0Alookup.c icq_snac_handlers_0Bstatus.c icq_snac_handlers_13userlist.c icq_snac_handlers_15extension.c icq_snac_handlers_17sigon.c digest.c misc.c misc.h miscicq.h
icq_la_LDFLAGS = -module -avoid-version
icq_la_CPPFLAGS = $(C_FVISIBILITY) $(ICQ_DEFS)

override libdir = $(plugindir)
