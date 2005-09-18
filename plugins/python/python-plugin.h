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

#ifndef __PYTHON_PLUGIN_H_

#define __PYTHON_PLUGIN_H_

#include <Python.h>
#include <ekg/dynstuff.h>

typedef struct
{
   PyObject_HEAD;
   char *name;
   int prio;
} ekg_pluginObj;

void ekg_plugin_dealloc(ekg_pluginObj *o);
int ekg_plugin_init(ekg_pluginObj *self, PyObject *args, PyObject *kwds);
PyObject* ekg_plugin_unload(ekg_pluginObj *self, PyObject *args);
PyObject* ekg_plugin_get_attr(ekg_pluginObj * self, char * attr);

staticforward PyMethodDef ekg_plugin_methods[] = {
	{"unload", (PyCFunction)ekg_plugin_unload, METH_NOARGS, "Unload plugin"},
    {NULL, NULL, 0, NULL}
};

static PyTypeObject ekg_plugin_type = {
        PyObject_HEAD_INIT(NULL)
        0,
        "plugin",
        sizeof(ekg_pluginObj),
        0,
        (destructor)ekg_plugin_dealloc,
        0,
        (getattrfunc)ekg_plugin_get_attr,
        0,
        0,
        0,
        0,
        0,
        0,
        0,							/*tp_hash */
        0,							/*tp_call*/
        0,							/*tp_str*/
        0,							/*tp_getattro*/
        0,							/*tp_setattro*/
        0,							/*tp_as_buffer*/
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
        "Plugin object",			/* tp_doc */
        0,							/* tp_traverse */
        0,							/* tp_clear */
        0,							/* tp_richcompare */
        0,							/* tp_weaklistoffset */
        0,							/* tp_iter */
        0,							/* tp_iternext */
        ekg_plugin_methods,		/* tp_methods */
        0,							/* tp_members */
        0,							/* tp_getset */
        0,							/* tp_base */
        0,							/* tp_dict */
        0,							/* tp_descr_get */
        0,							/* tp_descr_set */
        0,							/* tp_dictoffset */
        (initproc)ekg_plugin_init,	/* tp_init */
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
