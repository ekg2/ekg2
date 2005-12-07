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

#ifndef __PYTHON_USER_H_

#define __PYTHON_USER_H_

#include <Python.h>
#include <ekg/dynstuff.h>

typedef struct {
	PyObject_HEAD
	char * name;
	char * session;
} ekg_userObj;

PyObject * python_build_user(char * session, char * name);
PyObject * ekg_user_repr(ekg_userObj * self);
PyObject * ekg_user_str(ekg_userObj * self);
void ekg_user_dealloc(ekg_userObj * o);
int ekg_user_init(ekg_userObj *self, PyObject *args, PyObject *kwds);
PyObject *ekg_user_groups(ekg_userObj * self);
PyObject *ekg_user_get_attr(ekg_userObj * self, char * attr);

staticforward PyMethodDef ekg_user_methods[] = {
    {"groups", (PyCFunction)ekg_user_groups, METH_NOARGS, "Returns groups user belongs to"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject ekg_user_type = {
        PyObject_HEAD_INIT(NULL)
        0,
        "user",
        sizeof(ekg_userObj),
        0,
        (destructor)ekg_user_dealloc,
        0,
        (getattrfunc)ekg_user_get_attr,
        0,
        0,
        (reprfunc)ekg_user_repr,
        0,
        0,
        0,
        0,							/*tp_hash */
        0,							/*tp_call*/
        (reprfunc)ekg_user_str,					/*tp_str*/
        0,							/*tp_getattro*/
        0,							/*tp_setattro*/
        0,							/*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,               /*tp_flags*/
        "User object",                                          /* tp_doc */
        0,							/* tp_traverse */
        0,							/* tp_clear */
        0,							/* tp_richcompare */
        0,							/* tp_weaklistoffset */
        0,							/* tp_iter */
        0,							/* tp_iternext */
        ekg_user_methods,			                /* tp_methods */
        0,							/* tp_members */
        0,							/* tp_getset */
        0,							/* tp_base */
        0,							/* tp_dict */
        0,							/* tp_descr_get */
        0,							/* tp_descr_set */
        0,							/* tp_dictoffset */
        (initproc)ekg_user_init,	                        /* tp_init */
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
