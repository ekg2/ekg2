#include "perl_ekg.h"
#include "perl_core.h"
#include "perl_bless.h"

#define HAVE_IRC 1

#define debug_bless(args...) ;
#define bless_struct(name, x) /* create_sv_ptr(x) TODO ! poczytac zrodla irssi ;> */\
/*	sv_bless(newHV(), gv_stashpv(name, 0)) */\
	sv_bless(newRV_noinc(newSViv((IV) x)), gv_stashpv(name, 0))

void ekg2_bless_session(HV *hv, session_t *session);

#ifdef HAVE_IRC /* IRC */
void ekg2_bless_irc_server(HV *hv, session_t *session)
{
	irc_private_t *j = irc_private(session);
	connector_t   *s = NULL;
	
	debug("blessing server %s\n", session->uid);
        if (j->conntmplist && j->conntmplist->data) s = j->conntmplist->data;
        
	hv_store(hv, "session", 7, bless_struct("Ekg2::Session", session), 0);

	if (j->nick)	hv_store(hv, "nick",    4, new_pv(j->nick), 0);
	else 		hv_store(hv, "nick",    4, new_pv(session_get(session, "nickname")), 0);
	
	if (s) {
		hv_store(hv, "server",  6, new_pv(s->hostname), 0);
		hv_store(hv, "ip",      2, new_pv(s->adres), 0);
	}
	else {
		hv_store(hv, "server",  6, new_pv(session_get(session, "server")), 0);
		hv_store(hv, "ip",      2, new_pv("0.0.0.0"), 0);
	}
#if 1
// TODO: wywalic jak bless_struct() bedzie dzialac.
	ekg2_bless_session(hv, session);
#endif
}

void ekg2_bless_irc_channel(HV *hv, channel_t *chan)
{
	debug_bless("blessing channel %s\n", chan->name);

	hv_store(hv, "name", 4, new_pv(chan->name+4), 0);
	hv_store(hv, "mode", 4, new_pv(chan->mode_str), 0); 
	hv_store(hv, "topic",5, new_pv(chan->topic), 0);
	hv_store(hv, "window",  6, bless_struct("Ekg2::Window", chan->window), 0);
	hv_store(hv, "topicby", 7, new_pv(chan->topicby), 0);

	hv_store(hv, "name_", 5, new_pv(chan->name), 0); /* wywalic ? */
	hv_store(hv, "mode_", 5, new_pv(chan->mode), 0); /* wywalic ? */
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
	hv_store(hv, "channel", 7, bless_struct("Ekg2::Irc::Channel", ch->chanp), 0);

#if 1	 /* wywalic jak bless_struct() bedzie dzialac */
	hv_store(hv, "name", 4, new_pv(ch->chanp->name+4), 0);
#endif
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
	debug_bless("blessing command %s\n", command->name);
	hv_store(hv, "name",  4, new_pv(command->name), 0);
}

void ekg2_bless_window(HV *hv, window_t *window)
{
	debug_bless("blessing window %s\n", window->target);
/* taki bajer, pytanie czy potrzebne... */
	if (window->id == 1) 		hv_store(hv, "target", 6, new_pv("__status"), 0);
	else if (window->id == 0) 	hv_store(hv, "target", 6, new_pv("__debug"), 0);
	else 				hv_store(hv, "target", 6, new_pv(window->target), 0);

	hv_store(hv, "id", 2, newSViv(window->id), 0);
	hv_store(hv, "session", 7, bless_struct("Ekg2::Session", window->session), 0);
	hv_store(hv, "userlist",  8, create_sv_ptr(window->userlist), 0); /* buggy ? */

}
void ekg2_bless_user(HV *hv, userlist_t *user)
{
	debug_bless("blessing user %s\n", user->uid);
	hv_store(hv, "uid", 3, new_pv(user->uid), 0);
	hv_store(hv, "status", 6, new_pv(user->status), 0);
}

void ekg2_bless_session(HV *hv, session_t *session)
{
        debug("blessing session %s\n", session->uid);
        hv_store(hv, "connected", 9, newSViv(session->connected), 0);
        hv_store(hv, "uid",       3, new_pv(session->uid), 0);
	hv_store(hv, "status",	  6, new_pv(session->status), 0);
	hv_store(hv, "userlist",  8, create_sv_ptr(session->userlist), 0);
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


SV *ekg2_bless(int flag, int flag2, void *object)
{
        HV *stash, *hv;
//        debug("BLESS: OK %d %d %x\n",  flag, flag2, object);

        if (!object)
                return &PL_sv_undef;

        hv = newHV();
        hv_store(hv, "_ekg2", 4, create_sv_ptr(object), 0);

        switch(flag) {
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
                        stash = gv_stashpv("Ekg2::Userlist::users", 1);
                        ekg2_bless_user(hv, object);
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
/* ELSE */			
                default:
                        debug("@perl_bless.c ekg2_bless() unknown flag=%d\n", flag);
//                      return create_sv_ptr(object);
                        return &PL_sv_undef;
        }
        return sv_bless(newRV_noinc((SV*)hv), stash);
}

