#include "module.h"

MODULE = Ekg2::Irc::Server PACKAGE = Ekg2::Irc
PROTOTYPES: ENABLE

void servers()
PREINIT:
        session_t *s;
PPCODE:
        for (s = sessions; s; s = s->next) {
		if (!xstrncasecmp( session_uid_get( s ), IRC4, 4)) {
            		XPUSHs(sv_2mortal(bless_server( s )));
		}
        }
	
Ekg2::Irc::Server session2server(Ekg2::Session s)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4))
		RETVAL = s;
	else
		RETVAL = NULL;
OUTPUT:
	RETVAL
	
MODULE = Ekg2::Irc::Server   PACKAGE = Ekg2::Irc::Server  PREFIX = server_

Ekg2::Session server_session(Ekg2::Session s)
CODE:
	RETVAL = s;
OUTPUT:
	RETVAL

void server_raw(Ekg2::Session s, char *str)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) irc_write(s, "%s\r\n", str);

void server_quit(Ekg2::Session s, char *quitreason)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) irc_write(s, "QUIT :%s\r\n", quitreason);

void server_newnick(Ekg2::Session s, char *newnick)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) irc_write(s, "NICK %s\r\n", newnick);

void server_setmode(Ekg2::Session s, char *mode)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) irc_write(s, "MODE %s %s\r\n", irc_private(s)->nick, mode);
	
void server_oper(Ekg2::Session s, char *nick, char *password)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) irc_write(s, "OPER %s %s\r\n", nick, password);
	
void server_die(Ekg2::Session s, char *reason)
CODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) irc_write(s, "DIE %s\r\n", reason);
	
void server_channels(Ekg2::Session s)
PREINIT:
	list_t l;
PPCODE:
	if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) {
    		for (l = irc_private(s)->channels; l; l = l->next) {
            		XPUSHs(sv_2mortal(bless_channel( (channel_t *) l->data)));
	        }
	}

void server_people(Ekg2::Session s)
PREINIT:
        list_t l;
PPCODE:
        if (!xstrncasecmp( session_uid_get( (session_t *) s), IRC4, 4)) {
                for (l = irc_private(s)->people; l; l = l->next) {
                        XPUSHs(sv_2mortal(bless_person( (people_t *) l->data)));
                }
        }
