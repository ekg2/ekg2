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
#include "python-session.h"

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
    PyObject * name;

    static char *kwlist[] = {"name", NULL};

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist,
                                      &name))
        return -1;

    self->name = PyString_AsString(name);

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
	int cnt = 0;
	session_t * s;
	s = session_find(self->name);
	if (s->params) {
		list_t l;
		for (l = s->params; l; l = l->next) {
			cnt++;
		}
	}
	return cnt;
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

int ekg_session_set(ekg_sessionObj * self, PyObject * key, PyObject * value)
{
	session_t * s;
	s = session_find(self->name);
	const char *name = PyString_AsString(key);
	
	debug("[python] Setting '%s' option to '%s' for session %s\n", name,
		PyString_AsString(value), self->name);
	
	session_param_t * p = session_var_find(s, name);
	
	if (!p) {
		PyErr_SetString(PyExc_LookupError, "unknown variable");
		return -1;
    }

    if (PyInt_Check(value)) {
		session_set(s, name, itoa(PyInt_AsLong(value)));
	} else {
		session_set(s, name, PyString_AsString(value));
    }
	config_changed = 1;

    return 0;
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


/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
