/* $Id$ */

/*
 *  (C) Copyright 2004 Leszek Krupiñski <leafnode@pld-linux.org>
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

#ifndef __PYTHON_CONFIG_H_

#define __PYTHON_CONFIG_H_

#include <Python.h>
#include <ekg/dynstuff.h>

typedef struct
{
   PyObject_HEAD;
} ekg_configObj;

void ekg_config_dealloc(PyObject *o);
int ekg_config_len(ekg_configObj *self);
PyObject* ekg_config_get(ekg_configObj * self, PyObject * key);
int ekg_config_set(ekg_configObj * self, PyObject* key, PyObject* value);

static PyMappingMethods _config_mapping = {
        (inquiry)       ekg_config_len,
        (binaryfunc)    ekg_config_get,
        (objobjargproc) ekg_config_set
};

static PyTypeObject ekg_config_type = {
        PyObject_HEAD_INIT(NULL)
        0,
        "config",
        sizeof(PyObject),
        0,
        ekg_config_dealloc,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        &_config_mapping
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
