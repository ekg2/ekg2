#ifndef PERL_BLESS_H
#define PERL_BLESS_H

#undef _
#include <ekg/debug.h>
#include <ekg/dynstuff.h>
#include <ekg/scripts.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/userlist.h>
#include <ekg/windows.h>
#include <ekg/vars.h>

#include <plugins/irc/irc.h>

/* TODO: enum */
#define BLESS_SCRIPT 0
#define BLESS_SESSION 1
#define BLESS_VARIABLE 2
#define BLESS_PLUGIN 3
#define BLESS_WINDOW 4
#define BLESS_FSTRING 10
#define BLESS_COMMAND 5
#define BLESS_SESSION_PARAM 6
#define BLESS_TIMER 7
#define BLESS_USER 8
#define BLESS_LIST 11

#define BLESS_IRC_SERVER 20
#define BLESS_IRC_CHANNEL 21
#define BLESS_IRC_USER 22
#define BLESS_IRC_CHANNUSER 23
// Ekg2

#define bless_script(var)\
	(SV *) ekg2_bless(BLESS_SCRIPT, 0, var)

#define bless_variable(var)\
        (SV *) ekg2_bless(BLESS_VARIABLE, 0, var)

#define bless_session(session)\
        (SV *) ekg2_bless(BLESS_SESSION, 0, session)

#define bless_plugin(plugin)\
        (SV *) ekg2_bless(BLESS_PLUGIN, 0, plugin)

#define bless_window(window)\
	(SV *) ekg2_bless(BLESS_WINDOW, 0, window)

#define bless_command(command)\
	(SV *) ekg2_bless(BLESS_COMMAND, 0, command)

#define bless_session_param(param)\
	(SV *) ekg2_bless(BLESS_SESSION_PARAM, 0, param)

#define bless_timer(timer)\
	(SV *) ekg2_bless(BLESS_TIMER, 0, timer)
	
#define bless_user(user)\
	(SV *) ekg2_bless(BLESS_USER, 0, user)
#define bless_list(ptr, id)\
	(SV *) ekg2_bless(BLESS_LIST, id, ptr);

// Ekg2::Irc

#define bless_server(server)\
	(SV *) ekg2_bless(BLESS_IRC_SERVER, 0, server)

#define bless_channel(channel)\
	(SV *) ekg2_bless(BLESS_IRC_CHANNEL, 0, channel)

#define bless_person(person)\
	(SV *) ekg2_bless(BLESS_IRC_USER, 0, person)

#define bless_channperson(person)\
	(SV *) ekg2_bless(BLESS_IRC_CHANNUSER, 0, person)

#endif
