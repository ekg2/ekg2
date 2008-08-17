/* $Id$ */

/*
 *  (C) Copyright 2004-2005 Leszek Krupiñski <leafnode@pld-linux.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
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

#include <ekg/debug.h>
#include <ekg/userlist.h>

#include <ekg/commands.h>
#include <ekg/dynstuff.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/sessions.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/vars.h>
#include <ekg/xmalloc.h>
#include <ekg/queries.h>

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

PyObject *python_build_user(char * session, char * name)
{
	ekg_userObj *pyuser;
	char buf[100];
	session_t *s;
	userlist_t *u;

	debug("[python] checking for user '%s' in session '%s'\n", name, session);

	s = session_find(session);
	u = userlist_find(s, name);

	if (!u) {
		snprintf(buf, 99, "Can't find user '%s'", name);
		PyErr_SetString(PyExc_KeyError, buf);
		return NULL;
	}

	debug("[python] Building object for user '%s'\n", name);
	pyuser = PyObject_New(ekg_userObj, &ekg_user_type);
	pyuser->name = xstrdup(name);
	pyuser->session = xstrdup(session);
	Py_INCREF(pyuser);
	return (PyObject *)pyuser;
}


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
	if (!u) {
		PyErr_SetString(PyExc_RuntimeError, _("Can't find user (?)"));
		return NULL;
	}
	if (!xstrcmp(attr, "uid")) {
		if (u->uid) {
			return PyString_FromString(u->uid);
		} else {
			Py_RETURN_NONE;
		}
	} else if (!xstrcmp(attr, "nickname")) {
		if (u->nickname) {
			return PyString_FromString(u->nickname);
		} else {
			Py_RETURN_NONE;
		}
#if 0 /* using PRIVHANDLER below */
	} else if (!xstrcmp(attr, "first_name")) {
		if (u->first_name) {
			return PyString_FromString(u->first_name);
		} else {
			Py_RETURN_NONE;
		}
	} else if (!xstrcmp(attr, "last_name")) {
		if (u->last_name) {
			return PyString_FromString(u->last_name);
		} else {
			Py_RETURN_NONE;
		}
	} else if (!xstrcmp(attr, "mobile")) {
		if (u->mobile) {
			return PyString_FromString(u->mobile);
		} else {
			Py_RETURN_NONE;
		}
#endif
	} else if (!xstrcmp(attr, "status")) {
		if (u->status) {
			if (u->descr) {
				return Py_BuildValue("(ss)", ekg_status_string(u->status, 0), u->descr);
			} else {
				return Py_BuildValue("(so)", ekg_status_string(u->status, 0), Py_None);
			}
		} else {
			Py_RETURN_NONE;
		}
#if 0 /* XXX */
	} else if (!xstrcmp(attr, "resource")) {
		if (u->resource) {
			return PyString_FromString(u->resource);
		} else {
			Py_RETURN_NONE;
		}
#endif
	} else if (!xstrcmp(attr, "last_seen")) {
		if (u->last_seen) {
			return Py_BuildValue("i", u->last_seen);
		} else {
			Py_RETURN_NONE;
		}
#if 0 /* using PRIVHANDLER below */
	} else if (!xstrcmp(attr, "ip")) {
		if (u->ip) {
			struct sockaddr_in sin;
			sin.sin_addr.s_addr = u->ip;
			return PyString_FromString(inet_ntoa(sin.sin_addr));
		} else {
			Py_RETURN_NONE;
		}
	} else if (!xstrcmp(attr, "last_ip")) {
		if (u->last_ip) {
			struct sockaddr_in sin;
			sin.sin_addr.s_addr = u->last_ip;
			return PyString_FromString(inet_ntoa(sin.sin_addr));
		} else {
			Py_RETURN_NONE;
		}
#endif
	} else if (!xstrcmp(attr, "status_time")) {
		if (u->status_time) {
			return Py_BuildValue("i", u->status_time);
		} else {
			Py_RETURN_NONE;
		}
	} else if (!xstrcmp(attr, "last_status")) {
		if(u->last_status) {
			return Py_BuildValue("(ss)", ekg_status_string(u->last_status, 0), u->last_descr);
		} else {
			Py_RETURN_NONE;
		}
	} else { /* XXX, take a look at this */
		char *val;

		if (user_private_item_get_safe(u, attr, &val)) 
			return PyString_FromString(val);
		else {
/*			return Py_FindMethod(ekg_user_methods, (PyObject *) self, attr); */
			Py_RETURN_NONE;
		}
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
 * ekg_user_repr()
 *
 * __repr__ method
 *
 */

PyObject *ekg_user_repr(ekg_userObj *self)
{
	char buf[100];
	snprintf(buf, 99, "<user %s session %s>", self->name, self->session);
	return PyString_FromString(buf);
}

/**
 * ekg_user_str()
 *
 * __str__ method
 *
 */

PyObject *ekg_user_str(ekg_userObj *self)
{
	return PyString_FromString(self->name);
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
	struct ekg_group *gl;
	PyObject *list;
	int len = LIST_COUNT2(u->groups);

	list = PyList_New(len);
	len = 0;

	for (gl = u->groups; gl; gl = gl->next) {
		struct ekg_group *g = gl;
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
