#ifndef PERL_BLESS_H
#define PERL_BLESS_H

#ifndef __FreeBSD__
#define _XOPEN_SOURCE 600
#define __EXTENSIONS__
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

typedef enum {
	BLESS_SCRIPT = 0,
	BLESS_SESSION,
	BLESS_VARIABLE,
	BLESS_PLUGIN,
	BLESS_WINDOW,
	BLESS_WATCH,
	BLESS_COMMAND,
	BLESS_SESSION_PARAM,
	BLESS_TIMER,
	BLESS_USER,

	BLESS_FSTRING = 10,
	BLESS_LIST,

	BLESS_IRC_SERVER = 20,
	BLESS_IRC_CHANNEL,
	BLESS_IRC_USER,
	BLESS_IRC_CHANNUSER,
} perl_bless_t;

SV *ekg2_bless(perl_bless_t flag, int flag2, void *object);

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

#define bless_watch(watch)\
	(SV *) ekg2_bless(BLESS_WATCH, 0, watch)

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
