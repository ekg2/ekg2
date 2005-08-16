#include "perl_ekg.h"
#include "perl_core.h"
#include <ekg/windows.h>
#include <ekg/xmalloc.h>
#include <ekg/vars.h>


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
	XPUSHs(sv_2mortal(create_sv_ptr(time)) );
	XPUSHs(sv_2mortal(create_sv_ptr(time->self)) );

	PERL_HANDLER_FOOTER();

}

int perl_commands(script_t *scr, script_command_t *comm, char **params)
{
	PERL_HANDLER_HEADER((char *) comm->private);

	XPUSHs(sv_2mortal(new_pv(comm->comm)));
	XPUSHs(sv_2mortal(new_pv(*params)));

	PERL_HANDLER_FOOTER();
}

int perl_query(script_t *scr, script_query_t *scr_que, void *args[])
{
	int i;
	SV *perlargs[MAX_ARGS];
	SV *perlarg;
	int change = 1;
	
	char *tmp;

	PERL_HANDLER_HEADER(scr_que->private);
	
	for (i=0; i < scr_que->argc; i++) {
		perlarg = 0;
		switch ( scr_que->argv_type[i] ) {
			case (SCR_ARG_INT):
			
				if (change) perlarg = newRV_noinc ( newSViv( *(int  *) args[i] ) );
				else        perlarg = newSViv( *(int  *) args[i] );
				
				break;
			case (SCR_ARG_CHARP): 
				tmp = *(char **) args[i];
				
				if (change) perlarg = newRV_noinc( new_pv(tmp) );
				else        perlarg = new_pv(tmp);
				
				break;
			case (SCR_ARG_CHARPP): 
				tmp = *(char **) args[i];
				
				if (tmp) {
					if (change) perlarg = newRV_noinc( new_pv( *(char **) tmp) );
					else        perlarg = new_pv( *(char **) tmp);
				}

				break;
			default:
				debug("[NIMP] %s %d %d\n",scr_que->query_name, i, scr_que->argv_type[i]);

		
		}
		if (!perlarg) perlarg = newSViv(0); // = new_pv( *ARG_CHARPP(i)); // TODO: zmienic.
		if (change)   perlargs[i] = perlarg;
		XPUSHs(sv_2mortal(perlarg));
	}
#define PERL_RESTORE_ARGS 1
#if 1
        PUTBACK;
        perl_retcount = perl_call_pv(fullproc, G_EVAL);
        SPAGAIN;
        if (SvTRUE(ERRSV)) {
                error = SvPV(ERRSV, PL_na);
                print("script_error", error);
                ret = SCRIPT_HANDLE_UNBIND;
        }
        else if (perl_retcount > 0)
        {
                perl_ret = POPs;
                ret = SvIV(perl_ret);
        }
// tutaj przywrocic argumenty.

if (change) {
	for (i=0; i < scr_que->argc; i++) {
		switch ( scr_que->argv_type[i] ) {
			case (SCR_ARG_INT):
				*( (int **) args[i]) = SvIV(SvRV(perlargs[i]));
				break;

			case (SCR_ARG_CHARP): 
				xfree(*(char **) args[i]); // dobrze ? 
				*( (char **) args[i]) = xstrdup( SvPV_nolen(SvRV(perlargs[i])) ) ;
				break;
			case (SCR_ARG_CHARPP): 
				break;
			default:
				debug("dupa!\n");
		
		}
	}
}
        PUTBACK;
        FREETMPS;
        LEAVE;
        xfree(fullproc);
        if (ret < 0) return -1;
        else         return 0;
#else 
	PERL_HANDLER_FOOTER();
#endif

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
	
//	retcount = perl_call_pv(fullproc,
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
	perl_construct(my_perl);
	perl_parse(my_perl, xs_init, 3, args, NULL);
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

int perl_timer_bind(int freq, char *handler)
{
	char *script = SvPV(perl_eval_pv("caller", TRUE), PL_na);
	char *mod    = script + 14; /* 14 stala -> `Ekg2::Script::` */
	
	script_t *scr = script_find(&perl_lang, (char *) mod);
	debug("[perl_timer_bind] %s %s %x	%s \n", script, mod, scr, handler);
	script_timer_bind(&perl_lang, scr, freq, xstrdup(handler));

	return 0;
}

int perl_timer_unbind(script_timer_t *stimer)
{
	script_timer_unbind(stimer, 1);
}

int perl_variable_add(char *var, char *value, char *handler)
{
	char *script = SvPV(perl_eval_pv("caller", TRUE), PL_na);
	char *mod    = script + 14; /* 14 stala -> `Ekg2::Script::` */
	
	script_t *scr = script_find(&perl_lang, (char *) mod);
	debug("[perl_variable_add] %s %s %x    %s\n", script, mod, scr, handler);
	script_var_add(&perl_lang, scr, xstrdup(var), xstrdup(value), xstrdup(handler));

	return 0;

}

int perl_handler_bind(char *query_name, char *handler)
{
	char *script = SvPV(perl_eval_pv("caller", TRUE), PL_na);
	char *mod    = script + 14; /* 14 stala -> `Ekg2::Script::` */
	
	script_t *scr = script_find(&perl_lang, (char *) mod);
	debug("[perl_handler_bind] %s %s %x    %s\n", script, mod, scr, handler);
	script_query_bind(&perl_lang, scr, xstrdup(query_name), xstrdup(handler));

	return 0;
}


int perl_command_bind(char *command, char *handler)
{
	char *script = SvPV(perl_eval_pv("caller", TRUE), PL_na);
	char *mod    = script + 14; /* 14 stala -> `Ekg2::Script::` */
	
	script_t *scr = script_find(&perl_lang, (char *) mod);
	debug("[perl_command_bind] %s %s %x    %s\n", script, mod, scr, handler);
	script_command_bind(&perl_lang, scr, xstrdup(command), xstrdup(handler));

	return 0;
}


int perl_finalize()
{
	if (!my_perl)
		return -1;
	perl_destruct(my_perl);
	perl_free(my_perl);
	my_perl = NULL;
	return 0;
}
