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

#include "ekg2.h"

#include "python.h"

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include "python.h"
#include "python-config.h"

// * ***************************************************************************
// *
// * config object
// *
// * ***************************************************************************

/**
 * ekg_config_dealloc()
 *
 * deallocation of config object
 *
 */

void ekg_config_dealloc(PyObject * o)
{

}

/**
 * ekg_config_len()
 *
 * return length of config object sequence
 *
 */

int ekg_config_len(ekg_configObj * self)
{
	return g_slist_length(variables);
}

/**
 * ekg_config_get()
 *
 * return config option with given name
 *
 */

PyObject *ekg_config_get(ekg_configObj * self, PyObject * key)
{
    char *name = PyString_AsString(key);
    GSList *vl;
    debug("[python] Getting value for '%s' config option\n", name);

    for (vl = variables; vl; vl = vl->next) {
		variable_t *v = vl->data;
		if (!strcmp(v->name, name)) {
			if (v->type == VAR_BOOL || v->type == VAR_INT
					|| v->type == VAR_MAP) {
				return Py_BuildValue("i", *(int *) (v->ptr));
			} else {
				return Py_BuildValue("s", *(char **) (v->ptr));
			}
		}
    }

    return NULL;
}

/**
 * ekg_config_set()
 *
 * set configuration option
 *
 */

PyObject *ekg_config_set(ekg_configObj * self, PyObject * key, PyObject * value)
{
	char *name = PyString_AsString(key);
	variable_t *v;

	debug("[python] Setting '%s' config option to '%s'\n", name,
		PyString_AsString(value));

	v = variable_find(name);

	if (!v) {
		PyErr_SetString(PyExc_LookupError, "unknown variable");
		return NULL;
    }

    if (v->type == VAR_INT || v->type == VAR_BOOL || v->type == VAR_MAP) {
		if (!PyInt_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "invalid type");
			return NULL;
		}
		variable_set(name, ekg_itoa(PyInt_AsLong(value)));
	} else {
		if (!PyString_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "invalid type");
			return NULL;
		}
		variable_set(name, PyString_AsString(value));
    }
    Py_INCREF(Py_None);
    return Py_None;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "k&r"
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 * vim: noet
 */
