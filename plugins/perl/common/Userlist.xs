#include "module.h"

MODULE = Ekg2::Userlist  PACKAGE = Ekg2
PROTOTYPES: ENABLE

#####
MODULE = Ekg2::User	PACKAGE = Ekg2::User  PREFIX = userlist_

int _userlist_remove(Ekg2::Session session, Ekg2::User u)

int _userlist_remove_u(Ekg2::Userlist userlist, Ekg2::User u)
########

#*******************************
MODULE = Ekg2::Userlist	PACKAGE = Ekg2::Userlist  PREFIX = userlist_
#*******************************

# void userlist_users(Ekg2::Userlist userlist)
void userlist_users(void * userlist)
PREINIT:
        list_t l;
PPCODE:
        for (l = userlist ; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_user( (userlist_t *) l->data)));
        }

int userlist_add(Ekg2::Session session, const char *uid, const char *nickname)

