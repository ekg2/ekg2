/* $Id$ */

/*
 *  (C) Copyright 2004-2005 Leszek Krupiński <leafnode@pld-linux.org>
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

#ifndef __PYTHON_SESSION_H_

#define __PYTHON_SESSION_H_

#include <Python.h>
#include <ekg/dynstuff.h>

typedef struct {
	PyObject_HEAD
	char * name;
} ekg_sessionObj;

void ekg_session_dealloc(ekg_sessionObj * o);
PyObject * ekg_session_repr(ekg_sessionObj * self);
PyObject * ekg_session_str(ekg_sessionObj * self);
int ekg_session_init(ekg_sessionObj *self, PyObject *args, PyObject *kwds);
int ekg_session_len(ekg_sessionObj * self);
PyObject *ekg_session_set(ekg_sessionObj * self, PyObject * key, PyObject * value);
PyObject *ekg_session_connected(ekg_sessionObj * self);
PyObject *ekg_session_get_attr(ekg_sessionObj * self, char * attr);
PyObject *ekg_session_user_get(ekg_sessionObj * self, PyObject * pyargs);
PyObject *ekg_session_users(ekg_sessionObj * self);
PyObject *ekg_session_get(ekg_sessionObj * self, PyObject * key);
PyObject *ekg_session_status_set(ekg_sessionObj * self, PyObject * pyargs);
PyObject *ekg_session_status(ekg_sessionObj * self);
PyObject *ekg_session_connect(ekg_sessionObj * self);
PyObject *ekg_session_disconnect(ekg_sessionObj * self);

staticforward PyMethodDef ekg_session_methods[] = {
        {"connected", (PyCFunction)ekg_session_connected, METH_NOARGS, "Check if session is connected"},
        {"user_get", (PyCFunction)ekg_session_user_get, METH_VARARGS, "Return user object"},
        {"users", (PyCFunction)ekg_session_users, METH_NOARGS, "Return userlist"},
        {"status_set", (PyCFunction)ekg_session_status_set, METH_VARARGS, "Set status for session"},
        {"status", (PyCFunction)ekg_session_status, METH_NOARGS, "Get status tuple for session"},
        {"connect", (PyCFunction)ekg_session_connect, METH_NOARGS, "Connect session"},
        {"disconnect", (PyCFunction)ekg_session_disconnect, METH_NOARGS, "Disconnect session"},
        {NULL, NULL, 0, NULL}
};

static PyMappingMethods ekg_session_mapping = {
    (inquiry) ekg_session_len,
    (binaryfunc) ekg_session_get,
    (objobjargproc) ekg_session_set
};

static PyTypeObject ekg_session_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "session",
    sizeof(ekg_sessionObj),
    0,
    (destructor)ekg_session_dealloc,
    0,
    (getattrfunc)ekg_session_get_attr,
    0,
    0,
    (reprfunc)ekg_session_repr,
    0,
    0,
    &ekg_session_mapping,
    0,							/*tp_hash */
    0,							/*tp_call*/
    (reprfunc)ekg_session_str,				/*tp_str*/
    0,							/*tp_getattro*/
    0,							/*tp_setattro*/
    0,							/*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,           /*tp_flags*/
    "Session object",			                /* tp_doc */
    0,							/* tp_traverse */
    0,							/* tp_clear */
    0,							/* tp_richcompare */
    0,							/* tp_weaklistoffset */
    0,							/* tp_iter */
    0,							/* tp_iternext */
    ekg_session_methods,                                /* tp_methods */
    0,							/* tp_members */
    0,							/* tp_getset */
    0,							/* tp_base */
    0,							/* tp_dict */
    0,							/* tp_descr_get */
    0,							/* tp_descr_set */
    0,							/* tp_dictoffset */
    (initproc)ekg_session_init,                         /* tp_init */
    0,							/* tp_alloc */
    0,							/* tp_new */
};

#endif

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: sts=8 sw=8
 */
