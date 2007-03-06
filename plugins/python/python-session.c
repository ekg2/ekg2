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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include <ekg/debug.h>

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
#include "python-session.h"
#include "python-user.h"

// * ***************************************************************************
// *
// * session object
// *
// * ***************************************************************************

/**
 * ekg_session_init()
 *
 * initialization of session object
 *
 */

int ekg_session_init(ekg_sessionObj *self, PyObject *args, PyObject *kwds)
{
    char * name;

    static char *kwlist[] = {"name", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist,
                                      &name))
        return -1;

    self->name = name;

    return 0;
}

/**
 * ekg_session_get_attr()
 *
 * get attribute from session object
 *
 */

PyObject *ekg_session_get_attr(ekg_sessionObj * self, char * attr)
{
	return Py_FindMethod(ekg_session_methods, (PyObject *) self, attr);
}

/**
 * ekg_session_dealloc()
 *
 * deallocation of session object
 *
 */

void ekg_session_dealloc(ekg_sessionObj * o)
{
	if (o->name) {
		xfree(o->name);
	}
}

/**
 * ekg_session_len()
 *
 * return length of session object sequence
 *
 */

int ekg_session_len(ekg_sessionObj * self)
{
	session_t * s;
	s = session_find(self->name);
	/* XXX, leafnode what about _local_ ones? */
	return s ? s->global_vars_count : 0;
}

/**
 * ekg_session_get()
 *
 * return session option with given name
 *
 */

PyObject *ekg_session_get(ekg_sessionObj * self, PyObject * key)
{
        const char * name = PyString_AsString(key);
        const char * out;
        char buf[100];
        session_t * s;
        s = session_find(self->name);
        debug("[python] Getting '%s' value for '%s' session\n", name, self->name);
        out = session_get(s, name);
        if (out) {
                return Py_BuildValue("s", out);
        } else {
                snprintf(buf, 99, "Can't find variable '%s'", name);
                PyErr_SetString(PyExc_KeyError, buf);
                Py_INCREF(Py_None);
                return Py_None;
        }
}

/**
 * ekg_session_set()
 *
 * set session option
 *
 */

PyObject *ekg_session_set(ekg_sessionObj * self, PyObject * key, PyObject * value)
{
	session_t * s;
	session_param_t * p;
	const char *name = PyString_AsString(key);
	s = session_find(self->name);

	debug("[python] Setting '%s' option to '%s' for session %s\n", name,
		PyString_AsString(value), self->name);

	p = session_var_find(s, name);

	if (!p) {
		PyErr_SetString(PyExc_LookupError, "unknown variable");
		return NULL;
    }

    if (PyInt_Check(value)) {
		session_set(s, name, itoa(PyInt_AsLong(value)));
	} else {
		session_set(s, name, PyString_AsString(value));
    }
	config_changed = 1;

    Py_INCREF(Py_None);
    return Py_None;
}

/**
 * ekg_session_repr()
 *
 * __repr__ method
 *
 */

PyObject *ekg_session_repr(ekg_sessionObj *self)
{
        char buf[100];
        snprintf(buf, 99, "<session %s>", self->name);
        return PyString_FromString(buf);
}

/**
 * ekg_session_str()
 *
 * __str__ method
 *
 */

PyObject *ekg_session_str(ekg_sessionObj *self)
{
        return PyString_FromString(self->name);
}

/**
 * ekg_session_connect()
 *
 * connect session
 *
 */

PyObject *ekg_session_connect(ekg_sessionObj *self)
{
        command_exec(NULL, session_find(self->name), "/connect", 0);
        Py_RETURN_NONE;
}

/**
 * ekg_session_disconnect()
 *
 * disconnect session
 *
 */

PyObject *ekg_session_disconnect(ekg_sessionObj *self)
{
        command_exec(NULL, session_find(self->name), "/disconnect", 0);
        Py_RETURN_NONE;
}

/**
 * ekg_session_connected()
 *
 * return true if session is connected
 *
 */

PyObject *ekg_session_connected(ekg_sessionObj * self)
{
	session_t * s;
	s = session_find(self->name);
	debug("[python] Checking if session %s is connected\n", self->name);
	if (session_connected_get(s)) {
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

/**
 * ekg_session_user_get()
 *
 * return user object
 *
 */

PyObject *ekg_session_user_get(ekg_sessionObj * self, PyObject * pyargs)
{
        char *name = NULL;

        if (!PyArg_ParseTuple(pyargs, "s", &name))
                return NULL;

        return python_build_user(self->name, name);
}

/**
 * ekg_session_users()
 *
 * return userlist
 *
 */

PyObject *ekg_session_users(ekg_sessionObj * self)
{
        session_t * s = session_find(self->name);
        PyObject *list;
        list_t l;
        int len = 0;

        for (l = s->userlist; l; l = l->next) {
                len++;
        }

        list = PyList_New(len);
        len = 0;

        for (l = s->userlist; l; l = l->next) {
                userlist_t * u = l->data;
                PyList_SetItem(list, len, python_build_user(self->name, u->uid));
                len++;
        }
        Py_INCREF(list);
        return list;
}

/**
 * ekg_session_status_set()
 *
 * set status for session
 *
 */

PyObject *ekg_session_status_set(ekg_sessionObj * self, PyObject * pyargs)
{
        char *status = NULL;
        char *descr = NULL;
        char *command = NULL;

        if (!PyArg_ParseTuple(pyargs, "s|s", &status, &descr))
                return NULL;

        if (xstrcmp(status, EKG_STATUS_AVAIL) == 0) {
                command = xstrdup("back");
        } else if (xstrcmp(status, EKG_STATUS_FREE_FOR_CHAT) == 0) {
                command = xstrdup("ffc");
        } else {
                command = xstrdup(status);
        }
        if (descr == NULL) {
                descr = xstrdup("-");
        }
        command_exec(NULL, session_find(self->name), saprintf("/%s %s", command, descr), 0);
        Py_RETURN_TRUE;
}

/**
 * ekg_session_status()
 *
 * return status tuple for session
 *
 */

PyObject *ekg_session_status(ekg_sessionObj * self)
{
        session_t * s = session_find(self->name);
        return Py_BuildValue("(ss)", session_status_get(s), session_descr_get(s));
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
