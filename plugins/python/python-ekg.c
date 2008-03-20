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
#define __USE_POSIX /* needed for fileno() */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Python.h>

#include <ekg/debug.h>
#include <ekg/dynstuff.h>

#include <ekg/commands.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/userlist.h>
#include <ekg/scripts.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#include "python.h"
#include "python-ekg.h"
#include "python-session.h"
#include "python-plugin.h"
#include "python-window.h"

// * ***************************************************************************
// *
// * Helpers
// *
// * ***************************************************************************

/**
 * python_build_session()
 *
 * return session object with given name
 *
 */

PyObject *python_build_session(char * name)
{
        ekg_sessionObj *pysession;
        char buf[100];
        session_t *s;

        debug("[python] checking for  '%s' session\n", name);
        s = session_find((const char *) name);

        if (!s) {
                snprintf(buf, 99, "Can't find session '%s'", name);
                PyErr_SetString(PyExc_KeyError, buf);
                return NULL;
        }

        debug("[python] Building object for '%s' session\n", name);
        pysession = PyObject_New(ekg_sessionObj, &ekg_session_type);
        pysession->name = xmalloc(xstrlen(name)+1);
        xstrcpy(pysession->name, name);
        Py_INCREF(pysession);
        return (PyObject *)pysession;
}

/**
 * python_build_window_id()
 *
 * return window object with given id
 *
 */

PyObject *python_build_window_id(int id)
{
        window_t *w;

        if (!(w = window_exist(id))) {
                PyErr_SetString(PyExc_RuntimeError, _("Can't find window with given id"));
                return NULL;
        }

        return (PyObject *)python_build_window_w(w);
}

/**
 * python_build_window_w()
 *
 * build window object from window_t struct
 *
 */

PyObject *python_build_window_w(window_t *w)
{
        ekg_windowObj *pywindow;

        if (!w->session && sessions) {
                w->session = sessions;
        }
        pywindow = PyObject_New(ekg_windowObj, &ekg_window_type);
        pywindow->w = w;
        Py_INCREF(pywindow);
        return (PyObject *)pywindow;
}



// * ***************************************************************************
// *
// * ekg module
// *
// * ***************************************************************************

/**
 * ekg_cmd_command()
 *
 * run ekg command
 *
 */

