/* $Id$ */

/*
 *  (C) Copyright 2004-2005 Leszek Krupiński <leafnode@pld-linux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2-config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>

#include "python.h"
#include "python-user.h"

// * ***************************************************************************
// *
// * user object
// *
// * ***************************************************************************

/**
 * ekg_user_init()
 *
 * initialization of user object
 *
 */

int ekg_user_init(ekg_userObj *self, PyObject *args, PyObject *kwds)
{
    PyObject * name;
	PyObject * session;
    static char *kwlist[] = {"name", "session", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "ss", kwlist,
                                      &name, &session))
        return -1;

    self->name = PyString_AsString(name);
	self->session = PyString_AsString(session);

    return 0;
}

/**
 * ekg_user_get_attr()
 *
 * get attribute from user object
 *
 */

PyObject *ekg_user_get_attr(ekg_userObj * self, char * attr)
{
	session_t * s = session_find((const char *) self->session);
	userlist_t * u = userlist_find(s, self->name);
	if (!xstrcmp(attr, "uid")) {
		return PyString_FromString(u->uid);
	} else if (!xstrcmp(attr, "nickname")) {
		return PyString_FromString(u->nickname);
	} else if (!xstrcmp(attr, "first_name")) {
		return PyString_FromString(u->first_name);
	} else if (!xstrcmp(attr, "last_name")) {
		return PyString_FromString(u->last_name);
	} else if (!xstrcmp(attr, "mobile")) {
		return PyString_FromString(u->mobile);
	} else if (!xstrcmp(attr, "status")) {
		return Py_BuildValue("(ss)", u->status, u->descr);
	} else if (!xstrcmp(attr, "resource")) {
		return PyString_FromString(u->resource);
	} else if (!xstrcmp(attr, "last_seen")) {
		return Py_BuildValue("i", u->last_seen);
	} else if (!xstrcmp(attr, "ip")) {
		struct sockaddr_in sin;
		sin.sin_addr.s_addr = u->ip;
		return PyString_FromString(inet_ntoa(sin.sin_addr));
	} else if (!xstrcmp(attr, "last_ip")) {
		struct sockaddr_in sin;
		sin.sin_addr.s_addr = u->last_ip;
		return PyString_FromString(inet_ntoa(sin.sin_addr));
	} else if (!xstrcmp(attr, "status_time")) {
		return Py_BuildValue("i", u->status_time);
	} else if (!xstrcmp(attr, "last_status")) {
                if(u->last_status) {
                        return Py_BuildValue("(ss)", u->last_status, u->last_descr);
                } else {
                        Py_INCREF(Py_None);
                        return Py_None;
                }
	} else {
		return Py_FindMethod(ekg_user_methods, (PyObject *) self, attr);
	}
}

/**
 * ekg_user_dealloc()
 *
 * deallocation of user object
 *
 */

void ekg_user_dealloc(ekg_userObj * o)
{
	if (o->name) {
		xfree(o->name);
	}
	if (o->session) {
		xfree(o->session);
	}
}

/**
 * ekg_user_groups()
 *
 * return true if user is connected
 *
 */

PyObject *ekg_user_groups(ekg_userObj * self)
{
	session_t * s = session_find((const char *) self->session);
	userlist_t * u = userlist_find(s, self->name);
	list_t l;
    PyObject *list;
    int len = 0;

    for (l = u->groups; l; l = l->next) {
		len++;
    }

    list = PyList_New(len);
    len = 0;

    for (l = u->groups; l; l = l->next) {
		struct ekg_group *g = l->data;
		PyList_SetItem(list, len, PyString_FromString(g->name));
		len++;
    }
    Py_INCREF(list);
    return list;
}


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
