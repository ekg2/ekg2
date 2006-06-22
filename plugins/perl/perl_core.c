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
	"}\n";


#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <stdarg.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/scripts.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>
#undef _

#include "perl_ekg.h"
#include "perl_bless.h"
#include "perl_core.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

extern void boot_DynaLoader(pTHX_ CV* cv);
PerlInterpreter *my_perl;

int perl_variable_changed(script_t *scr, script_var_t *scr_var)
{
	PERL_HANDLER_HEADER((char *) scr_var->private);

	XPUSHs(sv_2mortal(new_pv(scr_var->name)) );
	XPUSHs(sv_2mortal(new_pv(scr_var->value)) );

	PERL_HANDLER_FOOTER();
}

int perl_timers(script_t *scr, script_timer_t *time, int type)
{
//	if (type) return;
	PERL_HANDLER_HEADER((char *) time->private);

	XPUSHs(sv_2mortal(newSViv(type)));
	XPUSHs(sv_2mortal(ekg2_bless(BLESS_TIMER, 0, time->self)) );

	PERL_HANDLER_FOOTER();
}

int perl_commands(script_t *scr, script_command_t *comm, char **params)
{
	char *tmp;
	PERL_HANDLER_HEADER((char *) comm->private);
	XPUSHs(sv_2mortal(new_pv(comm->self->name)));
	tmp = array_join(params, " ");
	XPUSHs(sv_2mortal(new_pv(tmp)));
	xfree(tmp);

	PERL_HANDLER_FOOTER();
}
/* IF WATCH_READ_LINE int type == char *line */
int perl_watches(script_t *scr, script_watch_t *scr_wat, int type, int fd, int watch)
{
//	if (type) return -1;
	
	PERL_HANDLER_HEADER((char *) scr_wat->private);
	XPUSHs(sv_2mortal(newSViv(type)));
	XPUSHs(sv_2mortal(newSViv(fd)));
	if (scr_wat->self->buf) /* WATCH_READ_LINE */
		XPUSHs(sv_2mortal(new_pv((char *) watch)));
	else 			/* WATCH_READ */
		XPUSHs(sv_2mortal(newSViv(watch)));
	XPUSHs(scr_wat->data);
	PERL_HANDLER_FOOTER();
}

int perl_query(script_t *scr, script_query_t *scr_que, void *args[])
{
	int i;
	SV *perlargs[MAX_ARGS];
	SV *perlarg;

	int change = 1;
	
	PERL_HANDLER_HEADER((char *) scr_que->private);
	for (i=0; i < scr_que->argc; i++) {
		perlarg = NULL;
		switch ( scr_que->argv_type[i] ) {
			case (SCR_ARG_INT):   /* int */
				perlarg = newSViv( *(int  *) args[i] );
				break;
			case (SCR_ARG_CHARP):  /* char * */
				perlarg = new_pv(*(char **) args[i]);
				break;
			case (SCR_ARG_CHARPP): {/* char ** */
				char *tmp = array_join((char **) args[i], " ");
				if (xstrlen(tmp)) 
					perlarg = new_pv(tmp);
				xfree(tmp);
				break;
				}
			case (SCR_ARG_WINDOW): /* window_t */
				perlarg = ekg2_bless(BLESS_WINDOW, 0, (*(window_t **) args[i]));
				break;
			case (SCR_ARG_FSTRING): /* fstring_t */
				perlarg = ekg2_bless(BLESS_FSTRING, 0, (*(fstring_t **) args[i]));
				break;
			default:
				debug("[NIMP] %s %d %d\n",scr_que->self->name, i, scr_que->argv_type[i]);
		}

		if (!perlarg) perlarg = newSViv(0); // TODO: zmienic. ?
		if (change)   perlargs[i] = (perlarg = newRV_noinc(perlarg));
		XPUSHs(sv_2mortal(perlarg));
	}
#define PERL_RESTORE_ARGS 1
#include "perl_core.h"
	PERL_HANDLER_FOOTER();
#undef PERL_RESTORE_ARGS
}


int perl_unload(script_t *scr)
{
	perl_private_t *p = perl_private(scr);
	xfree(p);
	script_private_set(scr, NULL);
	return 0;
}


int perl_load(script_t *scr)
{
	int mask = 0;
	perl_private_t *p;

	char *error;
	int retcount;
	SV *ret;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);
	XPUSHs(sv_2mortal(new_pv(scr->path)));
	XPUSHs(sv_2mortal(new_pv(scr->name)));
	PUTBACK;
	
	retcount = perl_call_pv("Ekg2::Core::eval_file",
				G_EVAL|G_SCALAR);
	SPAGAIN;
        error = NULL;
	if (SvTRUE(ERRSV)) {
		error = SvPV(ERRSV, PL_na);
		print("script_error", error);

	} else if (retcount > 0) {
		ret = POPs;
		mask = SvIV(ret);
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
	
	p = xmalloc(sizeof(perl_private_t));
	
	script_private_set(scr, p);

	return mask;
	
}

