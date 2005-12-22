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

#ifndef __PYTHON_EKG_H_

#define __PYTHON_EKG_H_

#include <Python.h>
#include <ekg/windows.h>

PyObject * python_build_session(char * name);
PyObject * python_build_window_id(int id);
PyObject * python_build_window_w(window_t *w);
PyObject * ekg_cmd_command(PyObject *self, PyObject *args);
PyObject * ekg_cmd_command_bind(PyObject *self, PyObject *args);
PyObject * ekg_cmd_handler_bind(PyObject *self, PyObject *args);
PyObject * ekg_cmd_timer_bind(PyObject * self, PyObject * args);
PyObject * ekg_cmd_variable_add(PyObject * self, PyObject * args);
PyObject * ekg_cmd_echo(PyObject *self, PyObject *args);
PyObject * ekg_cmd_printf(PyObject *self, PyObject *pyargs);
PyObject * ekg_cmd_debug(PyObject *self, PyObject *args);
PyObject * ekg_cmd_plugins(PyObject *self, PyObject *args);
PyObject * ekg_cmd_plugin_get(PyObject *self, PyObject *args);
PyObject * ekg_cmd_sessions(PyObject *self, PyObject *args);
PyObject * ekg_cmd_session_get(PyObject *self, PyObject *args);
PyObject * ekg_cmd_session_current(PyObject *self, PyObject *args);
PyObject * ekg_cmd_windows(PyObject *self, PyObject *args);
PyObject * ekg_cmd_window_get(PyObject *self, PyObject *args);
PyObject * ekg_cmd_window_new(PyObject *self, PyObject *args);
PyObject * ekg_cmd_window_current(PyObject *self, PyObject *args);


/**
 * metody modu³u ekg
 */

staticforward PyMethodDef ekg_methods[] = {
        { "command", ekg_cmd_command, METH_VARARGS, "" },
	{ "command_bind", ekg_cmd_command_bind, METH_VARARGS, "" },
	{ "handler_bind", ekg_cmd_handler_bind,  METH_VARARGS, "" },
	{ "timer_bind", ekg_cmd_timer_bind,  METH_VARARGS, "" },
        { "echo", ekg_cmd_echo, METH_VARARGS, "Print string to current window" },
        { "printf", ekg_cmd_printf, METH_VARARGS, "" },
        { "debug", ekg_cmd_debug, METH_VARARGS, "Log debug data" },
        { "plugins", ekg_cmd_plugins, METH_VARARGS, "Return list of plugins" },
        { "plugin_get", ekg_cmd_plugin_get, METH_VARARGS, "Return plugin object" },
        { "sessions", ekg_cmd_sessions, METH_VARARGS, "Return list of sessions" },
        { "session_get", ekg_cmd_session_get, METH_VARARGS, "Return session object" },
        { "session_current", ekg_cmd_session_current, METH_VARARGS, "Return session object" },
        { "variable_add", ekg_cmd_variable_add, METH_VARARGS, "Add variable with optional handler" },
        { "windows", ekg_cmd_windows, METH_VARARGS, "Return list of windows" },
        { "window_get", ekg_cmd_window_get, METH_VARARGS, "Return window with given name" },
        { "window_new", ekg_cmd_window_new, METH_VARARGS, "Create window" },
        { "window_current", ekg_cmd_window_current, METH_VARARGS, "Return current window object" },
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
