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

#ifndef __PYTHON_EKG_H_

#define __PYTHON_EKG_H_

#include <Python.h>

PyObject * ekg_cmd_command(PyObject *self, PyObject *args);
PyObject * ekg_cmd_echo(PyObject *self, PyObject *args);
PyObject * ekg_cmd_printf(PyObject *self, PyObject *pyargs);
PyObject * ekg_cmd_debug(PyObject *self, PyObject *args);

/**
 * metody modu³u ekg
 */

PyMethodDef ekg_methods[] = {
        { "command", ekg_cmd_command, METH_VARARGS, "" },
        { "echo", ekg_cmd_echo, METH_VARARGS, "" },
        { "printf", ekg_cmd_printf, METH_VARARGS, "" },
        { "debug", ekg_cmd_debug, METH_VARARGS, "" },
        { NULL, NULL, 0, NULL }
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