static void xs_init(pTHX)
{
	dXSUB_SYS;
/*
#if PERL_STATIC_LIBS == 1
	newXS("Irssi::Core::boot_Irssi_Core", boot_Irssi_Core, __FILE__);
#endif
*/	
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, __FILE__);
}

int perl_initialize()
{
	char *args[] = {"", "-e", "0"};
	char *code = NULL, *sub_code = NULL;
	
	my_perl = perl_alloc();
	PL_perl_destruct_level = 1;
	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 3, args, NULL);
/* 	PL_exit_flags |= PERL_EXIT_DESTRUCT_END; */
/*
	sub_code = saprintf("use lib qw(%s %s/autorun %s);\n"
			    "use Ekg2;",                 prepare_path("scripts", 0), prepare_path("scripts", 0), "/usr/local/share/ekg2/scripts");
*/
	code = saprintf(ekg_core_code, 0, "use Ekg2;");
	
	
	perl_eval_pv(code, TRUE);
	xfree(code);
	xfree(sub_code);
	return 0;	
}

void ekg2_callXS(void (*subaddr)(pTHX_ CV* cv), CV *cv, SV **mark)
{
	dSP;
	PUSHMARK(mark);
	(*subaddr)(aTHX_ cv);
	
	PUTBACK;
}

/* syf irssi */

static int magic_free_object(pTHX_ SV *sv, MAGIC *mg)
{
        sv_setiv(sv, 0);
        return 0;
}


static MGVTBL vtbl_free_object = { NULL, NULL, NULL, NULL, magic_free_object };


/*static */ SV *create_sv_ptr(void *object)
{
        SV *sv;

//	if (!object) return &PL_sv_undef;

        sv = newSViv((IV)object);

        sv_magic(sv, NULL, '~', NULL, 0);

        SvMAGIC(sv)->mg_private = 0x1551; /* HF */
        SvMAGIC(sv)->mg_virtual = &vtbl_free_object;

        return sv;
}


void *Ekg2_ref_object(SV *o)
{
        SV **sv;
        HV *hv;
        void *p;

        hv = hvref(o);
//	hv =  (HV *)SvRV(o);
        if (!hv)
                return NULL;

        sv = hv_fetch(hv, "_ekg2", 4, 0);
        if (!sv)
                debug("variable is damaged\n");
        p = (void *) (SvIV(*sv));
        return p;
}
/* <syf irssi */

int perl_bind_free(script_t *scr, void *data, /* niby to jest ale kiedys nie bedzie.. nie uzywac */ int type, void *private, ...)
{
	va_list ap;
	SV *watchdata = NULL;
	va_start(ap, private);

        switch (type) {
		case(SCRIPT_WATCHTYPE): 
		    debug("[perl_bind_free] watch = %x\n", watchdata = va_arg(ap, void *));
                case(SCRIPT_VARTYPE):
                case(SCRIPT_COMMANDTYPE):
                case(SCRIPT_QUERYTYPE):
                case(SCRIPT_TIMERTYPE):
		case(SCRIPT_PLUGINTYPE):
//		    debug("[perl_bind_free] type %d funcname %s\n", type, private);
		    xfree(private);
                    break;
        }
	va_end(ap);
        return 0;
}

script_t *perl_caller() {
	char *scriptname = SvPV(perl_eval_pv("caller", TRUE), PL_na)+14;
	return script_find(&perl_lang, (char *) scriptname);
}

void *perl_plugin_register(char *name, int type, void *formatinit)
{
	return script_plugin_init(&perl_lang, perl_caller(), name, type, formatinit);
}

script_timer_t *perl_timer_bind(int freq, char *handler)
{
	return script_timer_bind(&perl_lang, perl_caller(), freq, xstrdup(handler));
}

int perl_timer_unbind(script_timer_t *stimer)
{
	return script_timer_unbind(stimer, 1);
}

script_var_t *perl_variable_add(char *var, char *value, char *handler)
{
	return script_var_add(&perl_lang, perl_caller(), var, value, xstrdup(handler));
}

void *perl_watch_add(int fd, int type, void *handler, void *data)
{
	return script_watch_add(&perl_lang, perl_caller(), fd, type, xstrdup(handler), data);
}

void *perl_handler_bind(char *query_name, char *handler)
{
	return script_query_bind(&perl_lang, perl_caller(), query_name, xstrdup(handler));
}

void *perl_command_bind(char *command, char *params, char *poss, char *handler)
{
#ifdef SCRIPTS_NEW
	return script_command_bind(&perl_lang, perl_caller(), command, params, poss, xstrdup(handler));
#else
	return script_command_bind(&perl_lang, perl_caller(), command, xstrdup(handler));
#endif
}

int perl_finalize()
{
	if (!my_perl)
		return -1;
	PL_perl_destruct_level = 1;
	perl_destruct(my_perl);
	perl_free(my_perl);
	my_perl = NULL;
	return 0;
}