PyObject *ekg_cmd_command(PyObject * self, PyObject * args)
{
        char *command = NULL;

        if (!PyArg_ParseTuple(args, "s", &command)) {
                return NULL;
        }
        command_exec(NULL, NULL, command, 0);	// run command for current session
        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_variable_add()
 *
 */

PyObject *ekg_cmd_variable_add(PyObject * self, PyObject * args)
{
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        char *name = NULL;
        char *val  = NULL;
        script_t * scr = NULL;

        if (!PyArg_ParseTuple(args, "ss|O", &name, &val, &callback)) {
                return NULL;
        }

        if (callback) {
                if (!PyCallable_Check(callback)) {
                        print("generic_error", _("Third parameter to variable_add, if given, must be callable"));
                        PyErr_SetString(PyExc_TypeError, _("Third parameter to variable_add, if given, must be callable"));
                        return NULL;
                }
                Py_XINCREF(callback);
                module = PyObject_GetAttrString(callback, "__module__");
                scr = python_find_script(module);
        }

        script_var_add(&python_lang, scr, name, val, callback);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_handler_bind()
 *
 */

PyObject *ekg_cmd_handler_bind(PyObject * self, PyObject * args)
{
        char *bind_handler = NULL;
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        script_t * scr;

        if (!PyArg_ParseTuple(args, "sO", &bind_handler, &callback)) {
                return NULL;
        }

        if (!PyCallable_Check(callback)) {
                print("generic_error", _("Second parameter to handler_bind is not callable"));
                PyErr_SetString(PyExc_TypeError, _("Parameter must be callable"));
                return NULL;
        }
        Py_XINCREF(callback);

        module = PyObject_GetAttrString(callback, "__module__");
        scr = python_find_script(module);

        debug("[python] binding function to signal %s\n", bind_handler );

        script_query_bind(&python_lang, scr, bind_handler, callback);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_command_bind()
 *
 */

PyObject *ekg_cmd_command_bind(PyObject * self, PyObject * args)
{
        char *bind_command = NULL;
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        script_t * scr;

        if (!PyArg_ParseTuple(args, "sO", &bind_command, &callback)) {
                return NULL;
        }

        if (!PyCallable_Check(callback)) {
                print("generic_error", _("Second parameter to command_bind is not callable"));
                PyErr_SetString(PyExc_TypeError, _("Parameter must be callable"));
                return NULL;
        }
        Py_XINCREF(callback);

        module = PyObject_GetAttrString(callback, "__module__");
        scr = python_find_script(module);

        debug("[python] binding command %s to python function\n", bind_command);
#ifdef SCRIPTS_NEW
        script_command_bind(&python_lang, scr, bind_command, NULL, NULL, callback);
#else
        script_command_bind(&python_lang, scr, bind_command, callback);
#endif

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_timer_bind()
 *
 */

PyObject *ekg_cmd_timer_bind(PyObject * self, PyObject * args)
{
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        script_t * scr;
        int freq;

        if (!PyArg_ParseTuple(args, "iO", &freq, &callback)) {
                return NULL;
        }

        if (!PyCallable_Check(callback)) {
                print("generic_error", _("Second parameter to timer_bind is not callable"));
                PyErr_SetString(PyExc_TypeError, _("Parameter must be callable"));
                return NULL;
        }
        Py_XINCREF(callback);

        module = PyObject_GetAttrString(callback, "__module__");
        scr = python_find_script(module);

        script_timer_bind(&python_lang, scr, freq, callback);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_watch_add()
 *
 */

PyObject *ekg_cmd_watch_add(PyObject * self, PyObject * args)
{
        PyObject *callback = NULL;
        PyObject *module   = NULL;
        PyObject *fileobj  = NULL;
        script_t * scr;
        int type;
        FILE * fd;

        if (!PyArg_ParseTuple(args, "O!iiO", &PyFile_Type, &fileobj, &type, &callback)) {
                return NULL;
        }

        if (!PyCallable_Check(callback)) {
                print("generic_error", _("Second parameter to watch_add is not callable"));
                PyErr_SetString(PyExc_TypeError, _("Parameter must be callable"));
                return NULL;
        }
        Py_XINCREF(callback);

        fd = PyFile_AsFile(fileobj);
        Py_INCREF(fileobj);

        module = PyObject_GetAttrString(callback, "__module__");
        scr = python_find_script(module);

        script_watch_add(&python_lang, scr, fileno(fd), type, callback, fileobj);

        Py_INCREF(Py_None);
        return Py_None;
}


/**
 * ekg_cmd_echo()
 *
 * print text in "generic" format
 *
 */

PyObject *ekg_cmd_echo(PyObject * self, PyObject * args)
{
        char *str = NULL;
        int quiet = 0;

        if (!PyArg_ParseTuple(args, "s", &str)) {
                return NULL;
        }
        printq("generic", str);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_printf()
 *
 * extended display
 *
 */

PyObject *ekg_cmd_printf(PyObject * self, PyObject * pyargs)
{
        char *format = "generic", *args[9];
        int i;

        for (i = 0; i < 9; i++)
                args[i] = "";

        if (!PyArg_ParseTuple
                        (pyargs, "s|sssssssss:printf", &format, &args[0], &args[1],
                         &args[2], &args[3], &args[4], &args[5], &args[6], &args[7],
                         &args[8]))
                return NULL;

        print(format, args[0], args[1], args[2], args[3], args[4], args[5],
                        args[6], args[7], args[8]);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_debug()
 *
 * record debug message
 *
 */

PyObject *ekg_cmd_debug(PyObject * self, PyObject * pyargs)
{
        char *str = NULL;
        char *args[9];

        if (!PyArg_ParseTuple
                        (pyargs, "s|sssssssss:debug", &str, &args[0], &args[1], &args[2],
                         &args[3], &args[4], &args[5], &args[6], &args[7], &args[8]))
                return NULL;

        debug(str, args[0], args[1], args[2], args[3], args[4], args[5],
                        args[6], args[7], args[8]);

        Py_INCREF(Py_None);
        return Py_None;
}

/**
 * ekg_cmd_plugins()
 *
 * return plugin list
 *
 */

PyObject *ekg_cmd_plugins(PyObject * self, PyObject * pyargs)
{
        PyObject *list;
        plugin_t *p;
        int len = 0;

        for (p = plugins; p; p = p->next) {
                len++;
        }

        list = PyList_New(len);
        len = 0;

        for (p = plugins; p; p = p->next) {
                PyList_SetItem(list, len, PyString_FromString(p->name));
                len++;
        }
        Py_INCREF(list);
        return list;
}

/**
 * ekg_cmd_plugin_get()
 *
 * return plugin object
 *
 */

PyObject *ekg_cmd_plugin_get(PyObject * self, PyObject * pyargs)
{
        ekg_pluginObj *pyplugin;
        plugin_t *p;
        char *name = NULL;
        int prio = -1;

        if (!PyArg_ParseTuple(pyargs, "s:plugin_get", &name))
                return NULL;

        debug("[python] checking for plugin '%s'\n", name);

        p = plugin_find(name);
        if (p) {
                prio = p->prio;
        }

        debug("[python] Building object for plugin '%s'\n", name);
        pyplugin = PyObject_New(ekg_pluginObj, &ekg_plugin_type);
        pyplugin->loaded = prio < 0 ? 0 : 1;
        pyplugin->prio = prio < 0 ? 0 : prio;
        pyplugin->name = xmalloc(xstrlen(name)+1);
        xstrcpy(pyplugin->name, name);
        Py_INCREF(pyplugin);
        return (PyObject *)pyplugin;
}

/**
 * ekg_cmd_sessions()
 *
 * return session list
 *
 */

PyObject *ekg_cmd_sessions(PyObject * self, PyObject * pyargs)
{
        PyObject *list;
        session_t *s;
        int len = LIST_COUNT2(sessions);

        list = PyList_New(len);
        len = 0;

        for (s = sessions; s; s = s->next) {
                PyList_SetItem(list, len, python_build_session(s->uid));
                len++;
        }
        Py_INCREF(list);
        return list;
}

/**
 * ekg_cmd_session_get()
 *
 * return session object
 *
 */

PyObject *ekg_cmd_session_get(PyObject * self, PyObject * pyargs)
{
        char *name = NULL;

        if (!PyArg_ParseTuple(pyargs, "s:session_get", &name))
                return NULL;

        return (PyObject *)python_build_session(name);
}

/**
 * ekg_cmd_session_current()
 *
 * return session object
 *
 */

PyObject *ekg_cmd_session_current(PyObject * self, PyObject * pyargs)
{
	if (!session_current) {
/*		PyErr_SetString(PyExc_KeyError, "Some error?"); */
		return NULL;
	}

	return (PyObject *)python_build_session(session_current->uid);
}

/**
 * ekg_cmd_window_new()
 *
 * create window
 *
 */

PyObject *ekg_cmd_window_new(PyObject * self, PyObject * pyargs)
{
        char * name = NULL;
        window_t *w;

        if (!PyArg_ParseTuple(pyargs, "s", &name))
                return NULL;

        debug("[python] checking for window '%s'\n", name);

        w = window_find(name);
        if (w) {
                PyErr_SetString(PyExc_RuntimeError, _("Window with this name already exists"));
                return NULL;
        }

        debug("[python] Building object for window '%s'\n", name);
        w = window_new(name, window_current->session, 0);
        return (PyObject *)python_build_window_w(w);
}

/**
 * ekg_cmd_window_get()
 *
 * create window
 *
 */

PyObject *ekg_cmd_window_get(PyObject * self, PyObject * pyargs)
{
        char * name = NULL;
        window_t *w;

        if (!PyArg_ParseTuple(pyargs, "s", &name))
                return NULL;

        debug("[python] checking for window '%s'\n", name);

        w = window_find(name);
        if (!w) {
                Py_RETURN_NONE;
        } else {
                debug("[python] Building object for window '%s'\n", name);
                return (PyObject *)python_build_window_w(w);
        }
}

/**
 * ekg_cmd_windows()
 *
 * return window list
 *
 */

PyObject *ekg_cmd_windows(PyObject * self, PyObject * pyargs)
{
        PyObject *list;
        window_t *w;
        int len = LIST_COUNT2(windows);

        list = PyList_New(len);
        len = 0;

        for (w = windows; w; w = w->next) {
                PyList_SetItem(list, len, python_build_window_w(w));
                len++;
        }
        Py_INCREF(list);
        return list;
}


/**
 * ekg_cmd_window_current()
 *
 * create window
 *
 */

PyObject *ekg_cmd_window_current(PyObject * self, PyObject * pyargs)
{
        debug("[python] Building object for current'\n");
        return (PyObject *)python_build_window_w(window_current);
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
