#include "module.h"

MODULE = Ekg2::Userlist  PACKAGE = Ekg2
PROTOTYPES: ENABLE

#####
MODULE = Ekg2::User	PACKAGE = Ekg2::User  PREFIX = userlist_user_


int userlist_user_set_status(Ekg2::User u, char *status)
CODE:
	int n = ekg_status_int(status);
	if (n) {
		u->status = n;
		RETVAL = 1;
	} else
		RETVAL = 0;
OUTPUT:
	RETVAL

#*******************************
MODULE = Ekg2::Userlist	PACKAGE = Ekg2::Userlist  PREFIX = userlist_
#*******************************

void userlist_users(Ekg2::Userlist userlist)
PREINIT:
        list_t l;
PPCODE:
        for (l = userlist ; l; l = l->next) {
                XPUSHs(sv_2mortal(bless_user( (userlist_t *) l->data)));
        }

Ekg2::User userlist_add(Ekg2::Userlist userlist, const char *uid, const char *nickname)
CODE:
	RETVAL = userlist_add_u(&userlist, uid, nickname);
OUTPUT:
	RETVAL

int userlist_remove(Ekg2::Userlist userlist, Ekg2::User u)
CODE:
	RETVAL = userlist_remove_u(&userlist, u);
OUTPUT:
	RETVAL

Ekg2::User userlist_find(Ekg2::Userlist userlist, char *uid)
CODE:
	RETVAL = userlist_find_u(&userlist, uid);
OUTPUT:
	RETVAL

