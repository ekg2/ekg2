#ifndef PERL_CORE_H
#define PERL_CORE_H

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <ekg/xmalloc.h>
#define fix(s) ((s) ? (s) : "") /* xmalloc.h */

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
	    
/* syfffff irssi */

#define new_pv(a) newSVpv(fix(a), xstrlen(a))

#define is_hvref(o) \
        ((o) && SvROK(o) && SvRV(o) && (SvTYPE(SvRV(o)) == SVt_PVHV))

#define hvref(o) \
        (is_hvref(o) ? (HV *)SvRV(o) : NULL)

/* syfffff ekg2 */

#define PERL_HANDLER_HEADER(x) \
	char *fullproc, *error; \
	int perl_retcount, ret = 0;\
	SV *perl_ret;\
	if (!x) return -1;\
	fullproc = saprintf("Ekg2::Script::%s::%s", scr->name,  x);\
	{	/* tag will be closed in PERL_HANDLER_FOOTER macro */ \
		dSP;\
		ENTER;\
		SAVETMPS;\
		PUSHMARK(sp);

int perl_initialize();
int perl_finalize();

SV *create_sv_ptr(void *object);

#endif
/* zrobic to jakos ladniej... hack.*/

#undef RESTORE_ARGS
#undef PERL_HANDLER_FOOTER

#ifdef PERL_RESTORE_ARGS
#define RESTORE_ARGS(x)\
    if (change) {\
        for (i=0; i < scr_que->argc; i++) {\
                switch ( scr_que->argv_type[i] ) {\
                        case (QUERY_ARG_INT):\
				*( (int *) args[i]) = SvIV(SvRV(perlargs[i]));\
				break;\
\
                        case (QUERY_ARG_CHARP):\
/*				xfree(*(char **) args[i]);  */\
				*( (char **) args[i]) = xstrdup( SvPV_nolen(SvRV(perlargs[i])) ) ;\
				break;\
			case (QUERY_ARG_CHARPP): /* wazne, zrobic. */\
				break;\
\
                }\
        }\
    }
    
#else 
#define RESTORE_ARGS(x) ;
#endif

#define PERL_HANDLER_FOOTER()\
		PUTBACK;\
/*		perl_retcount = perl_call_sv(func, G_EVAL|G_DISCARD);*/\
		perl_retcount = perl_call_pv(fullproc, G_EVAL);\
		SPAGAIN;\
		if (SvTRUE(ERRSV)) {\
			error = SvPV(ERRSV, PL_na);\
			print("script_error", error);\
			ret = SCRIPT_HANDLE_UNBIND;\
		}\
		else if (perl_retcount > 0)\
		{\
			perl_ret = POPs;\
			ret = SvIV(perl_ret);\
		}\
		RESTORE_ARGS(0);\
/*		debug("%d %d\n", ret, perl_retcount); */\
		\
		PUTBACK;\
		FREETMPS;\
		LEAVE;\
		\
		if (ret < 0) return -1;\
		else         return ret; \
	} /* closing tag defined in PERL_HANDLER_HEADER() macro */ \
	xfree(fullproc);

