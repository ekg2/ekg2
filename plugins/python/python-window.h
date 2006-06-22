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

#ifndef __PYTHON_WINDOW_H_

#define __PYTHON_WINDOW_H_

#include <Python.h>
#include <ekg/windows.h>

typedef struct
{
   PyObject_HEAD;
   window_t *w;
} ekg_windowObj;

void ekg_window_dealloc(ekg_windowObj *o);
PyObject * ekg_window_repr(ekg_windowObj * self);
PyObject * ekg_window_str(ekg_windowObj * self);
int ekg_window_init(ekg_windowObj *self, PyObject *args, PyObject *kwds);
PyObject* ekg_window_switch_to(ekg_windowObj *self, PyObject *args);
PyObject* ekg_window_echo(ekg_windowObj * self, PyObject *args);
PyObject* ekg_window_echo_format(ekg_windowObj * self, PyObject *args);
PyObject* ekg_window_kill(ekg_windowObj * self, PyObject *args);
PyObject* ekg_window_get_attr(ekg_windowObj * self, char * attr);
PyObject* ekg_window_next(ekg_windowObj *self, PyObject *args);
PyObject* ekg_window_prev(ekg_windowObj *self, PyObject *args);

staticforward PyMethodDef ekg_window_methods[] = {
	{"switch_to", (PyCFunction)ekg_window_switch_to, METH_VARARGS, "Switch to this window"},
	{"echo", (PyCFunction)ekg_window_echo, METH_VARARGS, "Print string on this window"},
	{"echo_format", (PyCFunction)ekg_window_echo_format, METH_VARARGS, "Print formatted string on this window"},
	{"kill", (PyCFunction)ekg_window_kill, METH_VARARGS, "Kill window"},
        {"next", (PyCFunction)ekg_window_next, METH_VARARGS, "Return next window" },
        {"prev", (PyCFunction)ekg_window_prev, METH_VARARGS, "Return previous window" },
        {NULL, NULL, 0, NULL}
};

static PyTypeObject ekg_window_type = {
        PyObject_HEAD_INIT(NULL)
        0,
        "window",
        sizeof(ekg_windowObj),
        0,
        (destructor)ekg_window_dealloc,
        0,
        (getattrfunc)ekg_window_get_attr,
        0,
        0,
        (reprfunc)ekg_window_repr,
        0,
        0,
        0,
        0,							/*tp_hash */
        0,							/*tp_call*/
        (reprfunc)ekg_window_str,				/*tp_str*/
        0,							/*tp_getattro*/
        0,							/*tp_setattro*/
        0,							/*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
        "Window object",			/* tp_doc */
        0,							/* tp_traverse */
        0,							/* tp_clear */
        0,							/* tp_richcompare */
        0,							/* tp_weaklistoffset */
        0,							/* tp_iter */
        0,							/* tp_iternext */
        ekg_window_methods,		/* tp_methods */
        0,							/* tp_members */
        0,							/* tp_getset */
        0,							/* tp_base */
        0,							/* tp_dict */
        0,							/* tp_descr_get */
        0,							/* tp_descr_set */
        0,							/* tp_dictoffset */
        (initproc)ekg_window_init,	/* tp_init */
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
