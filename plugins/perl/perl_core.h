#ifndef PERL_CORE_H
#define PERL_CORE_H

#include <ekg/scripts.h>
#include <ekg/xmalloc.h>

/* syfffff irssi */

#define new_pv(a) newSVpv(fix(a), xstrlen(a))

#define is_hvref(o) \
        ((o) && SvROK(o) && SvRV(o) && (SvTYPE(SvRV(o)) == SVt_PVHV))


#define hvref(o) \
        (is_hvref(o) ? (HV *)SvRV(o) : NULL)


static const char *ekg_core_code =
"# NOTE: this is printed through printf()-like function,\n"
"# so no extra percent characters.\n"
"\n"
"# %%d : must be first - 1 if perl libraries are to be linked \n"
"#       statically with irssi binary, 0 if not\n"
"# %%s : must be second - use Irssi; use Irssi::Irc; etc..\n"
"package Ekg2::Core;\n"
"\n"
"use Symbol;\n"
"\n"
"sub is_static {\n"
"  return %d;\n"
"}\n"
"\n"
"sub destroy {\n"
"  eval { $_[0]->UNLOAD() if $_[0]->can('UNLOAD'); };\n"
"  Symbol::delete_package($_[0]);\n"
"}\n"
"\n"
"sub eval_data {\n"
"  my ($data, $id) = @_;\n"
"  destroy(\"Ekg2::Script::$id\");\n"
"\n"
"  my $package = \"Ekg2::Script::$id\";\n"
"  my $eval = qq{package $package; %s sub handler { $data }};\n"
"  {\n"
"      # hide our variables within this block\n"
"      my ($filename, $package, $data);\n"
"      eval $eval;\n"
"  }\n"
"  die $@ if $@;\n"
"\n"
"  my $ret;\n"
"  eval { $ret = $package->handler; };\n"
"  die $@ if $@;\n"

"  return $ret;\n"
"}\n"
"\n"
"sub eval_file {\n"
"  my ($filename, $id) = @_;\n"
"\n"
"  local *FH;\n"
"  open FH, $filename or die \"File not found: $filename\";\n"
"  local($/) = undef;\n"
"  my $data = <FH>;\n"
"  close FH;\n"
"  local($/) = \"\\n\";\n"
"\n"
"  eval_data($data, $id);\n"
"}\n"
;

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

#define fix(s) ((s) ? (s) : "") /* xmalloc.h */
	    
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
                        case (SCR_ARG_INT):\
				*( (int *) args[i]) = SvIV(SvRV(perlargs[i]));\
				break;\
\
                        case (SCR_ARG_CHARP):\
/*				xfree(*(char **) args[i]);  */\
				*( (char **) args[i]) = xstrdup( SvPV_nolen(SvRV(perlargs[i])) ) ;\
				break;\
			case (SCR_ARG_CHARPP): /* wazne, zrobic. */\
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

