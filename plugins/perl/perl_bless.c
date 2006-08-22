#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/scripts.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/windows.h>
#include <ekg/vars.h>

#include <plugins/irc/irc.h>
#undef _

#include "perl_core.h"
#include "perl_bless.h"

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#define debug_bless(args...) ;

void ekg2_bless_session(HV *hv, session_t *session);

#define HAVE_IRC 1

#ifdef HAVE_IRC
void ekg2_bless_irc_server(HV *hv, session_t *session)
{
	irc_private_t *j = irc_private(session);
	connector_t   *s = NULL;
	if (xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) {
		debug("[perl_ierror] not irc session in ekg2_bless_irc_server!\n");
		return;
	}
	debug_bless("blessing server %s\n", session->uid);

	if (j->conntmplist && j->conntmplist->data) s = j->conntmplist->data;
	if (s) {
		hv_store(hv, "server",  6, new_pv(s->hostname), 0);
		hv_store(hv, "ip",      2, new_pv(s->address), 0);
	} else {
		hv_store(hv, "server",  6, new_pv(session_get(session, "server")), 0);
		hv_store(hv, "ip",      2, new_pv("0.0.0.0"), 0);
	}
	if (j->nick)	hv_store(hv, "nick",    4, new_pv(j->nick), 0);
	else 		hv_store(hv, "nick",    4, new_pv(session_get(session, "nickname")), 0);
}

void ekg2_bless_irc_channel(HV *hv, channel_t *chan)
{
	debug_bless("blessing channel %s\n", chan->name);

	hv_store(hv, "name", 4, new_pv(chan->name+4), 0);
	hv_store(hv, "mode", 4, new_pv(chan->mode_str), 0); 
	hv_store(hv, "topic",5, new_pv(chan->topic), 0);
	hv_store(hv, "window", 6, ekg2_bless(BLESS_WINDOW, 0, chan->window), 0);
	hv_store(hv, "topicby", 7, new_pv(chan->topicby), 0);

	hv_store(hv, "name_", 5, new_pv(chan->name), 0); /* wywalic ? */
//	hv_store(hv, "mode_int", 5, newSViv(chan->mode), 0); /* ? */
}

void ekg2_bless_irc_user(HV *hv, people_t *person)
{
	hv_store(hv, "nick", 4,  new_pv(person->nick+4), 0);
	hv_store(hv, "realname", 8, new_pv(person->realname), 0);
	hv_store(hv, "hostname", 8, new_pv(person->host), 0);
	hv_store(hv, "ident",    5, new_pv(person->ident), 0);
	
//	hv_store(hv, "channels", 8, bless_struct("Ekg2::Irc::Channels", person->channels), 0);  
	hv_store(hv, "nick_", 5, new_pv(person->nick), 0); /* wywalic ? */
}

void ekg2_bless_irc_channuser(HV *hv, people_chan_t *ch)
{
	hv_store(hv, "mode", 4, newSViv(ch->mode), 0);  /* bitfield  */
	hv_store(hv, "sign", 4, new_pv(ch->sign),  0);
	hv_store(hv, "channel", 7, ekg2_bless(BLESS_IRC_CHANNEL, 0, ch->chanp), 0);
}

#endif /* HAVE_IRC */

void ekg2_bless_session_var(HV *hv, session_param_t *p)
{
        debug_bless("blessing var %s\n", p->key);
        hv_store(hv, "key",      3, new_pv(p->key), 0);
	hv_store(hv, "value",    5, new_pv(p->value), 0);
}

void ekg2_bless_var(HV *hv, variable_t *var)
{
        debug_bless("blessing var %s\n", var->name);
        hv_store(hv, "name",      4, new_pv(var->name), 0);

        switch (var->type) {
                case (VAR_FILE):
                case (VAR_DIR):
                case (VAR_THEME):
                case (VAR_STR): hv_store(hv, "value",     5, new_pv(*  (char**) var->ptr), 0);
                                break;
                case (VAR_BOOL):
                case (VAR_INT): hv_store(hv, "value",     5, newSViv(* (int*) (var->ptr)), 0);
                                break;
                default:        hv_store(hv, "value",     5, new_pv("_NIMPTYPE_"), 0);
        }
}

void ekg2_bless_command(HV *hv, command_t *command)
{
	char *temp;
	debug_bless("blessing command %s\n", command->name);
	hv_store(hv, "name",  4, new_pv(command->name), 0);
	temp = array_join(command->params, " ");	hv_store(hv, "param", 5, new_pv(temp), 0); xfree(temp);
	temp = array_join(command->possibilities, " ");	hv_store(hv, "poss",  4, new_pv(temp), 0); xfree(temp);
}

void ekg2_bless_fstring(HV *hv, fstring_t *fstr)
{
	hv_store(hv, "str", 3, new_pv(fstr->str), 0);
	hv_store(hv, "ts",  2, newSViv(fstr->ts), 0);
	hv_store(hv, "attr",4, create_sv_ptr(fstr->attr), 0);
}

void ekg2_bless_window(HV *hv, window_t *window)
{
	char *target = window_target(window);
	debug_bless("blessing window %s\n", target);

	hv_store(hv, "target", 6, new_pv(target), 0);
	hv_store(hv, "id", 2, newSViv(window->id), 0);
	hv_store(hv, "session", 7, ekg2_bless(BLESS_SESSION, 1, window->session), 0);
//	hv_store(hv, "userlist",  8, create_sv_ptr(window->userlist), 0); /* buggy ? */
//	hv_store(hv, "userlist", 8, ekg2_bless(BLESS_LIST, 1, window->userlist), 0); // obsolete: call userlist()

}

static inline char *inet_ntoa_u(uint32_t ip) {
	struct in_addr in;
	in.s_addr = ip;
	return inet_ntoa(in);
}

void ekg2_bless_user(HV *hv, userlist_t *user)
{
	debug_bless("blessing user %s\n", user->uid);
	hv_store(hv, "uid", 3, new_pv(user->uid), 0);
	hv_store(hv, "nickname", 8, new_pv(user->nickname), 0);
	hv_store(hv, "status", 6, new_pv(user->status), 0);
	hv_store(hv, "ip", 2, 	  new_pv(inet_ntoa_u(user->ip)), 0);
}

void ekg2_bless_session(HV *hv, session_t *session)
{
        debug_bless("blessing session %s\n", session->uid);
        hv_store(hv, "connected", 9, newSViv(session->connected), 0);
        hv_store(hv, "uid",       3, new_pv(session->uid), 0);
	hv_store(hv, "status",	  6, new_pv(session->status), 0);
//	hv_store(hv, "userlist",  8, create_sv_ptr(session->userlist), 0);
//	hv_store(hv, "userlist",  8, ekg2_bless(BLESS_LIST, 1, session->userlist), 0); // obsolete call userlist()
	hv_store(hv, "alias",	  5, new_pv(session->alias), 0);
}

void ekg2_bless_timer(HV *hv, struct timer *timer)
{
        debug_bless("blessing timer %s\n", timer->name);
        hv_store(hv, "name", 4, new_pv(timer->name), 0);
	hv_store(hv, "freq",  4, newSViv(timer->period), 0);
}

void ekg2_bless_plugin(HV *hv, plugin_t *plugin)
{
        debug_bless("blessing plugin %s\n", plugin->name);
        hv_store(hv, "prio", 4, newSViv(plugin->prio), 0);
        hv_store(hv, "name", 4, new_pv(plugin->name), 0);
}

void ekg2_bless_script(HV *hv, script_t *scr)
{
	debug_bless("blessing script %s\n", scr->name);
	hv_store(hv, "name", 4, new_pv(scr->name), 0);
	hv_store(hv, "path", 4, new_pv(scr->path), 0);
}


SV *ekg2_bless(perl_bless_t flag, int flag2, void *object)
{
        HV *stash, *hv;
        debug_bless("BLESS: OK %d %d %x\n",  flag, flag2, object);

        if (!object)
                return &PL_sv_undef;

        hv = newHV();
        hv_store(hv, "_ekg2", 4, create_sv_ptr(object), 0);

        switch(flag) {
/* native ekg2 */
		case BLESS_SCRIPT:
			stash = gv_stashpv("Ekg2::Script", 1);
			ekg2_bless_script(hv, object);
			break;
                case BLESS_SESSION:
                        stash = gv_stashpv("Ekg2::Session", 1);
                        ekg2_bless_session(hv, object);
                        break;
		case BLESS_SESSION_PARAM:
                        stash = gv_stashpv("Ekg2::Session::Param", 1);
                        ekg2_bless_session_var(hv, object);
			break;
                case BLESS_VARIABLE:
                        stash = gv_stashpv("Ekg2::Variable", 1);
                        ekg2_bless_var(hv, object);
                        break;
                case BLESS_PLUGIN:
                        stash = gv_stashpv("Ekg2::Plugin", 1);
                        ekg2_bless_plugin(hv, object);
                        break;
		case BLESS_WINDOW:
                        stash = gv_stashpv("Ekg2::Window", 1);
                        ekg2_bless_window(hv, object);
			break;
		case BLESS_COMMAND:
                        stash = gv_stashpv("Ekg2::Command", 1);
                        ekg2_bless_command(hv, object);
			break;
		case BLESS_TIMER:
                        stash = gv_stashpv("Ekg2::Timer", 1);
                        ekg2_bless_timer(hv, object);
			break;
		case BLESS_USER:
                        stash = gv_stashpv("Ekg2::User", 1);
                        ekg2_bless_user(hv, object);
			break;
		case BLESS_FSTRING:
			stash = gv_stashpv("Ekg2::Fstring", 1);
			ekg2_bless_fstring(hv, object);
			break;
		case BLESS_LIST:
			stash = gv_stashpv("Ekg2::Userlist", 1);
			hv_store(hv, "", 0, create_sv_ptr(object), 0);
			break;
#ifdef HAVE_IRC
/* IRC */			
		case BLESS_IRC_SERVER:
                        stash = gv_stashpv("Ekg2::Irc::Server", 1);
                        ekg2_bless_irc_server(hv, object);
			break;
		case BLESS_IRC_CHANNEL:
                        stash = gv_stashpv("Ekg2::Irc::Channel", 1);
                        ekg2_bless_irc_channel(hv, object);
			break;
		case BLESS_IRC_USER:
			stash = gv_stashpv("Ekg2::Irc::User", 1);
                        ekg2_bless_irc_user(hv, object);
			break;
		case BLESS_IRC_CHANNUSER:
			stash = gv_stashpv("Ekg2::Irc::Channel::User", 1);
                        ekg2_bless_irc_channuser(hv, object);
			break;

#endif
/* abstact script */
		
/* ELSE */
                default:
                        debug("@perl_bless.c ekg2_bless() unknown flag=%d flag1=%d obj=0x%x\n", flag, flag2, object);
//                      return create_sv_ptr(object);
                        return &PL_sv_undef;
        }
        return sv_bless(newRV_noinc((SV*)hv), stash);
}

